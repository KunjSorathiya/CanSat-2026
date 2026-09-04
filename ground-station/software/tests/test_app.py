import csv
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / "src"))

from app import GroundStation
from transport import LoopbackTransport, frame_encode

TEAM = "CAN-Team-01"


def packet(n: int, extra: str = "") -> str:
    return (
        f"{TEAM}; P-{n:03d}; Ti-00:00:{n:02d}:000; A-{n}.0; Pr-101325.00; T-25.0; "
        f"Ro-1.0; Pi-2.0; Ya-3.0; AX-0.10; AY-0.20; AZ-9.80;" + extra
    )


class AppPipelineTests(unittest.TestCase):
    def _run(self, transport, log_dir):
        station = GroundStation(transport, expected_team=TEAM, log_dir=log_dir)
        station.run_forever()  # synchronous; loopback stop() ends it
        return station

    def test_end_to_end_counts_and_logging(self):
        with tempfile.TemporaryDirectory() as d:
            t = LoopbackTransport(framed=True)
            for n in (1, 2, 3, 5, 5):  # 1 missing (P-4), 1 duplicate (P-5)
                t.push_packet(packet(n))
            t.push_packet("garbage-not-a-packet;;;")
            t.push_packet("#state=RX radio=1 battery=4.01")
            t.stop()

            station = self._run(t, d)
            stats = station.validator.stats
            self.assertEqual(stats.accepted, 5)  # 4 unique + duplicate still "accepted"
            self.assertEqual(stats.missing, 1)
            self.assertEqual(stats.duplicates, 1)
            self.assertEqual(station.health.packets_invalid, 1)  # the garbage line

            snap = station.snapshot()
            self.assertEqual(snap["bridge"].get("battery"), "4.01")
            self.assertEqual(snap["telemetry"]["packet_number"], 5)

            with (Path(d) / "telemetry.csv").open(encoding="utf-8") as stream:
                rows = list(csv.DictReader(stream))
            self.assertTrue(any(r["error"] for r in rows))          # garbage recorded
            self.assertTrue(any(r["packet_number"] == "3" for r in rows))
            raw = (Path(d) / "raw_packets.tsv").read_text(encoding="utf-8")
            self.assertIn("garbage-not-a-packet", raw)              # nothing discarded

    def test_crc_error_frame_is_recorded_not_parsed(self):
        with tempfile.TemporaryDirectory() as d:
            t = LoopbackTransport(framed=True)
            bad = bytearray(frame_encode(packet(1).encode()))
            bad[-3] ^= 0x40
            t.push_bytes(bytes(bad))
            t.push_packet(packet(2))
            t.stop()

            station = self._run(t, d)
            self.assertEqual(station.health.crc_errors, 1)
            self.assertEqual(station.validator.stats.accepted, 1)  # only P-002
            with (Path(d) / "telemetry.csv").open(encoding="utf-8") as stream:
                rows = list(csv.DictReader(stream))
            self.assertTrue(any("crc" in (r["error"] or "") for r in rows))

    def test_export_csv(self):
        with tempfile.TemporaryDirectory() as d:
            t = LoopbackTransport(framed=False)
            t.push_packet(packet(1))
            t.push_packet(packet(2))
            t.stop()
            station = self._run(t, d)
            dest = Path(d) / "export" / "out.csv"
            station.export_csv(dest)
            self.assertTrue(dest.exists())
            with dest.open(encoding="utf-8") as stream:
                rows = list(csv.DictReader(stream))
            self.assertGreaterEqual(len(rows), 2)


if __name__ == "__main__":
    unittest.main()


