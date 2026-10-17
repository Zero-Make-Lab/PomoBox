"""PomoBox helper: opens the PC mirror page when the PomoBox asks for it (Settings > PC Mirror).

While no page is connected it holds the PomoBox's serial port and listens for the firmware's open request
(MIRROR_OPEN_REQUEST in include/mirror_proto.h). When the request comes it lets go of the port and opens the page
in the browser; the page connects to the port by itself (Chrome remembers ports it was allowed). When the page lets
go again, the helper goes back to listening.

It opens the port with DTR and RTS released, so it never resets the board.

    python pomobox_helper.py                                   # opens https://zeromakelab.com/mirror
    python pomobox_helper.py --url http://localhost:4237/mirror

Needs pyserial (pip install pyserial). Stop it before flashing the board: the flasher needs the port too.
"""
import argparse
import time
import webbrowser

OPEN_REQUEST = "@pomobox open-mirror"
BAUD = 921600
MAX_LINE = 256
DEFAULT_URL = "https://zeromakelab.com/mirror"
# The USB chips PomoBox boards use: Silicon Labs CP210x, Espressif native USB, WCH CH34x, FTDI, Raspberry Pi RP2040
VENDORS = {0x10C4, 0x303A, 0x1A86, 0x0403, 0x2E8A}
RETRY_S = 2       # port missing or busy (the page has it): look again this often
HANDOFF_S = 20    # after opening the page, give it this long to take the port before listening again


def is_open_request(line: bytes) -> bool:
    return line.rstrip(b"\r") == OPEN_REQUEST.encode()


def split_lines(pending: bytes, chunk: bytes):
    """Complete lines in pending + chunk, and the unfinished tail (bounded)."""
    *lines, rest = (pending + chunk).split(b"\n")
    return lines, rest[-MAX_LINE:]


def pomobox_ports(ports):
    return [p.device for p in ports if p.vid in VENDORS]


def listen(device, url, log):
    """Holds device until the PomoBox asks for the page. True when it opened the page."""
    import serial

    port = serial.Serial()
    port.port, port.baudrate, port.timeout = device, BAUD, 0.5
    port.dtr = port.rts = False  # released before opening: no reset
    port.open()
    log(f"Listening to the PomoBox on {device}")
    pending = b""
    try:
        while True:
            lines, pending = split_lines(pending, port.read(4096))
            if any(is_open_request(line) for line in lines):
                log(f"The PomoBox asked for the mirror: opening {url}")
                port.close()
                webbrowser.open(url)
                return True
    finally:
        if port.is_open:
            port.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--url", default=DEFAULT_URL, help=f"page to open (default {DEFAULT_URL})")
    args = parser.parse_args()
    log = lambda text: print(time.strftime("%H:%M:%S"), text, flush=True)

    from serial import SerialException
    from serial.tools import list_ports

    log("PomoBox helper running. On the PomoBox: hold the knob, then Settings > PC Mirror.")
    waiting = None
    while True:
        devices = pomobox_ports(list_ports.comports())
        if not devices:
            if waiting != "plug":
                log("Waiting for a PomoBox on USB")
                waiting = "plug"
            time.sleep(RETRY_S)
            continue
        try:
            waiting = None
            if listen(devices[0], args.url, log):
                time.sleep(HANDOFF_S)
        except (SerialException, OSError):
            # Busy (the page has the port) or unplugged mid-read: try again shortly
            if waiting != "busy":
                log(f"{devices[0]} is in use (the mirror page has it); waiting")
                waiting = "busy"
            time.sleep(RETRY_S)
        except KeyboardInterrupt:
            return


if __name__ == "__main__":
    main()
