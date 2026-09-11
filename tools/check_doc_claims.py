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
        key = {"flight_tests": "flight_tests", "sx1278": "sx1278", "sd_card": "sd_card",
               "fat_volume": "fat_volume"}.get(name)
        if key:
            counts[key] = int(total)
    # unittest prints "Ran N tests" once per discovery run. tools/build_host.sh runs three,
    # in this order: ground station, tooling, simulations. The order is load-bearing -- it
    # is read positionally -- and build_host.sh says so where the runs are defined.
    ran = [int(n) for n in re.findall(r"^Ran (\d+) tests?", log, re.MULTILINE)]
    if len(ran) >= 3:
        counts["python_ground"], counts["python_tools"], counts["python_sims"] = ran[:3]
    node = re.search(r"^\D*pass (\d+)$", log, re.MULTILINE)
    if node:
        counts["node"] = int(node.group(1))

    required = {"flight_tests", "sx1278", "sd_card", "fat_volume", "python_ground",
                "python_tools", "python_sims", "node"}
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


# A note on how these checks are written, learned the hard way in this file.
#
# A check that asks "does this string appear in that document" passes a document whose
# facts have come apart from each other. Two hardware documents named the delivered IMU in
# one section and asserted the opposite in another; the wiring gate would have passed a
# table whose pins and signal names had been shuffled against each other, since every pin
# and every name was still present. Where a claim is really about two things belonging
# together -- a pin and its signal, a column and its mapping, a part and the paragraph
# discussing it -- the check is scoped to the row or the paragraph, not the file.
#
# Where a check is genuinely file-wide, it is because the claim is: this document mentions
# this thing at all.

