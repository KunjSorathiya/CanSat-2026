#!/usr/bin/env python3
"""Descent model for CanSat 2026: canopy sizing, descent time, and telemetry yield.

The rulebook caps the descent rate at 5 m/s and releases the vehicle from 100 ft. Those
two numbers together decide the size of the parachute, how long the descent lasts, and --
the part that is easy to miss -- how few telemetry packets the flight actually produces.
This module computes all three from first principles so the mechanical build starts from
an arithmetic answer instead of a guess at a canopy diameter.

Nothing here is a competition requirement beyond the 5 m/s cap and the 100 ft release.
Drag coefficients are canopy-type figures from the parachute literature, air density comes
from the gas law rather than a table, and every one of them is an input you can override.

Physics
-------
A body descending under a canopy reaches terminal velocity when weight balances drag:

    m g = 1/2 rho Cd S v^2      =>      v_t = sqrt(2 m g / (rho Cd S))

Solving instead for the area needed to hit a target rate:

    S = 2 m g / (rho Cd v_t^2)

and for a flat circular canopy of constructed area S, the diameter is D = sqrt(4 S / pi).

Descent time is *not* height / v_t. The vehicle starts at rest and accelerates into the
terminal rate, and over a 100 ft drop that transient is a real fraction of the fall. The
equation of motion m dv/dt = m g - 1/2 rho Cd S v^2 has a closed-form solution:

    v(t) = v_t tanh(g t / v_t)
    y(t) = (v_t^2 / g) ln cosh(g t / v_t)

which inverts exactly for the time to fall a given height:

    t(y) = (v_t / g) arccosh(exp(g y / v_t^2))

Both limits are the ones you would want: for small y it collapses to the free-fall
sqrt(2 y / g), and for large y to y / v_t plus a fixed start-up offset of v_t ln(2) / g.

`tests/test_descent.py` pins the module to those two limits and to the ISA sea-level air
density, so a change that breaks the physics fails the build rather than the launch.

Usage
-----
    python simulations/descent.py                       # the mission case, as a table
    python simulations/descent.py --mass 0.55           # the top of the mass tolerance
    python simulations/descent.py --temperature 35      # a hot launch day
    python simulations/descent.py --sweep               # every canopy type side by side
    python simulations/descent.py --format markdown     # paste into a document
"""

from __future__ import annotations

import argparse
import math
import sys
from dataclasses import dataclass

# ---- Physical constants -----------------------------------------------------------

# CODATA / ISO 80000 standard gravity. Surat is at ~13 deg N and near sea level, where the
# real value is within 0.1 % of this; the difference is far below the drag uncertainty.
STANDARD_GRAVITY = 9.80665

# Specific gas constant for dry air, J/(kg K). Humid air is *less* dense than dry air, so
# treating a monsoon-season launch as dry is the conservative direction: it over-estimates
# density, which under-estimates the canopy needed.
DRY_AIR_GAS_CONSTANT = 287.058

# ISA sea-level reference, used as the default atmosphere and as a test vector.
ISA_SEA_LEVEL_PRESSURE_PA = 101325.0
ISA_SEA_LEVEL_TEMPERATURE_C = 15.0

# ---- Rulebook constants ------------------------------------------------------------
# These three come from the 2026 guidelines and are the only figures in this file that are
# not an engineering choice. See documentation/requirements/requirements.md (REC-005,
# GEN-005, MIS-001).

# "Descent rate should be no more than 5 m/s" -- REC-005.
RULEBOOK_MAX_DESCENT_MPS = 5.0

# 100 ft, released from a drone -- MIS-001. Stated in feet, so the metric value is exact
# by definition of the international foot rather than rounded.
RELEASE_ALTITUDE_M = 30.48

# 500 g +/- 10 %, and exceeding it by more than 10 % is a disqualification -- GEN-005.
MASS_TARGET_KG = 0.500
MASS_TOLERANCE = 0.10

