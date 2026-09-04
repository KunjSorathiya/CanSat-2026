import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / "src"))

from telemetry import parse_packet
from validator import StreamValidator


def packet(number: int, team: str = "CAN-Team-01", ts: str = "00:00:01:000",
           extra: str = "") -> str:
    base = (
        f"{team}; P-{number:03d}; Ti-{ts}; A-10.0; Pr-101325.00; T-25.0; "
        f"Ro-1.0; Pi-2.0; Ya-3.0; AX-0.10; AY-0.20; AZ-9.80;"
    )
    return base + extra


def record(number: int, **kw):
    result = parse_packet(packet(number, **kw))
    assert result.record is not None, result.error
    return result.record


class ValidatorTests(unittest.TestCase):
    def test_sequential_ok(self):
        v = StreamValidator("CAN-Team-01")
        for n in range(1, 5):
            report = v.check(record(n))
            self.assertTrue(report.accepted)
            self.assertEqual(report.missing, 0)
        self.assertEqual(v.stats.accepted, 4)
        self.assertEqual(v.stats.missing, 0)

    def test_missing_packets_counted(self):
        v = StreamValidator("CAN-Team-01")
        v.check(record(1))
        report = v.check(record(4))
        self.assertEqual(report.missing, 2)
        self.assertEqual(v.stats.missing, 2)

    def test_duplicate_detected(self):
        v = StreamValidator("CAN-Team-01")
        v.check(record(1))
        v.check(record(2))
        report = v.check(record(2))
        self.assertTrue(report.duplicate)
        self.assertEqual(v.stats.duplicates, 1)

    def test_out_of_order_detected(self):
        v = StreamValidator("CAN-Team-01")
        v.check(record(5))
        report = v.check(record(3))
        self.assertTrue(report.out_of_order)
        self.assertEqual(v.stats.out_of_order, 1)

    def test_wrong_team_rejected(self):
        v = StreamValidator("CAN-Team-01")
        report = v.check(record(1, team="CAN-Team-99"))
        self.assertFalse(report.accepted)
        self.assertEqual(v.stats.wrong_team, 1)

    def test_timestamp_regression_noted(self):
        v = StreamValidator("CAN-Team-01")
        v.check(record(1, ts="00:00:05:000"))
        report = v.check(record(2, ts="00:00:01:000"))
        self.assertIn("timestamp regression", "; ".join(report.notes))
        self.assertEqual(v.stats.timestamp_regressions, 1)

    def test_implausible_gps_flagged_but_packet_kept(self):
        v = StreamValidator("CAN-Team-01")
        report = v.check(record(1, extra=" GP-Lat-991.0; GP-Lon-73.8;"))
        self.assertTrue(report.accepted)
        self.assertEqual(v.stats.gps_rejected, 1)
        self.assertIn("implausible GPS fix ignored", "; ".join(report.notes))

    def test_valid_gps_not_flagged(self):
        v = StreamValidator("CAN-Team-01")
        report = v.check(record(1, extra=" GP-Lat-18.5; GP-Lon-73.8; GP-Alt-12.0;"))
        self.assertEqual(v.stats.gps_rejected, 0)
        self.assertEqual(report.notes, [])

    def test_diagnostic_tags_are_parsed(self):
        rec = record(1, extra=" GP-Lat-18.5; GP-Lon-73.8; MODE-FLIGHT; FAULTS-2;")
        self.assertEqual(rec.mode, "FLIGHT")
        self.assertEqual(rec.fault_count, 2)
        self.assertAlmostEqual(rec.gps_lat, 18.5)


if __name__ == "__main__":
    unittest.main()


class VehicleRestartTests(unittest.TestCase):
    """The vehicle's watchdog is designed to reboot it. The ground station must cope."""

    def _record(self, number, timestamp_ms):
        packet = (f"CAN-Team-01; P-{number:03d}; "
                  f"Ti-{timestamp_ms // 3600000:02d}:"
                  f"{timestamp_ms // 60000 % 60:02d}:"
                  f"{timestamp_ms // 1000 % 60:02d}:{timestamp_ms % 1000:03d}; "
                  "A-10.0; Pr-101325.00; T-25.0; Ro-1.0; Pi-2.0; Ya-3.0; "
                  "AX-0.10; AY-0.20; AZ-9.80;")
        result = parse_packet(packet, "CAN-Team-01")
        self.assertIsNotNone(result.record, result.error)
        return result.record

    def test_a_reboot_is_recognised_rather_than_read_as_duplicates(self):
        validator = StreamValidator("CAN-Team-01")
        for n in range(1, 21):
            validator.check(self._record(n, n * 1000))

        # Watchdog reboot: the counter restarts at P-001 and the mission clock at zero.
        report = validator.check(self._record(1, 0))
        self.assertTrue(report.restarted)
        self.assertFalse(report.duplicate)
        self.assertFalse(report.out_of_order)
        self.assertEqual(validator.stats.restarts, 1)

        # The rest of the flight counts cleanly instead of being reported as duplicates.
        for n in range(2, 11):
            follow_up = validator.check(self._record(n, n * 1000))
            self.assertFalse(follow_up.duplicate, f"P-{n} after restart")
            self.assertFalse(follow_up.out_of_order, f"P-{n} after restart")
        self.assertEqual(validator.stats.duplicates, 0)
        self.assertEqual(validator.stats.out_of_order, 0)

    def test_loss_counting_survives_a_reboot(self):
        validator = StreamValidator("CAN-Team-01")
        for n in range(1, 11):
            validator.check(self._record(n, n * 1000))
        validator.check(self._record(1, 0))          # reboot
        validator.check(self._record(2, 2000))
        validator.check(self._record(5, 5000))       # 3 genuinely lost after the reboot
        self.assertEqual(validator.stats.missing, 2)

    def test_a_real_duplicate_is_still_a_duplicate(self):
        validator = StreamValidator("CAN-Team-01")
        for n in range(1, 6):
            validator.check(self._record(n, n * 1000))
        report = validator.check(self._record(3, 3000))
        self.assertTrue(report.duplicate)
        self.assertFalse(report.restarted)
        self.assertEqual(validator.stats.restarts, 0)

    def test_a_corrupted_p001_without_a_clock_regression_is_not_a_restart(self):
        # A counter restart alone could be a corrupted packet number. Both signals are
        # required, or a single bad packet would reset the ground station's whole view.
        validator = StreamValidator("CAN-Team-01")
        for n in range(1, 11):
            validator.check(self._record(n, n * 1000))
        report = validator.check(self._record(1, 20000))  # clock still moving forward
        self.assertFalse(report.restarted)
        self.assertTrue(report.duplicate)

    def test_a_clock_regression_without_p001_is_not_a_restart(self):
        validator = StreamValidator("CAN-Team-01")
        for n in range(1, 11):
            validator.check(self._record(n, n * 1000))
        report = validator.check(self._record(11, 500))   # timestamp glitch only
        self.assertFalse(report.restarted)
        self.assertEqual(validator.stats.timestamp_regressions, 1)

    def test_the_first_packet_of_a_session_is_never_a_restart(self):
        validator = StreamValidator("CAN-Team-01")
        report = validator.check(self._record(1, 0))
        self.assertFalse(report.restarted)
        self.assertEqual(validator.stats.restarts, 0)

    def test_multiple_reboots_are_each_counted(self):
        validator = StreamValidator("CAN-Team-01")
        for _ in range(3):
            for n in range(1, 6):
                validator.check(self._record(n, n * 1000))
        self.assertEqual(validator.stats.restarts, 2)
