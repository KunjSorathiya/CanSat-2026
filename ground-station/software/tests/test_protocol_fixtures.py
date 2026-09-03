"""The shared protocol fixtures, from the Python ground station's point of view.

`test-data/protocol-fixtures.tsv` is read by three parsers:

    firmware/common/src/telemetry.cpp        (C++, flight + shared library)
    ground-station/software/src/telemetry.py (this one)
    ground-station/web/index.html            (JavaScript, web console)

They must agree on every case. A ground station that accepts a packet the transmitter
would never send — or rejects one it does send — miscounts packet loss, which is the
number an operator uses to judge the link.
"""

import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(Path(__file__).parents[1] / "src"))

from telemetry import parse_packet  # noqa: E402

FIXTURE_FILE = REPO_ROOT / "test-data" / "protocol-fixtures.tsv"


def load_fixtures():
    cases = []
    with FIXTURE_FILE.open(encoding="utf-8") as handle:
        for raw in handle:
            line = raw.rstrip("\n").rstrip("\r")
            if not line.strip() or line.startswith("#"):
                continue
            parts = line.split("\t", 2)
            if len(parts) != 3:
                raise AssertionError(f"malformed fixture line: {line!r}")
            cases.append(tuple(parts))
    return cases


class ProtocolFixtureTests(unittest.TestCase):
    def setUp(self):
        self.cases = load_fixtures()

    def test_fixture_file_is_present_and_complete(self):
        self.assertTrue(FIXTURE_FILE.is_file(), f"missing {FIXTURE_FILE}")
        self.assertGreaterEqual(len(self.cases), 30)
        ids = [case_id for case_id, _, _ in self.cases]
        self.assertEqual(len(ids), len(set(ids)), "duplicate fixture ids")
        for _, expect, _ in self.cases:
            self.assertIn(expect, ("ok", "err"))

    def test_every_fixture_parses_to_its_recorded_verdict(self):
        for case_id, expect, packet in self.cases:
            with self.subTest(fixture=case_id):
                result = parse_packet(packet)
                verdict = "ok" if result.record is not None else "err"
                self.assertEqual(
                    verdict, expect,
                    f"{case_id}: expected {expect}, got {verdict} ({result.error})")

    def test_rejected_fixtures_all_carry_a_reason(self):
        # A rejection with no explanation is undiagnosable during a mission.
        for case_id, expect, packet in self.cases:
            if expect != "err":
                continue
            with self.subTest(fixture=case_id):
                result = parse_packet(packet)
                self.assertIsNotNone(result.error)
                self.assertNotEqual(result.error.strip(), "")

    def test_accepted_fixtures_expose_every_mandatory_field(self):
        mandatory = ("team_id", "packet_number", "timestamp", "altitude", "pressure",
                     "temperature", "roll", "pitch", "yaw", "ax", "ay", "az")
        for case_id, expect, packet in self.cases:
            if expect != "ok":
                continue
            with self.subTest(fixture=case_id):
                record = parse_packet(packet).record
                for field in mandatory:
                    self.assertTrue(hasattr(record, field), f"{case_id} lacks {field}")

    def test_packet_number_rule_matches_the_other_implementations(self):
        # Digits only, no sign, no interior space, inside the 32-bit counter. These four
        # are the cases where int() alone would have disagreed with C++ and JavaScript.
        for case_id in ("packet_number_zero", "negative_packet_number",
                        "packet_number_overflow", "packet_number_spaced"):
            packet = next(p for i, _, p in self.cases if i == case_id)
            with self.subTest(fixture=case_id):
                self.assertIsNone(parse_packet(packet).record)


if __name__ == "__main__":
    unittest.main()