# ---- Canopy drag coefficients ------------------------------------------------------
#
# Cd for a parachute is meaningless without saying which area it is referenced to. Every
# figure below is referenced to the CONSTRUCTED (flat, laid-out) area of the canopy, which
# is the convention in the parachute literature and the one a builder can actually measure
# with a tape. A hemispherical canopy inflates to roughly two thirds of its flat diameter,
# so a Cd quoted against the *projected* area of the inflated shape is a much larger number
# describing the same parachute -- do not mix the two.
#
# The spread between these is the single largest uncertainty in this model: a flat sheet and
# a cruciform differ by a factor of ~1.1 in Cd, which is ~5 % on diameter. That is why the
# sweep exists, and why the recommendation is to build to the pessimistic end and measure.
CANOPY_TYPES: dict[str, tuple[float, str]] = {
    # A flat circular sheet with shroud lines at its edge -- the simplest thing to cut from
    # ripstop nylon or a plastic sheet, and what a first CanSat canopy usually is.
    "flat-circular": (0.80, "flat circular sheet, lines at the hem"),
    # The same sheet with a central vent cut in it. The vent costs a little drag and buys a
    # great deal of stability -- an unvented flat canopy oscillates, and REC-006 scores
    # descent stability.
    "vented-flat": (0.75, "flat circular with a central vent, ~10 % of diameter"),
    # Four panels in a cross. Markedly more stable than a flat circular and easy to sew.
    "cruciform": (0.85, "cruciform / cross canopy"),
    # A shaped canopy that inflates to a hemisphere. Best drag per unit of cloth, but it has
    # to be cut in gores and sewn, which is a different afternoon of work.
    "hemispherical": (1.40, "shaped hemispherical canopy, referenced to flat area"),
}

DEFAULT_CANOPY = "vented-flat"


# ---- Atmosphere ---------------------------------------------------------------------


def air_density(pressure_pa: float = ISA_SEA_LEVEL_PRESSURE_PA,
                temperature_c: float = ISA_SEA_LEVEL_TEMPERATURE_C) -> float:
    """Density of dry air, kg/m^3, from the ideal gas law.

    Both arguments are quantities the vehicle already measures. The BMP280 reports pressure
    in Pa and temperature in deg C in every telemetry packet, so a descent computed after a
    flight can use the air the vehicle was actually falling through rather than a standard
    atmosphere -- see `documentation/mission/concept-of-operations.md`.

    This matters more than it looks. A 35 deg C launch day is ~6.5 % less dense than the ISA
    15 deg C reference, which needs ~7 % more canopy area for the same descent rate.
    """
    if pressure_pa <= 0.0:
        raise ValueError("pressure must be positive")
    kelvin = temperature_c + 273.15
    if kelvin <= 0.0:
        raise ValueError("temperature must be above absolute zero")
    return pressure_pa / (DRY_AIR_GAS_CONSTANT * kelvin)


# ---- Canopy sizing --------------------------------------------------------------------


def terminal_velocity(mass_kg: float, area_m2: float, drag_coefficient: float,
                      density_kgm3: float, gravity: float = STANDARD_GRAVITY) -> float:
    """Steady-state descent rate of `mass_kg` under a canopy of constructed area `area_m2`."""
    if min(mass_kg, area_m2, drag_coefficient, density_kgm3) <= 0.0:
        raise ValueError("mass, area, drag coefficient and density must all be positive")
    return math.sqrt(2.0 * mass_kg * gravity / (density_kgm3 * drag_coefficient * area_m2))


def required_area(mass_kg: float, target_mps: float, drag_coefficient: float,
                  density_kgm3: float, gravity: float = STANDARD_GRAVITY) -> float:
    """Constructed canopy area, m^2, that brings `mass_kg` down at exactly `target_mps`."""
    if min(mass_kg, target_mps, drag_coefficient, density_kgm3) <= 0.0:
        raise ValueError("mass, target rate, drag coefficient and density must be positive")
    return 2.0 * mass_kg * gravity / (density_kgm3 * drag_coefficient * target_mps ** 2)


def circular_diameter(area_m2: float) -> float:
    """Diameter of a flat circular canopy of constructed area `area_m2`, in metres."""
    if area_m2 <= 0.0:
        raise ValueError("area must be positive")
    return math.sqrt(4.0 * area_m2 / math.pi)


