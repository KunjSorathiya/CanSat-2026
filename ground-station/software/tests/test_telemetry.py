import csv
import tempfile
import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).parents[1] / "src"))

from logger import PacketLog, detect_missing
from telemetry import parse_packet

PACKET = "CAN-Team-01; P-001; Ti-00:00:01:000; A-10.0; Pr-101325.00; T-25.0; Ro-1.0; Pi-2.0; Ya-3.0; AX-0.10; AY-0.20; AZ-9.80;"


class TelemetryTests(unittest.TestCase):
    def test_rulebook_packet_parses(self):
        result = parse_packet(PACKET, "CAN-Team-01")
        self.assertIsNone(result.error)
        self.assertEqual(result.record.packet_number, 1)
        self.assertEqual(result.record.pressure, 101325.0)

    def test_invalid_order_is_rejected(self):
        result = parse_packet(PACKET.replace("; T-25.0", "; AX-25.0"), "CAN-Team-01")
        self.assertIsNone(result.record)
        self.assertIn("expected T-", result.error)

    def test_placeholder_team_is_rejected(self):
        result = parse_packet(PACKET.replace("CAN-Team-01", "CAN-Team-XX"))
        self.assertEqual(result.error, "invalid team identifier")

    def test_missing_packets_are_counted(self):
        self.assertEqual(detect_missing(None, 1), 0)
        self.assertEqual(detect_missing(1, 4), 2)
        self.assertEqual(detect_missing(4, 2), 0)

    def test_optional_gps_fields_are_preserved(self):
        packet = PACKET + " GP-Lat-18.5; GP-Lon-73.8; GP-Alt-12.0;"
        result = parse_packet(packet, "CAN-Team-01")
        self.assertIsNotNone(result.record)
        self.assertEqual(result.record.optional["GP-Lat"], "GP-Lat-18.5")

    def test_duplicate_packet_is_visible_to_sequence_check(self):
        self.assertEqual(detect_missing(1, 1), 0)

    def test_empty_and_corrupt_packets_are_rejected(self):
        self.assertIsNone(parse_packet("").record)
        self.assertIsNone(parse_packet(PACKET.replace("A-10.0", "A-not-a-number")).record)

    def test_precision_and_extra_field_behavior(self):
        result = parse_packet(PACKET.replace("A-10.0", "A-10"))
        self.assertIsNone(result.record)
        extra = parse_packet(PACKET + " MQ-1;")
        self.assertIsNotNone(extra.record)

    def test_raw_and_parsed_logging(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            log = PacketLog(root / "raw.tsv", root / "telemetry.csv")
            result = parse_packet(PACKET)
            log.append(PACKET, result.record, result.error)
            self.assertIn(PACKET, (root / "raw.tsv").read_text(encoding="utf-8"))
            with (root / "telemetry.csv").open(encoding="utf-8") as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(rows[0]["packet_number"], "1")


if __name__ == "__main__":
    unittest.main()
