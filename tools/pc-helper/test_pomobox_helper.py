"""Tests for the PomoBox helper: python -m unittest discover tools/pc-helper"""
import json
import pathlib
import re
import threading
import unittest
import urllib.error
import urllib.request

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


class Page(unittest.TestCase):
    def test_opens_the_mirror_marked_as_from_the_helper(self):
        # the site then knows a helper runs here, and may ask it to let go of the port (public/flash/helper.js)
        self.assertEqual(helper.DEFAULT_URL, "https://zeromakelab.com/mirror?helper=1")


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


class Release(unittest.TestCase):
    """The release endpoint: zeroFlash and the mirror page ask the helper to let go of the port first."""

    def test_lease_lasts_from_the_latest_ask(self):
        now = [100.0]
        lease = helper.Lease(clock=lambda: now[0])
        self.assertFalse(lease.active())
        lease.release()
        now[0] += helper.LEASE_S - 1
        self.assertTrue(lease.active())
        lease.release()                      # asking again extends it
        now[0] += helper.LEASE_S - 1
        self.assertTrue(lease.active())
        now[0] += 2
        self.assertFalse(lease.active())

    def test_only_this_site_and_local_previews_may_ask_from_a_browser(self):
        for origin in ("https://zeromakelab.com", "https://www.zeromakelab.com", "http://localhost:4231",
                       "http://127.0.0.1:4173", None):
            self.assertTrue(helper.origin_allowed(origin), origin)
        for origin in ("https://evil.example", "http://zeromakelab.com", "https://zeromakelab.com.evil.example",
                       "http://localhost:4231.evil.example", "null"):
            self.assertFalse(helper.origin_allowed(origin), origin)

    def _server(self, lease):
        server = helper.serve_release(lease, lambda text: None, port=0)
        self.addCleanup(server.shutdown)
        return f"http://127.0.0.1:{server.server_address[1]}"

    def _post(self, url, origin=None, method="POST"):
        request = urllib.request.Request(url, method=method, headers={"Origin": origin} if origin else {})
        try:
            with urllib.request.urlopen(request, timeout=5) as response:
                return response.status, dict(response.headers), response.read()
        except urllib.error.HTTPError as error:
            return error.code, dict(error.headers), b""

    def test_release_from_the_site_lets_go_and_says_so(self):
        lease = helper.Lease()
        lease.port_closed.clear()            # the helper holds the port...
        threading.Timer(0.3, lease.port_closed.set).start()  # ...and its listen loop closes it soon after
        status, headers, body = self._post(self._server(lease) + "/release", "https://zeromakelab.com")
        self.assertEqual(status, 200)
        self.assertEqual(headers["Access-Control-Allow-Origin"], "https://zeromakelab.com")
        self.assertTrue(json.loads(body)["released"])
        self.assertTrue(lease.active())

    def test_another_site_cannot_make_it_let_go(self):
        lease = helper.Lease()
        status, headers, _ = self._post(self._server(lease) + "/release", "https://evil.example")
        self.assertEqual(status, 403)
        self.assertNotIn("Access-Control-Allow-Origin", headers)
        self.assertFalse(lease.active())

    def test_preflight_allows_the_private_network_request(self):
        status, headers, _ = self._post(self._server(helper.Lease()) + "/release", "https://zeromakelab.com", "OPTIONS")
        self.assertEqual(status, 204)
        self.assertEqual(headers["Access-Control-Allow-Private-Network"], "true")

    def test_a_second_helper_does_not_start(self):
        server = helper.serve_release(helper.Lease(), lambda text: None, port=0)
        self.addCleanup(server.shutdown)
        self.assertIsNone(helper.serve_release(helper.Lease(), lambda text: None, port=server.server_address[1]))


if __name__ == "__main__":
    unittest.main()
