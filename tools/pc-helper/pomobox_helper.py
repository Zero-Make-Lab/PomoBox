"""PomoBox helper: opens the PC mirror page when the PomoBox asks for it (Settings > PC Mirror).

While no page is connected it holds the PomoBox's serial port and listens for the firmware's open request
(MIRROR_OPEN_REQUEST in include/mirror_proto.h). When the request comes it lets go of the port and opens the page
in the browser; the page connects to the port by itself (Chrome remembers ports it was allowed). When the page lets
go again, the helper goes back to listening.

It opens the port with DTR and RTS released, so it never resets the board.

Anything else that needs the port (zeroFlash, the mirror page opened by hand, a flasher) asks the helper to step
aside first: POST http://127.0.0.1:47321/release. The helper lets go of the port and stays off it for LEASE_S
seconds; asking again extends that, so zeroFlash asks every few seconds while it installs. Only this computer can
reach it, and only the ZeroMakeLab site and local previews may ask from a browser.

    python pomobox_helper.py                                   # opens https://zeromakelab.com/mirror
    python pomobox_helper.py --url http://localhost:4237/mirror
    python pomobox_helper.py --log helper.log                  # for pythonw (no console), e.g. at Windows start
    python pomobox_helper.py --release                         # ask a running helper to let go, e.g. before pio upload

Needs pyserial (pip install pyserial).
"""
import argparse
import json
import threading
import time
import urllib.request
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

OPEN_REQUEST = "@pomobox open-mirror"
BAUD = 921600
MAX_LINE = 256
DEFAULT_URL = "https://zeromakelab.com/mirror"
# The USB chips PomoBox boards use: Silicon Labs CP210x, Espressif native USB, WCH CH34x, FTDI, Raspberry Pi RP2040
VENDORS = {0x10C4, 0x303A, 0x1A86, 0x0403, 0x2E8A}
RETRY_S = 2       # port missing or busy (the page has it): look again this often
HANDOFF_S = 20    # after opening the page, give it this long to take the port before listening again
RELEASE_PORT = 47321          # http://127.0.0.1:47321/release
LEASE_S = 15                  # a release keeps the helper off the port this long; asking again extends it
RELEASE_WAIT_S = 2.0          # the release answer waits at most this long for the port to be closed
ALLOWED_ORIGINS = ("https://zeromakelab.com", "https://www.zeromakelab.com")
LOCAL_ORIGINS = ("http://localhost:", "http://127.0.0.1:")


def is_open_request(line: bytes) -> bool:
    return line.rstrip(b"\r") == OPEN_REQUEST.encode()


def split_lines(pending: bytes, chunk: bytes):
    """Complete lines in pending + chunk, and the unfinished tail (bounded)."""
    *lines, rest = (pending + chunk).split(b"\n")
    return lines, rest[-MAX_LINE:]


def pomobox_ports(ports):
    return [p.device for p in ports if p.vid in VENDORS]


def origin_allowed(origin):
    """Browsers say who is asking. None: not a browser (curl, --release), which is fine on this computer."""
    if origin is None:
        return True
    return origin in ALLOWED_ORIGINS or any(origin.startswith(o) and origin[len(o):].isdigit() for o in LOCAL_ORIGINS)


class Lease:
    """Whether the helper must stay off the port: a release lasts LEASE_S from the latest ask."""

    def __init__(self, clock=time.monotonic):
        self.clock, self.until = clock, 0.0
        self.lock = threading.Lock()
        self.port_closed = threading.Event()
        self.port_closed.set()  # nothing held at start

    def release(self):
        with self.lock:
            self.until = max(self.until, self.clock() + LEASE_S)

    def active(self):
        with self.lock:
            return self.clock() < self.until

    def remaining(self):
        with self.lock:
            return max(0.0, self.until - self.clock())


def listen(device, url, log, lease=None):
    """Holds device until the PomoBox asks for the page (True) or something else needs the port (False)."""
    import serial

    lease = lease or Lease()
    port = serial.Serial()
    port.port, port.baudrate, port.timeout = device, BAUD, 0.25
    port.dtr = port.rts = False  # released before opening: no reset
    port.open()
    lease.port_closed.clear()
    log(f"Listening to the PomoBox on {device}")
    pending = b""
    try:
        while True:
            if lease.active():
                log(f"Letting go of {device}: another program asked for it")
                return False
            lines, pending = split_lines(pending, port.read(4096))
            if any(is_open_request(line) for line in lines):
                log(f"The PomoBox asked for the mirror: opening {url}")
                port.close()
                lease.port_closed.set()
                webbrowser.open(url)
                return True
    finally:
        if port.is_open:
            port.close()
        lease.port_closed.set()


