#!/usr/bin/env python3
"""Post-flight analysis for CanSat 2026 -- the four-hour window, done in one command.

    python analysis/flight_analysis.py FLIGHT_LOG [--compare OTHER_LOG] --out results/
            [--mass 0.50] [--canopy-diameter 0.80] [--team CAN-Team-25]

``FLIGHT_LOG`` may be any of the three things a flight leaves behind, and the format is
detected from the file itself:

* the **onboard SD log**, as ``tools/read_flight_log.py`` extracts it (``flight-1.csv``);
* the **ground station's parsed CSV** (``logs/telemetry.csv``, or a ``replay --export``);
* a **file of raw packets** -- the station's ``raw_packets.tsv``, or one packet per line.

Pass the SD log as the main input and the ground CSV as ``--compare``: the SD log is the
complete record, and the difference between the two is the radio loss.

It writes the three graphs the rulebook requires -- altitude, temperature and pressure
against time -- the optional analysis that scores extra credit, and a ``summary.md`` with
every number, ready to paste into the report. The notebook beside this file,
``flight_analysis.ipynb``, walks through the same functions one step at a time.

**What this cannot see, and says so in its output.** The onboard log writes one row per
transmitted packet, so everything here is sampled at the radio's cadence -- about 3.1 Hz in
flight -- not at the 30 Hz the sensors run at. Short events (the parachute snatch, the
landing impact) last tens of milliseconds and are almost certainly *between* samples, so
every peak acceleration reported is a lower bound. And yaw on this vehicle is relative: the
IMU has no magnetometer.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
for _extra in (ROOT / "ground-station" / "software" / "src", ROOT / "simulations"):
    if str(_extra) not in sys.path:
        sys.path.insert(0, str(_extra))

from telemetry import parse_packet  # noqa: E402  -- the ground station's parser: one format, one definition
from logger import unescape_raw  # noqa: E402
import descent  # noqa: E402

# ---- rulebook and firmware constants ------------------------------------------------------
RULEBOOK_MAX_DESCENT_MPS = 5.0          # REC-005
LAUNCH_ALTITUDE_GAIN_M = 15.0           # launch_altitude_gain_m in flight/config.hpp
DESCENT_GATE_MPS = -2.0                 # landing_descent_rate_mps -- F-20's descent gate
GPS_MIN_SATELLITES = 4                  # kMinGpsSatellites
GPS_MAX_HDOP = 5.0                      # kMaxGpsHdop
GRAVITY = 9.80665
DRY_AIR_R = 287.05

# Must match TelemetryBuilder::sd_header() -- the order is positional, because the last
# column (the packet) is the only one allowed to contain the separator's cousin, ';'.
SD_COLUMNS = ("mission_ms,packet_number,state,fault_total,altitude_m,pressure_pa,temperature_c,"
              "roll_deg,pitch_deg,yaw_deg,ax_mps2,ay_mps2,az_mps2,gps_valid,gps_lat,gps_lon,"
              "gps_alt,gps_satellites,gps_hdop,sound_mv_pp,sound_clipped,sound_gate_pct,"
              "packet").split(",")


# ==========================================================================================
# Loading
# ==========================================================================================

@dataclass
class Sample:
    t_s: float
    packet: int
    altitude: float
    pressure: float
    temperature: float
    roll: float
    pitch: float
    yaw: float
    ax: float
    ay: float
    az: float
    state: Optional[str] = None
    faults: Optional[int] = None
    gps_lat: Optional[float] = None
    gps_lon: Optional[float] = None
    gps_alt: Optional[float] = None
    gps_sats: Optional[int] = None
    gps_hdop: Optional[float] = None
    sound: Optional[float] = None
    sound_clipped: Optional[bool] = None


@dataclass
class Flight:
    """One power cycle of samples, as numpy arrays, plus where they came from."""
    source: Path
    kind: str                     # "sd", "ground-csv" or "packets"
    team: Optional[str]
    samples: list[Sample]
    notes: list[str] = field(default_factory=list)

    def __post_init__(self) -> None:
        def col(name, dtype=float):
            return np.array([np.nan if getattr(s, name) is None else getattr(s, name)
                             for s in self.samples], dtype=dtype)
        self.t = col("t_s")
        self.packet = np.array([s.packet for s in self.samples], dtype=int)
        for name in ("altitude", "pressure", "temperature", "roll", "pitch", "yaw",
                     "ax", "ay", "az", "gps_lat", "gps_lon", "gps_alt", "gps_hdop", "sound"):
            setattr(self, name, col(name))
        self.gps_sats = col("gps_sats")
        self.state = [s.state for s in self.samples]
        self.accel = np.sqrt(self.ax ** 2 + self.ay ** 2 + self.az ** 2)

    def __len__(self) -> int:
        return len(self.samples)

    @property
    def has_state(self) -> bool:
        return any(s is not None for s in self.state)


def _f(text: str) -> Optional[float]:
    text = (text or "").strip()
    if not text:
        return None
    try:
        value = float(text)
    except ValueError:
        return None
    return value if math.isfinite(value) else None


def _sample_from_record(record, t_s: Optional[float] = None) -> Sample:
    tags = record.tags
    faults = tags.get("FAULTS")
    return Sample(
        t_s=record.timestamp_ms / 1000.0 if t_s is None else t_s,
        packet=record.packet_number,
        altitude=record.altitude, pressure=record.pressure, temperature=record.temperature,
        roll=record.roll, pitch=record.pitch, yaw=record.yaw,
        ax=record.ax, ay=record.ay, az=record.az,
        state=tags.get("MODE"),
        faults=int(faults) if faults and faults.isdigit() else None,
        gps_lat=record.gps_lat, gps_lon=record.gps_lon, gps_alt=record.gps_alt,
        sound=record.sound_mv,
    )


def _load_sd(lines: list[str], notes: list[str]) -> tuple[list[Sample], Optional[str]]:
    samples, team, legacy = [], None, 0
    for line in lines:
        if not line.strip() or line.startswith("mission_ms") or line.startswith("team_id"):
            continue
        parts = line.rstrip("\r\n").split(",", len(SD_COLUMNS) - 1)
        if len(parts) == len(SD_COLUMNS) and parts[0].strip().isdigit():
            row = dict(zip(SD_COLUMNS, parts))
            parsed = parse_packet(row["packet"])
            team = team or (parsed.record.team_id if parsed.record else None)
            gps_ok = row["gps_valid"].strip() == "1"
            samples.append(Sample(
                t_s=int(row["mission_ms"]) / 1000.0, packet=int(row["packet_number"]),
                altitude=float(row["altitude_m"]), pressure=float(row["pressure_pa"]),
                temperature=float(row["temperature_c"]), roll=float(row["roll_deg"]),
                pitch=float(row["pitch_deg"]), yaw=float(row["yaw_deg"]),
                ax=float(row["ax_mps2"]), ay=float(row["ay_mps2"]), az=float(row["az_mps2"]),
                state=row["state"].strip() or None, faults=int(_f(row["fault_total"]) or 0),
                gps_lat=_f(row["gps_lat"]) if gps_ok else None,
                gps_lon=_f(row["gps_lon"]) if gps_ok else None,
                gps_alt=_f(row["gps_alt"]) if gps_ok else None,
                gps_sats=int(_f(row["gps_satellites"])) if gps_ok and _f(row["gps_satellites"]) is not None else None,
                gps_hdop=_f(row["gps_hdop"]) if gps_ok else None,
                sound=_f(row["sound_mv_pp"]),
                sound_clipped=(row["sound_clipped"].strip() == "1") if row["sound_clipped"].strip() else None,
            ))
        elif "CAN-Team" in line:
            # F-19: an older firmware wrote bare radio packets into the same file. They are
            # real data, so they are kept -- through the same parser as everything else.
            parsed = parse_packet(line[line.index("CAN-Team"):].strip())
            if parsed.record:
                samples.append(_sample_from_record(parsed.record))
                legacy += 1
    if legacy:
        notes.append(f"{legacy} rows were bare radio packets rather than SD columns (F-19); "
                     "they were read as packets and carry no GPS quality or sound clipping")
    return samples, team


def _load_ground_csv(path: Path, notes: list[str]) -> tuple[list[Sample], Optional[str]]:
    samples, team, rejected = [], None, 0
    with path.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            if str(row.get("valid", "")).strip().lower() not in ("true", "1"):
                rejected += 1
                continue
            raw = unescape_raw(row.get("raw_packet") or "")
            parsed = parse_packet(raw) if raw else None
            if parsed and parsed.record:
                samples.append(_sample_from_record(parsed.record))
                team = team or parsed.record.team_id
    if rejected:
        notes.append(f"{rejected} ground-station rows were marked invalid and skipped; "
                     "their reasons are in the CSV's error column")
    return samples, team


def _load_packets(lines: list[str], notes: list[str]) -> tuple[list[Sample], Optional[str]]:
    samples, team, bad = [], None, 0
    for line in lines:
        if "CAN-Team" not in line:
            continue
        text = unescape_raw(line[line.index("CAN-Team"):].strip())
        parsed = parse_packet(text)
        if parsed.record:
            samples.append(_sample_from_record(parsed.record))
            team = team or parsed.record.team_id
        else:
            bad += 1
    if bad:
        notes.append(f"{bad} packet lines did not parse and were skipped")
    return samples, team


def detect_kind(path: Path) -> str:
    with path.open(encoding="utf-8", errors="replace") as stream:
        for line in stream:
            if not line.strip():
                continue
            if line.startswith("mission_ms"):
                return "sd"
            if line.startswith("receipt_time"):
                return "ground-csv"
            if re.match(r"^\d+,\d+,[A-Z_]+,", line):
                return "sd"
            return "packets"
    raise ValueError(f"{path} is empty")


def load(path, session: Optional[int] = None) -> Flight:
    """Load any flight log. A log holding several power cycles is split at every reset of
    the packet number; by default the session that climbed highest is returned."""
    path = Path(path)
    kind = detect_kind(path)
    notes: list[str] = []
    if kind == "ground-csv":
        samples, team = _load_ground_csv(path, notes)
    else:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        samples, team = (_load_sd if kind == "sd" else _load_packets)(lines, notes)
    if not samples:
        raise ValueError(f"{path}: no telemetry could be read from it")

    sessions, current = [], [samples[0]]
    for s in samples[1:]:
        if s.packet < current[-1].packet and s.packet <= 3:
            sessions.append(current)
            current = [s]
        else:
            current.append(s)
    sessions.append(current)
    if len(sessions) > 1:
        heights = [max(x.altitude for x in sess) for sess in sessions]
        chosen = int(np.argmax(heights)) if session is None else session
        notes.append(f"the log holds {len(sessions)} power cycles (packet numbering restarts); "
                     f"using #{chosen + 1}, which reaches {heights[chosen]:.1f} m")
        samples = sessions[chosen]

    # Duplicates keep their first copy, and the rest is put in packet order.
    seen, unique = set(), []
    for s in samples:
        if s.packet not in seen:
            seen.add(s.packet)
            unique.append(s)
    if len(unique) < len(samples):
        notes.append(f"{len(samples) - len(unique)} duplicate packet numbers were dropped")
    unique.sort(key=lambda s: s.packet)
    return Flight(source=path, kind=kind, team=team, samples=unique, notes=notes)


# ==========================================================================================
# Analysis
# ==========================================================================================

def _smooth(values: np.ndarray, window: int = 3) -> np.ndarray:
    if len(values) < window:
        return values.copy()
    kernel = np.ones(window) / window
    padded = np.pad(values, window // 2, mode="edge")
    return np.convolve(padded, kernel, mode="valid")[: len(values)]


def vertical_rate(flight: Flight) -> np.ndarray:
    """Vertical speed, m/s, positive up, from the lightly smoothed barometric altitude."""
    if len(flight) < 3:
        return np.zeros(len(flight))
    return np.gradient(_smooth(flight.altitude, 3), flight.t)


def _first_sustained(mask: np.ndarray, t: np.ndarray, hold_s: float, start: int = 0) -> Optional[int]:
    run_start = None
    for i in range(start, len(mask)):
        if mask[i]:
            run_start = i if run_start is None else run_start
            if t[i] - t[run_start] >= hold_s:
                return run_start
        else:
            run_start = None
    return None


@dataclass
class Phases:
    ground_altitude: float
    launch_t: Optional[float] = None
    apex_t: Optional[float] = None
    apex_altitude: Optional[float] = None
    release_t: Optional[float] = None
    landing_t: Optional[float] = None
    flight_declared_t: Optional[float] = None
    landed_declared_t: Optional[float] = None
    method: str = ""

    def window(self, before_s: float = 15.0, after_s: float = 20.0) -> tuple[float, float]:
        start = (self.launch_t if self.launch_t is not None else 0.0) - before_s
        end = (self.landing_t if self.landing_t is not None else start + 60.0) + after_s
        return start, end


def find_phases(flight: Flight) -> Phases:
    """Launch, apex, release and landing -- from the data, not from the vehicle's own state,
    which is recorded beside them for comparison. The vehicle's FLIGHT is declared on the
    climb and its LANDED three seconds after rest, so neither is the physical event."""
    t, alt = flight.t, flight.altitude
    rate = vertical_rate(flight)

    declared = {name: next((t[i] for i, s in enumerate(flight.state) if s == name), None)
                for name in ("FLIGHT", "LANDED")}

    ph = Phases(ground_altitude=float(np.median(alt)))
    # The pad phase is most of any real log -- the command window alone is five minutes --
    # so a low percentile of the whole log is the ground, and it ignores the few uncalibrated
    # rows at power-on that read ~23 m.
    baseline = float(np.percentile(alt, 25))
    # Those uncalibrated rows also sit 15 m "above" the ground for longer than 0.3 s, which
    # is a launch by the threshold alone. The search starts once the barometer first reads
    # near the ground, which is after calibration and before any real climb.
    settled = np.where(np.abs(alt - baseline) < 2.0)[0]
    climb_idx = (_first_sustained(alt - baseline > LAUNCH_ALTITUDE_GAIN_M, t, 0.3,
                                  start=int(settled[0])) if len(settled) else None)
    if climb_idx is None:
        ph.method = "no climb above 15 m was found - phases not detected"
        ph.flight_declared_t, ph.landed_declared_t = declared["FLIGHT"], declared["LANDED"]
        return ph

    pad = (t < t[climb_idx] - 5.0) & (t > t[climb_idx] - 40.0)
    if pad.sum() < 3:
        pad = t < t[climb_idx]
    ph.ground_altitude = float(np.median(alt[pad])) if pad.any() else float(alt[0])

    above = alt - ph.ground_altitude
    launch = climb_idx
    while launch > 0 and above[launch - 1] > 0.5:
        launch -= 1
    ph.launch_t = float(t[launch])

    apex = int(np.argmax(_smooth(alt, 3)))
    ph.apex_t, ph.apex_altitude = float(t[apex]), float(above[apex])

    gate = _first_sustained(rate < DESCENT_GATE_MPS, t, 1.0, start=apex)
    if gate is not None:
        # The first sample below the hover plateau is already a few tenths of a second into
        # the fall -- and that first part is free fall, before the canopy takes load, so
        # the time it took is known: d = g t^2 / 2. Back-calculating from the drop it shows
        # puts release within a sample's noise rather than within a sample interval.
        release = gate
        while release > apex + 1 and above[release - 1] < ph.apex_altitude - 0.4:
            release -= 1
        plateau = float(np.median(above[max(apex, release - 6):release]))
        dropped = max(0.0, plateau - float(above[release]))
        estimate = float(t[release]) - math.sqrt(2.0 * dropped / GRAVITY)
        ph.release_t = max(float(t[release - 1]), estimate)

        ground = np.where(above[gate:] < 1.0)[0]
        if len(ground):
            idx = gate + int(ground[0])
            ph.landing_t = float(t[idx])
            # Where the steady descent line meets the ground is a better touchdown time than
            # the first sample under a metre, which can be most of a sample interval late or
            # early. Accepted only if it lands near that sample.
            steady = (t >= ph.release_t + 1.5) & (t <= t[idx] - 0.4)
            if steady.sum() >= 3:
                slope, intercept = np.polyfit(t[steady], above[steady], 1)
                if slope < 0:
                    crossing = -intercept / slope
                    if t[idx] - 1.0 <= crossing <= t[idx] + 0.5:
                        ph.landing_t = float(crossing)

    ph.flight_declared_t, ph.landed_declared_t = declared["FLIGHT"], declared["LANDED"]
    ph.method = "barometric altitude, the firmware's own thresholds (15 m climb, -2 m/s descent gate)"
    return ph


def pad_reference(flight: Flight, phases: Phases) -> tuple[float, float]:
    """(pressure Pa, temperature K) on the pad just before the launch."""
    if phases.launch_t is not None:
        pad = _between(flight, phases.launch_t - 20.0, phases.launch_t - 1.0)
        if pad.sum() >= 3:
            return (float(np.median(flight.pressure[pad])),
                    float(np.median(flight.temperature[pad])) + 273.15)
    return float(np.median(flight.pressure)), float(np.median(flight.temperature)) + 273.15


def corrected_height(flight: Flight, phases: Phases) -> np.ndarray:
    """Height above the pad from pressure and the *measured* temperature -- the hypsometric
    equation -- rather than the vehicle's altitude.

    The firmware's ``sensors::pressure_altitude_m()`` is the ISA formula,
    ``44330 (1 - (p/p0)^(1/5.255))``, which assumes the standard atmosphere's temperature.
    Real height per pascal scales with the real absolute temperature, so on a 31 C day the
    vehicle's altitude reads about 5 % low, and any rate taken from it reads 5 % low too --
    which is the difference between a 5.05 m/s descent and a compliant-looking 4.8. The sealed
    image cannot be changed, so the correction is made here.

    The BMP280's temperature is the board's, a little above the air's when self-heated; an
    error of 3 K in it moves this height by 1 %.
    """
    p0, t_k = pad_reference(flight, phases)
    return (DRY_AIR_R * t_k / GRAVITY) * np.log(p0 / flight.pressure)


def _haversine_m(lat1, lon1, lat2, lon2) -> float:
    r = 6_371_000.0
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dp, dl = p2 - p1, math.radians(lon2 - lon1)
    a = math.sin(dp / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dl / 2) ** 2
    return 2 * r * math.asin(math.sqrt(a))


def _bearing_deg(lat1, lon1, lat2, lon2) -> float:
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dl = math.radians(lon2 - lon1)
    x = math.sin(dl) * math.cos(p2)
    y = math.cos(p1) * math.sin(p2) - math.sin(p1) * math.cos(p2) * math.cos(dl)
    return (math.degrees(math.atan2(x, y)) + 360.0) % 360.0


def _pearson(a: np.ndarray, b: np.ndarray) -> Optional[float]:
    ok = np.isfinite(a) & np.isfinite(b)
    if ok.sum() < 4 or np.std(a[ok]) == 0 or np.std(b[ok]) == 0:
        return None
    return float(np.corrcoef(a[ok], b[ok])[0, 1])


def _between(flight: Flight, start: Optional[float], end: Optional[float]) -> np.ndarray:
    if start is None or end is None:
        return np.zeros(len(flight), dtype=bool)
    return (flight.t >= start) & (flight.t <= end)


def data_quality(flight: Flight, phases: Optional[Phases] = None) -> dict:
    t, pk = flight.t, flight.packet
    gaps = np.diff(pk) - 1
    missing = int(gaps[gaps > 0].sum()) if len(gaps) else 0
    expected = int(pk[-1] - pk[0] + 1)
    intervals = np.diff(t)
    out = {
        "source": str(flight.source),
        "format": flight.kind,
        "team": flight.team,
        "samples": len(flight),
        "first_packet": int(pk[0]),
        "last_packet": int(pk[-1]),
        "missing_packets": missing,
        "loss_percent": round(100.0 * missing / expected, 2) if expected else 0.0,
        "duration_s": round(float(t[-1] - t[0]), 1),
        "median_interval_ms": round(float(np.median(intervals)) * 1000, 0) if len(intervals) else None,
        "time_regressions": int((intervals < 0).sum()) if len(intervals) else 0,
        "rows_with_gps": int(np.isfinite(flight.gps_lat).sum()),
        "rows_with_sound": int(np.isfinite(flight.sound).sum()),
        "max_fault_total": max((s.faults for s in flight.samples if s.faults is not None), default=None),
        "notes": list(flight.notes),
    }
    if phases and phases.launch_t is not None:
        window = _between(flight, *phases.window(0.0, 0.0))
        wpk = pk[window]
        if len(wpk) > 1:
            wgaps = np.diff(wpk) - 1
            out["missing_packets_in_flight"] = int(wgaps[wgaps > 0].sum())
            # Mean, not median: the max-rate pattern is one 374 ms slot and two 296 ms ones,
            # so the median interval overstates the rate by a tenth.
            span = float(t[window][-1] - t[window][0])
            out["flight_rate_hz"] = round((len(wpk) - 1) / span, 2) if span > 0 else None
    return out


def descent_analysis(flight: Flight, phases: Phases, mass_kg: Optional[float] = None,
                     canopy_diameter_m: Optional[float] = None) -> dict:
    """Descent rate by regression over the steady part of the fall, and -- given the mass and
    the canopy -- the drag coefficient that rate implies. That Cd is the number the whole
    parachute sizing rested on, and this is the first measurement of it."""
    out: dict = {}
    if phases.release_t is None or phases.landing_t is None:
        out["note"] = "release or landing not found; no descent analysis"
        return out

    t = flight.t
    height = corrected_height(flight, phases)
    reported = flight.altitude - phases.ground_altitude
    out["descent_time_s"] = round(phases.landing_t - phases.release_t, 2)
    top = _between(flight, phases.release_t - 3.0, phases.release_t)
    drop = float(np.median(height[top])) if top.any() else (phases.apex_altitude or 0.0)
    out["release_height_m"] = round(drop, 2)
    out["average_rate_mps"] = round(drop / max(out["descent_time_s"], 1e-6), 2)

    # Steady segment: skip the free fall and the canopy snatch at the top, and the last
    # half-second at the bottom, where the barometer sees ground effect and the impact.
    start, end = phases.release_t + 1.5, phases.landing_t - 0.5
    steady = _between(flight, start, end)
    if steady.sum() < 4:
        start, end = phases.release_t + 0.8, phases.landing_t - 0.2
        steady = _between(flight, start, end)
    out["steady_samples"] = int(steady.sum())
    if steady.sum() >= 3:
        slope, intercept = np.polyfit(t[steady], height[steady], 1)
        residual = height[steady] - (slope * t[steady] + intercept)
        n = steady.sum()
        stderr = (math.sqrt(float((residual ** 2).sum()) / max(n - 2, 1))
                  / math.sqrt(float(((t[steady] - t[steady].mean()) ** 2).sum()))) if n > 2 else float("nan")
        v = -float(slope)
        out["terminal_rate_mps"] = round(v, 3)
        out["terminal_rate_stderr_mps"] = round(stderr, 3)
        out["within_rulebook_cap"] = bool(v <= RULEBOOK_MAX_DESCENT_MPS)
        v_reported = -float(np.polyfit(t[steady], reported[steady], 1)[0])
        out["terminal_rate_from_vehicle_altitude_mps"] = round(v_reported, 3)
        out["vehicle_altitude_bias_percent"] = round(100.0 * (v_reported - v) / v, 1)

        p = float(np.nanmean(flight.pressure[steady]))
        temp_c = float(np.nanmean(flight.temperature[steady]))
        rho = descent.air_density(p, temp_c)
        out["air_density_kgm3"] = round(rho, 4)
        if mass_kg and canopy_diameter_m:
            area = descent.circular_area(canopy_diameter_m)
            cd = 2.0 * mass_kg * GRAVITY / (rho * area * v * v)
            out["implied_drag_coefficient"] = round(cd, 3)
            model_cd = descent.CANOPY_TYPES[descent.DEFAULT_CANOPY][0]
            model_v = descent.terminal_velocity(mass_kg, area, model_cd, rho)
            out["model_rate_mps"] = round(model_v, 3)
            out["model_drag_coefficient"] = model_cd
            out["rate_vs_model_percent"] = round(100.0 * (v - model_v) / model_v, 1)
    return out


def dynamics_analysis(flight: Flight, phases: Phases) -> dict:
    """Accelerations, attitude stability during the descent, spin and pendulum frequency."""
    window = _between(flight, *phases.window(0.0, 0.0)) if phases.launch_t is not None else np.ones(len(flight), bool)
    spacing = np.diff(flight.t[window]) if window.sum() > 1 else np.diff(flight.t)
    out: dict = {"note_sampling": (
        f"sampled once per packet (median {np.median(spacing) * 1000:.0f} ms in flight); "
        "peak accelerations are lower bounds - the snatch and the impact last tens of ms")}
    if phases.release_t is None or phases.landing_t is None:
        return out
    t = flight.t

    snatch = _between(flight, phases.release_t, phases.release_t + 2.0)
    impact = _between(flight, phases.landing_t - 1.0, phases.landing_t + 2.0)
    if snatch.any():
        out["max_accel_after_release_mps2"] = round(float(np.nanmax(flight.accel[snatch])), 2)
        out["min_accel_after_release_mps2"] = round(float(np.nanmin(flight.accel[snatch])), 2)
    if impact.any():
        out["max_accel_at_landing_mps2"] = round(float(np.nanmax(flight.accel[impact])), 2)

    steady = _between(flight, phases.release_t + 1.0, phases.landing_t - 0.3)
    if steady.sum() >= 4:
        roll, pitch = flight.roll[steady], flight.pitch[steady]
        tilt = np.sqrt(roll ** 2 + pitch ** 2)
        out["descent_roll_std_deg"] = round(float(np.std(roll)), 2)
        out["descent_pitch_std_deg"] = round(float(np.std(pitch)), 2)
        out["descent_tilt_mean_deg"] = round(float(np.mean(tilt)), 2)
        out["descent_tilt_max_deg"] = round(float(np.max(tilt)), 2)

        yaw = np.degrees(np.unwrap(np.radians(flight.yaw[steady])))
        spin = float(np.polyfit(t[steady], yaw, 1)[0])
        out["spin_rate_dps"] = round(spin, 1)
        out["spin_rpm"] = round(spin / 6.0, 2)

        # Pendulum frequency from the attitude swing. Resampled onto a uniform grid first --
        # the max-rate pattern is not evenly spaced -- and only trusted below Nyquist.
        ts = t[steady]
        dt = float(np.median(np.diff(ts)))
        grid = np.arange(ts[0], ts[-1], dt)
        if len(grid) >= 8:
            swing = np.interp(grid, ts, roll) + 1j * np.interp(grid, ts, pitch)
            swing = swing - swing.mean()
            spectrum = np.abs(np.fft.fft(swing * np.hanning(len(grid)))) ** 2
            freqs = np.abs(np.fft.fftfreq(len(grid), dt))
            valid = freqs > 1.0 / (ts[-1] - ts[0])
            if valid.any():
                peak = freqs[valid][int(np.argmax(spectrum[valid]))]
                out["pendulum_frequency_hz"] = round(float(peak), 2)
                out["nyquist_hz"] = round(0.5 / dt, 2)
                out["pendulum_resolution_hz"] = round(1.0 / (ts[-1] - ts[0]), 2)
    return out


def environment_analysis(flight: Flight, phases: Phases) -> dict:
    """Temperature against altitude, and the barometer checked against itself."""
    out: dict = {}
    if phases.launch_t is None:
        return out
    end = phases.landing_t if phases.landing_t is not None else phases.launch_t + 60.0
    flying = _between(flight, phases.launch_t, end)
    above = flight.altitude - phases.ground_altitude
    if flying.sum() >= 4:
        lapse = float(np.polyfit(above[flying], flight.temperature[flying], 1)[0]) * 1000.0
        out["temperature_lapse_K_per_km"] = round(lapse, 1)
        out["temperature_range_c"] = [round(float(np.nanmin(flight.temperature[flying])), 1),
                                      round(float(np.nanmax(flight.temperature[flying])), 1)]
        out["lapse_note"] = ("the standard atmosphere cools 0.2 K over 30 m - below this "
                             "sensor's 0.1 K resolution plus noise, so a lapse rate from one "
                             "flight is not meaningful; self-heating dominates")

    if flying.sum() >= 4:
        p0, t_k = pad_reference(flight, phases)
        height = corrected_height(flight, phases)
        high = flying & (height > 5.0)
        out["ground_pressure_pa"] = round(p0, 1)
        out["ground_temperature_c"] = round(t_k - 273.15, 1)
        if high.sum() >= 3:
            scale = float(np.median(above[high] / height[high]))
            out["vehicle_altitude_scale"] = round(scale, 3)
            out["vehicle_altitude_scale_expected"] = round(288.15 / t_k, 3)
            out["altitude_note"] = ("the vehicle computes altitude with the ISA formula, which "
                                    "assumes 15 C; its altitude reads low by the ratio of 288 K "
                                    "to the real temperature, and the descent here uses the "
                                    "temperature-corrected height instead")
    return out


def gps_analysis(flight: Flight, phases: Phases) -> dict:
    ok = np.isfinite(flight.gps_lat) & np.isfinite(flight.gps_lon)
    if np.isfinite(flight.gps_sats).any():
        ok &= ~(flight.gps_sats < GPS_MIN_SATELLITES)
    if np.isfinite(flight.gps_hdop).any():
        ok &= ~(flight.gps_hdop > GPS_MAX_HDOP)
    out: dict = {"fixes": int(ok.sum())}
    if ok.sum() < 2 or phases.release_t is None:
        return out
    t = flight.t
    before = ok & (t <= phases.release_t) & (t >= phases.release_t - 10.0)
    after = ok & (t >= (phases.landing_t or t[-1]) + 1.0)
    if before.any() and after.any():
        lat1, lon1 = float(np.median(flight.gps_lat[before])), float(np.median(flight.gps_lon[before]))
        lat2, lon2 = float(np.median(flight.gps_lat[after])), float(np.median(flight.gps_lon[after]))
        out["release_position"] = [round(lat1, 6), round(lon1, 6)]
        out["landing_position"] = [round(lat2, 6), round(lon2, 6)]
        out["drift_m"] = round(_haversine_m(lat1, lon1, lat2, lon2), 1)
        out["drift_bearing_deg"] = round(_bearing_deg(lat1, lon1, lat2, lon2), 0)
        out["drift_note"] = "median of fixes in the 10 s before release against those after landing; NEO-6M error is ~2.5 m"
    pad = ok & (t < (phases.launch_t or t[0]) - 1.0)
    top = ok & _between(flight, (phases.release_t or 0) - 10.0, phases.release_t)
    if np.isfinite(flight.gps_alt).any() and pad.any() and top.any():
        out["gps_height_gain_m"] = round(float(np.median(flight.gps_alt[top]) - np.median(flight.gps_alt[pad])), 1)
    return out


def sound_analysis(flight: Flight, phases: Phases) -> dict:
    out: dict = {"rows": int(np.isfinite(flight.sound).sum())}
    if out["rows"] < 4 or phases.release_t is None or phases.landing_t is None:
        return out
    snd = flight.sound
    rate = -vertical_rate(flight)
    descent_mask = _between(flight, phases.release_t + 1.0, phases.landing_t - 0.3)
    pad = _between(flight, (phases.launch_t or 0) - 20.0, (phases.launch_t or 0) - 2.0)
    lift = _between(flight, phases.launch_t, phases.release_t)
    for label, mask in (("pad", pad), ("lift_and_hover", lift), ("descent", descent_mask)):
        if np.isfinite(snd[mask]).any():
            out[f"median_{label}_mv"] = round(float(np.nanmedian(snd[mask])), 1)
    snatch = _between(flight, phases.release_t, phases.release_t + 2.0)
    impact = _between(flight, phases.landing_t - 0.5, phases.landing_t + 1.5)
    if np.isfinite(snd[snatch]).any():
        out["peak_after_release_mv"] = round(float(np.nanmax(snd[snatch])), 1)
    if np.isfinite(snd[impact]).any():
        out["peak_at_landing_mv"] = round(float(np.nanmax(snd[impact])), 1)
    r = _pearson(snd[descent_mask], rate[descent_mask] ** 2)
    if r is not None:
        out["descent_sound_vs_speed_squared_r"] = round(r, 2)
    out["note"] = ("relative peak-to-peak level in mV, not a sound pressure level. At terminal "
                   "velocity the speed barely varies, so a weak correlation with speed is expected "
                   "and does not mean the microphone failed; transients shorter than one packet "
                   "interval are usually missed")
    return out


def compare_logs(primary: Flight, other: Flight, phases: Phases) -> dict:
    """What one log has that the other does not -- with the SD log as primary, the radio loss."""
    a, b = set(primary.packet.tolist()), set(other.packet.tolist())
    missing = sorted(a - b)
    out = {"in_primary": len(a), "in_other": len(b), "missing_from_other": len(missing),
           "missing_percent": round(100.0 * len(missing) / max(len(a), 1), 2)}
    if phases.launch_t is not None:
        start, end = phases.window(0.0, 0.0)
        by_packet = dict(zip(primary.packet.tolist(), primary.t.tolist()))
        in_flight = [p for p in missing if start <= by_packet[p] <= end]
        out["missing_during_flight"] = in_flight
        out["missing_during_descent"] = [p for p in in_flight if phases.release_t is not None
                                         and by_packet[p] >= phases.release_t]
    shared = sorted(a & b)
    if shared:
        ia = {p: i for i, p in enumerate(primary.packet.tolist())}
        ib = {p: i for i, p in enumerate(other.packet.tolist())}
        diffs = [abs(primary.altitude[ia[p]] - other.altitude[ib[p]]) for p in shared]
        out["max_altitude_disagreement_m"] = round(float(max(diffs)), 2)
    return out


@dataclass
class Analysis:
    flight: Flight
    phases: Phases
    quality: dict
    descent: dict
    dynamics: dict
    environment: dict
    gps: dict
    sound: dict
    comparison: Optional[dict] = None
    mass_kg: Optional[float] = None
    canopy_diameter_m: Optional[float] = None

    def as_dict(self) -> dict:
        return {"quality": self.quality, "phases": self.phases.__dict__, "descent": self.descent,
                "dynamics": self.dynamics, "environment": self.environment, "gps": self.gps,
                "sound": self.sound, "comparison": self.comparison,
                "inputs": {"mass_kg": self.mass_kg, "canopy_diameter_m": self.canopy_diameter_m}}


def analyse(flight: Flight, mass_kg: Optional[float] = None, canopy_diameter_m: Optional[float] = None,
            compare: Optional[Flight] = None) -> Analysis:
    phases = find_phases(flight)
    return Analysis(
        flight=flight, phases=phases, quality=data_quality(flight, phases),
        descent=descent_analysis(flight, phases, mass_kg, canopy_diameter_m),
        dynamics=dynamics_analysis(flight, phases),
        environment=environment_analysis(flight, phases),
        gps=gps_analysis(flight, phases), sound=sound_analysis(flight, phases),
        comparison=compare_logs(flight, compare, phases) if compare is not None else None,
        mass_kg=mass_kg, canopy_diameter_m=canopy_diameter_m)


# ==========================================================================================
# Figures
# ==========================================================================================

def _plt():
    import matplotlib
    if "matplotlib.pyplot" not in sys.modules and not hasattr(sys, "ps1") and "ipykernel" not in sys.modules:
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    return plt


PHASE_COLOURS = {"lift": "#fff3e0", "descent": "#e3f2fd", "landed": "#e8f5e9"}


def _shade(ax, ph: Phases) -> None:
    if ph.launch_t is not None and ph.release_t is not None:
        ax.axvspan(ph.launch_t, ph.release_t, color=PHASE_COLOURS["lift"], zorder=0)
    if ph.release_t is not None and ph.landing_t is not None:
        ax.axvspan(ph.release_t, ph.landing_t, color=PHASE_COLOURS["descent"], zorder=0)
    for when, name in ((ph.release_t, "release"), (ph.landing_t, "landing")):
        if when is not None:
            ax.axvline(when, color="#546e7a", lw=0.8, ls="--")


def _flight_window(an: Analysis, full: bool):
    if full or an.phases.launch_t is None:
        return np.ones(len(an.flight), dtype=bool)
    return _between(an.flight, *an.phases.window())


def plot_series(an: Analysis, column: str, ylabel: str, title: str, full: bool = False):
    plt = _plt()
    fl, ph = an.flight, an.phases
    m = _flight_window(an, full)
    fig, ax = plt.subplots(figsize=(8, 3.6))
    _shade(ax, ph)
    values = getattr(fl, column)
    ax.plot(fl.t[m], values[m], ".-", color="#0d47a1", lw=1.0, ms=3)
    ax.set_xlabel("mission time since power-on (s)")
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.grid(alpha=0.3)
    secondary = ax.secondary_xaxis("top", functions=(
        lambda x: np.interp(x, fl.t, fl.packet), lambda p: np.interp(p, fl.packet, fl.t)))
    secondary.set_xlabel("packet number")
    fig.tight_layout()
    return fig


def plot_altitude(an: Analysis, full: bool = False):
    fig = plot_series(an, "altitude", "altitude (m)",
                      "Altitude against time — shaded: lift and hover, then descent", full)
    ax = fig.axes[0]
    if an.phases.apex_t is not None:
        corrected = an.descent.get("release_height_m")
        label = f"apex {an.phases.apex_altitude:.1f} m as transmitted"
        if corrected is not None:
            label += f"\n{corrected:.1f} m temperature-corrected"
        ax.annotate(label, (an.phases.apex_t, an.phases.apex_altitude + an.phases.ground_altitude),
                    textcoords="offset points", xytext=(12, -34), ha="left", fontsize=8,
                    arrowprops=dict(arrowstyle="-", color="#9e9e9e", lw=0.7))
    return fig


def plot_temperature(an: Analysis, full: bool = False):
    return plot_series(an, "temperature", "temperature (°C)", "Temperature against time", full)


def plot_pressure(an: Analysis, full: bool = False):
    fig = plot_series(an, "pressure", "pressure (Pa)", "Pressure against time", full)
    fig.axes[0].ticklabel_format(axis="y", useOffset=False)
    return fig


def plot_descent(an: Analysis):
    plt = _plt()
    fl, ph, d = an.flight, an.phases, an.descent
    fig, (a1, a2) = plt.subplots(2, 1, figsize=(8, 5.4), sharex=True)
    lo = (ph.release_t or fl.t[0]) - 5.0
    hi = (ph.landing_t or fl.t[-1]) + 5.0
    m = (fl.t >= lo) & (fl.t <= hi)
    above = corrected_height(fl, ph)
    for ax in (a1, a2):
        _shade(ax, ph)
        ax.grid(alpha=0.3)
    a1.plot(fl.t[m], (fl.altitude - ph.ground_altitude)[m], ".", ms=3, color="#9e9e9e",
            label="vehicle altitude (ISA formula)")
    a1.plot(fl.t[m], above[m], "o-", ms=3, color="#0d47a1", label="temperature-corrected height")
    if "terminal_rate_mps" in d and ph.release_t is not None:
        tt = np.linspace(ph.release_t + 1.5, ph.landing_t - 0.5, 10)
        steady = _between(fl, ph.release_t + 1.5, ph.landing_t - 0.5)
        slope, intercept = np.polyfit(fl.t[steady], above[steady], 1) if steady.sum() >= 3 else (0, 0)
        a1.plot(tt, slope * tt + intercept, "--", color="#e65100",
                label=f"fit: {d['terminal_rate_mps']:.2f} ± {d['terminal_rate_stderr_mps']:.2f} m/s")
    a1.set_ylabel("m above pad")
    a1.legend(fontsize=8, loc="lower left")
    a1.set_title("Descent — shaded from release to landing")
    rate = np.gradient(_smooth(above, 3), fl.t)
    a2.plot(fl.t[m], rate[m], "o-", ms=3, color="#00695c", label="vertical rate (corrected)")
    a2.axhline(-RULEBOOK_MAX_DESCENT_MPS, color="#b71c1c", ls=":", label="rulebook cap −5 m/s")
    if "model_rate_mps" in d:
        a2.axhline(-d["model_rate_mps"], color="#546e7a", ls="--", lw=0.9,
                   label=f"model (Cd {d['model_drag_coefficient']}): −{d['model_rate_mps']:.2f} m/s")
    a2.set_ylabel("m/s (up positive)")
    a2.set_xlabel("mission time since power-on (s)")
    a2.legend(fontsize=8, loc="upper left")
    a2.set_xlim(lo, hi)
    fig.tight_layout()
    return fig


def plot_acceleration(an: Analysis, full: bool = False):
    plt = _plt()
    fl, ph = an.flight, an.phases
    m = _flight_window(an, full)
    fig, ax = plt.subplots(figsize=(8, 3.6))
    _shade(ax, ph)
    for name, colour in (("ax", "#1565c0"), ("ay", "#2e7d32"), ("az", "#6a1b9a")):
        ax.plot(fl.t[m], getattr(fl, name)[m], lw=0.9, color=colour, label=name.upper())
    ax.plot(fl.t[m], fl.accel[m], lw=1.4, color="#212121", label="|a|")
    ax.axhline(GRAVITY, color="#9e9e9e", ls=":", lw=0.8)
    ax.set_xlabel("mission time since power-on (s)")
    ax.set_ylabel("m/s²")
    ax.set_title("Acceleration — sampled per packet, so peaks are lower bounds")
    ax.legend(fontsize=8, ncol=4, loc="upper left")
    ax.grid(alpha=0.3)
    fig.tight_layout()
    return fig


def plot_orientation(an: Analysis, full: bool = False):
    plt = _plt()
    fl, ph = an.flight, an.phases
    m = _flight_window(an, full)
    fig, (a1, a2) = plt.subplots(2, 1, figsize=(8, 5), sharex=True)
    for ax in (a1, a2):
        _shade(ax, ph)
        ax.grid(alpha=0.3)
    a1.plot(fl.t[m], fl.roll[m], lw=1.0, label="roll")
    a1.plot(fl.t[m], fl.pitch[m], lw=1.0, label="pitch")
    a1.set_ylabel("degrees")
    a1.legend(fontsize=8)
    a1.set_title("Orientation")
    a2.plot(fl.t[m], fl.yaw[m], lw=1.0, color="#6a1b9a", label="yaw (relative — no magnetometer)")
    a2.set_ylabel("degrees")
    a2.set_xlabel("mission time since power-on (s)")
    a2.legend(fontsize=8)
    fig.tight_layout()
    return fig


def plot_sound(an: Analysis, full: bool = False):
    plt = _plt()
    fl, ph = an.flight, an.phases
    m = _flight_window(an, full) & np.isfinite(fl.sound)
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(9, 3.6), gridspec_kw={"width_ratios": [2, 1]})
    _shade(a1, ph)
    a1.plot(fl.t[m], fl.sound[m], ".-", ms=3, lw=0.9, color="#4527a0")
    a1.set_xlabel("mission time since power-on (s)")
    a1.set_ylabel("mV peak-to-peak (relative)")
    a1.set_title("Acoustic level")
    a1.grid(alpha=0.3)
    if ph.release_t is not None and ph.landing_t is not None:
        dm = _between(fl, ph.release_t + 1.0, ph.landing_t - 0.3) & np.isfinite(fl.sound)
        speed = -vertical_rate(fl)
        a2.plot(speed[dm] ** 2, fl.sound[dm], "o", ms=4, color="#4527a0")
        r = an.sound.get("descent_sound_vs_speed_squared_r")
        a2.set_title(f"Descent: level vs speed²" + (f"  (r = {r})" if r is not None else ""), fontsize=9)
    a2.set_xlabel("descent speed² (m²/s²)")
    a2.grid(alpha=0.3)
    fig.tight_layout()
    return fig


def plot_gps(an: Analysis):
    plt = _plt()
    fl, ph = an.flight, an.phases
    ok = np.isfinite(fl.gps_lat)
    fig, ax = plt.subplots(figsize=(5.2, 5.2))
    if ok.sum() >= 2:
        lat0, lon0 = float(np.median(fl.gps_lat[ok])), float(np.median(fl.gps_lon[ok]))
        north = (fl.gps_lat - lat0) * 111_320.0
        east = (fl.gps_lon - lon0) * 111_320.0 * math.cos(math.radians(lat0))
        scatter = ax.scatter(east[ok], north[ok], c=fl.t[ok], s=10, cmap="viridis")
        fig.colorbar(scatter, ax=ax, label="mission time (s)")
        ax.set_xlabel("east (m)")
        ax.set_ylabel("north (m)")
        ax.set_aspect("equal", adjustable="datalim")
        if "drift_m" in an.gps:
            ax.set_title(f"GPS track — drift {an.gps['drift_m']} m toward {an.gps['drift_bearing_deg']:.0f}°",
                         fontsize=10)
        else:
            ax.set_title("GPS track")
    else:
        ax.text(0.5, 0.5, "no GPS fixes in this log", ha="center", va="center", transform=ax.transAxes)
    ax.grid(alpha=0.3)
    fig.tight_layout()
    return fig


def plot_correlations(an: Analysis):
    plt = _plt()
    fl, ph = an.flight, an.phases
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(9, 3.8))
    if ph.launch_t is not None:
        end = ph.landing_t if ph.landing_t is not None else ph.launch_t + 60
        m = _between(fl, ph.launch_t - 10.0, end + 5.0)
        above = fl.altitude - ph.ground_altitude
        a1.plot(above[m], fl.pressure[m], "o", ms=3, label="measured")
        p0 = an.environment.get("ground_pressure_pa")
        if p0:
            h = np.linspace(min(0.0, float(np.nanmin(above[m]))), float(np.nanmax(above[m])), 50)
            a1.plot(h, p0 * (1 - 2.25577e-5 * h) ** 5.25588, "-", color="#e65100", label="standard atmosphere")
        a1.set_xlabel("altitude above pad (m)")
        a1.set_ylabel("pressure (Pa)")
        a1.ticklabel_format(axis="y", useOffset=False)
        a1.legend(fontsize=8)
        a1.set_title("Pressure vs altitude", fontsize=10)
        a2.plot(above[m], fl.temperature[m], "o", ms=3, color="#c62828")
        a2.set_xlabel("altitude above pad (m)")
        a2.set_ylabel("temperature (°C)")
        lapse = an.environment.get("temperature_lapse_K_per_km")
        a2.set_title("Temperature vs altitude" + (f"  ({lapse} K/km)" if lapse is not None else ""), fontsize=10)
    for ax in (a1, a2):
        ax.grid(alpha=0.3)
    fig.tight_layout()
    return fig


FIGURES = (
    ("01-altitude.png", plot_altitude),
    ("02-temperature.png", plot_temperature),
    ("03-pressure.png", plot_pressure),
    ("04-descent.png", plot_descent),
    ("05-acceleration.png", plot_acceleration),
    ("06-orientation.png", plot_orientation),
    ("07-sound.png", plot_sound),
    ("08-gps-track.png", plot_gps),
    ("09-correlations.png", plot_correlations),
)


def save_figures(an: Analysis, out_dir, full_session_too: bool = True) -> list[Path]:
    plt = _plt()
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    written = []
    for name, fn in FIGURES:
        fig = fn(an)
        path = out_dir / name
        fig.savefig(path, dpi=150)
        plt.close(fig)
        written.append(path)
    if full_session_too:
        fig = plot_altitude(an, full=True)
        fig.axes[0].set_title("Altitude against time — the whole power cycle")
        path = out_dir / "00-altitude-full-session.png"
        fig.savefig(path, dpi=150)
        plt.close(fig)
        written.insert(0, path)
    return written


# ==========================================================================================
# Summary
# ==========================================================================================

def _row(label, value, unit="") -> str:
    if value is None:
        return f"| {label} | — |"
    return f"| {label} | {value}{(' ' + unit) if unit else ''} |"


def summary_markdown(an: Analysis, figures: Optional[list[Path]] = None) -> str:
    q, ph, d, dy, env, g, snd = (an.quality, an.phases, an.descent, an.dynamics,
                                 an.environment, an.gps, an.sound)
    rel = lambda x: None if x is None else round(x, 2)  # noqa: E731
    lines = [
        "# Post-flight analysis",
        "",
        f"Source: `{q['source']}` ({q['format']}), team `{q['team']}`.",
        "",
        "## Data",
        "",
        "| | |", "|---|---|",
        _row("Samples", q["samples"]),
        _row("Packets", f"P-{q['first_packet']:03d} to P-{q['last_packet']:03d}"),
        _row("Missing packet numbers", f"{q['missing_packets']} ({q['loss_percent']} %)"),
        _row("Missing during the flight", q.get("missing_packets_in_flight")),
        _row("Rate during the flight", q.get("flight_rate_hz"), "Hz"),
        _row("Rows with GPS / with sound", f"{q['rows_with_gps']} / {q['rows_with_sound']}"),
        "",
        "## Phases",
        "",
        "Detected from barometric altitude with the firmware's own thresholds. The vehicle's "
        "declared `FLIGHT` comes on the climb and `LANDED` three seconds after rest, so neither "
        "is the physical event.",
        "",
        "| Event | Mission time (s) |", "|---|---|",
        _row("Launch (climb begins)", rel(ph.launch_t)),
        _row("Vehicle declared FLIGHT", rel(ph.flight_declared_t)),
        _row("Apex", f"{rel(ph.apex_t)} — {rel(ph.apex_altitude)} m above the pad" if ph.apex_t is not None else None),
        _row("Release", rel(ph.release_t)),
        _row("Landing", rel(ph.landing_t)),
        _row("Vehicle declared LANDED", rel(ph.landed_declared_t)),
        "",
        "## Descent",
        "",
        "| | |", "|---|---|",
        _row("Descent time", d.get("descent_time_s"), "s"),
        _row("Average rate, apex to ground", d.get("average_rate_mps"), "m/s"),
        _row("Release height (temperature-corrected)", d.get("release_height_m"), "m"),
        _row("Steady descent rate (regression, corrected)",
             f"{d['terminal_rate_mps']} ± {d['terminal_rate_stderr_mps']}" if "terminal_rate_mps" in d else None, "m/s"),
        _row("Same rate from the vehicle's own altitude",
             f"{d['terminal_rate_from_vehicle_altitude_mps']} ({d['vehicle_altitude_bias_percent']} %)"
             if "terminal_rate_from_vehicle_altitude_mps" in d else None, "m/s"),
        _row("Within the 5 m/s rulebook cap", {True: "**yes**", False: "**NO**"}.get(d.get("within_rulebook_cap"))),
        _row("Air density during descent", d.get("air_density_kgm3"), "kg/m³"),
        _row("Implied drag coefficient", d.get("implied_drag_coefficient")),
        _row("Model rate at the model's Cd", f"{d['model_rate_mps']} (Cd {d['model_drag_coefficient']})" if "model_rate_mps" in d else None, "m/s"),
        _row("Measured against model", d.get("rate_vs_model_percent"), "%"),
        "",
        "## Dynamics",
        "",
        f"*{dy.get('note_sampling', '')}*",
        "",
        "| | |", "|---|---|",
        _row("Peak |a| within 2 s of release", dy.get("max_accel_after_release_mps2"), "m/s²"),
        _row("Minimum |a| within 2 s of release (free fall)", dy.get("min_accel_after_release_mps2"), "m/s²"),
        _row("Peak |a| at landing", dy.get("max_accel_at_landing_mps2"), "m/s²"),
        _row("Roll / pitch spread in descent (1σ)",
             f"{dy['descent_roll_std_deg']} / {dy['descent_pitch_std_deg']}" if "descent_roll_std_deg" in dy else None, "°"),
        _row("Tilt in descent, mean / max",
             f"{dy['descent_tilt_mean_deg']} / {dy['descent_tilt_max_deg']}" if "descent_tilt_mean_deg" in dy else None, "°"),
        _row("Spin rate (relative yaw)", f"{dy['spin_rate_dps']} °/s = {dy['spin_rpm']} rpm" if "spin_rate_dps" in dy else None),
        _row("Pendulum frequency", f"{dy['pendulum_frequency_hz']} Hz (resolution {dy['pendulum_resolution_hz']} Hz, Nyquist {dy['nyquist_hz']} Hz)" if "pendulum_frequency_hz" in dy else None),
        "",
        "## Environment, GPS, sound",
        "",
        "| | |", "|---|---|",
        _row("Temperature range in flight", env.get("temperature_range_c"), "°C"),
        _row("Temperature lapse (see note)", env.get("temperature_lapse_K_per_km"), "K/km"),
        _row("Vehicle altitude ÷ corrected height (expected 288 K ÷ T)",
             f"{env['vehicle_altitude_scale']} (expected {env['vehicle_altitude_scale_expected']})"
             if "vehicle_altitude_scale" in env else None),
        _row("GPS fixes used", g.get("fixes")),
        _row("Drift, release to landing", f"{g['drift_m']} m toward {g['drift_bearing_deg']:.0f}°" if "drift_m" in g else None),
        _row("Landing position", g.get("landing_position")),
        _row("GPS height gain, pad to release", g.get("gps_height_gain_m"), "m"),
        _row("Sound: pad / lift / descent (median)",
             " / ".join(str(snd.get(k, "—")) for k in ("median_pad_mv", "median_lift_and_hover_mv", "median_descent_mv")), "mV"),
        _row("Sound peak after release / at landing",
             f"{snd.get('peak_after_release_mv', '—')} / {snd.get('peak_at_landing_mv', '—')}", "mV"),
        _row("Descent level vs speed², Pearson r", snd.get("descent_sound_vs_speed_squared_r")),
    ]
    if snd.get("note"):
        lines += ["", f"*Sound: {snd['note']}.*"]
    if env.get("altitude_note"):
        lines += ["", f"*Altitude: {env['altitude_note']}.*"]
    if env.get("lapse_note"):
        lines += ["", f"*Lapse rate: {env['lapse_note']}.*"]
    if an.comparison:
        c = an.comparison
        lines += ["", "## Onboard log against the ground station", "", "| | |", "|---|---|",
                  _row("Packets in the primary log", c["in_primary"]),
                  _row("Packets in the other log", c["in_other"]),
                  _row("Missing from the other", f"{c['missing_from_other']} ({c['missing_percent']} %)"),
                  _row("Missing during the flight", c.get("missing_during_flight")),
                  _row("Missing during the descent", c.get("missing_during_descent")),
                  _row("Largest altitude disagreement on shared packets", c.get("max_altitude_disagreement_m"), "m")]
    if q["notes"]:
        lines += ["", "## Notes on the input", ""] + [f"- {n}" for n in q["notes"]]
    if figures:
        lines += ["", "## Figures", ""] + [f"- `{p.name}`" for p in figures]
    lines += ["", "*Generated by `analysis/flight_analysis.py`. Every acceleration peak is a lower "
              "bound: the log is sampled once per packet. Yaw is relative — the IMU has no magnetometer.*", ""]
    return "\n".join(lines)


def run(log, out_dir, compare=None, mass_kg=None, canopy_diameter_m=None) -> Analysis:
    flight = load(log)
    other = load(compare) if compare else None
    an = analyse(flight, mass_kg, canopy_diameter_m, other)
    out_dir = Path(out_dir)
    figures = save_figures(an, out_dir)
    (out_dir / "summary.md").write_text(summary_markdown(an, figures), encoding="utf-8")
    (out_dir / "analysis.json").write_text(json.dumps(an.as_dict(), indent=2, default=str) + "\n",
                                           encoding="utf-8")
    return an


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description="CanSat 2026 post-flight analysis.")
    parser.add_argument("log", type=Path, help="SD log, ground-station CSV, or raw packet file")
    parser.add_argument("--compare", type=Path, default=None,
                        help="a second log of the same flight, e.g. the ground CSV beside the SD log")
    parser.add_argument("--out", type=Path, default=Path("analysis-output"))
    parser.add_argument("--mass", type=float, default=None, help="flight mass in kg, for the drag coefficient")
    parser.add_argument("--canopy-diameter", type=float, default=0.80, help="canopy flat diameter in m")
    args = parser.parse_args(argv)
    an = run(args.log, args.out, args.compare, args.mass, args.canopy_diameter)
    d = an.descent
    print(f"{an.quality['samples']} samples from {args.log} ({an.quality['format']})")
    if "terminal_rate_mps" in d:
        print(f"descent {d['descent_time_s']} s, steady rate {d['terminal_rate_mps']} m/s "
              f"({'within' if d['within_rulebook_cap'] else 'OVER'} the 5 m/s cap)")
    if "implied_drag_coefficient" in d:
        print(f"implied drag coefficient {d['implied_drag_coefficient']}")
    elif args.mass is None:
        print("pass --mass to get the drag coefficient the descent implies")
    print(f"figures and summary.md written to {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