def circular_area(diameter_m: float) -> float:
    """Constructed area of a flat circular canopy of diameter `diameter_m`, in m^2."""
    if diameter_m <= 0.0:
        raise ValueError("diameter must be positive")
    return math.pi * diameter_m ** 2 / 4.0


# ---- Descent kinematics ----------------------------------------------------------------


def fall_time(height_m: float, terminal_mps: float,
              gravity: float = STANDARD_GRAVITY) -> float:
    """Time to fall `height_m` from rest under a canopy already open, in seconds.

    Exact solution of m dv/dt = mg - k v^2, not height / terminal_mps. The difference is
    the start-up transient: the vehicle spends the first fraction of a second below its
    terminal rate, so a real descent takes slightly *longer* than the naive quotient.
    """
    if height_m < 0.0:
        raise ValueError("height must not be negative")
    if terminal_mps <= 0.0:
        raise ValueError("terminal velocity must be positive")
    if height_m == 0.0:
        return 0.0
    # arccosh(exp(x)) overflows for large x if evaluated literally. Above the point where
    # exp(x) would lose precision, arccosh(exp(x)) -> x + ln 2 to well under a microsecond,
    # so use the asymptote rather than an infinity.
    x = gravity * height_m / terminal_mps ** 2
    inner = x + math.log(2.0) if x > 300.0 else math.acosh(math.exp(x))
    return terminal_mps * inner / gravity


def velocity_at(time_s: float, terminal_mps: float,
                gravity: float = STANDARD_GRAVITY) -> float:
    """Descent rate `time_s` after the canopy opens, in m/s."""
    if time_s < 0.0:
        raise ValueError("time must not be negative")
    if terminal_mps <= 0.0:
        raise ValueError("terminal velocity must be positive")
    return terminal_mps * math.tanh(gravity * time_s / terminal_mps)


def free_fall_distance(time_s: float, gravity: float = STANDARD_GRAVITY) -> float:
    """Distance fallen in `time_s` with no canopy at all, in metres.

    Used for the deployment delay: between release and the canopy taking load, the vehicle
    is in something very close to free fall. Drag on the bare vehicle at those speeds is
    small enough that ignoring it errs on the side of a longer, faster fall.
    """
    if time_s < 0.0:
        raise ValueError("time must not be negative")
    return 0.5 * gravity * time_s ** 2


# ---- The whole descent ------------------------------------------------------------------


@dataclass(frozen=True)
class Descent:
    """One complete descent, from release to touchdown."""

    mass_kg: float
    canopy: str
    drag_coefficient: float
    density_kgm3: float
    area_m2: float
    diameter_m: float
    release_altitude_m: float
    deployment_delay_s: float
    free_fall_m: float
    canopy_fall_m: float
    terminal_mps: float
    canopy_time_s: float
    total_time_s: float
    impact_mps: float
    telemetry_period_ms: float
    packets_in_descent: int

    def compliant(self, limit_mps: float = RULEBOOK_MAX_DESCENT_MPS) -> bool:
        """Does the vehicle touch down at or under the rulebook's descent cap?"""
        return self.impact_mps <= limit_mps


