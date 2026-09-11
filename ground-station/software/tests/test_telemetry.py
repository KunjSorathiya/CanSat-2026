import csv
import json
import shutil
import subprocess
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



class YawReferenceTests(unittest.TestCase):
    """The Ya- field is two different quantities depending on the YR tag beside it."""

    BASE = ("CAN-Team-07; P-001; Ti-00:00:01:000; A-10.0; Pr-101325.00; T-25.0; "
            "Ro-1.0; Pi-2.0; Ya-30.0; AX-0.10; AY-0.20; AZ-9.80;")

    def test_magnetic_yaw_is_reported_as_a_bearing(self):
        result = parse_packet(self.BASE + " YR-M;")
        self.assertTrue(result)
        record = result.record
        self.assertEqual(record.yaw_reference, "magnetic")
        self.assertTrue(record.yaw_is_magnetic)
        # Yaw runs anticlockwise about the vehicle's up axis; a bearing runs clockwise.
        self.assertAlmostEqual(record.heading, 330.0)

    def test_a_relative_yaw_yields_no_bearing(self):
        result = parse_packet(self.BASE + " YR-G;")
        self.assertTrue(result)
        record = result.record
        self.assertEqual(record.yaw_reference, "gyro")
        self.assertFalse(record.yaw_is_magnetic)
        # A bearing derived from a relative yaw would be wrong by an unknown constant, so
        # none is offered rather than one that looks usable.
        self.assertIsNone(record.heading)

    def test_a_packet_without_the_tag_says_nothing_either_way(self):
        result = parse_packet(self.BASE)
        self.assertTrue(result)
        self.assertIsNone(result.record.yaw_reference)
        self.assertIsNone(result.record.heading)

    def test_the_bearing_wraps_into_zero_to_three_sixty(self):
        packet = self.BASE.replace("Ya-30.0", "Ya--150.0")
        record = parse_packet(packet + " YR-M;").record
        self.assertAlmostEqual(record.heading, 150.0)
        packet = self.BASE.replace("Ya-30.0", "Ya-0.0")
        record = parse_packet(packet + " YR-M;").record
        self.assertAlmostEqual(record.heading, 0.0)

    def test_the_csv_row_records_which_kind_of_yaw_it_holds(self):
        row = parse_packet(self.BASE + " YR-M;").record.csv_row("t")
        self.assertEqual(row["yaw_reference"], "magnetic")
        self.assertAlmostEqual(row["heading"], 330.0)
        row = parse_packet(self.BASE + " YR-G;").record.csv_row("t")
        self.assertEqual(row["yaw_reference"], "gyro")
        self.assertEqual(row["heading"], "")


if __name__ == "__main__":
    unittest.main()


class RecordSurfaceTests(unittest.TestCase):
    """The Python and JavaScript parsers expose the same record under the same names.

    The shared fixtures hold the two parsers to one definition of a *valid packet*. They say
    nothing about what the parsed record is called afterwards, and a name is exactly what a
    hand-port gets wrong: reading `record.fault_count` in JavaScript when the field is
    `record.faults` yields `undefined` and renders as a dash, where the same mistake in
    Python raises immediately. `fault_count` was that mismatch.
    """

    CONSOLE = Path(__file__).parents[3] / "ground-station" / "web" / "index.html"

    # The C++ record is deliberately not part of this comparison. It is the transmitter's
    # struct, it names its units -- `altitude_m`, `acceleration_x_mps2` -- and that is worth
    # more on an embedded target than matching a receiver's field names. The two compared
    # here are both receivers, one a documented hand-port of the other, which is what makes
    # a name mismatch between them a porting hazard rather than a convention difference.
    #
    # Names that exist on one side only, on purpose.
    PYTHON_ONLY = {
        # Python exposes booleans the console derives at the point of use instead.
        "has_gps",          # JavaScript tests `gps_lat !== null`
        "yaw_is_magnetic",  # JavaScript tests `yaw_reference === "magnetic"`
        # The raw optional-field list, kept by the Python parser for the CSV writer.
        "optional",
    }

    NODE_SCRIPT = """
const {readFileSync} = require("fs");
const html = readFileSync(process.argv[2], "utf8");
const core = html.split("// PORTABLE-CORE:BEGIN")[1].split("// PORTABLE-CORE:END")[0];
const {parsePacket} = new Function(core + "\\nreturn {parsePacket};")();
const result = parsePacket(process.argv[3], null);
if (result.error) { console.error("parse failed: " + result.error); process.exit(1); }
console.log(JSON.stringify(Object.keys(result.record)));
"""

    def _javascript_fields(self):
        node = shutil.which("node")
        if node is None:
            raise unittest.SkipTest("node not found - the console's parser cannot be run here")
        with tempfile.TemporaryDirectory() as tmp:
            script = Path(tmp) / "fields.js"
            script.write_text(self.NODE_SCRIPT, encoding="utf-8")
            result = subprocess.run(
                [node, str(script), str(self.CONSOLE), self.PACKET],
                capture_output=True, text=True, timeout=60)
        if result.returncode != 0:
            raise AssertionError(f"could not read the console's record: {result.stderr}")
        return set(json.loads(result.stdout))

    PACKET = ("CAN-Team-01; P-001; Ti-00:00:01:000; A-10.0; Pr-101325.00; T-25.0; "
              "Ro-1.0; Pi-2.0; Ya-3.0; AX-0.10; AY-0.20; AZ-9.80; "
              "GP-Lat-18.000000; GP-Lon-73.000000; GP-Alt-20.0; "
              "MODE-READY; FAULTS-2; CAL-1; ARM-1; YR-M;")

    def _python_fields(self):
        record = parse_packet(self.PACKET).record
        self.assertIsNotNone(record)
        return {name for name in dir(record)
                if not name.startswith("_") and not callable(getattr(record, name))}

    def test_the_two_parsers_agree_on_field_names(self):
        python = self._python_fields()
        javascript = self._javascript_fields()
        self.assertGreater(len(javascript), 15, "the JavaScript parser should assign many fields")
        only_python = sorted(python - javascript - self.PYTHON_ONLY)
        only_javascript = sorted(javascript - python)
        self.assertEqual(only_python, [], f"exposed by Python and not by the console: {only_python}")
        self.assertEqual(only_javascript, [],
                         f"exposed by the console and not by Python: {only_javascript}")

    def test_the_exception_list_still_describes_reality(self):
        # A name that has since appeared on both sides should leave the list, not sit in it
        # claiming a difference that no longer exists.
        python = self._python_fields()
        javascript = self._javascript_fields()
        for name in self.PYTHON_ONLY:
            self.assertIn(name, python, f"{name} is listed as Python-only and Python lacks it")
            self.assertNotIn(name, javascript,
                             f"{name} is listed as Python-only but the console now has it too")


