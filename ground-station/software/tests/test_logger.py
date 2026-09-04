"""Logging robustness.

The raw log is the forensic record of a flight: it keeps corrupted payloads verbatim, so
it has to survive content that would otherwise break its own format. And a logging failure
must never take reception down with it — telemetry is worth more than its log.
"""

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / "src"))

from logger import PacketLog, detect_missing, escape_raw, unescape_raw  # noqa: E402
from telemetry import parse_packet  # noqa: E402

PACKET = ("CAN-Team-01; P-001; Ti-00:00:01:000; A-10.0; Pr-101325.00; T-25.0; "
          "Ro-1.0; Pi-2.0; Ya-3.0; AX-0.10; AY-0.20; AZ-9.80;")


class EscapingTests(unittest.TestCase):
    def test_plain_text_is_unchanged(self):
        self.assertEqual(escape_raw(PACKET), PACKET)

    def test_tabs_and_newlines_are_escaped(self):
        self.assertEqual(escape_raw("a\tb"), "a\\tb")
        self.assertEqual(escape_raw("a\nb"), "a\\nb")
        self.assertEqual(escape_raw("a\r\nb"), "a\\r\\nb")

    def test_backslash_is_escaped_so_the_mapping_stays_reversible(self):
        self.assertEqual(escape_raw("a\\tb"), "a\\\\tb")
        self.assertEqual(unescape_raw(escape_raw("a\\tb")), "a\\tb")

    def test_other_control_characters_become_hex_escapes(self):
        self.assertEqual(escape_raw("a\x01b"), "a\\x01b")
        self.assertEqual(escape_raw("\x7f"), "\\x7f")

    def test_round_trip_over_every_byte_value(self):
        text = "".join(chr(n) for n in range(0, 256))
        self.assertEqual(unescape_raw(escape_raw(text)), text)

    def test_escaped_text_never_contains_a_separator(self):
        text = "".join(chr(n) for n in range(0, 256))
        escaped = escape_raw(text)
        self.assertNotIn("\t", escaped)
        self.assertNotIn("\n", escaped)
        self.assertNotIn("\r", escaped)


class RawLogTests(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        base = Path(self.dir.name)
        self.log = PacketLog(base / "raw.tsv", base / "parsed.csv")

    def tearDown(self):
        self.dir.cleanup()

    def test_one_line_per_record_even_for_corrupted_payloads(self):
        corrupted = "CAN-Team-01; P-002;\n\tinjected\r\nsecond line"
        self.log.append(PACKET, parse_packet(PACKET).record)
        self.log.append(corrupted, None, error="malformed")
        lines = Path(self.log.raw_path).read_text(encoding="utf-8").splitlines()
        self.assertEqual(len(lines), 2, f"raw log split a record: {lines}")

    def test_a_corrupted_payload_is_recoverable_from_the_raw_log(self):
        corrupted = "junk\twith\ttabs\nand a newline"
        self.log.append(corrupted, None, error="malformed")
        line = Path(self.log.raw_path).read_text(encoding="utf-8").splitlines()[0]
        _timestamp, _, stored = line.partition("\t")
        self.assertEqual(unescape_raw(stored), corrupted)

    def test_nothing_is_discarded(self):
        self.log.append("first", None, error="x")
        self.log.append("second", None, error="y")
        text = Path(self.log.raw_path).read_text(encoding="utf-8")
        self.assertIn("first", text)
        self.assertIn("second", text)

    def test_valid_packets_still_reach_the_csv(self):
        record = parse_packet(PACKET).record
        self.log.append(PACKET, record)
        rows = Path(self.log.parsed_path).read_text(encoding="utf-8").splitlines()
        self.assertEqual(len(rows), 2)  # header + one row
        self.assertIn("CAN-Team-01", rows[1])


class WriteFailureTests(unittest.TestCase):
    """A full disk or a removed drive must not end the mission."""

    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        base = Path(self.dir.name)
        self.log = PacketLog(base / "raw.tsv", base / "parsed.csv")

    def tearDown(self):
        self.dir.cleanup()

    def test_a_failing_write_is_counted_not_raised(self):
        def boom(*_args, **_kwargs):
            raise OSError(28, "No space left on device")

        self.log.raw_path = _FailingPath(self.log.raw_path, boom)
        self.log.append(PACKET, parse_packet(PACKET).record)  # must not raise
        self.assertEqual(self.log.write_errors, 1)
        self.assertIn("No space left", self.log.last_error)

    def test_the_station_keeps_logging_the_other_file(self):
        def boom(*_args, **_kwargs):
            raise OSError(13, "Permission denied")

        self.log.raw_path = _FailingPath(self.log.raw_path, boom)
        self.log.append(PACKET, parse_packet(PACKET).record)
        # The CSV write is independent and must still have happened.
        rows = Path(self.log.parsed_path).read_text(encoding="utf-8").splitlines()
        self.assertEqual(len(rows), 2)
        self.assertEqual(self.log.write_errors, 1)

    def test_errors_accumulate_across_packets(self):
        def boom(*_args, **_kwargs):
            raise OSError(28, "No space left on device")

        self.log.raw_path = _FailingPath(self.log.raw_path, boom)
        for _ in range(5):
            self.log.append(PACKET, parse_packet(PACKET).record)
        self.assertEqual(self.log.write_errors, 5)

    def test_a_healthy_log_reports_no_errors(self):
        self.log.append(PACKET, parse_packet(PACKET).record)
        self.assertEqual(self.log.write_errors, 0)
        self.assertEqual(self.log.last_error, "")


class _FailingPath:
    """Minimal stand-in for Path whose open() raises, to simulate a disk failure."""

    def __init__(self, real: Path, raiser):
        self._real = real
        self._raiser = raiser

    def open(self, *args, **kwargs):
        return self._raiser(*args, **kwargs)

    def __getattr__(self, name):
        return getattr(self._real, name)

    def __fspath__(self):
        return str(self._real)


class DetectMissingTests(unittest.TestCase):
    def test_first_packet_has_no_gap(self):
        self.assertEqual(detect_missing(None, 1), 0)

    def test_consecutive_packets_have_no_gap(self):
        self.assertEqual(detect_missing(4, 5), 0)

    def test_a_gap_counts_the_packets_that_never_arrived(self):
        self.assertEqual(detect_missing(4, 8), 3)

    def test_a_repeat_or_regression_is_not_a_gap(self):
        self.assertEqual(detect_missing(9, 9), 0)
        self.assertEqual(detect_missing(9, 2), 0)


if __name__ == "__main__":
    unittest.main()