def descend(mass_kg: float = MASS_TARGET_KG,
            diameter_m: float | None = None,
            target_mps: float = RULEBOOK_MAX_DESCENT_MPS,
            canopy: str = DEFAULT_CANOPY,
            drag_coefficient: float | None = None,
            pressure_pa: float = ISA_SEA_LEVEL_PRESSURE_PA,
            temperature_c: float = ISA_SEA_LEVEL_TEMPERATURE_C,
            release_altitude_m: float = RELEASE_ALTITUDE_M,
            deployment_delay_s: float = 0.0,
            telemetry_period_ms: float = 700.0,
            gravity: float = STANDARD_GRAVITY) -> Descent:
    """Model one descent end to end.

    Give `diameter_m` to evaluate a canopy you already have; leave it out to have the
    canopy sized for `target_mps` instead. `deployment_delay_s` is the time between release
    and the canopy taking load, during which the vehicle is treated as free-falling.
    """
    if canopy not in CANOPY_TYPES and drag_coefficient is None:
        raise ValueError(f"unknown canopy type {canopy!r}; "
                         f"choose from {', '.join(sorted(CANOPY_TYPES))} or pass a Cd")
    cd = drag_coefficient if drag_coefficient is not None else CANOPY_TYPES[canopy][0]
    density = air_density(pressure_pa, temperature_c)

    if diameter_m is None:
        area = required_area(mass_kg, target_mps, cd, density, gravity)
        diameter = circular_diameter(area)
    else:
        diameter = diameter_m
        area = circular_area(diameter)
    v_terminal = terminal_velocity(mass_kg, area, cd, density, gravity)

    if deployment_delay_s < 0.0:
        raise ValueError("deployment delay must not be negative")
    free_fall = free_fall_distance(deployment_delay_s, gravity)
    canopy_fall = release_altitude_m - free_fall
    if canopy_fall <= 0.0:
        raise ValueError("the vehicle reaches the ground before the canopy opens: "
                         f"{free_fall:.1f} m of free fall from {release_altitude_m:.1f} m")

    # The canopy phase is modelled from rest. That is conservative in the direction that
    # matters -- a vehicle already moving downward at deployment arrives sooner, not later,
    # and the impact rate is the terminal rate either way.
    canopy_time = fall_time(canopy_fall, v_terminal, gravity)
    total_time = deployment_delay_s + canopy_time
    impact = velocity_at(canopy_time, v_terminal, gravity)

    if telemetry_period_ms <= 0.0:
        raise ValueError("telemetry period must be positive")
    packets = int(total_time * 1000.0 / telemetry_period_ms)

    return Descent(mass_kg=mass_kg, canopy=canopy, drag_coefficient=cd,
                   density_kgm3=density, area_m2=area, diameter_m=diameter,
                   release_altitude_m=release_altitude_m,
                   deployment_delay_s=deployment_delay_s, free_fall_m=free_fall,
                   canopy_fall_m=canopy_fall, terminal_mps=v_terminal,
                   canopy_time_s=canopy_time, total_time_s=total_time,
                   impact_mps=impact, telemetry_period_ms=telemetry_period_ms,
                   packets_in_descent=packets)


# ---- Reporting ---------------------------------------------------------------------------


def _rows(d: Descent) -> list[tuple[str, str]]:
    return [
        ("Mass", f"{d.mass_kg * 1000:.0f} g"),
        ("Canopy", f"{d.canopy} (Cd {d.drag_coefficient:.2f}, constructed area)"),
        ("Air density", f"{d.density_kgm3:.4f} kg/m^3"),
        ("Constructed area", f"{d.area_m2:.4f} m^2"),
        ("Flat diameter", f"{d.diameter_m * 100:.1f} cm"),
        ("Terminal rate", f"{d.terminal_mps:.2f} m/s"),
        ("Release altitude", f"{d.release_altitude_m:.2f} m"),
        ("Deployment delay", f"{d.deployment_delay_s:.2f} s "
                             f"({d.free_fall_m:.2f} m of free fall)"),
        ("Descent time", f"{d.total_time_s:.2f} s"),
        ("Impact rate", f"{d.impact_mps:.2f} m/s"),
        ("Packets in descent", f"{d.packets_in_descent} at "
                               f"{1000.0 / d.telemetry_period_ms:.2f} Hz"),
        ("Rulebook <= 5 m/s", "yes" if d.compliant() else "NO"),
    ]


def format_text(d: Descent) -> str:
    width = max(len(label) for label, _ in _rows(d))
    lines = [f"{label.ljust(width)}  {value}" for label, value in _rows(d)]
    return "\n".join(lines)


def format_markdown(d: Descent) -> str:
    lines = ["| Quantity | Value |", "|---|---|"]
    lines += [f"| {label} | {value} |" for label, value in _rows(d)]
    return "\n".join(lines)