class LoggingHealthTests(unittest.TestCase):
    """A ground station that receives but cannot record must say so."""

    def test_snapshot_reports_a_healthy_log(self):
        with tempfile.TemporaryDirectory() as tmp:
            transport = LoopbackTransport()
            station = GroundStation(transport, expected_team="CAN-Team-01", log_dir=tmp)
            transport.push_packet(packet(1))
            transport.stop()
            station.run_forever()
            logging_state = station.snapshot()["logging"]
            self.assertEqual(logging_state["write_errors"], 0)
            self.assertEqual(logging_state["last_error"], "")

    def test_a_logging_failure_is_surfaced_and_does_not_stop_reception(self):
        with tempfile.TemporaryDirectory() as tmp:
            transport = LoopbackTransport()
            station = GroundStation(transport, expected_team="CAN-Team-01", log_dir=tmp)

            def boom(*_args, **_kwargs):
                raise OSError(28, "No space left on device")

            station.log.raw_path = _FailingPath(station.log.raw_path, boom)
            transport.push_packet(packet(1))
            transport.push_packet(packet(2))
            transport.stop()
            station.run_forever()  # must not raise

            logging_state = station.snapshot()["logging"]
            self.assertEqual(logging_state["write_errors"], 2)
            self.assertIn("No space left", logging_state["last_error"])
            # Reception carried on regardless: both packets were parsed and counted.
            self.assertEqual(station.validator.stats.accepted, 2)


class _FailingPath:
    def __init__(self, real, raiser):
        self._real = real
        self._raiser = raiser

    def open(self, *args, **kwargs):
        return self._raiser(*args, **kwargs)

    def __getattr__(self, name):
        return getattr(self._real, name)

    def __fspath__(self):
        return str(self._real)


class BridgeStatusTests(unittest.TestCase):
    """The bridge's radio report is what warns an operator before a link fails."""

    def _station_with_status(self, line):
        transport = LoopbackTransport()
        with tempfile.TemporaryDirectory() as tmp:
            station = GroundStation(transport, expected_team=TEAM, log_dir=tmp)
            transport.push_packet(line)
            transport.stop()
            station.run_forever()
            return station.snapshot()["bridge"]

    def test_the_radio_report_is_parsed_into_the_snapshot(self):
        bridge = self._station_with_status(
            "#state=RX radio=1 frames=42 dropped=3 rssi=-97 snr=9.5")
        self.assertEqual(bridge["radio"], "1")
        self.assertEqual(bridge["frames"], "42")
        self.assertEqual(bridge["dropped"], "3")
        self.assertEqual(bridge["rssi"], "-97")
        self.assertEqual(bridge["snr"], "9.5")

    def test_the_sync_word_reaches_the_dashboard(self):
        """The rulebook's test and launch words are one reflash apart.

        Which one the radio is actually using is the bridge's to report; a ground station
        that displays it from its own constant displays it correctly until the day of the
        launch, which is the only day it matters.
        """
        bridge = self._station_with_status(
            "#state=RX radio=1 frames=42 dropped=3 rssi=-97 snr=9.5 sync=0xF3")
        self.assertEqual(bridge["sync"], "0xF3")
        self.assertEqual(
            self._station_with_status("#bridge=online radio=1 sync=0xA5")["sync"], "0xA5")

    def test_a_bridge_that_reports_no_sync_word_leaves_the_field_absent(self):
        """An older bridge image says nothing about its sync word, and the dashboard shows
        `--` for it rather than inventing one."""
        bridge = self._station_with_status("#state=RX radio=1 frames=42")
        self.assertNotIn("sync", bridge)

    def test_a_negative_snr_survives_parsing(self):
        bridge = self._station_with_status("#state=RX radio=1 rssi=-121 snr=-7.5")
        self.assertEqual(bridge["rssi"], "-121")
        self.assertEqual(bridge["snr"], "-7.5")

    def test_a_radio_loss_line_is_recorded(self):
        bridge = self._station_with_status("#radio=lost")
        self.assertEqual(bridge["radio"], "lost")

    def test_status_lines_are_not_counted_as_telemetry(self):
        transport = LoopbackTransport()
        with tempfile.TemporaryDirectory() as tmp:
            station = GroundStation(transport, expected_team=TEAM, log_dir=tmp)
            transport.push_packet("#state=RX radio=1 rssi=-90 snr=8.0")
            transport.push_packet(packet(1))
            transport.stop()
            station.run_forever()
            link = station.health.snapshot()
            self.assertEqual(link["status_frames"], 1)
            self.assertEqual(link["packets_ok"], 1)
