"""The post-flight analysis, held to a synthetic flight whose answers are known.

The analysis only runs for real once, inside a four-hour window, on data nobody has seen. So
it is tested here against ``test-data/synthetic-flight/``: a flight generated from stated
parameters, in the exact formats the vehicle and the ground station write, and put through the
real ground-station replay. If the analysis cannot recover the release time, the descent rate,
the drag coefficient, the spin, the pendulum and the lost packet it was given, it fails here
rather than on launch day.

Run with ``python -m unittest discover -s analysis/tests``; ``tools/build_host.sh`` runs it.
"""

from __future__ import annotations

import json
import os
import re
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

os.environ.setdefault("MPLBACKEND", "Agg")

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "analysis"))

import flight_analysis as fa  # noqa: E402
import synthetic_flight as sf  # noqa: E402

DATA = ROOT / "test-data" / "synthetic-flight"
SD = DATA / "sd-flight.csv"
GROUND_CSV = DATA / "ground-telemetry.csv"
GROUND_PACKETS = DATA / "ground-packets.txt"
TRUTH = json.loads((DATA / "truth.json").read_text(encoding="utf-8"))


def _analysis(path=SD, compare=GROUND_CSV, mass=TRUTH["mass_kg"]):
    flight = fa.load(path)
    other = fa.load(compare) if compare else None
    return fa.analyse(flight, mass, TRUTH["canopy_diameter_m"], other)


class TheFixtureIsTheGeneratorsOutput(unittest.TestCase):
    """A committed fixture that its generator no longer produces would test yesterday's code."""

    def test_the_sd_log_and_the_ground_packets_regenerate_byte_for_byte(self):
        sd_rows, packets, _ = sf.generate()
        self.assertEqual(SD.read_text(encoding="utf-8"),
                         sf.SD_HEADER + "\n" + "\n".join(sd_rows) + "\n")
        heard = set(GROUND_PACKETS.read_text(encoding="utf-8").splitlines())
        self.assertTrue(heard <= set(packets))
        self.assertEqual(len(packets) - len(heard), TRUTH["packets_lost_on_ground"])

    def test_the_fixture_says_it_is_synthetic(self):
        self.assertIn("SYNTHETIC", TRUTH["generated"])

    def test_every_packet_passes_the_real_ground_station_parser(self):
        rows = GROUND_CSV.read_text(encoding="utf-8").splitlines()
        self.assertEqual(len(rows) - 1, TRUTH["packets_transmitted"] - TRUTH["packets_lost_on_ground"])
        self.assertNotIn(",False,", "\n".join(rows[1:]))


class ItMatchesTheFirmware(unittest.TestCase):
    def test_sd_columns_are_the_firmware_header(self):
        source = (ROOT / "firmware/flight-computer/src/telemetry_builder.cpp").read_text(encoding="utf-8")
        body = source[source.index("std::string TelemetryBuilder::sd_header()"):]
        header = "".join(re.findall(r'"([^"]*)"', body[: body.index(";")]))
        self.assertEqual(header.split(","), fa.SD_COLUMNS)
        self.assertEqual(sf.SD_HEADER, header)

    def test_detection_thresholds_are_the_firmware_thresholds(self):
        config = (ROOT / "firmware/flight-computer/include/flight/config.hpp").read_text(encoding="utf-8")
        gps = (ROOT / "firmware/flight-computer/include/flight/gps_parser.hpp").read_text(encoding="utf-8")
        launch = float(re.search(r"launch_altitude_gain_m\s*=\s*([0-9.]+)", config).group(1))
        gate = float(re.search(r"landing_descent_rate_mps\s*=\s*([0-9.]+)", config).group(1))
        self.assertEqual(fa.LAUNCH_ALTITUDE_GAIN_M, launch)
        self.assertEqual(fa.DESCENT_GATE_MPS, -gate)
        self.assertEqual(fa.GPS_MIN_SATELLITES, int(re.search(r"kMinGpsSatellites\s*=\s*(\d+)", gps).group(1)))
        self.assertEqual(fa.GPS_MAX_HDOP, float(re.search(r"kMaxGpsHdop\s*=\s*([0-9.]+)", gps).group(1)))

    def test_the_synthetic_altitude_uses_the_firmware_formula(self):
        source = (ROOT / "firmware/flight-computer/src/sensor_math.cpp").read_text(encoding="utf-8")
        self.assertIn("44330.0 * (1.0 - std::pow(pressure_pa / reference_pressure_pa, 1.0 / 5.255))", source)
        self.assertIn("44330.0 * (1.0 - (pressure / p0) ** (1.0 / 5.255))",
                      (ROOT / "analysis/synthetic_flight.py").read_text(encoding="utf-8"))


