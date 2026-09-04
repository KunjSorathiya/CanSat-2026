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
    for measured in ("118", "167", "206", "247"):
        checker.check(f"link-budget.md quotes the measured {measured}-byte packet size",
                      f"**{measured}**" in link_budget)

    # ---- acquisition and sensor configuration ---------------------------------------
    config = read("firmware/flight-computer/include/flight/config.hpp")
    sensor_period = constant(config, "sensor_period_ms")
    dlpf = constant(config, "imu_dlpf_cfg")
    sample_div = constant(config, "imu_sample_rate_div")
    gyro_bias_bound = decimal(config, "calib_max_gyro_bias_dps")

    checker.check("config: 33 ms sensor period", sensor_period == 33, str(sensor_period))
    checker.check("config: IMU DLPF_CFG 4", dlpf == 4, str(dlpf))
    checker.check("config: SMPLRT_DIV 4", sample_div == 4, str(sample_div))
    checker.check("config: gyro bias bounded at 25 dps", gyro_bias_bound == 25.0,
                  str(gyro_bias_bound))

    sensor_rates = read("documentation/design/sensor-rates.md")
    achieved_hz = round(1000.0 / (sensor_period or 33))
    checker.check(f"sensor-rates.md states the {achieved_hz} Hz acquisition rate",
                  f"**{achieved_hz} Hz**" in sensor_rates, str(achieved_hz))
    checker.check("sensor-rates.md states the 33 ms period",
                  f"{sensor_period} ms" in sensor_rates)
    checker.check("sensor-rates.md states the 21 Hz IMU bandwidth for DLPF 4",
                  "21 Hz" in sensor_rates)

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
    checker.check("flight loop ticks every 2 ms", "tick_delay_ms(2)" in flight_main)
    checker.check("architecture states the 2 ms tick", "2 ms tick" in architecture)

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

    # ---- rulebook constants that must never drift -----------------------------------
    checker.check("post-impact window is at least the rulebook's 5 s",
                  (constant(config, "post_impact_transmission_ms") or 0) >= 5000)
    checker.check("telemetry period never exceeds the 1 Hz rulebook minimum",
                  (period_ms or 0) <= 1000)

    return checker.report()


if __name__ == "__main__":
    sys.exit(main())
