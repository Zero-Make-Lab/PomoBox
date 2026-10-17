"""Tests for the PomoBox helper: python -m unittest discover tools/pc-helper"""
import pathlib
import re
import unittest

import pomobox_helper as helper

ROOT = pathlib.Path(__file__).resolve().parents[2]


class OpenRequest(unittest.TestCase):
    def test_matches_the_firmware(self):
        header = (ROOT / "include" / "mirror_proto.h").read_text(encoding="utf-8")
        found = re.search(r'MIRROR_OPEN_REQUEST\s*=\s*"([^"]+)"', header)
        self.assertIsNotNone(found, "MIRROR_OPEN_REQUEST not in mirror_proto.h")
        self.assertEqual(helper.OPEN_REQUEST, found.group(1))

    def test_baud_matches_the_firmware(self):
        header = (ROOT / "include" / "mirror_proto.h").read_text(encoding="utf-8")
        self.assertEqual(helper.BAUD, int(re.search(r"MIRROR_BAUD\s*=\s*(\d+)", header).group(1)))

    def test_whole_lines_only(self):
        self.assertTrue(helper.is_open_request(b"@pomobox open-mirror"))
        self.assertTrue(helper.is_open_request(b"@pomobox open-mirror\r"))  # CRLF from Serial.println
        self.assertFalse(helper.is_open_request(b"ENC delta=1"))
        self.assertFalse(helper.is_open_request(b"x@pomobox open-mirror"))
        self.assertFalse(helper.is_open_request(b"@pomobox open-mirror please"))


class Lines(unittest.TestCase):
    def test_splits_reads_into_lines_keeping_the_partial_tail(self):
        lines, rest = helper.split_lines(b"", b"Device ON\r\n@pomobox open")
        self.assertEqual(lines, [b"Device ON\r"])
        lines, rest = helper.split_lines(rest, b"-mirror\r\nENC")
        self.assertEqual(lines, [b"@pomobox open-mirror\r"])
        self.assertEqual(rest, b"ENC")

    def test_a_tail_without_newlines_stays_bounded(self):
        _, rest = helper.split_lines(b"", b"x" * 10000)
        self.assertLessEqual(len(rest), helper.MAX_LINE)


class Ports(unittest.TestCase):
    class Port:
        def __init__(self, device, vid):
            self.device, self.vid = device, vid

    def test_only_pomobox_usb_chips(self):
        ports = [self.Port("COM1", None), self.Port("COM3", 0x10C4), self.Port("COM7", 0x303A), self.Port("COM9", 0x046D)]
        self.assertEqual(helper.pomobox_ports(ports), ["COM3", "COM7"])


if __name__ == "__main__":
    unittest.main()
