#!/usr/bin/env python3
"""Generate a SYNTHETIC flight, in the exact formats a real one leaves behind.

    python analysis/synthetic_flight.py            # writes test-data/synthetic-flight/

**Nothing this produces is flight data.** It exists because the post-flight analysis has to
be written, tested and rehearsed before there is a flight, and the four-hour window after
the launch is the wrong time to discover that a column is named differently than assumed.

What it writes, and why each file is shaped the way it is:

``sd-flight.csv``
    The onboard log as ``tools/read_flight_log.py`` extracts it: the firmware's own column
    header, then one row per transmitted packet -- the row is appended in
    ``Controller::emit_telemetry()``, so the log runs at the radio's cadence, not at the 30 Hz
    sensor rate.

``ground-packets.txt``
    What the ground station heard: the same packets, minus a few lost to the link, including
    one lost mid-descent on purpose.

``ground-telemetry.csv``
    ``ground-packets.txt`` pushed through the **real** ground-station replay and export, so
    the CSV the analysis reads is the CSV the station writes.

``truth.json``
    The parameters the flight was generated from. The analysis tests recover them from the
    files above and fail if they cannot.

The profile follows the sealed flight image: telemetry at 1.43 Hz through the command
window, ``MAX_RATE`` pressed early, recalibration and arming, the rich/lean/lean max-rate
pattern, a drone lift and hover, release, a short free fall, a canopy descent integrated
from the same drag equation ``simulations/descent.py`` uses, landing, tipping over, and the
post-impact window.
"""

from __future__ import annotations

import argparse
import json
import math
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "simulations"))
import descent  # noqa: E402

OUT_DIR = ROOT / "test-data" / "synthetic-flight"

# Must match TelemetryBuilder::sd_header() in firmware/flight-computer/src/telemetry_builder.cpp.
SD_HEADER = ("mission_ms,packet_number,state,fault_total,altitude_m,pressure_pa,temperature_c,"
             "roll_deg,pitch_deg,yaw_deg,ax_mps2,ay_mps2,az_mps2,gps_valid,gps_lat,gps_lon,"
             "gps_alt,gps_satellites,gps_hdop,sound_mv_pp,sound_clipped,sound_gate_pct,packet")

# Must match firmware/common/include/cansat/link_profile.hpp.
NORMAL_PERIOD_MS = 700
RICH_SLOT_MS = 374
LEAN_SLOT_MS = 296

STATE_LETTER = {"INIT": "I", "SELF_TEST": "T", "READY": "R", "FLIGHT": "F",
                "LANDED": "L", "RECOVERY": "V", "FAULT": "X"}


def default_truth() -> dict:
    """The flight to generate. Every number the tests check comes from here."""
    return {
        "team_id": "CAN-Team-25",
        "seed": 20260914,
        "mass_kg": 0.500,
        "canopy_diameter_m": 0.80,
        "drag_coefficient": 0.75,
        "ground_pressure_pa": 100950.0,
        "ground_temperature_c": 31.0,
        "release_altitude_m": 30.48,
        "max_rate_pressed_s": 95.0,
        "lift_start_s": 130.0,
        "climb_rate_mps": 3.048,
        "hover_s": 20.0,
        "deployment_delay_s": 0.40,
        "pendulum_hz": 0.60,
        "pendulum_amplitude_deg": 9.0,
        "spin_dps": 25.0,
        "wind_east_mps": 1.6,
        "wind_north_mps": 0.7,
        "site_lat": 21.16450,
        "site_lon": 72.78480,
        "site_msl_m": 12.0,
        "end_s": 205.0,
        "ground_lost_packets_fraction": 0.015,
    }


# ---- the physics ------------------------------------------------------------------------

def _true_trajectory(truth: dict):
    """Height above the pad and descent rate on a 5 ms grid, plus the event times."""
    dt = 0.005
    t_end = truth["end_s"]
    t = np.arange(0.0, t_end + dt, dt)
    h = np.zeros_like(t)
    v = np.zeros_like(t)          # positive up

    lift0 = truth["lift_start_s"]
    climb = truth["climb_rate_mps"]
    top = truth["release_altitude_m"]
    climb_end = lift0 + top / climb
    release = climb_end + truth["hover_s"]
    opening = release + truth["deployment_delay_s"]

    density = descent.air_density(truth["ground_pressure_pa"], truth["ground_temperature_c"])
    area = descent.circular_area(truth["canopy_diameter_m"])
    k = 0.5 * density * truth["drag_coefficient"] * area / truth["mass_kg"]
    g = descent.STANDARD_GRAVITY

    touchdown = None
    for i in range(1, len(t)):
        ti = t[i]
        if ti < lift0:
            h[i], v[i] = 0.0, 0.0
        elif ti < climb_end:
            h[i], v[i] = climb * (ti - lift0), climb
        elif ti < release:
            h[i], v[i] = top, 0.0
        elif touchdown is None:
            speed = -v[i - 1]                      # downward positive
            drag = k * speed * speed if ti >= opening else 0.0
            speed = speed + (g - drag) * dt
            h[i] = h[i - 1] - speed * dt
            v[i] = -speed
            if h[i] <= 0.0:
                h[i], v[i] = 0.0, 0.0
                touchdown = ti
        else:
            h[i], v[i] = 0.0, 0.0

    terminal = math.sqrt(g / k)
    return t, h, v, {"climb_end_s": climb_end, "release_s": release, "opening_s": opening,
                     "touchdown_s": touchdown, "terminal_mps": terminal, "density": density}