def sweep(mass_kg: float, target_mps: float, pressure_pa: float, temperature_c: float,
          release_altitude_m: float, telemetry_period_ms: float) -> list[Descent]:
    """One sized canopy per canopy type, so the cloth choice can be compared directly."""
    return [descend(mass_kg=mass_kg, target_mps=target_mps, canopy=name,
                    pressure_pa=pressure_pa, temperature_c=temperature_c,
                    release_altitude_m=release_altitude_m,
                    telemetry_period_ms=telemetry_period_ms)
            for name in CANOPY_TYPES]


def format_sweep(results: list[Descent], markdown: bool = False) -> str:
    header = ("Canopy", "Cd", "Diameter", "Area", "Rate", "Descent")
    rows = [(d.canopy, f"{d.drag_coefficient:.2f}", f"{d.diameter_m * 100:.1f} cm",
             f"{d.area_m2:.4f} m^2", f"{d.terminal_mps:.2f} m/s",
             f"{d.total_time_s:.2f} s")
            for d in results]
    if markdown:
        out = ["| " + " | ".join(header) + " |", "|" + "---|" * len(header)]
        out += ["| " + " | ".join(r) + " |" for r in rows]
        return "\n".join(out)
    widths = [max(len(header[i]), max(len(r[i]) for r in rows)) for i in range(len(header))]
    out = ["  ".join(h.ljust(w) for h, w in zip(header, widths))]
    out.append("  ".join("-" * w for w in widths))
    out += ["  ".join(c.ljust(w) for c, w in zip(r, widths)) for r in rows]
    return "\n".join(out)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Canopy sizing and descent model for CanSat 2026.")
    parser.add_argument("--mass", type=float, default=MASS_TARGET_KG,
                        help=f"flight mass in kg (default {MASS_TARGET_KG})")
    parser.add_argument("--diameter", type=float, default=None,
                        help="evaluate a canopy of this flat diameter in metres, "
                             "instead of sizing one")
    parser.add_argument("--target", type=float, default=RULEBOOK_MAX_DESCENT_MPS,
                        help=f"target descent rate in m/s (default {RULEBOOK_MAX_DESCENT_MPS})")
    parser.add_argument("--canopy", default=DEFAULT_CANOPY, choices=sorted(CANOPY_TYPES),
                        help=f"canopy type (default {DEFAULT_CANOPY})")
    parser.add_argument("--cd", type=float, default=None,
                        help="override the canopy's drag coefficient")
    parser.add_argument("--pressure", type=float, default=ISA_SEA_LEVEL_PRESSURE_PA,
                        help="ambient pressure in Pa (the vehicle measures this)")
    parser.add_argument("--temperature", type=float, default=ISA_SEA_LEVEL_TEMPERATURE_C,
                        help="ambient temperature in deg C (the vehicle measures this)")
    parser.add_argument("--release", type=float, default=RELEASE_ALTITUDE_M,
                        help=f"release altitude in m (default {RELEASE_ALTITUDE_M}, 100 ft)")
    parser.add_argument("--deployment-delay", type=float, default=0.0,
                        help="seconds of free fall before the canopy takes load")
    parser.add_argument("--telemetry-period", type=float, default=700.0,
                        help="telemetry period in ms (default 700, the flight profile)")
    parser.add_argument("--sweep", action="store_true",
                        help="size a canopy of every type and compare")
    parser.add_argument("--format", choices=("text", "markdown"), default="text")
    args = parser.parse_args(argv)

    try:
        if args.sweep:
            results = sweep(args.mass, args.target, args.pressure, args.temperature,
                            args.release, args.telemetry_period)
            print(format_sweep(results, markdown=args.format == "markdown"))
            return 0
        result = descend(mass_kg=args.mass, diameter_m=args.diameter,
                         target_mps=args.target, canopy=args.canopy,
                         drag_coefficient=args.cd, pressure_pa=args.pressure,
                         temperature_c=args.temperature, release_altitude_m=args.release,
                         deployment_delay_s=args.deployment_delay,
                         telemetry_period_ms=args.telemetry_period)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    print(format_markdown(result) if args.format == "markdown" else format_text(result))
    if not result.compliant():
        print(f"\nWARNING: {result.impact_mps:.2f} m/s exceeds the rulebook's "
              f"{RULEBOOK_MAX_DESCENT_MPS} m/s cap.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
