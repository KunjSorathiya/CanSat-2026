import sys
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).parents[1] / "src"))

import health
from health import RATE_WINDOW_S, LinkHealth


class HealthTests(unittest.TestCase):
    def test_counts_and_loss(self):
        h = LinkHealth()
        for _ in range(8):
            h.on_frame("packet")
            h.on_packet_result(True)
        h.on_frame("packet")
        h.on_packet_result(True, missing=2)
        self.assertEqual(h.packets_ok, 9)
        self.assertEqual(h.missing_packets, 2)
        # expected = ok + missing = 11 -> 2/11
        self.assertAlmostEqual(h.loss_pct, round(200 / 11, 1))

    def test_crc_and_status_frames(self):
        h = LinkHealth()
        h.on_frame("status")
        h.on_frame("crc")
        h.on_frame("crc")
        self.assertEqual(h.status_frames, 1)
        self.assertEqual(h.crc_errors, 2)

    def test_connected_flag(self):
        h = LinkHealth()
        self.assertFalse(h.connected)
        h.on_frame("packet")
        self.assertTrue(h.connected)

    def test_rate_matches_a_paced_stream(self):
        clock = [1000.0]
        with mock.patch.object(health.time, "monotonic", lambda: clock[0]):
            h = LinkHealth()
            for _ in range(9):          # 2 Hz for 4 s
                h.on_frame("packet")
                clock[0] += 0.5
            clock[0] -= 0.5             # sit on the last arrival
            self.assertAlmostEqual(h.rate_hz, 2.0, places=2)

    def test_burst_does_not_inflate_the_rate(self):
        # A duplicate, or a serial buffer flushing several frames at once, arrives with
        # dt == 0. That must not be reported as an enormous packet rate.
        clock = [1000.0]
        with mock.patch.object(health.time, "monotonic", lambda: clock[0]):
            h = LinkHealth()
            for _ in range(10):
                h.on_frame("packet")    # all in the same instant
            self.assertLessEqual(h.rate_hz, 10.0 / RATE_WINDOW_S)

    def test_rate_falls_to_zero_when_the_link_drops(self):
        clock = [1000.0]
        with mock.patch.object(health.time, "monotonic", lambda: clock[0]):
            h = LinkHealth()
            for _ in range(4):
                h.on_frame("packet")
                clock[0] += 0.5
            self.assertGreater(h.rate_hz, 0.0)
            clock[0] += RATE_WINDOW_S + 1.0
            self.assertEqual(h.rate_hz, 0.0)

    def test_snapshot_keys(self):
        h = LinkHealth()
        snap = h.snapshot()
        for key in ("connected", "rate_hz", "packets_ok", "missing", "loss_pct",
                    "crc_errors", "duplicates"):
            self.assertIn(key, snap)


if __name__ == "__main__":
    unittest.main()