def _transmit_times(truth: dict) -> list[tuple[float, str]]:
    """(mission time, 'rich'|'lean') for every packet, following the flight image's schedule."""
    times = []
    t = 0.70
    pressed = truth["max_rate_pressed_s"]
    while t < pressed:
        times.append((t, "rich"))
        t += NORMAL_PERIOD_MS / 1000.0
    slot = 0
    while t < truth["end_s"]:
        shape = "rich" if slot == 0 else "lean"
        times.append((t, shape))
        t += (RICH_SLOT_MS if shape == "rich" else LEAN_SLOT_MS) / 1000.0
        slot = (slot + 1) % 3
    return times


def _clock(ms: int) -> str:
    hh, rem = divmod(ms, 3_600_000)
    mm, rem = divmod(rem, 60_000)
    ss, mss = divmod(rem, 1000)
    return f"{hh:02d}:{mm:02d}:{ss:02d}:{mss:03d}"


def generate(truth: dict | None = None) -> tuple[list[str], list[str], dict]:
    """Return (sd_rows, packets, truth-with-derived-events)."""
    truth = dict(default_truth() if truth is None else truth)
    rng = np.random.default_rng(truth["seed"])
    grid_t, grid_h, grid_v, events = _true_trajectory(truth)

    pressed = truth["max_rate_pressed_s"]
    recalibrated = pressed + 2.7
    armed_at = pressed + 3.0
    lift0 = truth["lift_start_s"]
    release = events["release_s"]
    opening = events["opening_s"]
    touchdown = events["touchdown_s"]
    flight_declared = None
    landed_at = touchdown + 3.0
    recovery_at = landed_at + 5.0

    p0 = truth["ground_pressure_pa"]
    lat0, lon0 = truth["site_lat"], truth["site_lon"]
    m_per_deg_lat = 111_320.0
    m_per_deg_lon = 111_320.0 * math.cos(math.radians(lat0))

    sd_rows, packets = [], []
    gps_hold = None
    gps_hold_t = -10.0
    yaw = 0.0
    last_t = 0.0
    number = 0

    for t, shape in _transmit_times(truth):
        i = min(int(round(t / 0.005)), len(grid_t) - 1)
        h_true, v_true = float(grid_h[i]), float(grid_v[i])
        number += 1
        ms = int(round(t * 1000))

        # ---- state ----
        if flight_declared is None and t > armed_at and h_true > 15.0:
            flight_declared = t + 0.3
        if t < 1.5:
            state = "READY"
        elif flight_declared is not None and t >= recovery_at:
            state = "RECOVERY"
        elif flight_declared is not None and t >= landed_at:
            state = "LANDED"
        elif flight_declared is not None and t >= flight_declared:
            state = "FLIGHT"
        else:
            state = "READY"
        calibrated = (3.0 <= t < pressed) or t >= recalibrated
        armed = t >= armed_at

        # ---- barometer ----
        # Pressure is physics: the hypsometric equation at the real air temperature. Altitude
        # is the firmware: sensors::pressure_altitude_m(), the ISA formula, which assumes a
        # standard-atmosphere temperature. On a 31 C day the two differ by ~5 %, exactly as
        # they will on the launch day, so the analysis is tested against the real bias.
        air_k = truth["ground_temperature_c"] + 273.15
        h_meas = h_true + rng.normal(0.0, 0.15)
        pressure = p0 * math.exp(-descent.STANDARD_GRAVITY * h_meas / (287.05 * air_k))
        firmware_altitude = 44330.0 * (1.0 - (pressure / p0) ** (1.0 / 5.255))
        altitude = firmware_altitude if t >= 3.0 else 23.0 + rng.normal(0.0, 0.15)
        temperature = (truth["ground_temperature_c"] + 0.3 * t / truth["end_s"]
                       - 0.0065 * h_true + rng.normal(0.0, 0.05))

        # ---- attitude and acceleration, body frame ----
        in_descent = opening <= t < touchdown
        after_landing = touchdown is not None and t >= touchdown
        if in_descent:
            phase = 2.0 * math.pi * truth["pendulum_hz"] * (t - opening)
            decay = math.exp(-(t - opening) / 12.0)
            roll = truth["pendulum_amplitude_deg"] * decay * math.sin(phase) + rng.normal(0, 0.6)
            pitch = 0.6 * truth["pendulum_amplitude_deg"] * decay * math.cos(phase) + rng.normal(0, 0.6)
            yaw += truth["spin_dps"] * (t - last_t)
            swing = math.radians(roll)
            ax = 9.80665 * math.sin(swing) * 0.15 + rng.normal(0, 0.35)
            ay = 9.80665 * math.sin(math.radians(pitch)) * 0.15 + rng.normal(0, 0.35)
            az = 9.80665 + rng.normal(0, 0.45)
        elif release <= t < opening:
            roll, pitch = rng.normal(0, 2.0), rng.normal(0, 2.0)
            ax, ay, az = rng.normal(0, 0.4), rng.normal(0, 0.4), 0.6 + rng.normal(0, 0.3)
        elif after_landing:
            tip = min(1.0, (t - touchdown) / 0.8)
            roll, pitch = 72.0 * tip + rng.normal(0, 0.3), -4.0 + rng.normal(0, 0.3)
            az = 9.80665 * math.cos(math.radians(72.0 * tip)) + rng.normal(0, 0.05)
            ax = 9.80665 * math.sin(math.radians(72.0 * tip)) + rng.normal(0, 0.05)
            ay = rng.normal(0, 0.05)
        elif lift0 <= t < release:
            roll = 3.0 * math.sin(2 * math.pi * 0.4 * t) + rng.normal(0, 0.5)
            pitch = 2.0 * math.cos(2 * math.pi * 0.4 * t) + rng.normal(0, 0.5)
            ax, ay = rng.normal(0, 0.6), rng.normal(0, 0.6)
            az = 9.80665 + (0.8 if t < lift0 + 1.0 else 0.0) + rng.normal(0, 0.6)
        else:
            roll, pitch = 1.5 + rng.normal(0, 0.3), -0.8 + rng.normal(0, 0.3)
            ax, ay, az = 0.12 + rng.normal(0, 0.05), -0.2 + rng.normal(0, 0.05), 9.79 + rng.normal(0, 0.05)
        if t < 3.0:
            yaw = 0.0
        elif not in_descent:
            yaw += 0.002 * (t - last_t) + rng.normal(0, 0.05)
        last_t = t
        yaw_wrapped = ((yaw + 180.0) % 360.0) - 180.0

        # ---- GPS: 1 Hz fixes, held between updates, from 40 s ----
        east = north = 0.0
        if t >= release:
            fall_t = min(t, touchdown) - release
            east, north = truth["wind_east_mps"] * fall_t, truth["wind_north_mps"] * fall_t
        if t >= 40.0 and t - gps_hold_t >= 1.0:
            gps_hold_t = math.floor(t)
            gps_hold = (lat0 + (north + rng.normal(0, 1.2)) / m_per_deg_lat,
                        lon0 + (east + rng.normal(0, 1.2)) / m_per_deg_lon,
                        truth["site_msl_m"] + h_true + rng.normal(0, 2.5),
                        int(rng.integers(8, 11)), round(float(rng.uniform(0.9, 1.4)), 1))

        # ---- microphone: pad, drone, flow noise, the two transients ----
        if touchdown is not None and touchdown <= t < touchdown + 0.35:
            sound, clipped = 3300.0, 1
        elif opening <= t < opening + 0.35:
            sound, clipped = 1450.0 + rng.normal(0, 60), 0
        elif lift0 - 1.0 <= t < release:
            sound, clipped = 420.0 + rng.normal(0, 55), 0
        elif release <= t < touchdown:
            speed = -v_true
            sound, clipped = 30.0 + 18.0 * speed * speed + rng.normal(0, 25), 0
        else:
            sound, clipped = 35.0 + abs(rng.normal(0, 6)), 0
        sound = max(0.0, min(3300.0, sound))
        gate = min(100.0, max(0.0, sound / 33.0 + rng.normal(0, 2)))

        # ---- the packet ----
        fields = [truth["team_id"], f"P-{number:03d}", f"Ti-{_clock(ms)}",
                  f"A-{altitude:.1f}", f"Pr-{pressure:.2f}", f"T-{temperature:.1f}",
                  f"Ro-{roll:.1f}", f"Pi-{pitch:.1f}", f"Ya-{yaw_wrapped:.1f}",
                  f"AX-{ax:.2f}", f"AY-{ay:.2f}", f"AZ-{az:.2f}"]
        if shape == "rich":
            if gps_hold is not None:
                fields += [f"GP-Lat-{gps_hold[0]:.5f}", f"GP-Lon-{gps_hold[1]:.5f}",
                           f"GP-Alt-{gps_hold[2]:.0f}"]
            fields.append(f"SN-{sound:.1f}")
            fields.append(f"ST-{STATE_LETTER[state]}{int(armed)}{int(calibrated)}0")
        packet = "; ".join(fields) + ";"
        packets.append(packet)

        gps_cols = (["1", f"{gps_hold[0]:.6f}", f"{gps_hold[1]:.6f}", f"{gps_hold[2]:.1f}",
                     str(gps_hold[3]), f"{gps_hold[4]:.1f}"] if gps_hold is not None
                    else ["0", "", "", "", "", ""])
        sd_rows.append(",".join([str(ms), str(number), state, "0",
                                 f"{altitude:.1f}", f"{pressure:.2f}", f"{temperature:.1f}",
                                 f"{roll:.1f}", f"{pitch:.1f}", f"{yaw_wrapped:.1f}",
                                 f"{ax:.2f}", f"{ay:.2f}", f"{az:.2f}"] + gps_cols +
                                [f"{sound:.1f}", str(clipped), f"{gate:.1f}", packet]))

    fall = touchdown - release
    truth.update({
        "generated": "SYNTHETIC - not flight data",
        "climb_end_s": round(events["climb_end_s"], 3),
        "release_s": round(release, 3),
        "canopy_opening_s": round(opening, 3),
        "touchdown_s": round(touchdown, 3),
        "descent_time_s": round(fall, 3),
        "terminal_rate_mps": round(events["terminal_mps"], 4),
        "air_density_kgm3": round(events["density"], 5),
        "drift_m": round(math.hypot(truth["wind_east_mps"] * fall, truth["wind_north_mps"] * fall), 2),
        "flight_declared_s": round(flight_declared, 3),
        "landed_declared_s": round(landed_at, 3),
        "armed_s": armed_at,
        "packets_transmitted": number,
    })
    return sd_rows, packets, truth


