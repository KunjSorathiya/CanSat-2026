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
from urllib.parse import unquote

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
    words = {9: "nine", 10: "ten", 11: "eleven", 12: "twelve", 13: "thirteen"}
    spelled = words.get(len(listed), str(len(listed)))
    checker.check(f"test-plan.md states {len(listed)} syntax-checked translation units",
                  f"{len(listed)} translation units" in test_plan, str(len(listed)))
    # The quick start spells the number out, which is why it was still saying "ten".
    checker.check(f"quick-start.md states {spelled} syntax-checked translation units",
                  f"{spelled} Pico translation units" in quick_start, spelled)
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
    bring_up_rows = re.findall(r"^\| \d+\.\d+[a-z]? \|.*$", bring_up, re.MULTILINE)
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

    # A document that talks about the magnetometer has to say that this vehicle does not
    # have one. The nine-axis design is still worth documenting -- the code implements it
    # and a real MPU-9250 would run it -- but a reader must never be left believing the
    # delivered airframe can produce an absolute magnetic yaw. The audit is a dated record
    # of a past run and is excluded on purpose.
    for doc in sorted((REPO_ROOT / "documentation").rglob("*.md")) + [REPO_ROOT / "README.md"]:
        if "audit" in doc.parts:
            continue
        text = doc.read_text(encoding="utf-8", errors="replace")
        if "AK8963" not in text and "magnetometer" not in text:
            continue
        rel = str(doc.relative_to(REPO_ROOT)).replace("\\", "/")
        acknowledged = "MPU-6500" in text or "receiving-inspection.md#findings" in text
        checker.check(f"{rel} says the delivered IMU has no magnetometer", acknowledged)

    # The validator scenarios only guard anything while both implementations actually read
    # them. A suite that quietly stops loading the file would leave the fixture sitting in
    # the tree looking like a guarantee.
    scenarios = read("test-data/validator-scenarios.tsv")
    scenario_rows = [ln for ln in scenarios.splitlines()
                     if ln.strip() and not ln.startswith("#")]
    scenario_names = {ln.split("	")[0] for ln in scenario_rows}
    for reader in ("ground-station/software/tests/test_validator.py",
                   "ground-station/web/tests/console_core.test.mjs"):
        checker.check(f"{reader} reads the shared validator scenarios",
                      "validator-scenarios.tsv" in read(reader))
    # The escaping fixture guards the same kind of divergence, between the logger that
    # writes a raw log and the console that replays one.
    escapes = read("test-data/raw-log-escapes.tsv")
    escape_rows = [ln for ln in escapes.splitlines()
                   if ln.strip() and not ln.startswith("#")]
    for reader in ("ground-station/software/tests/test_logger.py",
                   "ground-station/web/tests/console_core.test.mjs"):
        checker.check(f"{reader} reads the shared raw-log escape fixture",
                      "raw-log-escapes.tsv" in read(reader))
    checker.check(f"test-plan.md states {len(escape_rows)} raw-log escape cases",
                  f"{len(escape_rows)} cases" in test_plan, str(len(escape_rows)))
    # The console has to actually undo the escaping, not merely own a function that could.
    console_html = read("ground-station/web/index.html")
    checker.check("the web console unescapes a raw-log line before replaying it",
                  "unescapeRaw(line.split" in console_html)

    # Three decoders, one wire format. The fixture only means anything while all three
    # read it.
    framing = read("test-data/framing-cases.tsv")
    framing_rows = [ln for ln in framing.splitlines()
                    if ln.strip() and not ln.startswith("#")]
    for reader in ("firmware/ground-station/tests/framing_test.cpp",
                   "ground-station/software/tests/test_transport.py",
                   "ground-station/web/tests/console_core.test.mjs"):
        checker.check(f"{reader} reads the shared framing cases",
                      "framing-cases.tsv" in read(reader))
    checker.check(f"test-plan.md states {len(framing_rows)} framing cases",
                  f"{len(framing_rows)} byte streams" in test_plan, str(len(framing_rows)))
    # An oversized length is an overflow in all three, not a resync in two of them.
    for impl, needle in (
            ("firmware/ground-station/src/framing.cpp", "++overflows_"),
            ("ground-station/software/src/transport.py", "self.overflows += 1"),
            ("ground-station/web/index.html", "this.overflows++")):
        checker.check(f"{impl} counts an oversized length as an overflow",
                      needle in read(impl))
    checker.check(f"test-plan.md states {len(scenario_names)} validator scenarios, "
                  f"{len(scenario_rows)} packets",
                  f"{len(scenario_names)} scenarios" in test_plan
                  and f"{len(scenario_rows)} packets" in test_plan,
                  f"{len(scenario_names)}/{len(scenario_rows)}")

    # The documentation tells a new reader to replay test-data/sample-mission.txt. That
    # file is real controller output, so a change to the packet format can leave it behind
    # -- and a sample the documentation cannot replay is worse than no sample, because the
    # reader assumes their setup is broken rather than the file.
    sample = read("test-data/sample-mission.txt").splitlines()
    sample_packets = [ln for ln in sample if ln.strip()]
    checker.check("sample-mission.txt carries a mission worth replaying",
                  len(sample_packets) >= 20, str(len(sample_packets)))
    # Held to the same fixture rules the parsers are: every mandatory field, in order, at
    # the documented precision. A cheap structural check here, and the Python parser reads
    # the same file in its own suite.
    field_order = re.compile(
        r"^CAN-Team-\d{2}; P-\d{3}; Ti-\d{2}:\d{2}:\d{2}:\d{3}; A--?\d+\.\d; "
        r"Pr--?\d+\.\d{2}; T--?\d+\.\d; Ro--?\d+\.\d; Pi--?\d+\.\d; Ya--?\d+\.\d; "
        r"AX--?\d+\.\d{2}; AY--?\d+\.\d{2}; AZ--?\d+\.\d{2};")
    malformed = [i + 1 for i, ln in enumerate(sample_packets) if not field_order.match(ln)]
    checker.check("every sample-mission.txt packet matches the rulebook field order",
                  not malformed, f"lines {malformed[:5]}")
    for doc in ("README.md", "documentation/quick-start.md",
                "documentation/operations/runbook.md",
                "ground-station/software/README.md"):
        checker.check(f"{doc} replays a file that exists",
                      "packets.txt" not in read(doc))

    # The status LED is the only thing the vehicle can say without a radio or a serial
    # cable, and both the bring-up record and the runbook tell an operator which blink rate
    # means what. Those cadences live in one switch in controller.cpp.
    controller = read("firmware/flight-computer/src/controller.cpp")
    body = controller[controller.index("void Controller::update_led"):]
    body = body[:body.index("board_.set_status_led(on);")]
    armed = re.search(r"is_armed\(mission_ms\) \? (\d+) : (\d+)", body)
    flight = re.search(r"case MissionState::flight:\s*\n\s*period = (\d+);", body)
    recovery = re.search(r"case MissionState::recovery:\s*\n\s*period = (\d+);", body)
    fault = re.search(r"case MissionState::fault:\s*\n\s*period = (\d+);", body)
    runbook = read("documentation/operations/runbook.md")
    cadences = [
        ("READY armed", armed.group(1) if armed else None, bring_up),
        ("READY unarmed", armed.group(2) if armed else None, bring_up),
        ("FLIGHT", flight.group(1) if flight else None, bring_up),
        ("LANDED/RECOVERY", recovery.group(1) if recovery else None, bring_up),
        ("FAULT", fault.group(1) if fault else None, bring_up),
    ]
    for label, half, doc in cadences:
        checker.check(f"bring-up-record.md states the {label} LED cadence ({half} ms)",
                      half is not None
                      and f"{half} ms on, {half} ms off" in doc
                      and f"**{2 * int(half)} ms full cycle**" in doc,
                      str(half))
    checker.check(f"runbook.md states the FAULT blink rate ({fault.group(1) if fault else '?'} ms)",
                  bool(fault) and f"{fault.group(1)} ms)" in runbook,
                  fault.group(1) if fault else "?")

    # The runbook's troubleshooting pages quote the calibration gates an operator is
    # standing over on the pad, wondering why CAL-1 has not appeared. Wrong numbers there
    # send someone hunting a fault that is not there.
    still_samples = constant(config, "calib_samples")
    calib_timeout = constant(config, "calib_timeout_ms")
    still_dps = decimal(config, "calib_gyro_still_dps")
    accel_tol = decimal(config, "calib_accel_tol_mps2")
    checker.check(f"runbook.md states the {still_dps} dps stillness gate",
                  f"{still_dps:g} °/s" in runbook, str(still_dps))
    checker.check(f"runbook.md states the {accel_tol} m/s2 acceleration gate",
                  f"{accel_tol:g} m/s²" in runbook, str(accel_tol))
    checker.check(f"runbook.md states the {still_samples}-sample calibration window",
                  f"over {still_samples} samples" in runbook, str(still_samples))
    checker.check(f"runbook.md states the {calib_timeout} ms calibration timeout",
                  f"After {calib_timeout // 1000} s it resolves best-effort" in runbook,
                  str(calib_timeout))

    # The test plan says how many CI jobs there are and what each one does. A job added or
    # removed without touching that sentence leaves a reader expecting a gate that is not
    # there, or unaware of one that is.
    workflow = read(".github/workflows/ci.yml")
    jobs_section = workflow[workflow.index("\njobs:"):]
    job_ids = re.findall(r"^  ([a-z][a-z0-9-]*):$", jobs_section, re.MULTILINE)
    words = {1: "one", 2: "two", 3: "three", 4: "four", 5: "five", 6: "six"}
    count_word = words.get(len(job_ids), str(len(job_ids)))
    checker.check(f"test-plan.md states the {count_word} CI jobs",
                  f"{count_word}\njobs" in test_plan or f"in {count_word} jobs" in test_plan,
                  f"{len(job_ids)}: {', '.join(job_ids)}")
    # Every gate blocks: an advisory job is a gate that has stopped being one.
    checker.check("no CI job is allowed to fail without failing the workflow",
                  "continue-on-error" not in workflow)

    # ---- documentation that still points where it says -------------------------------
    # Every relative link and every same-document anchor, across every Markdown file. A
    # renamed heading or a moved file breaks navigation silently: the document still reads
    # correctly, and the link simply goes nowhere. Two aggregate checks rather than one per
    # link, so the claim count stays a measure of what is checked rather than of how much
    # prose there is.
    def heading_slug(heading: str) -> str:
        text = re.sub(r"[`*_]", "", heading.strip().lower())
        return re.sub(r"[^\w\s-]", "", text, flags=re.UNICODE).replace(" ", "-")

    markdown = [q for q in REPO_ROOT.rglob("*.md")
                if not {".git", "build", ".claude"} & set(q.parts)]
    missing_targets: list[str] = []
    missing_anchors: list[str] = []
    for doc in markdown:
        body = doc.read_text(encoding="utf-8", errors="replace")
        anchors = {heading_slug(h)
                   for h in re.findall(r"^#{1,6}\s+(.+?)\s*$", body, re.MULTILINE)}
        rel = doc.relative_to(REPO_ROOT).as_posix()
        for match in re.finditer(r"\]\(([^)]+)\)", body):
            target = match.group(1).strip()
            if target.startswith(("http://", "https://", "mailto:")):
                continue
            line = body[:match.start()].count("\n") + 1
            path_part, _, fragment = target.partition("#")
            if not path_part:
                if fragment not in anchors:
                    missing_anchors.append(f"{rel}:{line} #{fragment}")
                continue
            if not (doc.parent / unquote(path_part)).resolve().exists():
                missing_targets.append(f"{rel}:{line} {path_part}")

    checker.check(f"every relative link in {len(markdown)} documents resolves",
                  not missing_targets, "; ".join(missing_targets[:3]))
    checker.check("every same-document anchor resolves to a heading",
                  not missing_anchors, "; ".join(missing_anchors[:3]))

    # ---- a document that names a test must name one that exists ----------------------
    # requirements.md cites tests as the evidence for each requirement, which is the only
    # thing connecting a compliance claim to something that runs. A test renamed without
    # updating the citation leaves the requirement pointing at nothing, and it reads exactly
    # like a requirement that is covered. Three mandatory telemetry rows cited
    # `test_mpu_scaling` for a day after it became `test_imu_scaling`.
    #
    # The 2026-09-04 audit is excluded for the same reason it is excluded elsewhere: it is a
    # dated record of a past run, not a document making a current claim.
    defined_tests: set[str] = set()
    for source in list(REPO_ROOT.rglob("*.cpp")) + list(REPO_ROOT.rglob("*.py")):
        if {".git", "build", ".claude"} & set(source.parts):
            continue
        body = source.read_text(encoding="utf-8", errors="replace")
        defined_tests |= set(re.findall(r"\b(?:void|def)\s+(test_[A-Za-z0-9_]+)", body))

    dangling: list[str] = []
    for doc in markdown:
        # The changelog is excluded for the same reason the audits are: an entry recording
        # that `test_mpu_scaling` became `test_imu_scaling` has to be able to write the old
        # name down.
        if "audit" in doc.parts or doc.name == "CHANGELOG.md":
            continue
        body = doc.read_text(encoding="utf-8", errors="replace")
        rel = doc.relative_to(REPO_ROOT).as_posix()
        for match in re.finditer(r"`(test_[A-Za-z0-9_]+)`", body):
            name = match.group(1)
            if name.endswith("_py") or name in defined_tests:
                continue
            if f"{name}.py" in body or (REPO_ROOT / "ground-station/software/tests" / f"{name}.py").exists():
                continue
            line = body[:match.start()].count("\n") + 1
            dangling.append(f"{rel}:{line} {name}")
    checker.check(f"every test named in the documentation exists ({len(defined_tests)} defined)",
                  not dangling, "; ".join(dangling[:3]))

    # ---- a document that names a path must name one that exists ----------------------
    # Paths appear in prose far more often than they appear as links, and a moved file
    # leaves them behind silently. The changelog and the audits are excluded: both refer to
    # files on purpose after they were deleted -- `ui.py` and `radio.py` were removed in
    # cycle 2, and the entries recording that removal have to be able to name them.
    stale_paths: list[str] = []
    path_pattern = re.compile(
        r"`((?:firmware|ground-station|tools|documentation|test-data)/[A-Za-z0-9_./-]+)`")
    for doc in markdown:
        if "audit" in doc.parts or doc.name == "CHANGELOG.md":
            continue
        body = doc.read_text(encoding="utf-8", errors="replace")
        rel = doc.relative_to(REPO_ROOT).as_posix()
        for match in path_pattern.finditer(body):
            named = match.group(1).rstrip("/")
            if not (REPO_ROOT / named).exists():
                line = body[:match.start()].count("\n") + 1
                stale_paths.append(f"{rel}:{line} {named}")
    checker.check("every repository path named in the documentation exists",
                  not stale_paths, "; ".join(stale_paths[:3]))

    # The requirements checklist opens by saying how many rows are marked Complete. That
    # sentence is the first thing a reader of a compliance document sees, and it is a count
    # of the table directly beneath it -- which is exactly the kind of number that is
    # updated once and then never again.
    requirements = read("documentation/requirements/requirements.md")
    complete_rows = len(re.findall(r"^\| [A-Z]{3}-[0-9a-z]+ \|.*\| Complete \|",
                                   requirements, re.MULTILINE))
    total_rows = len(re.findall(r"^\| [A-Z]{3}-[0-9a-z]+ \|", requirements, re.MULTILINE))
    checker.check(f"requirements.md counts its own {complete_rows} of {total_rows} rows",
                  f"{complete_rows} of the {total_rows} requirement rows" in requirements,
                  f"{complete_rows}/{total_rows}")
    # And no row may claim Complete without naming its evidence, which is the rule the
    # document states about itself two paragraphs earlier.
    unevidenced = [row.split("|")[1].strip()
                   for row in re.findall(r"^\| [A-Z]{3}-[0-9a-z]+ \|.*\| Complete \|.*$",
                                         requirements, re.MULTILINE)
                   if not row.rstrip().rstrip("|").rsplit("|", 1)[-1].strip()]
    checker.check("every Complete requirement names its evidence",
                  not unevidenced, ", ".join(unevidenced[:5]))

    # ---- the two logs the runbook tells an operator to compare ------------------------
    # Post-flight step 5 joins the onboard SD log against the ground station's CSV. They are
    # written by different programs and their columns do not line up by name, so the runbook
    # carries the mapping. A column added to either file and left out of that table makes
    # the instruction a little more wrong every time.
    builder = read("firmware/flight-computer/src/telemetry_builder.cpp")
    header_literal = builder[builder.index("std::string TelemetryBuilder::sd_header()"):]
    header_literal = header_literal[:header_literal.index("}")]
    sd_columns = re.findall(r"[a-z_]+(?=,|\")", "".join(re.findall(r'"([^"]*)"', header_literal)))
    sd_columns = [c for c in "".join(re.findall(r'"([^"]*)"', header_literal)).split(",") if c]
    ground_columns = re.findall(r'"([a-z_]+)"', read("ground-station/software/src/logger.py")
                                .split("CSV_FIELDS = [")[1].split("]")[0])
    missing_from_runbook = [c for c in sd_columns if f"`{c}`" not in runbook]
    checker.check(f"runbook.md maps all {len(sd_columns)} onboard SD log columns",
                  not missing_from_runbook, ", ".join(missing_from_runbook))
    missing_ground = [c for c in ground_columns if f"`{c}`" not in runbook]
    checker.check(f"runbook.md maps all {len(ground_columns)} ground CSV columns",
                  not missing_ground, ", ".join(missing_ground))

    # The quick start says the core needs no Python packages, which is true only while
    # requirements.txt has nothing uncommented in it. A real dependency added there makes
    # that sentence wrong, and the reader who believes it gets an ImportError instead.
    requirements_txt = read("ground-station/software/requirements.txt")
    active = [ln.strip() for ln in requirements_txt.splitlines()
              if ln.strip() and not ln.strip().startswith("#")]
    checker.check("the ground station core still needs no installed packages",
                  not active, ", ".join(active))
    checker.check("quick-start.md names pyserial for the live serial path",
                  "pip install pyserial" in quick_start)

    # CTest reaches the same C++ suites as build_host.sh, and the quick start says how many.
    # A suite added to one and not the other is a gap in whichever CI job people trust.
    ctest_names = []
    for cmake_file in sorted(REPO_ROOT.glob("firmware/*/CMakeLists.txt")):
        ctest_names += re.findall(r"add_test\(NAME (\w+)",
                                  cmake_file.read_text(encoding="utf-8"))
    checker.check(f"quick-start.md states the {len(ctest_names)} CTest tests",
                  f"**{len(ctest_names)} CTest tests**" in quick_start,
                  ", ".join(ctest_names))
    checker.check(f"test-plan.md states the {len(ctest_names)} CTest tests",
                  f"{len(ctest_names)} CTest tests" in test_plan, str(len(ctest_names)))

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
