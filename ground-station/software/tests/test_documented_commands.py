"""The commands the documentation tells a reader to run, run.

Two defects were found by typing documented commands in exactly the form the documents give
them, and neither would have been caught by testing the code those commands reach:

* every document told the reader to replay `packets.txt`, a file the repository has never
  contained, so the first ground-station command in the quick start ended in
  `FileNotFoundError`;
* the runbook's post-flight step replayed the raw log, and the transport handed each line to
  the parser complete with its receipt timestamp, so a real flight log decoded to
  `received=0` -- no error, no warning, four hours after a launch.

A command in a document is a promise. These tests are how it stays one.
"""

import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
SOFTWARE = REPO_ROOT / "ground-station" / "software"
MAIN = SOFTWARE / "src" / "main.py"
SAMPLE = REPO_ROOT / "test-data" / "sample-mission.txt"

sys.path.insert(0, str(SOFTWARE / "src"))

from logger import PacketLog  # noqa: E402
from telemetry import parse_packet  # noqa: E402


def run_main(*args, cwd=None):
    """Run src/main.py the way the documentation runs it: from the software directory."""
    result = subprocess.run([sys.executable, str(MAIN), *args],
                            capture_output=True, text=True, timeout=120,
                            cwd=str(cwd or SOFTWARE))
    return result


def summary(stdout):
    """Parse the "received=N accepted=N ..." line the replay command prints."""
    match = re.search(r"received=(\d+) accepted=(\d+) rejected=(\d+)", stdout)
    if match is None:
        raise AssertionError(f"no summary line in output:\n{stdout}")
    return tuple(int(g) for g in match.groups())


class DocumentedReplayTests(unittest.TestCase):
    def test_the_sample_mission_the_documents_name_exists(self):
        self.assertTrue(SAMPLE.is_file(),
                        f"{SAMPLE} is named by the README, the quick start and the runbook")

    def test_the_documented_replay_command_accepts_every_packet(self):
        with tempfile.TemporaryDirectory() as tmp:
            result = run_main("replay", str(SAMPLE), "--team", "CAN-Team-01",
                              "--output", tmp, "--export", str(Path(tmp) / "flight.csv"))
            self.assertEqual(result.returncode, 0, result.stderr)
            received, accepted, rejected = summary(result.stdout)
            self.assertGreaterEqual(received, 20)
            self.assertEqual(accepted, received)
            self.assertEqual(rejected, 0)
            self.assertTrue((Path(tmp) / "flight.csv").is_file())

    def test_a_raw_log_this_station_wrote_replays_through_the_documented_command(self):
        """The runbook's post-flight step, end to end: write a log, then replay that log."""
        with tempfile.TemporaryDirectory() as tmp:
            raw = Path(tmp) / "raw_packets.tsv"
            log = PacketLog(raw_path=raw, parsed_path=Path(tmp) / "telemetry.csv")
            packets = [line for line in SAMPLE.read_text(encoding="utf-8").splitlines()
                       if line.strip()]
            for packet in packets:
                log.append(packet, parse_packet(packet).record, 0, "")

            result = run_main("replay", str(raw), "--team", "CAN-Team-01",
                              "--output", str(Path(tmp) / "out"),
                              "--export", str(Path(tmp) / "analysis.csv"))
            self.assertEqual(result.returncode, 0, result.stderr)
            received, accepted, rejected = summary(result.stdout)
            # The whole point: the log the station wrote replays as the mission it recorded.
            self.assertEqual(received, len(packets))
            self.assertEqual(accepted, len(packets))
            self.assertEqual(rejected, 0)

    def test_the_replay_command_reports_a_file_it_cannot_read(self):
        # The failure mode that hid the raw-log defect was a silent zero. A missing file
        # must not be silent either.
        with tempfile.TemporaryDirectory() as tmp:
            result = run_main("replay", str(Path(tmp) / "nothing-here.txt"),
                              "--team", "CAN-Team-01", "--output", tmp)
            self.assertNotEqual(result.returncode, 0)


class DocumentedCommandsAreRealTests(unittest.TestCase):
    """Every command in a fenced bash block that invokes main.py must parse as valid.

    Not run for effect -- argparse is asked whether the flags exist. A document offering a
    flag the parser does not have is the same class of defect as one naming a file that is
    not there.
    """

    DOCS = [
        REPO_ROOT / "README.md",
        REPO_ROOT / "documentation" / "quick-start.md",
        REPO_ROOT / "documentation" / "operations" / "runbook.md",
        SOFTWARE / "README.md",
    ]

    def _documented_invocations(self):
        found = []
        for doc in self.DOCS:
            for line in doc.read_text(encoding="utf-8").splitlines():
                stripped = line.strip().lstrip("|").strip()
                match = re.search(r"python src/main\.py ([^`|]+)", stripped)
                if match:
                    found.append((doc.name, match.group(1).strip().rstrip("`").strip()))
        return found

    def test_every_documented_invocation_parses(self):
        sys.path.insert(0, str(SOFTWARE / "src"))
        from main import build_parser

        invocations = self._documented_invocations()
        self.assertGreaterEqual(len(invocations), 5, "no documented commands were found")
        parser = build_parser()
        for source, argument_text in invocations:
            with self.subTest(source=source, command=argument_text):
                args = argument_text.split()
                # argparse exits on an unknown flag; that exit is the failure being caught.
                try:
                    parser.parse_args(args)
                except SystemExit:
                    self.fail(f"{source} documents a command argparse rejects: {argument_text}")