def write(out_dir: Path = OUT_DIR, truth: dict | None = None) -> dict:
    sd_rows, packets, truth = generate(truth)
    out_dir.mkdir(parents=True, exist_ok=True)

    (out_dir / "sd-flight.csv").write_text(SD_HEADER + "\n" + "\n".join(sd_rows) + "\n",
                                            encoding="utf-8")

    # Ground copy: a small fraction lost at random, and one lost mid-descent on purpose, so
    # the analysis has a real radio gap to find in the part of the flight that matters.
    rng = np.random.default_rng(truth["seed"] + 1)
    keep = rng.random(len(packets)) >= truth["ground_lost_packets_fraction"]
    mid_descent = next(i for i, row in enumerate(sd_rows)
                       if int(row.split(",")[0]) / 1000.0 > truth["release_s"] + 3.0)
    keep[mid_descent] = False
    heard = [p for p, k in zip(packets, keep) if k]
    truth["packets_lost_on_ground"] = int(len(packets) - len(heard))
    truth["packet_lost_mid_descent"] = mid_descent + 1
    (out_dir / "ground-packets.txt").write_text("\n".join(heard) + "\n", encoding="utf-8")

    # The CSV comes out of the real ground-station pipeline, not out of this script.
    with tempfile.TemporaryDirectory() as tmp:
        result = subprocess.run(
            [sys.executable, str(ROOT / "ground-station" / "software" / "src" / "main.py"),
             "replay", str(out_dir / "ground-packets.txt"), "--team", truth["team_id"],
             "--output", tmp, "--export", str(out_dir / "ground-telemetry.csv")],
            capture_output=True, text=True, cwd=str(ROOT))
        if result.returncode != 0:
            raise RuntimeError("ground-station replay failed:\n" + result.stdout + result.stderr)

    (out_dir / "truth.json").write_text(json.dumps(truth, indent=2) + "\n", encoding="utf-8")
    return truth


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", type=Path, default=OUT_DIR)
    args = parser.parse_args()
    truth = write(args.out)
    print(f"SYNTHETIC flight written to {args.out}")
    print(f"  {truth['packets_transmitted']} packets, {truth['packets_lost_on_ground']} lost on the ground copy")
    print(f"  release {truth['release_s']} s, touchdown {truth['touchdown_s']} s, "
          f"terminal {truth['terminal_rate_mps']} m/s")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