class Loading(unittest.TestCase):
    def test_all_three_formats_are_detected_and_agree(self):
        kinds = {p.name: fa.load(p) for p in (SD, GROUND_CSV, GROUND_PACKETS)}
        self.assertEqual([f.kind for f in kinds.values()], ["sd", "ground-csv", "packets"])
        self.assertEqual(len(kinds["sd-flight.csv"]), TRUTH["packets_transmitted"])
        self.assertEqual(len(kinds["ground-telemetry.csv"]), len(kinds["ground-packets.txt"]))
        for flight in kinds.values():
            self.assertEqual(flight.team, TRUTH["team_id"])

    def test_the_status_field_survives_the_ground_station(self):
        flight = fa.load(GROUND_CSV)
        self.assertIn("FLIGHT", flight.state)
        self.assertIn("LANDED", flight.state)

    def test_the_sd_log_keeps_gps_quality_the_packet_does_not_carry(self):
        import numpy as np
        flight = fa.load(SD)
        sats = flight.gps_sats[np.isfinite(flight.gps_sats)]
        self.assertGreater(len(sats), 100)
        self.assertTrue((sats >= 8).all())

    def test_a_log_holding_two_power_cycles_uses_the_one_that_flew(self):
        with tempfile.TemporaryDirectory() as tmp:
            lines = SD.read_text(encoding="utf-8").splitlines()
            bench = lines[1:40]
            path = Path(tmp) / "two-sessions.csv"
            path.write_text("\n".join([lines[0]] + bench + lines[1:]) + "\n", encoding="utf-8")
            flight = fa.load(path)
            self.assertEqual(len(flight), TRUTH["packets_transmitted"])
            self.assertTrue(any("power cycles" in n for n in flight.notes))

    def test_duplicates_keep_the_first_copy(self):
        with tempfile.TemporaryDirectory() as tmp:
            packets = GROUND_PACKETS.read_text(encoding="utf-8").splitlines()
            path = Path(tmp) / "dupes.txt"
            path.write_text("\n".join(packets[:50] + packets[45:50] + packets[50:]) + "\n", encoding="utf-8")
            flight = fa.load(path)
            self.assertEqual(len(flight), len(packets))
            self.assertTrue(any("duplicate" in n for n in flight.notes))

    def test_bare_packets_in_an_sd_file_are_read_not_dropped(self):
        # F-19: an older firmware wrote raw radio packets into FLIGHT.CSV.
        with tempfile.TemporaryDirectory() as tmp:
            lines = SD.read_text(encoding="utf-8").splitlines()
            legacy = [row.split(",", len(fa.SD_COLUMNS) - 1)[-1] for row in lines[1:21]]
            path = Path(tmp) / "legacy.csv"
            path.write_text("\n".join([lines[0]] + legacy + lines[21:]) + "\n", encoding="utf-8")
            flight = fa.load(path)
            self.assertEqual(len(flight), TRUTH["packets_transmitted"])
            self.assertTrue(any("F-19" in n for n in flight.notes))

    def test_the_old_sample_mission_still_loads(self):
        flight = fa.load(ROOT / "test-data" / "sample-mission.txt")
        self.assertEqual(flight.team, "CAN-Team-01")
        self.assertGreater(len(flight), 20)