def release_handler(lease, log):
    class Handler(BaseHTTPRequestHandler):
        def _cors(self):
            origin = self.headers.get("Origin")
            if origin and origin_allowed(origin):
                self.send_header("Access-Control-Allow-Origin", origin)
                self.send_header("Vary", "Origin")
                self.send_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS")
                self.send_header("Access-Control-Allow-Private-Network", "true")  # Chrome's local-network preflight

        def _answer(self, status, body=None):
            data = json.dumps(body).encode() if body is not None else b""
            self.send_response(status)
            self._cors()
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(data)

        def do_OPTIONS(self):
            self._answer(204 if origin_allowed(self.headers.get("Origin")) else 403)

        def do_POST(self):
            if self.path.split("?")[0] != "/release":
                return self._answer(404, {"error": "not found"})
            if not origin_allowed(self.headers.get("Origin")):
                return self._answer(403, {"error": "origin not allowed"})
            lease.release()
            closed = lease.port_closed.wait(RELEASE_WAIT_S)
            self._answer(200, {"released": closed, "seconds": round(lease.remaining())})

        do_GET = do_POST  # simple to call by hand: curl or a browser address bar

        def log_message(self, *args):  # requests are routine: keep the log for what matters
            pass

    return Handler


class ReleaseServer(ThreadingHTTPServer):
    # HTTPServer turns on SO_REUSEADDR, which on Windows lets a second process bind the same port: one helper only
    allow_reuse_address = False
    daemon_threads = True


def serve_release(lease, log, port=RELEASE_PORT):
    """The release endpoint, on this computer only. None when another helper already has it."""
    try:
        server = ReleaseServer(("127.0.0.1", port), release_handler(lease, log))
    except OSError:
        return None
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def ask_release(port=RELEASE_PORT):
    with urllib.request.urlopen(urllib.request.Request(f"http://127.0.0.1:{port}/release", method="POST"), timeout=5) as r:
        return json.loads(r.read() or b"{}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--url", default=DEFAULT_URL, help=f"page to open (default {DEFAULT_URL})")
    parser.add_argument("--log", help="append the log to this file (for pythonw, which has no console)")
    parser.add_argument("--release", action="store_true", help="ask a running helper to let go of the port, then exit")
    args = parser.parse_args()

    if args.release:
        try:
            print(f"Helper let go of the port for {ask_release().get('seconds', LEASE_S)} s")
        except OSError:
            print("No PomoBox helper running")
        return

    def log(text):
        line = f"{time.strftime('%Y-%m-%d %H:%M:%S')} {text}"
        if args.log:
            with open(args.log, "a", encoding="utf-8") as f:
                f.write(line + "\n")
        else:
            print(line, flush=True)

    from serial import SerialException
    from serial.tools import list_ports

    lease = Lease()
    if serve_release(lease, log) is None:
        log(f"Another PomoBox helper is already running (127.0.0.1:{RELEASE_PORT} is taken); exiting")
        return
    log("PomoBox helper running. On the PomoBox: hold the knob, then Settings > PC Mirror.")
    waiting = None
    while True:
        if lease.active():
            if waiting != "released":
                log("Staying off the port while another program uses it")
                waiting = "released"
            time.sleep(min(1.0, lease.remaining()) or 0.1)
            continue
        devices = pomobox_ports(list_ports.comports())
        if not devices:
            if waiting != "plug":
                log("Waiting for a PomoBox on USB")
                waiting = "plug"
            time.sleep(RETRY_S)
            continue
        try:
            waiting = None
            if listen(devices[0], args.url, log, lease):
                time.sleep(HANDOFF_S)
        except (SerialException, OSError):
            # Busy (the page or a flasher has the port) or unplugged mid-read: try again shortly
            if waiting != "busy":
                log(f"{devices[0]} is in use (the mirror page or a flasher has it); waiting")
                waiting = "busy"
            time.sleep(RETRY_S)
        except KeyboardInterrupt:
            return


if __name__ == "__main__":
    main()