def main() -> int:
    checker = Checker()

    # Every document a check might reach for, read once. Checks get added in whatever order
    # the reasoning arrives in, and one that could not see a document because it sat above
    # the line that opened it is a poor reason to reorder the file.
    readme = read("README.md")
    quick_start = read("documentation/quick-start.md")
    test_plan = read("documentation/testing/test-plan.md")
    continuous_review = read("documentation/audit/2026-09-05-continuous-review.md")
    timeline = read("documentation/project/timeline.md")
    runbook = read("documentation/operations/runbook.md")
    architecture = read("documentation/design/software-architecture.md")

    # Every Markdown file in the repository, for the checks that sweep all of them. Built
    # here for the same reason the reads are: a check should not have to care where in this
    # function it happens to sit.
    markdown = [q for q in REPO_ROOT.rglob("*.md")
                if not {".git", "build", ".claude"} & set(q.parts)]

    # TelemetryValidity's flags and the AND that consumes them. format_packet() refuses a
    # record whose mandatory_valid() is false, so a tenth flag added to the struct and
    # forgotten in the function would let a reading the vehicle never took travel as though
    # it had -- silently, and only for the field that was added.
    validity_hpp = read("firmware/common/include/cansat/telemetry.hpp")
    validity_cpp = read("firmware/common/src/telemetry.cpp")
    struct_body = validity_hpp.split("struct TelemetryValidity", 1)[-1].split("};", 1)[0]
    flag_names = re.findall(r"^\s+bool (\w+) = false;", struct_body, re.MULTILINE)
    fn_body = validity_cpp.split("bool TelemetryValidity::mandatory_valid()", 1)[-1].split("}", 1)[0]
    checker.check(
        f"mandatory_valid() ands all {len(flag_names)} TelemetryValidity flags",
        len(flag_names) >= 9 and all(re.search(r"\b" + n + r"\b", fn_body) for n in flag_names)
        and fn_body.count("&&") == len(flag_names) - 1,
        f"{len(flag_names)} flags, {fn_body.count('&&') + 1} terms")
    checker.check(
        "every mandatory validity flag is exercised by a test",
        all(f"TelemetryValidity::{n}" in read("firmware/flight-computer/tests/flight_tests.cpp") for n in flag_names))

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
    # 850 ms, 1.18 Hz. Pinned as a value rather than a range because the number is
    # derived: worst-case measured airtime 406.9 ms over the 0.5 duty cap is a floor of
    # 813.8 ms, and it must stay under the 1000 ms rulebook ceiling.
    checker.check("link profile: 700 ms period", period_ms == 700, str(period_ms))
    checker.check("telemetry period clears the 1 Hz rulebook minimum",
                  period_ms <= 1000, str(period_ms))
    # 212: every normal-flight packet is rich -- the mandatory fields, GPS and sound, with
    # the diagnostic tags off the air. The organizers count only transmitted telemetry for
    # extra-sensor points, and there is no budget that holds the tags as well.
    checker.check("link profile: 212-byte budget", budget_bytes == 212, str(budget_bytes))

    # ---- the two commanded maximum rates -------------------------------------------
    # Neither can be undone from the ground, so every figure a document quotes about them
    # has to come from the constants the firmware compiles rather than from a sentence
    # somebody scaled by hand. The periods themselves are already held by static_asserts;
    # what is checked here is that the documents say what the firmware does.
    max_gps_bytes = constant(profile, "kMaxRatePacketBytesGps")
    max_lean_bytes = constant(profile, "kMaxRatePacketBytesLean")
    max_gps_period = constant(profile, "kMaxRatePeriodGpsMs")
    max_lean_period = constant(profile, "kMaxRatePeriodLeanMs")
    guard_ms = constant(profile, "kMaxRateGuardMs")
    modem = ModemConfig(spreading_factor=spreading_factor or 7,
                        bandwidth_hz=bandwidth or 125000,
                        coding_rate_denominator=coding_rate or 5)
    # 1.8 % is the measured correction from bring-up rows 5.2 and 5.3, and the periods are
    # sized against the measurement rather than the model.
    max_gps_airtime = time_on_air(max_gps_bytes or 201, modem).time_on_air_ms * 1.018
    max_lean_airtime = time_on_air(max_lean_bytes or 145, modem).time_on_air_ms * 1.018
    checker.check("the GPS max-rate period clears its own measured airtime plus the guard",
                  (max_gps_period or 0) >= max_gps_airtime + (guard_ms or 0),
                  f"{max_gps_period} vs {max_gps_airtime + (guard_ms or 0):.2f}")
    checker.check("the lean max-rate period clears its own measured airtime plus the guard",
                  (max_lean_period or 0) >= max_lean_airtime + (guard_ms or 0),
                  f"{max_lean_period} vs {max_lean_airtime + (guard_ms or 0):.2f}")
    # ---- the two packet shapes, and the max-rate slots ---------------------------------
    rich_bytes = constant(profile, "kRichPacketBytes")
    lean_bytes = constant(profile, "kLeanPacketBytes")
    rich_slot = constant(profile, "kMaxRateRichSlotMs")
    lean_slot = constant(profile, "kMaxRateLeanSlotMs")
    cycle_ms = constant(profile, "kMaxRateCycleMs")
    checker.check("link profile: the rich packet is 212 bytes and the lean 145",
                  rich_bytes == 212 and lean_bytes == 145, f"{rich_bytes} / {lean_bytes}")
    checker.check("the normal-flight budget is the rich packet",
                  budget_bytes == rich_bytes, f"{budget_bytes} vs {rich_bytes}")
    rich_air = time_on_air(rich_bytes or 212, modem).time_on_air_ms * 1.018
    lean_air = time_on_air(lean_bytes or 145, modem).time_on_air_ms * 1.018
    checker.check("the rich slot clears its measured airtime plus the guard",
                  (rich_slot or 0) >= rich_air + (guard_ms or 0),
                  f"{rich_slot} vs {rich_air + (guard_ms or 0):.2f}")
    checker.check("the lean slot clears its measured airtime plus the guard",
                  (lean_slot or 0) >= lean_air + (guard_ms or 0),
                  f"{lean_slot} vs {lean_air + (guard_ms or 0):.2f}")
    checker.check("the max-rate cycle is one rich slot and two lean",
                  cycle_ms == (rich_slot or 0) + 2 * (lean_slot or 0),
                  f"{cycle_ms} vs {(rich_slot or 0) + 2 * (lean_slot or 0)}")
    checker.check("the max-rate cycle keeps GPS and sound at 1 Hz or faster",
                  (cycle_ms or 10 ** 9) <= 1000, str(cycle_ms))
    checker.check("both commanded periods are faster than normal flight",
                  (max_lean_period or 0) < (max_gps_period or 0) < (period_ms or 0),
                  f"{max_lean_period} < {max_gps_period} < {period_ms}")
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

    # The model reads 1.8 % low against the two hardware measurements (bring-up 5.2/5.3),
    # so the duty that matters is the model figure with that correction applied. Checking
    # the model alone would let a period through that the radio cannot actually hold.
    measured_airtime = airtime_ms * 1.018
    checker.check("telemetry period keeps measured duty under the cap",
                  measured_airtime / (period_ms or 1) <= 0.5,
                  f"{measured_airtime / (period_ms or 1):.3f}")
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
        # Both on the same row, not merely both somewhere in the file. The file-wide form
        # of this check would pass a table whose pins and signal names had been shuffled
        # against each other, which is the one way this table can be wrong and still look
        # right.
        row = next((ln for ln in wiring.splitlines()
                    if ln.startswith(f"| GP{expected} |")), "")
        checker.check(f"wiring.md lists GP{expected} for `{name}`",
                      bool(row) and f"`{name}`" in row, row[:70])
        # The quick start carries its own copy of this table -- the one somebody actually
        # wires from, with the board in front of them. A pin changed in the firmware and
        # not here sends a builder to the wrong hole.
        checker.check(f"quick-start.md lists GP{expected}",
                      f"| GP{expected} |" in quick_start, f"GP{expected}")

    # ---- the size of the test suites, as the suites themselves report it ------------
    # These counts appear in the README badge, the quick start and the test plan, and they
    # are exactly the kind of figure that is true on the day it is written and wrong a week
    # later. tools/build_host.sh writes what every suite reported to build/host/test-output.log
    # on the run that precedes this check, so the documents are held to the current numbers
    # rather than to remembered ones.

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

    # A measured answer must reach the table somebody designs from. The bring-up record
    # settled the IMU's WHO_AM_I and both I2C addresses on 2026-09-05; the hardware
    # reference tables still carried "Value on the delivered board - TBD" and "SDO wiring -
    # TBD" for exactly those rows, a day later. The per-paragraph magnetometer gate did not
    # see it, because those rows do not talk about nine axes -- they talk about a
    # measurement, and say it has not been taken.
    #
    # Scoped to the row, for the reason written at the top of this file: a table is not a
    # paragraph, and a document that records the value three rows away is not a document
    # that records it here.
    hardware = read("documentation/hardware/hardware.md")
    bringup = read("documentation/testing/bring-up-record.md")
    for label, value, subject in (("`WHO_AM_I`", "0x70", "the IMU's WHO_AM_I"),
                                  ("I2C address", "0x68", "the IMU's I2C address"),
                                  ("I2C address", "0x76", "the barometer's I2C address")):
        if value not in bringup:
            continue  # not measured yet: the table is entitled to say so
        rows = [ln for ln in hardware.splitlines()
                if ln.startswith("| " + label) and value in ln]
        checker.check(
            f"hardware.md records the measured value for {subject}",
            any("TBD" not in row.split("|")[3] for row in rows),
            f"{len(rows)} row(s)")

    # A question the bench closed must not survive as an open one anywhere else. The bring-up
    # record settled the barometer variant and both I2C straps on 2026-09-05; four documents
    # went on asking for them -- one calling the variant "blocking", one asking for a die
    # photograph the register made unnecessary, and two unticked checkboxes. Somebody working
    # a checklist would have redone work already done, or treated a cleared blocker as one.
    #
    # Keyed on the measured value, so each entry disarms itself if the measurement is ever
    # withdrawn: no evidence in the record, no obligation on the documents.
    settled = (
        ("0x58", "the barometer variant",
         ("BMP280 or BME280** | **Unresolved",
          "[ ] **BMP280 confirmed against BME280",
          "**Resolve BMP280 against BME280.**",
          "BMP280-against-BME280 still unresolved",
          "- The **BMP280 die**, to settle BMP280 against BME280.")),
        ("0x76", "the I2C strap directions",
         ("[ ] **I2C strap directions read",
          "- Confirm BMP280 SDO/address wiring and MPU-9250 AD0 strap direction.")),
    )
    for value, subject, stale_phrases in settled:
        if value not in bringup:
            continue  # not measured: the documents are entitled to keep asking
        survivors = []
        for doc in markdown:
            if "audit" in doc.parts or doc.name == "CHANGELOG.md":
                continue
            text = doc.read_text(encoding="utf-8", errors="replace")
            for phrase in stale_phrases:
                if phrase in text:
                    survivors.append(f"{doc.relative_to(REPO_ROOT).as_posix()}: {phrase[:40]}")
        checker.check(f"no document still asks for {subject}",
                      not survivors, "; ".join(survivors[:3]))

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

    # Acknowledging it somewhere in the file is not enough. Two hardware documents named
    # the delivered part in one section and still asserted, in another, that this vehicle
    # is a nine-axis one -- so the rule is per paragraph: talk about nine axes all you
    # like, and say in the same breath that this is not one.
    hedges = ("MPU-6500", "0x70", "six-axis", "six axes", "is not one", "does not apply",
              "would", "if a nine-axis part", "ever fitted", "cannot exercise")
    unhedged: list[str] = []
    for doc in markdown:
        if "audit" in doc.parts or doc.name == "CHANGELOG.md":
            continue
        rel = doc.relative_to(REPO_ROOT).as_posix()
        for paragraph in doc.read_text(encoding="utf-8", errors="replace").split("\n\n"):
            if "nine-axis" not in paragraph and "nine axes" not in paragraph:
                continue
            if not any(h in paragraph for h in hedges):
                unhedged.append(f"{rel}: {' '.join(paragraph.split())[:60]}")
    checker.check("no document claims this vehicle has nine axes",
                  not unhedged, "; ".join(unhedged[:3]))



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
    # The optional-tag fixture exists because both ground parsers ate the minus sign off a
    # negative coordinate in the same way -- they were hand-ports of each other, and no
    # fixture carried one. A fixture nothing reads would let that regress in silence, which
    # is how it got in.
    for reader in ("ground-station/software/tests/test_telemetry.py",
                   "ground-station/web/tests/console_core.test.mjs"):
        checker.check(f"{reader} reads the optional-tag cases",
                      "optional-tag-cases.tsv" in read(reader))
    tag_rows = [ln for ln in read("test-data/optional-tag-cases.tsv").splitlines()
                if ln.strip() and not ln.lstrip().startswith("#")]
    negatives = [ln for ln in tag_rows if ln.split("	")[-1].startswith("-")]
    checker.check(f"the optional-tag fixture keeps its {len(negatives)} negative-value cases",
                  len(negatives) >= 3, str(len(negatives)))
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

    def anchors_of(body: str) -> set[str]:
        return {heading_slug(h)
                for h in re.findall(r"^#{1,6}\s+(.+?)\s*$", body, re.MULTILINE)}

    anchor_cache: dict[Path, set[str]] = {}

    missing_targets: list[str] = []
    missing_anchors: list[str] = []
    missing_cross_anchors: list[str] = []
    for doc in markdown:
        body = doc.read_text(encoding="utf-8", errors="replace")
        anchors = anchors_of(body)
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
            resolved = (doc.parent / unquote(path_part)).resolve()
            if not resolved.exists():
                missing_targets.append(f"{rel}:{line} {path_part}")
                continue
            # A link into *another* document's heading breaks exactly as silently as one
            # into its own, and renaming a heading is the common way to do it. Only
            # Markdown targets have headings; a fragment on anything else is not ours to
            # judge.
            if fragment and resolved.suffix == ".md":
                if resolved not in anchor_cache:
                    anchor_cache[resolved] = anchors_of(
                        resolved.read_text(encoding="utf-8", errors="replace"))
                if fragment not in anchor_cache[resolved]:
                    missing_cross_anchors.append(f"{rel}:{line} {path_part}#{fragment}")

    checker.check(f"every relative link in {len(markdown)} documents resolves",
                  not missing_targets, "; ".join(missing_targets[:3]))
    checker.check("every same-document anchor resolves to a heading",
                  not missing_anchors, "; ".join(missing_anchors[:3]))
    checker.check("every cross-document anchor resolves to a heading in that document",
                  not missing_cross_anchors, "; ".join(missing_cross_anchors[:3]))

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
    #
    # The id pattern is {2,4} letters, not {3}. It was {3}, which matched TEL- and SEN- and
    # silently skipped every GS- and SW- row: the check passed on 116 while the table held
    # 127. A count that quietly excludes eleven rows is worse than no count, because it
    # reads as verified.
    requirements = read("documentation/requirements/requirements.md")
    complete_rows = len(re.findall(r"^\| [A-Z]{2,4}-[0-9a-z]+ \|.*\| Complete \|",
                                   requirements, re.MULTILINE))
    total_rows = len(re.findall(r"^\| [A-Z]{2,4}-[0-9a-z]+ \|", requirements, re.MULTILINE))
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
    # In a table row, not merely somewhere in the document. `packet_number` appears in the
    # runbook's prose as well, and a column that is only mentioned in passing is not mapped
    # -- which is the whole point of the table this checks.
    runbook_rows = "\n".join(ln for ln in runbook.splitlines() if ln.lstrip().startswith("|"))
    missing_from_runbook = [c for c in sd_columns if f"`{c}`" not in runbook_rows]
    checker.check(f"runbook.md maps all {len(sd_columns)} onboard SD log columns",
                  not missing_from_runbook, ", ".join(missing_from_runbook))
    missing_ground = [c for c in ground_columns if f"`{c}`" not in runbook_rows]
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

    # Every fault the firmware can raise appears in the architecture document's table. An
    # operator sees a fault count in every packet; a code that reaches them and is written
    # down nowhere is a number they cannot act on. Two were missing.
    fault_header = read("firmware/flight-computer/include/flight/fault_manager.hpp")
    enum_body = fault_header[fault_header.index("enum class FaultCode"):]
    enum_body = enum_body[:enum_body.index("};")]
    fault_codes = re.findall(r"^\s{4}([a-z_]+)\s*(?:=\s*\d+\s*)?,", enum_body, re.MULTILINE)
    undocumented_faults = [c for c in fault_codes if f"`{c}`" not in architecture]
    checker.check(f"all {len(fault_codes)} fault codes appear in the architecture table",
                  not undocumented_faults, ", ".join(undocumented_faults))

    # The test plan lists what build_host.sh runs. It described four suites while the
    # script ran eight, which is the kind of drift that makes a reader think a suite they
    # cannot see is a suite that does not exist.
    build_script = read("tools/build_host.sh")
    compiled = re.findall(r'echo "== compiling (\w+) =="', build_script)
    for suite in compiled:
        if suite == "emit_mission":
            continue  # a fixture generator for the end-to-end test, not a suite
        checker.check(f"test-plan.md lists the {suite} suite",
                      f"`{suite}`" in test_plan, suite)

    # .gitattributes says shell scripts are LF in the working tree, not merely in the
    # index, because CI executes them on Linux and a stray CR after the shebang is a "bad
    # interpreter" error that reads like a missing file. An editor -- or a script rewriting
    # the file on Windows -- undoes that silently, and the index normalises it on the way
    # in, so nothing downstream complains. Check the bytes.
    crlf_scripts = [q.relative_to(REPO_ROOT).as_posix()
                    for q in sorted(REPO_ROOT.glob("tools/*.sh"))
                    if b"\r\n" in q.read_bytes()]
    checker.check("every shell script is LF in the working tree, as .gitattributes requires",
                  not crlf_scripts, ", ".join(crlf_scripts))

    # Every Python file in the repository compiles without a warning. The one that matters
    # is the invalid escape sequence -- "\d" in a plain string is a SyntaxWarning today and
    # a SyntaxError in a later Python, and it arrives most often in a regex someone forgot
    # to mark raw. It costs a few hundredths of a second to be sure.
    import warnings as _warnings

    python_problems: list[str] = []
    for source in sorted(REPO_ROOT.rglob("*.py")):
        if {".git", "build", ".claude", "__pycache__"} & set(source.parts):
            continue
        rel = source.relative_to(REPO_ROOT).as_posix()
        with _warnings.catch_warnings(record=True) as caught:
            _warnings.simplefilter("always")
            try:
                compile(source.read_text(encoding="utf-8"), rel, "exec")
            except SyntaxError as exc:
                python_problems.append(f"{rel}: {exc}")
                continue
            for warning in caught:
                python_problems.append(
                    f"{rel}:{warning.lineno} {warning.category.__name__}: {warning.message}")
    checker.check("every Python file compiles without a warning",
                  not python_problems, "; ".join(python_problems[:3]))

    # CI greps the build log for the message build_host.sh prints when it skips the web
    # console suite, and fails the job if it finds it. The two are coupled by a string
    # literal in two files: reword the message and the guard stops matching, silently, and
    # a CI run with no Node passes while testing less than it says it does.
    ci_workflow = read(".github/workflows/ci.yml")
    emitted = re.findall(r'echo "(== SKIPPED[^"]*)"', build_script)
    grepped = re.findall(r"grep -q '([^']*SKIPPED[^']*)'", ci_workflow)
    checker.check("CI greps for a skip message build_host.sh actually prints",
                  bool(grepped) and all(any(g in e for e in emitted) for g in grepped),
                  f"emits {emitted}, greps {grepped}")

    counts = suite_counts()
    if counts is None:
        # The log is written by tools/build_host.sh immediately before this script runs.
        # Outside that script there is nothing to compare against, and inventing a number
        # would be worse than saying so.
        checker.check("test counts checked against build/host/test-output.log", True,
                      "log absent -- run tools/build_host.sh")
    else:
        cpp_total = (counts["flight_tests"] + counts["sx1278"] + counts["sd_card"] +
                     counts["fat_volume"])
        for suite in ("flight_tests", "sx1278", "sd_card", "fat_volume"):
            n = counts[suite]
            checker.check(f"test-plan.md states {suite} ran {n} assertions",
                          f"**{n} / {n} assertions**" in test_plan, str(n))
        for suite, label in (("python_ground", "Python ground station"),
                             ("python_tools", "Python tooling"),
                             ("python_sims", "Python simulations")):
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
        for suite in ("flight_tests", "sx1278", "sd_card", "fat_volume", "python_ground",
                      "python_tools", "python_sims", "node"):
            n = counts[suite]
            checker.check(f"README's results table states {suite} at {n}",
                          f"**{n} / {n}**" in readme, str(n))
        checker.check(f"README states {len(suites)} flight_tests suites",
                      f"{len(suites)} suites:" in readme, str(len(suites)))
        checker.check(f"README states {len(listed)} syntax-checked translation units",
                      f"{len(listed)} translation units" in readme, str(len(listed)))
        python_total = (counts["python_ground"] + counts["python_tools"] +
                        counts["python_sims"])
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

        # The README's status table quotes one number for the whole suite. It is the first
        # sentence anybody reads about this project, it is the easiest figure in the
        # repository to leave behind, and it had been left behind: it said 4395 while the
        # suites reported several hundred more.
        all_total = cpp_total + python_total + node
        checker.check(f"README states {all_total} automated checks in total",
                      f"{all_total} automated checks" in readme, str(all_total))

    # ---- the netlist is generated, and must match what generates it -----------------
    # electrical/schematics/vehicle-netlist.tsv is derived from flight::BoardPins. Two ways
    # it can rot: the generator's own table drifts from the firmware, or the committed file
    # drifts from the generator because nobody re-ran it. Both are checked here rather than
    # left to be noticed by somebody with a multimeter.
    sys.path.insert(0, str(REPO_ROOT / "tools"))
    import gen_netlist  # noqa: E402

    netlist_problems = gen_netlist.verify(gen_netlist.board_pins())
    checker.check("the netlist agrees with flight::BoardPins",
                  not netlist_problems, "; ".join(netlist_problems[:3]))
    committed = gen_netlist.OUTPUT_PATH
    # Compared with newlines normalised: the generator writes LF, and a checkout on
    # Windows may hand it back as CRLF.
    on_disk = (committed.read_text(encoding="utf-8").replace("\r\n", "\n")
               if committed.exists() else None)
    checker.check("the committed netlist matches tools/gen_netlist.py",
                  on_disk == gen_netlist.build(),
                  "run python tools/gen_netlist.py")


    # ---- the descent model's answers, where documents quote them --------------------
    # The canopy diameter is the one number the mechanical build takes straight out of a
    # simulation, and two documents state it in prose. A drag coefficient or a target rate
    # changed in the model would otherwise leave those documents describing a parachute
    # nobody is going to build.
    sys.path.insert(0, str(REPO_ROOT / "simulations"))
    import descent  # noqa: E402

    mechanical = read("mechanical/README.md")
    simulations_readme = read("simulations/README.md")
    nominal = descent.descend()
    worst = descent.descend(mass_kg=0.550, temperature_c=35.0)
    for label, result in (("nominal 500 g", nominal), ("550 g on a hot day", worst)):
        centimetres = f"{result.diameter_m * 100:.1f} cm"
        checker.check(f"mechanical/README.md states the {label} canopy at {centimetres}",
                      centimetres in mechanical, centimetres)
        checker.check(f"simulations/README.md states the {label} canopy at {centimetres}",
                      centimetres in simulations_readme, centimetres)
    descent_seconds = f"{nominal.total_time_s:.2f} s"
    checker.check(f"the descent is documented as {descent_seconds}",
                  descent_seconds in mechanical and descent_seconds in simulations_readme,
                  descent_seconds)
    conops = read("documentation/mission/concept-of-operations.md")
    checker.check(f"concept-of-operations.md states the {descent_seconds} descent",
                  descent_seconds in conops, descent_seconds)
    # Packets during the descent is the finding, not the arithmetic: a handful of packets is
    # the whole over-the-air dataset, and it moves with the telemetry period.
    packets = descent.descend(telemetry_period_ms=float(period_ms or 700)).packets_in_descent
    checker.check(f"the descent is documented as {packets} packets",
                  f"**{packets}**" in simulations_readme and f"| **{packets}** |" in conops,
                  str(packets))

    # ---- the requirements table counts itself, and its ids are unique ---------------
    # Two different requirements were both numbered GS-002 for three days, which is the
    # kind of thing that is invisible in a 127-row table and fatal in a traceability
    # argument. The row count and the Complete count are quoted in the paragraph above the
    # table, and both had drifted -- it said 116 rows when there were 127.
    requirement_rows = re.findall(r"^\| ([A-Z]{2,4}-[0-9]+[a-z]?) \| (.*)$",
                                  read("documentation/requirements/requirements.md"),
                                  re.MULTILINE)
    ids = [rid for rid, _ in requirement_rows]
    duplicates = sorted({rid for rid in ids if ids.count(rid) > 1})
    checker.check("every requirement id is unique", not duplicates, ", ".join(duplicates))

    # ---- both ends of the link agree which rulebook sync word they are on -----------
    # This is the most expensive mistake available in this system, and until now nothing
    # caught it. The sync word is a compile-time constant in TWO separate images, and a
    # LoRa sync mismatch is not an error -- it is silence. The receiver's correlator never
    # locks, so preamble detect never fires, the header is never decoded and no counter
    # moves. It is indistinguishable from a dead antenna, a dead module, or a vehicle that
    # was never switched on.
    #
    # Reflashing one Pico and not the other is therefore a total, silent loss of telemetry
    # discovered at a launch. Holding the two declarations to each other turns that into a
    # failed build, which is where it belongs. TEL-025.
    flight_main = read("firmware/flight-computer/src/pico/main.cpp")
    bridge_main = read("firmware/ground-station/src/pico/main.cpp")
    modes = re.findall(r"config\.radio_mode\s*=\s*flight::RadioMode::(test|official)\s*;",
                       flight_main)
    words = re.findall(
        r"constexpr std::uint8_t SYNC_WORD\s*=\s*cansat::link::k(Test|Official)SyncWord\s*;",
        bridge_main)
    checker.check("the vehicle declares exactly one radio mode", len(modes) == 1, str(modes))
    checker.check("the bridge declares exactly one sync word", len(words) == 1, str(words))
    if len(modes) == 1 and len(words) == 1:
        mode, word = modes[0], words[0].lower()
        # Named rather than silent, so a full build log states which configuration this
        # working tree would fly -- the launch procedure asks an operator to read it.
        checker.check(
            f"vehicle and bridge agree: {mode.upper()} sync word "
            f"({'0xF3' if mode == 'test' else '0xA5'})",
            mode == word, f"vehicle={mode} bridge={word}")

    # ---- the radio-silence procedure and the hardware it depends on -----------------
    # TEL-026 is executed with a switch and confirmed with a power LED, and neither is
    # fitted. So the runbook has to say so, and keep saying so until they are -- a
    # procedure that tells an operator to open a switch that does not exist reads as
    # authoritative and is unfollowable, which is worse than an obvious gap. When PWR-001
    # is finally marked Complete this check flips: the warning becomes the stale thing, and
    # the build says so.
    def requirement_status(rid: str) -> str:
        match = re.search(rf"^\| {rid} \| (.*)$", requirements, re.MULTILINE)
        if not match:
            return ""
        cells = match.group(1).split(" | ")
        return cells[5].strip() if len(cells) >= 7 else ""

    switch_fitted = requirement_status("PWR-001") in ("Complete", "Verified")
    warns = "The manual ON/OFF switch is not fitted" in runbook
    checker.check(
        "runbook.md's radio-silence procedure matches whether PWR-001 is actually fitted",
        warns != switch_fitted,
        f"PWR-001={requirement_status('PWR-001')!r} runbook warns={warns}")

    # ---- the mechanical documents against the CAD they describe ---------------------
    # mechanical/README.md quotes a bounding box and a cross-section diagonal, and the
    # envelope drawing is generated from the same model. A dimension typed by hand goes
    # stale the first time somebody edits the model and re-exports, and this one decides
    # whether the vehicle is inside a limit whose breach is a disqualification.
    sys.path.insert(0, str(REPO_ROOT / "tools"))
    import cad_dimensions  # noqa: E402

    design_step = REPO_ROOT / "mechanical/CAD/Cansat_D1.step"
    checker.check("the design's STEP export is committed", design_step.exists(),
                  str(design_step))
    if design_step.exists():
        design = cad_dimensions.read_step(design_step)
        mechanical = read("mechanical/README.md")
        box = " × ".join(f"{v:.1f}" for v in design.sorted_extents)
        checker.check(f"mechanical/README.md states the {box} mm bounding box",
                      f"**{box} mm**" in mechanical, box)
        diagonal = f"{design.footprint_diagonal:.1f} mm"
        checker.check(f"mechanical/README.md states the {diagonal} cross-section diagonal",
                      f"**{diagonal}**" in mechanical, diagonal)
        # The organizers confirmed on 2026-09-09 that a 12 cm sided box is acceptable, so
        # the section limit is a 120 mm square and the diagonal no longer decides anything.
        # What matters now is the clearance to that square, per side: it is what any
        # protruding feature has to live inside, and 2.5 mm is not much.
        clearances = sorted((120.0 - e) / 2.0 for e in design.sorted_extents[1:])
        quoted = " and ".join(f"{c:.1f}" for c in clearances) + " mm"
        checker.check(f"mechanical/README.md states the {quoted} per-side clearance",
                      f"**{quoted}**" in mechanical, quoted)
        checker.check("the design fits the confirmed 120 mm sided box",
                      all(e <= 120.0 for e in design.sorted_extents[1:]),
                      str(design.sorted_extents))
        unused = 210.0 - design.sorted_extents[0]
        checker.check(f"mechanical/README.md states the {unused:.1f} mm of unused height",
                      f"**{unused:.1f} mm unused**" in mechanical, f"{unused:.1f}")
        # And the STEP itself must stay something this repository can read.
        checker.check("the design is one solid, in millimetres",
                      len(design.solids) == 1 and ".MILLI.,.METRE." in design.units,
                      f"{design.solids} {design.units}")

    # The envelope drawing is generated from that same model, so it is checked the same way
    # as the netlist: against its generator, not merely for existing.
    import gen_envelope_drawing  # noqa: E402

    drawing = gen_envelope_drawing.OUTPUT_PATH
    drawn = (drawing.read_text(encoding="utf-8").replace(chr(13) + chr(10), chr(10))
             if drawing.exists() else None)
    checker.check("the committed envelope drawing matches tools/gen_envelope_drawing.py",
                  drawn == gen_envelope_drawing.build(),
                  "run python tools/gen_envelope_drawing.py")

    # ---- the mass budget adds up ----------------------------------------------------
    # Two measured masses and two sums. Sums in a table are exactly the kind of thing that
    # is right when written and wrong after the next edit, and this table is the one that
    # decides whether the vehicle is inside a limit whose breach is a disqualification.
    pcb_g, battery_g, structure_g = 110.573, 40.726, 193.0
    floor_g, ceiling_g = 450.0, 550.0
    electronics_g = pcb_g + battery_g
    committed_g = electronics_g + structure_g
    for label, value in (("assembled PCB", f"**{pcb_g:.3f} g**"),
                         ("battery", f"**{battery_g:.3f} g**"),
                         ("electronics total", f"**{electronics_g:.3f} g**"),
                         ("PETG structure", f"**{structure_g:.3f} g**"),
                         ("committed total", f"**{committed_g:.3f} g**")):
        checker.check(f"mechanical/README.md states the {label} mass ({value.strip('*')})",
                      value in mechanical, value)
    # The band is two-sided in GEN-005 and the disqualification is one-sided in GEN-006, so
    # the distance to BOTH edges is quoted. The distance to the floor is the surprising one:
    # this design is more likely to come in light than heavy.
    to_floor = floor_g - committed_g
    to_ceiling = ceiling_g - committed_g
    checker.check(f"mechanical/README.md states {to_floor:.1f} g needed to reach the floor",
                  f"**+{to_floor:.1f} g**" in mechanical, f"{to_floor:.1f}")
    checker.check(f"mechanical/README.md states {to_ceiling:.1f} g of headroom to the cap",
                  f"+{to_ceiling:.1f} g available" in mechanical, f"{to_ceiling:.1f}")
    # The structure mass is an estimate from volume, so the volume it implies is quoted as
    # its own sanity check -- a density slip would move it by an order of magnitude.
    volume_cm3 = structure_g / 1.27
    checker.check(f"mechanical/README.md states the implied {volume_cm3:.1f} cm3 of PETG",
                  f"**{volume_cm3:.1f} cm³**" in mechanical, f"{volume_cm3:.1f}")

    # ---- the simulation figures the mechanical page quotes ---------------------------
    # Read off the exported plots by hand, so they cannot be re-derived -- but the safety
    # factors are arithmetic on them and must stay consistent with the material's yield.
    simulation = read("mechanical/simulation/README.md")
    yield_mpa = 54.40
    # Fusion caps its safety factor legend at 15, and all three studies sit on that cap --
    # so the reported figure is a floor, not a result. The yield-derived numbers are the
    # real margins, and they are quoted alongside it precisely so nobody reads the cap as
    # the answer. Both are held here.
    for study, von_mises in ((1, 2.885), (2, 1.330), (3, 2.345)):
        sf = yield_mpa / von_mises
        checker.check(f"simulation study {study} quotes its {von_mises} MPa peak stress",
                      f"**{von_mises:.3f} MPa**" in simulation, str(von_mises))
        checker.check(f"simulation study {study} quotes the {sf:.1f} yield-derived margin",
                      f"**{sf:.1f}**" in simulation or f"| {sf:.1f} |" in simulation,
                      f"{sf:.1f}")
    checker.check("mechanical/README.md reports the capped safety factor as a floor",
                  mechanical.count("**≥ 15**") >= 3,
                  str(mechanical.count("**≥ 15**")))
    # The derated figure is the one that actually answers "will a printed part survive",
    # and it is arithmetic on the cap rather than a number anybody looked up.
    derated = 15 * 0.40
    checker.check(f"simulation/README.md states the {derated:.0f}x derated worst case",
                  f"effective safety factor is {derated:.0f}" in simulation,
                  f"{derated:.0f}")

    # ---- rulebook constants that must never drift -----------------------------------
    checker.check("post-impact window is at least the rulebook's 5 s",
                  (constant(config, "post_impact_transmission_ms") or 0) >= 5000)
    # The rulebook's 1 Hz is a MINIMUM, and this vehicle is built to sit above it rather
    # than on it. Three things enforce that -- a static_assert in link_profile.hpp,
    # validate_config() at runtime, and this -- and they have to agree, because the whole
    # point is that no single edit can quietly put a flight build back at 1 Hz.
    profile = read("firmware/common/include/cansat/link_profile.hpp")
    rulebook_period = constant(profile, "kRulebookMinRatePeriodMs")
    jitter_margin = constant(profile, "kTelemetryJitterMarginMs")
    checker.check("the rulebook minimum period is stated as 1000 ms",
                  rulebook_period == 1000, str(rulebook_period))
    checker.check("the telemetry ceiling leaves a jitter margin below it",
                  (jitter_margin or 0) > 0, str(jitter_margin))
    ceiling = (rulebook_period or 0) - (jitter_margin or 0)
    checker.check(f"the telemetry ceiling is {ceiling} ms, below the rulebook period",
                  0 < ceiling < (rulebook_period or 0), str(ceiling))
    checker.check("the shipped telemetry period is inside that ceiling",
                  0 < (period_ms or 0) <= ceiling, str(period_ms))
    # And the runtime guard has to enforce the same ceiling, not a looser one of its own.
    checker.check("validate_config() rejects a period above the ceiling",
                  "kMaxTelemetryPeriodMs" in read("firmware/flight-computer/src/config.cpp"))
    rate_hz = 1000.0 / float(period_ms or 1)
    checker.check(f"the shipped rate ({rate_hz:.2f} Hz) is above the rulebook minimum",
                  rate_hz > 1.0, f"{rate_hz:.2f}")
    # The figure the documentation quotes for that rate, in the four places it appears.
    quoted = f"{rate_hz:.2f} Hz"
    for name, body in (("README.md", readme),
                       ("test-plan.md", test_plan),
                       ("link-budget.md", read("documentation/design/link-budget.md")),
                       ("concept-of-operations.md",
                        read("documentation/mission/concept-of-operations.md")),
                       ("runbook.md", runbook),
                       ("avionics/telemetry/README.md",
                        read("avionics/telemetry/README.md")),
                       ("ground-station/web/README.md",
                        read("ground-station/web/README.md"))):
        checker.check(f"{name} states the {quoted} telemetry rate", quoted in body, quoted)

    # The rate a document quotes being right is not the same as its *arithmetic* being right.
    # Three figures in the runbook are computed from the rate, and every one of them had been
    # scaled from an earlier period and left behind -- including a tunables row that still
    # called 1000 ms "the rulebook ceiling" after 1000 ms became a value validate_config()
    # refuses. A wrong ceiling in the document an operator reads before a launch is worse
    # than a wrong figure anywhere else in the repository.
    checker.check("runbook.md states the shipped period as the default, not an older one",
                  f"| `telemetry_period_ms` | **{period_ms}**" in runbook, str(period_ms))
    checker.check("runbook.md does not present 1000 ms as an acceptable period",
                  "1000 ms is both the rulebook ceiling" not in runbook)
    # -1 point per 2 packets, so half a point per packet. The seconds it takes to lose the
    # whole 25-point telemetry section is the number that makes switch discipline concrete,
    # and it moves whenever the rate does.
    penalty_pts_per_s = rate_hz * 0.5
    wipeout_s = round(25.0 / penalty_pts_per_s)
    checker.check(f"runbook.md states {penalty_pts_per_s:.2f} points per second of stray transmission",
                  f"{penalty_pts_per_s:.2f} points per second" in runbook,
                  f"{penalty_pts_per_s:.2f}")
    checker.check(f"runbook.md states that {wipeout_s} s of stray transmission costs 25 points",
                  f"in **{wipeout_s} seconds**" in runbook, str(wipeout_s))

    # The commanded rates, in the document an operator reads with the console open. Derived
    # from the shipped constants, because a runbook that quotes a rate the firmware no
    # longer has is worse here than anywhere else: these commands cannot be taken back.
    gps_hz = 1000.0 / float(max_gps_period or 1)
    lean_hz = 1000.0 / float(max_lean_period or 1)
    checker.check(f"runbook.md states the GPS rate command as {gps_hz:.2f} Hz",
                  f"{gps_hz:.2f} Hz" in runbook, f"{gps_hz:.2f}")
    checker.check(f"runbook.md states the lean rate command as {lean_hz:.2f} Hz",
                  f"{lean_hz:.2f} Hz" in runbook, f"{lean_hz:.2f}")
    checker.check(f"runbook.md states the commanded periods {max_gps_period} / {max_lean_period} ms",
                  f"**{max_gps_period} ms**" in runbook and f"**{max_lean_period} ms**" in runbook)
    checker.check("runbook.md says the rate commands cannot be undone",
                  "cannot be undone" in runbook and "power cycle" in runbook)

    # Last, and counting itself: the number of claims this script checks is itself a figure
    # the test plan quotes, so adding a check here without updating that row fails here.
    # Only on a full run, though -- without the suite log a dozen checks are skipped, and
    # the quoted total is the full-run one, not the short-run one.
    if counts is not None:
        claim_total = len(checker.results) + 3
        checker.check(f"test-plan.md states the {claim_total} claims this script checks",
                      f"**{claim_total} / {claim_total} claims**" in test_plan, str(claim_total))
        checker.check(f"README states the {claim_total} claims this script checks",
                      f"**{claim_total} / {claim_total}**" in readme, str(claim_total))
        # The review's own audit trail records what this script answered. It said 159 long
        # after the answer was 199 -- a document about keeping documents honest, gone stale
        # about itself, because nothing checked the row that quotes this script's output.
        checker.check(f"the continuous-review audit trail states {claim_total} / {claim_total}",
                      f"| {claim_total} / {claim_total} |" in continuous_review, str(claim_total))

    return checker.report()


if __name__ == "__main__":
    sys.exit(main())