class Phases(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.an = _analysis()

    def test_uncalibrated_power_on_rows_are_not_a_launch(self):
        self.assertGreater(self.an.phases.launch_t, 100.0)

    def test_launch_release_and_landing_are_found(self):
        ph = self.an.phases
        self.assertAlmostEqual(ph.launch_t, TRUTH["lift_start_s"], delta=1.0)
        self.assertAlmostEqual(ph.release_t, TRUTH["release_s"], delta=0.15)
        self.assertAlmostEqual(ph.landing_t, TRUTH["touchdown_s"], delta=0.3)

    def test_the_vehicle_states_are_reported_beside_the_physical_events(self):
        ph = self.an.phases
        self.assertAlmostEqual(ph.flight_declared_t, TRUTH["flight_declared_s"], delta=0.5)
        self.assertAlmostEqual(ph.landed_declared_t, TRUTH["landed_declared_s"], delta=0.5)
        self.assertLess(ph.flight_declared_t, ph.release_t)
        self.assertGreater(ph.landed_declared_t, ph.landing_t)


class Descent(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.an = _analysis()

    def test_descent_time(self):
        self.assertAlmostEqual(self.an.descent["descent_time_s"], TRUTH["descent_time_s"], delta=0.3)

    def test_the_steady_rate_is_recovered_from_corrected_height(self):
        self.assertAlmostEqual(self.an.descent["terminal_rate_mps"], TRUTH["terminal_rate_mps"],
                               delta=0.03 * TRUTH["terminal_rate_mps"])
        self.assertTrue(self.an.descent["within_rulebook_cap"])

    def test_the_vehicle_altitude_reads_low_by_the_isa_temperature_ratio(self):
        # The firmware's ISA formula on a 31 C day: this is the bias the correction exists for.
        expected = 288.15 / (TRUTH["ground_temperature_c"] + 273.15)
        self.assertAlmostEqual(self.an.environment["vehicle_altitude_scale"], expected, delta=0.01)
        self.assertLess(self.an.descent["vehicle_altitude_bias_percent"], -3.5)
        self.assertGreater(self.an.descent["vehicle_altitude_bias_percent"], -7.5)

    def test_the_drag_coefficient_is_recovered(self):
        self.assertAlmostEqual(self.an.descent["implied_drag_coefficient"], TRUTH["drag_coefficient"],
                               delta=0.06 * TRUTH["drag_coefficient"])

    def test_release_height(self):
        self.assertAlmostEqual(self.an.descent["release_height_m"], TRUTH["release_altitude_m"], delta=0.5)

    def test_no_mass_means_no_drag_coefficient_and_no_error(self):
        an = _analysis(mass=None)
        self.assertIn("terminal_rate_mps", an.descent)
        self.assertNotIn("implied_drag_coefficient", an.descent)

    def test_the_ground_csv_alone_gives_the_same_rate(self):
        an = _analysis(path=GROUND_CSV, compare=None)
        self.assertAlmostEqual(an.descent["terminal_rate_mps"], TRUTH["terminal_rate_mps"],
                               delta=0.04 * TRUTH["terminal_rate_mps"])


class DynamicsGpsSoundComparison(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.an = _analysis()

    def test_free_fall_after_release_reads_near_zero(self):
        self.assertLess(self.an.dynamics["min_accel_after_release_mps2"], 3.0)

    def test_spin_rate(self):
        self.assertAlmostEqual(self.an.dynamics["spin_rate_dps"], TRUTH["spin_dps"], delta=0.05 * TRUTH["spin_dps"])

    def test_pendulum_frequency_within_resolution(self):
        d = self.an.dynamics
        self.assertLess(d["pendulum_frequency_hz"], d["nyquist_hz"])
        self.assertAlmostEqual(d["pendulum_frequency_hz"], TRUTH["pendulum_hz"], delta=d["pendulum_resolution_hz"])

    def test_the_sampling_limit_is_stated(self):
        self.assertIn("lower bounds", self.an.dynamics["note_sampling"])

    def test_gps_drift(self):
        self.assertAlmostEqual(self.an.gps["drift_m"], TRUTH["drift_m"], delta=3.0)

    def test_flight_rate_is_the_mean_not_the_median_interval(self):
        self.assertAlmostEqual(self.an.quality["flight_rate_hz"], 3.0 / 0.966, delta=0.05)

    def test_the_packet_lost_in_the_descent_is_found(self):
        c = self.an.comparison
        self.assertEqual(c["missing_from_other"], TRUTH["packets_lost_on_ground"])
        self.assertIn(TRUTH["packet_lost_mid_descent"], c["missing_during_descent"])

    def test_the_microphone_levels_separate_the_phases(self):
        s = self.an.sound
        self.assertGreater(s["median_lift_and_hover_mv"], 5 * s["median_pad_mv"])
        self.assertIn("not a sound pressure level", s["note"])


class Output(unittest.TestCase):
    def test_figures_summary_and_json_are_written(self):
        with tempfile.TemporaryDirectory() as tmp:
            an = fa.run(SD, tmp, GROUND_CSV, TRUTH["mass_kg"], TRUTH["canopy_diameter_m"])
            names = sorted(p.name for p in Path(tmp).iterdir())
            for required in ("01-altitude.png", "02-temperature.png", "03-pressure.png", "summary.md", "analysis.json"):
                self.assertIn(required, names)
            summary = (Path(tmp) / "summary.md").read_text(encoding="utf-8")
            self.assertIn("Steady descent rate", summary)
            self.assertIn("lower bound", summary)
            json.loads((Path(tmp) / "analysis.json").read_text(encoding="utf-8"))
            self.assertIsNotNone(an.phases.release_t)

    def test_a_bench_log_with_no_flight_does_not_crash(self):
        with tempfile.TemporaryDirectory() as tmp:
            lines = SD.read_text(encoding="utf-8").splitlines()
            path = Path(tmp) / "bench.csv"
            path.write_text("\n".join(lines[:150]) + "\n", encoding="utf-8")
            an = fa.run(path, Path(tmp) / "out")
            self.assertIsNone(an.phases.launch_t)
            self.assertIn("note", an.descent)

    def test_the_cli_runs(self):
        with tempfile.TemporaryDirectory() as tmp:
            import contextlib
            import io
            with contextlib.redirect_stdout(io.StringIO()) as printed:
                self.assertEqual(fa.main([str(SD), "--compare", str(GROUND_CSV), "--mass", "0.5", "--out", tmp]), 0)
            self.assertIn("implied drag coefficient", printed.getvalue())


class TheNotebook(unittest.TestCase):
    """Jupyter is not a dependency, so the notebook is executed the way a kernel would: every
    code cell, in order, in one namespace -- against the synthetic flight it points at."""

    NOTEBOOK = ROOT / "analysis" / "flight_analysis.ipynb"

    def test_it_is_valid_nbformat_4(self):
        nb = json.loads(self.NOTEBOOK.read_text(encoding="utf-8"))
        self.assertEqual(nb["nbformat"], 4)
        for cell in nb["cells"]:
            self.assertIn(cell["cell_type"], ("markdown", "code"))
            if cell["cell_type"] == "code":
                self.assertEqual(cell["outputs"], [], "commit the notebook without outputs")

    def test_it_uses_no_ipython_only_syntax(self):
        nb = json.loads(self.NOTEBOOK.read_text(encoding="utf-8"))
        for cell in nb["cells"]:
            if cell["cell_type"] == "code":
                for line in cell["source"]:
                    self.assertFalse(line.lstrip().startswith(("%", "!")), line)

    def test_every_cell_runs_in_order(self):
        import matplotlib.pyplot as plt
        nb = json.loads(self.NOTEBOOK.read_text(encoding="utf-8"))
        tmp = tempfile.mkdtemp()
        cwd = os.getcwd()
        try:
            os.chdir(ROOT / "analysis")
            namespace = {"__name__": "__main__"}
            import io
            import contextlib
            import warnings
            with contextlib.redirect_stdout(io.StringIO()), warnings.catch_warnings():
                warnings.simplefilter("ignore")
                for index, cell in enumerate(nb["cells"]):
                    if cell["cell_type"] != "code":
                        continue
                    source = "".join(cell["source"]).replace(
                        'OUTPUT_DIR = "analysis-output"', f"OUTPUT_DIR = {tmp!r}")
                    exec(compile(source, f"notebook-cell-{index}", "exec"), namespace)
            self.assertTrue((Path(tmp) / "summary.md").exists())
            self.assertIn("synthetic-flight", namespace["PRIMARY_LOG"])
        finally:
            os.chdir(cwd)
            plt.close("all")
            shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    unittest.main()
