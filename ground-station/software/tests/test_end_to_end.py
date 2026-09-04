"""End-to-end integration: the real vehicle's output through the real ground pipeline.

Every other test checks one side of the system against its own idea of the format. This
one runs the actual flight controller (`emit_mission`, built by `tools/build_host.sh`),
takes the packets it transmits, and pushes them through the ground station exactly as the
bridge would: CRC framing, frame decoding, parsing, validation, logging, CSV export.

If the two halves ever disagree — a field order, a precision rule, a packet-number policy,
an optional-field placement — this fails, and no amount of per-side testing would have
caught it.

The test skips (loudly) when the binary has not been built, so the Python suite still runs
on its own.
"""

import csv
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(Path(__file__).parents[1] / "src"))

from app import GroundStation  # noqa: E402
from transport import LoopbackTransport, frame_encode  # noqa: E402

BINARIES = [
    REPO_ROOT / "build" / "host" / "emit_mission",
    REPO_ROOT / "build" / "host" / "emit_mission.exe",
]
TEAM = "CAN-Team-01"
PACKET_COUNT = 40


def find_binary():
    for candidate in BINARIES:
        if candidate.is_file():
            return candidate
    return None


def vehicle_packets(count=PACKET_COUNT):
    binary = find_binary()
    if binary is None:
        raise unittest.SkipTest(
            "emit_mission not built - run tools/build_host.sh to include this test")
    result = subprocess.run([str(binary), str(count)], capture_output=True, text=True,
                            timeout=60)
    if result.returncode != 0:
        raise AssertionError(f"emit_mission failed: {result.stderr}")
    return [line for line in result.stdout.splitlines() if line.strip()]


class EndToEndTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.packets = vehicle_packets()

    def run_pipeline(self, packets, framed=True):
        """Push packets through the ground station as the bridge would deliver them."""
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        transport = LoopbackTransport(framed=framed)
        station = GroundStation(transport, expected_team=TEAM, log_dir=tmp.name)
        for packet in packets:
            transport.push_packet(packet)
        transport.stop()
        station.run_forever()
        return station, Path(tmp.name)

    # ------------------------------------------------------------------ shape
    def test_the_vehicle_produced_packets_at_all(self):
        self.assertGreaterEqual(len(self.packets), PACKET_COUNT)

    def test_every_transmitted_packet_survives_the_whole_pipeline(self):
        station, _ = self.run_pipeline(self.packets)
        stats = station.validator.stats
        self.assertEqual(stats.received, len(self.packets))
        self.assertEqual(stats.accepted, len(self.packets))
        self.assertEqual(stats.rejected, 0)
        self.assertEqual(stats.missing, 0)
        self.assertEqual(stats.duplicates, 0)
        self.assertEqual(stats.out_of_order, 0)

    def test_no_frame_is_reported_as_corrupt(self):
        station, _ = self.run_pipeline(self.packets)
        link = station.health.snapshot()
        self.assertEqual(link["crc_errors"], 0)
        self.assertEqual(link["packets_invalid"], 0)
        self.assertEqual(link["packets_ok"], len(self.packets))

    def test_packet_numbering_is_sequential_from_one(self):
        station, _ = self.run_pipeline(self.packets)
        snapshot = station.snapshot()
        self.assertEqual(snapshot["telemetry"]["packet_number"], len(self.packets))
        self.assertTrue(self.packets[0].startswith(f"{TEAM}; P-001; "))

    # ------------------------------------------------------------------ values
    def test_the_values_the_vehicle_sent_are_the_values_the_ground_station_reads(self):
        station, _ = self.run_pipeline(self.packets[:1])
        telemetry = station.snapshot()["telemetry"]
        first = self.packets[0]
        # Read the fields straight out of the transmitted text and compare.
        fields = [part.strip() for part in first.split(";") if part.strip()]
        self.assertEqual(telemetry["team_id"], fields[0])
        self.assertEqual(f"A-{telemetry['altitude']:.1f}", fields[3])
        self.assertEqual(f"Pr-{telemetry['pressure']:.2f}", fields[4])
        self.assertEqual(f"T-{telemetry['temperature']:.1f}", fields[5])
        self.assertEqual(f"Ro-{telemetry['roll']:.1f}", fields[6])
        self.assertEqual(f"Pi-{telemetry['pitch']:.1f}", fields[7])
        self.assertEqual(f"Ya-{telemetry['yaw']:.1f}", fields[8])
        self.assertEqual(f"AX-{telemetry['ax']:.2f}", fields[9])
        self.assertEqual(f"AY-{telemetry['ay']:.2f}", fields[10])
        self.assertEqual(f"AZ-{telemetry['az']:.2f}", fields[11])

    def test_optional_gps_and_diagnostic_fields_cross_the_boundary(self):
        station, _ = self.run_pipeline(self.packets)
        telemetry = station.snapshot()["telemetry"]
        self.assertTrue(telemetry["gps_fix"])
        self.assertIsNotNone(telemetry["gps_lat"])
        self.assertIsNotNone(telemetry["gps_lon"])
        self.assertIn(telemetry["mode"], ("READY", "FLIGHT", "LANDED", "RECOVERY"))
        self.assertIsNotNone(telemetry["fault_count"])

    def test_the_mission_actually_progressed(self):
        # A run that never leaves READY would pass every other assertion while testing
        # far less of the system, so check the state machine moved.
        modes = set()
        for packet in self.packets:
            for field in packet.split(";"):
                field = field.strip()
                if field.startswith("MODE-"):
                    modes.add(field[5:])
        self.assertIn("READY", modes)
        self.assertIn("FLIGHT", modes)

    def test_altitude_varies_over_the_mission(self):
        altitudes = []
        for packet in self.packets:
            for field in packet.split(";"):
                field = field.strip()
                if field.startswith("A-"):
                    altitudes.append(float(field[2:]))
                    break
        self.assertGreater(max(altitudes) - min(altitudes), 5.0)

    # ------------------------------------------------------------------ logging
    def test_every_packet_reaches_the_csv_and_the_raw_log(self):
        station, log_dir = self.run_pipeline(self.packets)
        raw_lines = (log_dir / "raw_packets.tsv").read_text(encoding="utf-8").splitlines()
        self.assertEqual(len(raw_lines), len(self.packets))

        with (log_dir / "telemetry.csv").open(encoding="utf-8", newline="") as handle:
            rows = list(csv.DictReader(handle))
        self.assertEqual(len(rows), len(self.packets))
        self.assertTrue(all(row["team_id"] == TEAM for row in rows))
        self.assertEqual([int(row["packet_number"]) for row in rows],
                         list(range(1, len(self.packets) + 1)))
        self.assertEqual(station.log.write_errors, 0)

    def test_the_exported_csv_matches_the_log(self):
        station, log_dir = self.run_pipeline(self.packets)
        destination = log_dir / "export" / "flight.csv"
        station.export_csv(destination)
        self.assertTrue(destination.is_file())
        self.assertEqual(destination.read_text(encoding="utf-8"),
                         (log_dir / "telemetry.csv").read_text(encoding="utf-8"))

    # ------------------------------------------------------------------ transport
    def test_a_lost_packet_is_counted_not_hidden(self):
        # Drop one packet from the middle of the real stream.
        thinned = self.packets[:5] + self.packets[6:]
        station, _ = self.run_pipeline(thinned)
        self.assertEqual(station.validator.stats.missing, 1)
        self.assertAlmostEqual(station.health.snapshot()["loss_pct"],
                               100.0 / len(self.packets), places=2)

    def test_a_corrupted_frame_is_rejected_by_crc_not_parsed(self):
        transport = LoopbackTransport(framed=True)
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        station = GroundStation(transport, expected_team=TEAM, log_dir=tmp.name)

        good = frame_encode(self.packets[0].encode())
        corrupted = bytearray(frame_encode(self.packets[1].encode()))
        corrupted[-5] ^= 0xFF  # flip a payload bit, leaving the CRC stale
        transport.push_bytes(good)
        transport.push_bytes(bytes(corrupted))
        transport.stop()
        station.run_forever()

        link = station.health.snapshot()
        self.assertEqual(link["crc_errors"], 1)
        self.assertEqual(link["packets_ok"], 1)
        # The corrupted payload is still on record, exactly as received.
        raw = (Path(tmp.name) / "raw_packets.tsv").read_text(encoding="utf-8")
        self.assertIn("transport crc error",
                      (Path(tmp.name) / "telemetry.csv").read_text(encoding="utf-8"))
        self.assertEqual(len(raw.splitlines()), 2)

    def test_the_stream_also_survives_an_unframed_link(self):
        # File replay and plain serial deliver packets without the bridge's CRC framing.
        station, _ = self.run_pipeline(self.packets, framed=False)
        self.assertEqual(station.validator.stats.accepted, len(self.packets))


if __name__ == "__main__":
    unittest.main()