class OptionalTagSplittingTests(unittest.TestCase):
    """Every case in test-data/optional-tag-cases.tsv, as this parser splits it.

    The optional tags are `<key>-<value>` and the keys themselves contain dashes, so both
    ground parsers split on the last one. That is the separator right up until the value is
    negative, and then the last dash is the minus sign: `GP-Lat--18.5` split to the key
    `GP-Lat-` and the value `18.5`, sign eaten and key unrecognisable, so a
    southern-hemisphere fix vanished from the console, the CSV and the map with no error and
    no rejection counter.

    Both implementations were wrong the same way because they were hand-ports of each other
    and no fixture had a negative coordinate. The web console reads this same file, so the
    two are held to one definition rather than to two sets of similarly-named tests.
    """

    CASES = Path(__file__).parents[3] / "test-data" / "optional-tag-cases.tsv"

    def _load(self):
        cases = []
        with self.CASES.open(encoding="utf-8") as handle:
            for line in handle:
                line = line.rstrip("\n").rstrip("\r")
                if not line.strip() or line.lstrip().startswith("#"):
                    continue
                name, field, key, value = line.split("\t", 3)
                cases.append((name, field, key, value))
        return cases

    def test_the_fixture_covers_the_sound_field(self):
        # SN- went on the air when the organizers ruled that only transmitted telemetry earns
        # extra-sensor points. Both parsers read this file, so both are held to it.
        keys = {c[2] for c in self._load()}
        self.assertIn("SN", keys)

    def test_the_fixture_file_is_present_and_covers_negative_values(self):
        cases = self._load()
        self.assertGreaterEqual(len(cases), 8)
        negatives = [c for c in cases if c[3].startswith("-")]
        self.assertGreaterEqual(len(negatives), 3, "the bug this file exists for is negatives")

    def test_every_case_splits_into_its_recorded_key_and_value(self):
        for name, field, key, value in self._load():
            with self.subTest(case=name):
                packet = PACKET + " " + field + ";"
                result = parse_packet(packet)
                self.assertTrue(result, f"{name}: packet rejected outright")
                self.assertIn(key, result.record.tags, f"{name}: key not recognised")
                self.assertEqual(result.record.tags[key], value)

    def test_a_southern_hemisphere_fix_survives_the_whole_parser(self):
        # The end-to-end shape of the original bug: not just the split, but the value
        # reaching the field the console, the CSV and the map read.
        packet = (PACKET + " GP-Lat--18.500000; GP-Lon--73.000000; GP-Alt-5.0;")
        result = parse_packet(packet)
        self.assertTrue(result)
        self.assertAlmostEqual(result.record.gps_lat, -18.5, places=6)
        self.assertAlmostEqual(result.record.gps_lon, -73.0, places=6)
