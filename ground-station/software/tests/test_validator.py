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
