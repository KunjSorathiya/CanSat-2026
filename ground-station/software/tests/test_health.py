import sys
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).parents[1] / "src"))

import health
from health import RATE_WINDOW_S, RULEBOOK_MIN_RATE_HZ, LinkHealth


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
                    "crc_errors", "duplicates", "rate_meets_rulebook"):
            self.assertIn(key, snap)


class RulebookRateTests(unittest.TestCase):
    """The receiving end of the 1 Hz minimum.

    The vehicle refuses to build at or below 1 Hz. This is the half that says whether what
    actually arrived cleared it -- which is a different question, because the rate the
    ground station sees is the transmitted rate minus whatever the link lost.
    """

    @staticmethod
    def _run(period_s, seconds, clock_start=1000.0):
        clock = [clock_start]
        with mock.patch.object(health.time, "monotonic", lambda: clock[0]):
            h = LinkHealth()
            steps = int(seconds / period_s)
            for _ in range(steps):
                h.on_frame("packet")
                clock[0] += period_s
            # Step back onto the last arrival so `connected` and the window agree with
            # the packets that were just fed in.
            clock[0] -= period_s
            return h, h.rate_meets_rulebook, h.rate_hz

    def test_a_link_that_has_not_run_long_enough_is_not_judged(self):
        # Two packets a second apart is 1 Hz, but one second is not enough of a link to
        # call it. None means "not measured yet" and must not be confused with False.
        _, verdict, _ = self._run(period_s=1.0, seconds=2.0)
        self.assertIsNone(verdict)

    def test_a_link_with_no_packets_at_all_is_not_judged(self):
        h = LinkHealth()
        self.assertIsNone(h.rate_meets_rulebook)

    def test_the_flight_profile_rate_passes(self):
        # 700 ms is what the vehicle actually transmits at: 1.43 Hz.
        h, verdict, rate = self._run(period_s=0.7, seconds=20.0)
        self.assertTrue(verdict)
        self.assertGreater(rate, RULEBOOK_MIN_RATE_HZ)

    def test_exactly_one_hertz_passes_but_only_just(self):
        # 1.00 Hz meets the rulebook minimum, so the ground station must not report it as
        # a failure -- the vehicle is the end that refuses to sit here, not this one.
        _, verdict, rate = self._run(period_s=1.0, seconds=20.0)
        self.assertTrue(verdict)
        self.assertAlmostEqual(rate, 1.0, places=2)

    def test_a_slow_link_is_reported_as_below_the_rulebook(self):
        h, verdict, rate = self._run(period_s=2.0, seconds=40.0)
        self.assertFalse(verdict)
        self.assertLess(rate, RULEBOOK_MIN_RATE_HZ)

    def test_a_dropped_link_is_not_judged_rather_than_failed(self):
        # Silence is a transport fault, not a slow vehicle, and reporting it as
        # non-compliance would point an operator at the wrong end of the link.
        clock = [1000.0]
        with mock.patch.object(health.time, "monotonic", lambda: clock[0]):
            h = LinkHealth()
            for _ in range(30):
                h.on_frame("packet")
                clock[0] += 0.7
            self.assertTrue(h.rate_meets_rulebook)
            clock[0] += 10.0
            self.assertFalse(h.connected)
            self.assertIsNone(h.rate_meets_rulebook)


if __name__ == "__main__":
    unittest.main()
