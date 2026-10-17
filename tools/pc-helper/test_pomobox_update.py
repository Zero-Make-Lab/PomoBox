"""Tests for the button-free updater: python -m unittest discover tools/pc-helper"""
import pathlib
import re
import unittest

import pomobox_update as updater

ROOT = pathlib.Path(__file__).resolve().parents[2]


class Status(unittest.TestCase):
    def test_reads_each_reply(self):
        self.assertEqual(updater.parse_status(b"@pomobox update ready\r"), ("ready", None))
        self.assertEqual(updater.parse_status(b"@pomobox update ack 8192\r"), ("ack", 8192))
        self.assertEqual(updater.parse_status(b"@pomobox update done"), ("done", None))
        self.assertEqual(updater.parse_status(b"@pomobox update error checksum\r"), ("error", "checksum"))

    def test_ignores_other_lines_and_finds_a_reply_after_frame_bytes(self):
        self.assertIsNone(updater.parse_status(b"ENC delta=1"))
        self.assertIsNone(updater.parse_status(b"@pomobox open-mirror"))
        self.assertEqual(updater.parse_status(b"\xa5R\x0a\x00@pomobox update ready"), ("ready", None))

    def test_a_malformed_ack_is_not_progress(self):
        self.assertIsNone(updater.parse_status(b"@pomobox update ack lots"))


class Command(unittest.TestCase):
    def test_command_matches_what_the_firmware_parses(self):
        self.assertEqual(updater.update_command(b"abc"), b"update 3 900150983cd24fb0d6963f7d28e17f72\n")

    def test_block_size_matches_the_firmware(self):
        header = (ROOT / "include" / "serial_update.h").read_text(encoding="utf-8")
        self.assertEqual(updater.BLOCK, int(re.search(r"UPDATE_BLOCK\s*=\s*(\d+)", header).group(1)))

    def test_refuses_an_image_too_big_for_the_slot(self):
        header = (ROOT / "include" / "serial_update.h").read_text(encoding="utf-8")
        limit = int(re.search(r"UPDATE_MAX_SIZE\s*=\s*(0x[0-9A-Fa-f]+|\d+)", header).group(1), 0)
        self.assertEqual(updater.MAX_SIZE, limit)

    def test_rejects_a_merged_image(self):
        # A merged image starts with the bootloader at 0x1000 (0xFF padding before it); an app image starts with 0xE9
        self.assertIsNotNone(updater.image_problem(b"\xff" * 4096 + b"\xe9"))
        self.assertIsNone(updater.image_problem(b"\xe9" + b"\x00" * 100))
        self.assertIsNotNone(updater.image_problem(b""))


if __name__ == "__main__":
    unittest.main()
