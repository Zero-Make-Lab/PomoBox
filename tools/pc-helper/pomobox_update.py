"""Update a PomoBox over USB without the BOOT button.

The PomoBox's running firmware takes the new app over its serial link and installs it itself
(include/serial_update.h); its settings stay. Needs firmware that already has this (any build from 2026-10-17 on);
older firmware needs one install the old way, holding BOOT.

    python pomobox_update.py .pio/build/esp32dev/firmware.bin
    python pomobox_update.py firmware.bin --port COM3
    python pomobox_update.py firmware.bin --paced          # only for a PomoBox on v1.1.0 (see below)

v1.1.0 wrote flash while bytes were still arriving, which loses some at full speed. --paced sends each block in
pieces small enough for the UART's 128-byte FIFO, with a gap after each, so v1.1.0 can take the update that fixes it.

The file is the app image PlatformIO builds (firmware.bin), not a merged image. Close the mirror page and stop the
PomoBox helper first: the port can only be open in one place.
"""
import argparse
import hashlib
import sys
import time

BAUD = 921600
BLOCK = 4096            # UPDATE_BLOCK in serial_update.h
MAX_SIZE = 0x140000     # UPDATE_MAX_SIZE: one app slot
PIECE, GAP_S = 96, 0.012  # --paced: under the UART FIFO, and longer than a flash write
REPLY = b"@pomobox update "
READY_S, ACK_S, DONE_S = 20, 10, 20  # ready comes after the PomoBox erases the target area


def update_command(image: bytes) -> bytes:
    return f"update {len(image)} {hashlib.md5(image).hexdigest()}\n".encode()


def pieces(block: bytes, paced: bool):
    """The writes for one block: all at once, or --paced in FIFO-sized pieces."""
    if not paced:
        return [block]
    return [block[i:i + PIECE] for i in range(0, len(block), PIECE)]


def image_problem(image: bytes):
    """Why this file can't go in an app slot, or None."""
    if not image:
        return "the file is empty"
    if len(image) > MAX_SIZE:
        return f"the image is {len(image)} bytes; one app slot holds {MAX_SIZE}"
    if image[0] != 0xE9:
        return "this is not an ESP32 app image (a merged image from 0x0 goes through zeroFlash or esptool instead)"
    return None


def parse_status(line: bytes):
    """("ready"|"done", None), ("ack", bytes received), ("error", reason) for an update reply, else None."""
    at = line.find(REPLY)
    if at < 0:
        return None
    words = line[at + len(REPLY):].decode("ascii", "replace").strip().split()
    if not words:
        return None
    if words[0] in ("ready", "done"):
        return words[0], None
    if words[0] == "ack" and len(words) == 2 and words[1].isdigit():
        return "ack", int(words[1])
    if words[0] == "error":
        return "error", " ".join(words[1:]) or "unknown"
    return None


class Link:
    def __init__(self, port):
        self.port, self.pending = port, b""

    def wait(self, want, seconds):
        """The next update reply, failing on an error reply or silence."""
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            *lines, self.pending = (self.pending + self.port.read(4096)).split(b"\n")
            self.pending = self.pending[-512:]
            for line in lines:
                status = parse_status(line)
                if status and status[0] == "error":
                    raise RuntimeError(f"the PomoBox stopped the update: {status[1]}")
                if status and status[0] in want:
                    return status
        raise RuntimeError(f"no answer from the PomoBox (waited {seconds} s for {'/'.join(want)})")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("image", help="firmware.bin built by PlatformIO")
    parser.add_argument("--port", help="serial port (default: the first PomoBox found)")
    parser.add_argument("--paced", action="store_true", help="slow, safe sending for a PomoBox on v1.1.0")
    args = parser.parse_args()

    import serial
    from serial.tools import list_ports
    from pomobox_helper import pomobox_ports

    image = open(args.image, "rb").read()
    problem = image_problem(image)
    if problem:
        sys.exit(f"Can't update: {problem}.")
    device = args.port or next(iter(pomobox_ports(list_ports.comports())), None)
    if not device:
        sys.exit("No PomoBox found on USB.")

    port = serial.Serial()
    port.port, port.baudrate, port.timeout = device, BAUD, 0.2
    port.dtr = port.rts = False  # released before opening: no reset
    try:
        port.open()
    except serial.SerialException:
        sys.exit(f"{device} is busy. Close the mirror page and stop the PomoBox helper, then try again.")

    link = Link(port)
    try:
        print(f"Updating the PomoBox on {device}: {len(image)} bytes")
        port.write(update_command(image))
        link.wait({"ready"}, READY_S)
        started = time.monotonic()
        for offset in range(0, len(image), BLOCK):
            block = image[offset:offset + BLOCK]
            for piece in pieces(block, args.paced):
                port.write(piece)
                if args.paced:
                    port.flush()
                    time.sleep(GAP_S)
            sent = offset + len(block)
            while link.wait({"ack"}, ACK_S)[1] < sent:
                pass
            print(f"\r  {sent * 100 // len(image):3d} %", end="", flush=True)
        link.wait({"done"}, DONE_S)
        print(f"\nDone in {time.monotonic() - started:.1f} s. The PomoBox checked the image and is restarting.")
    except RuntimeError as error:
        sys.exit(f"\nUpdate failed: {error}. The PomoBox keeps its current firmware.")
    finally:
        port.close()


if __name__ == "__main__":
    main()
