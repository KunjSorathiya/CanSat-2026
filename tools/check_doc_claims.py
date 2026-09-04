#!/usr/bin/env python3
"""Check that the numbers the documentation states still match the source that defines them.

Documentation drifts silently. A constant changes, the prose that quotes it does not, and
the next person reads a figure that has not been true for weeks — which on this project
would mean wiring to the wrong pin or trusting a telemetry rate the radio cannot deliver.

This runs in `tools/build_host.sh`, so a change that makes a document wrong fails the same
build as a change that makes a test wrong.

    python tools/check_doc_claims.py
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from link_budget import ModemConfig, time_on_air  # noqa: E402


def read(relative: str) -> str:
    return (REPO_ROOT / relative).read_text(encoding="utf-8")


def constant(source: str, name: str) -> int | None:
    """Read an integer constant, tolerating C++ digit separators (125'000)."""
    match = re.search(rf"\b{name}\s*=\s*([0-9][0-9_']*)", source)
    if not match:
        return None
    return int(match.group(1).replace("_", "").replace("'", ""))


def decimal(source: str, name: str) -> float | None:
    match = re.search(rf"\b{name}\s*=\s*([0-9]+\.?[0-9]*)", source)
    return float(match.group(1)) if match else None


def suite_counts() -> dict[str, int] | None:
    """Read what each suite reported from the log tools/build_host.sh just wrote.

    Assertion totals cannot be counted statically -- a table-driven test runs one CHECK
    many times -- so the only honest source for them is the suites' own output.
    """
    log_path = REPO_ROOT / "build" / "host" / "test-output.log"
    if not log_path.exists():
        return None
    log = log_path.read_text(encoding="utf-8", errors="replace")

    counts: dict[str, int] = {}
    # Each C++ suite prints its assertion total, then a line naming itself.
    for total, name in re.findall(r"(\d+)/\d+ checks passed\s*\n(\S+)", log):
        key = {"flight_tests": "flight_tests", "sx1278": "sx1278", "sd_card": "sd_card"}.get(name)
        if key:
            counts[key] = int(total)
    # unittest prints "Ran N tests" once per discovery run: ground station first, tooling second.
    ran = [int(n) for n in re.findall(r"^Ran (\d+) tests?", log, re.MULTILINE)]
    if len(ran) >= 2:
        counts["python_ground"], counts["python_tools"] = ran[0], ran[1]
    node = re.search(r"^\D*pass (\d+)$", log, re.MULTILINE)
    if node:
        counts["node"] = int(node.group(1))

    required = {"flight_tests", "sx1278", "sd_card", "python_ground", "python_tools", "node"}
    return counts if required <= counts.keys() else None


class Checker:
    def __init__(self) -> None:
        self.results: list[tuple[str, bool, str]] = []

    def check(self, name: str, ok: bool, detail: str = "") -> None:
        self.results.append((name, bool(ok), detail))

    def report(self) -> int:
        failed = [r for r in self.results if not r[1]]
        for name, ok, detail in self.results:
            marker = "  ok  " if ok else "FAIL  "
            suffix = f"   [{detail}]" if detail and not ok else ""
            print(f"{marker}{name}{suffix}")
        total = len(self.results)
        print(f"\n{total - len(failed)}/{total} documented claims match the source")
        return 1 if failed else 0


def main() -> int:
    checker = Checker()

    # ---- the radio link profile, quoted throughout the design documents -------------
    profile = read("firmware/common/include/cansat/link_profile.hpp")
    spreading_factor = constant(profile, "kSpreadingFactor")
    bandwidth = constant(profile, "kBandwidthHz")
    coding_rate = constant(profile, "kCodingRate")
    period_ms = constant(profile, "kTelemetryPeriodMs")
    budget_bytes = constant(profile, "kWorstCasePacketBytes")

    checker.check("link profile: SF7", spreading_factor == 7, str(spreading_factor))
    checker.check("link profile: 125 kHz", bandwidth == 125000, str(bandwidth))
    checker.check("link profile: CR 4/5", coding_rate == 5, str(coding_rate))
    checker.check("link profile: 1000 ms period", period_ms == 1000, str(period_ms))
    checker.check("link profile: 255-byte budget", budget_bytes == 255, str(budget_bytes))
    checker.check("link profile: sync words 0xF3 / 0xA5",
                  "0xF3" in profile and "0xA5" in profile)

    # The airtime figure the link budget quotes must still be what the model computes.
    link_budget = read("documentation/design/link-budget.md")
    airtime_ms = time_on_air(
        budget_bytes or 255,
        ModemConfig(spreading_factor=spreading_factor or 7, bandwidth_hz=bandwidth or 125000,
                    coding_rate_denominator=coding_rate or 5),
    ).time_on_air_ms
    checker.check(f"link-budget.md quotes the computed airtime ({airtime_ms:.1f} ms)",
                  f"{airtime_ms:.1f}" in link_budget, f"{airtime_ms:.1f}")
    for measured in ("118", "167", "212"):
        checker.check(f"link-budget.md quotes the measured {measured}-byte packet size",
                      f"**{measured}**" in link_budget)

    # The web console carries its own copy of the two rulebook sync words, because it is a
    # single self-contained HTML file that cannot include a C++ header. A copy is a thing
    # that drifts, so it is held to the original here.
    console = read("ground-station/web/index.html")
    test_sync = re.search(r"kTestSyncWord\s*=\s*(0x[0-9A-Fa-f]+)", profile)
    launch_sync = re.search(r"kOfficialSyncWord\s*=\s*(0x[0-9A-Fa-f]+)", profile)
    for label, match, js_name in (("test", test_sync, "SYNC_TEST"),
                                  ("launch", launch_sync, "SYNC_LAUNCH")):
        word = match.group(1).upper().replace("0X", "0x") if match else "?"
        checker.check(f"web console's {js_name} matches the {label} sync word {word}",
                      f"const {js_name} = {word};" in console, word)

    # ---- acquisition and sensor configuration ---------------------------------------
    config = read("firmware/flight-computer/include/flight/config.hpp")
    sensor_period = constant(config, "sensor_period_ms")
    gyro_dlpf = constant(config, "imu_gyro_dlpf_cfg")
    accel_dlpf = constant(config, "imu_accel_dlpf_cfg")
    sample_div = constant(config, "imu_sample_rate_div")
    gyro_bias_bound = decimal(config, "calib_max_gyro_bias_dps")

    checker.check("config: 33 ms sensor period", sensor_period == 33, str(sensor_period))
    # The MPU-9250 filters the gyroscope and the accelerometer from two separate
    # registers, so there are two settings to keep the documents honest about.
    checker.check("config: gyro DLPF_CFG 4", gyro_dlpf == 4, str(gyro_dlpf))
    checker.check("config: accel A_DLPF_CFG 4", accel_dlpf == 4, str(accel_dlpf))
    checker.check("config: SMPLRT_DIV 4", sample_div == 4, str(sample_div))
    checker.check("config: gyro bias bounded at 25 dps", gyro_bias_bound == 25.0,
                  str(gyro_bias_bound))

    sensor_rates = read("documentation/design/sensor-rates.md")
    achieved_hz = round(1000.0 / (sensor_period or 33))
    checker.check(f"sensor-rates.md states the {achieved_hz} Hz acquisition rate",
                  f"**{achieved_hz} Hz**" in sensor_rates, str(achieved_hz))
    checker.check("sensor-rates.md states the 33 ms period",
                  f"{sensor_period} ms" in sensor_rates)
    checker.check("sensor-rates.md states the 21.2 Hz accelerometer bandwidth for DLPF 4",
                  "21.2 Hz" in sensor_rates)
    checker.check("sensor-rates.md states the 20 Hz gyroscope bandwidth for DLPF 4",
                  "20 Hz" in sensor_rates)
    checker.check("sensor-rates.md states the 100 Hz magnetometer rate",
                  "100 Hz" in sensor_rates)

    mag_mode = re.search(r"mag_mode\s*=\s*sensors::MagMode::(\w+)", config)
    checker.check("config: magnetometer in 100 Hz continuous mode",
                  bool(mag_mode) and mag_mode.group(1) == "continuous_100hz",
                  mag_mode.group(1) if mag_mode else "None")
    checker.check("config: magnetometer at 16-bit resolution",
                  "MagResolution::bits16" in config)
    # An uncalibrated magnetometer must never ship claiming an absolute heading.
    checker.check("config: magnetometer calibration ships invalid",
                  "sensors::MagCalibration mag_calibration{};" in config)

    architecture = read("documentation/design/software-architecture.md")
    checker.check("software-architecture.md states the sensor period",
                  f"| {sensor_period} ms |" in architecture)
    checker.check("software-architecture.md states the telemetry period",
                  f"| {period_ms} ms |" in architecture)

    # ---- watchdogs and loop timing --------------------------------------------------
    flight_main = read("firmware/flight-computer/src/pico/main.cpp")
    bridge_main = read("firmware/ground-station/src/pico/main.cpp")
    checker.check("flight watchdog is 2000 ms", "watchdog_enable(2000" in flight_main)
    checker.check("bridge watchdog is 3000 ms", "watchdog_enable(3000" in bridge_main)
    # The tick is configuration now, and main() passes it through rather than hard-coding it.
    tick_ms = constant(config, "loop_tick_ms")
    checker.check("config: 2 ms loop tick", tick_ms == 2, str(tick_ms))
    checker.check("flight main uses the configured tick",
                  "tick_delay_ms(config.loop_tick_ms)" in flight_main)
    checker.check(f"architecture states the {tick_ms} ms tick",
                  f"{tick_ms} ms tick" in architecture or f"| {tick_ms} ms |" in architecture)

    # The tick must stay under half the GPS UART FIFO fill time, and the documents that
    # explain why must quote the same number the formula produces.
    gps_baud = constant(config, "gps_baud")
    fifo_bytes = constant(config, "gps_uart_fifo_bytes")
    fifo_ms = 1000.0 * (fifo_bytes or 32) * 10.0 / (gps_baud or 9600)
    checker.check("config: GPS at 9600 baud", gps_baud == 9600, str(gps_baud))
    checker.check("config: 32-byte UART FIFO", fifo_bytes == 32, str(fifo_bytes))
    checker.check(f"loop tick is under half the {fifo_ms:.1f} ms FIFO fill time",
                  (tick_ms or 0) <= fifo_ms / 2.0)
    checker.check(f"sensor-rates.md quotes the {fifo_ms:.1f} ms FIFO fill time",
                  f"{fifo_ms:.1f} ms" in sensor_rates, f"{fifo_ms:.1f}")

    # ---- GPIO map: the documented pin table must match the firmware ------------------
    pins = {
        "i2c_sda": 4, "i2c_scl": 5, "sd_cs": 6, "imu_int": 7, "gps_tx": 12, "gps_rx": 13,
        "status_led": 14, "spi_miso": 16, "lora_cs": 17, "spi_sck": 18, "spi_mosi": 19,
        "lora_reset": 20, "lora_dio0": 21, "lora_dio1": 22, "battery_adc": 26,
    }
    wiring = read("documentation/design/wiring.md")
    for name, expected in pins.items():
        match = re.search(rf"\b{name}\s*=\s*(\d+)", config)
        actual = int(match.group(1)) if match else None
        checker.check(f"config: {name} on GP{expected}", actual == expected, str(actual))
        checker.check(f"wiring.md lists GP{expected} for `{name}`",
                      f"| GP{expected} |" in wiring and f"`{name}`" in wiring)

    # ---- the size of the test suites, as the suites themselves report it ------------
    # These counts appear in the README badge, the quick start and the test plan, and they
    # are exactly the kind of figure that is true on the day it is written and wrong a week
    # later. tools/build_host.sh writes what every suite reported to build/host/test-output.log
    # on the run that precedes this check, so the documents are held to the current numbers
    # rather than to remembered ones.
    test_plan = read("documentation/testing/test-plan.md")
    readme = read("README.md")
    quick_start = read("documentation/quick-start.md")
    timeline = read("documentation/project/timeline.md")

    # Every suite main() calls must have a row explaining what it proves. A test nobody
    # documented is a test nobody can tell you the purpose of when it fails.
    flight_test_source = read("firmware/flight-computer/tests/flight_tests.cpp")
    main_body = flight_test_source[flight_test_source.index("int main("):]
    suites = re.findall(r"^\s{4}(test_[a-z0-9_]+)\(", main_body, re.MULTILINE)
    documented = set(re.findall(r"^\| `(test_[a-z0-9_]+)`", test_plan, re.MULTILINE))
    undocumented = [name for name in suites if name not in documented]
    checker.check(f"every one of the {len(suites)} flight_tests suites has a test-plan row",
                  not undocumented, ", ".join(undocumented))
    checker.check(f"test-plan.md states {len(suites)} flight_tests suites",
                  f"{len(suites)} suites" in test_plan, str(len(suites)))

    # The syntax check is the only thing standing between a Pico-only source file and a
    # defect nobody sees until the firmware is built. A driver added to src/pico/ and left
    # out of tools/check_pico_syntax.sh is checked by nothing at all, so the list is held to
    # the directory rather than to whoever remembered to edit it.
    syntax_script = read("tools/check_pico_syntax.sh")
    listed = re.findall(r"^\s+(firmware/\S+\.cpp)$", syntax_script, re.MULTILINE)
    on_disk = sorted(str(q.relative_to(REPO_ROOT)).replace("\\", "/")
                     for q in REPO_ROOT.glob("firmware/*/src/pico/*.cpp"))
    unchecked = [q for q in on_disk if q not in listed]
    checker.check(f"every one of the {len(on_disk)} Pico source files is syntax-checked",
                  not unchecked, ", ".join(unchecked))
    checker.check(f"test-plan.md states {len(listed)} syntax-checked translation units",
                  f"{len(listed)} translation units" in test_plan, str(len(listed)))
    unnamed = [Path(q).stem for q in listed if f"`{Path(q).stem}`" not in test_plan]
    checker.check("test-plan.md names every syntax-checked translation unit",
                  not unnamed, ", ".join(unnamed))

    # The test plan describes each Python suite and says how many tests it holds. Those
    # per-file figures drifted further than the totals did -- test_app.py was documented at
    # 3 tests while holding 11 -- so each one is checked against the file it describes.
    # test_end_to_end.py is prose rather than a heading, and states its figure as "N checks".
    for test_file in sorted((REPO_ROOT / "ground-station/software/tests").glob("test_*.py")):
        n = len(re.findall(r"^\s+def (test_\w+)", test_file.read_text(encoding="utf-8"),
                           re.MULTILINE))
        name = test_file.name
        stated = (f"### `{name}` — {n} tests" in test_plan
                  or (name in test_plan and f"{n} checks:" in test_plan))
        checker.check(f"test-plan.md describes {name} as {n} tests", stated, str(n))

    # How much of the vehicle has actually been measured is a status this project states in
    # more than one place, and the temptation is always to leave it as it was. The
    # bring-up record is the only source: a row with a verdict is measured, a blank one is
    # not, and the README quotes the two totals rather than an impression of them.
    bring_up = read("documentation/testing/bring-up-record.md")
    bring_up_rows = re.findall(r"^\| \d+\.\d+ \|.*$", bring_up, re.MULTILINE)
    measured = [r for r in bring_up_rows if "✅" in r or "⚠" in r or "❌" in r]
    checker.check(f"README states {len(measured)} of {len(bring_up_rows)} bring-up rows measured",
                  f"{len(measured)} of {len(bring_up_rows)} recorded measurements" in readme,
                  f"{len(measured)}/{len(bring_up_rows)}")

    # The delivered IMU is a six-axis MPU-6500, not the nine-axis part it was sold as, and
    # the driver's own list of accepted WHO_AM_I values is the record of which parts this
    # firmware will run on. A document naming a different set is describing a different
    # vehicle.
    imu = read("firmware/flight-computer/include/flight/pico/mpu9250.hpp")
    inspection = read("documentation/hardware/receiving-inspection.md")
    for name, part in (("kWhoAmIMpu9250", "MPU-9250"), ("kWhoAmIMpu9255", "MPU-9255"),
                       ("kWhoAmIMpu6500", "MPU-6500")):
        match = re.search(rf"{name}\s*=\s*(0x[0-9A-Fa-f]+)", imu)
        value = match.group(1) if match else "?"
        checker.check(f"firmware accepts {part} as WHO_AM_I {value}", bool(match), value)
    delivered = re.search(r"kWhoAmIMpu6500\s*=\s*(0x[0-9A-Fa-f]+)", imu)
    word = delivered.group(1) if delivered else "?"
    checker.check(f"receiving-inspection.md records the delivered {word}",
                  f"`{word}`" in inspection, word)
    checker.check(f"README records the delivered {word} rather than a nine-axis part",
                  f"`{word}`" in readme and "MPU-6500" in readme, word)

    counts = suite_counts()
    if counts is None:
        # The log is written by tools/build_host.sh immediately before this script runs.
        # Outside that script there is nothing to compare against, and inventing a number
        # would be worse than saying so.
        checker.check("test counts checked against build/host/test-output.log", True,
                      "log absent -- run tools/build_host.sh")
    else:
        cpp_total = counts["flight_tests"] + counts["sx1278"] + counts["sd_card"]
        for suite in ("flight_tests", "sx1278", "sd_card"):
            n = counts[suite]
            checker.check(f"test-plan.md states {suite} ran {n} assertions",
                          f"**{n} / {n} assertions**" in test_plan, str(n))
        for suite, label in (("python_ground", "Python ground station"),
                             ("python_tools", "Python tooling")):
            n = counts[suite]
            checker.check(f"test-plan.md states {label} ran {n} tests",
                          f"**{n} / {n} tests**" in test_plan, str(n))
        node = counts["node"]
        checker.check(f"test-plan.md states the web console ran {node} tests",
                      f"**{node} / {node} tests**" in test_plan, str(node))
        checker.check(f"timeline.md states the {node} web console tests",
                      f"{node} Node tests" in timeline, str(node))
        checker.check(f"README badge states {cpp_total} C++ assertions",
                      f"C%2B%2B%20tests-{cpp_total}%20assertions" in readme, str(cpp_total))
        # The README carries the same results table as the test plan, in shorter form. It
        # is the first page anyone reads, so it is the worst place for a stale figure.
        for suite in ("flight_tests", "sx1278", "sd_card", "python_ground",
                      "python_tools", "node"):
            n = counts[suite]
            checker.check(f"README's results table states {suite} at {n}",
                          f"**{n} / {n}**" in readme, str(n))
        checker.check(f"README states {len(suites)} flight_tests suites",
                      f"{len(suites)} suites:" in readme, str(len(suites)))
        checker.check(f"README states {len(listed)} syntax-checked translation units",
                      f"{len(listed)} translation units" in readme, str(len(listed)))
        python_total = counts["python_ground"] + counts["python_tools"]
        checker.check(f"quick-start.md states {cpp_total} C++ assertions",
                      f"**{cpp_total} C++ assertions" in quick_start, str(cpp_total))
        checker.check(f"quick-start.md states {python_total} Python tests",
                      f"{python_total} Python tests" in quick_start, str(python_total))
        checker.check(f"quick-start.md states {node} Node tests",
                      f"{node} Node tests" in quick_start, str(node))
        checker.check(f"software-architecture.md states {counts['flight_tests']} flight-core assertions",
                      f"{len(suites)} C++ suites with {counts['flight_tests']} assertions" in architecture,
                      str(counts["flight_tests"]))
        checker.check(f"software-architecture.md states {python_total} Python and {node} Node tests",
                      f"{python_total} Python tests" in architecture and f"{node} Node tests" in architecture)

    # ---- rulebook constants that must never drift -----------------------------------
    checker.check("post-impact window is at least the rulebook's 5 s",
                  (constant(config, "post_impact_transmission_ms") or 0) >= 5000)
    checker.check("telemetry period never exceeds the 1 Hz rulebook minimum",
                  (period_ms or 0) <= 1000)

    # Last, and counting itself: the number of claims this script checks is itself a figure
    # the test plan quotes, so adding a check here without updating that row fails here.
    # Only on a full run, though -- without the suite log a dozen checks are skipped, and
    # the quoted total is the full-run one, not the short-run one.
    if counts is not None:
        claim_total = len(checker.results) + 2
        checker.check(f"test-plan.md states the {claim_total} claims this script checks",
                      f"**{claim_total} / {claim_total} claims**" in test_plan, str(claim_total))
        checker.check(f"README states the {claim_total} claims this script checks",
                      f"**{claim_total} / {claim_total}**" in readme, str(claim_total))

    return checker.report()


if __name__ == "__main__":
    sys.exit(main())
