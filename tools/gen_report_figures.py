#!/usr/bin/env python3
"""Generate the figures the final project report embeds.

Every figure here is drawn from a number this repository already holds -- the descent
model, the mass budget, the simulation write-up, the link profile, the test suite -- and
none of them is typed in twice. Re-run after any of those move:

    python tools/gen_report_figures.py

The outputs land in documentation/project/figures/ and are committed, because the report
has to build on a machine that does not have matplotlib.
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "simulations"))
import descent  # noqa: E402

OUT = ROOT / "documentation" / "project" / "figures"
OUT.mkdir(parents=True, exist_ok=True)

# One palette, used everywhere, so the report reads as one document.
VEHICLE = "#0d47a1"
GROUND = "#00695c"
WARN = "#e65100"
BAD = "#b71c1c"
GOOD = "#1b5e20"
MUTED = "#546e7a"
INK = "#212121"

# Multi-line labels are assembled with this rather than escapes, so the long guard
# strings below stay readable at one line per clause.
NL = chr(10)

plt.rcParams.update({
    "font.size": 9,
    "axes.titlesize": 10,
    "axes.titleweight": "bold",
    "axes.labelsize": 9,
    "axes.edgecolor": MUTED,
    "axes.labelcolor": INK,
    "text.color": INK,
    "xtick.color": MUTED,
    "ytick.color": MUTED,
    "figure.dpi": 200,
    "savefig.bbox": "tight",
    "savefig.facecolor": "white",
})


def save(fig, name: str) -> None:
    path = OUT / name
    fig.savefig(path)
    plt.close(fig)
    print(f"  {path.relative_to(ROOT)}")


def fig_mission_profile() -> None:
    """Altitude against time since arming, with the state transitions marked.

    The submitted image arms only after its five-minute command window, which cannot be drawn
    at the scale of a 15-second flight, so the clock starts at arming. The lift and hover are
    the concept of operations' assumed profile (10 s climb, 20 s hover). The descent is the
    model for a 500 g vehicle -- the submitted mass is inside the band but not recorded --
    counted at the max-rate pattern's mean packet interval, which is what the flight uses.
    """
    mean_packet_ms = 966.0 / 3.0   # kMaxRateCycleMs over one rich and two lean slots
    r = descent.descend(mass_kg=0.500, diameter_m=0.80, telemetry_period_ms=mean_packet_ms)
    climb_s, hover_s, pad_s = 10.0, 20.0, 8.0
    release_t = pad_s + climb_s + hover_s

    ts, alts = [], []
    step = 0.05
    t = 0.0
    while t <= pad_s:
        ts.append(t); alts.append(0.0); t += step
    while t <= pad_s + climb_s:
        ts.append(t); alts.append(30.48 * (t - pad_s) / climb_s); t += step
    while t <= release_t:
        ts.append(t); alts.append(30.48); t += step
    # The canopy fall, from the model's own closed form rather than height / rate:
    # x(t) = (v^2/g) ln cosh(g t / v), the integral of descent.velocity_at.
    n = int(r.total_time_s / step)
    g = descent.STANDARD_GRAVITY
    v = r.terminal_mps
    for i in range(n + 1):
        dt = i * step
        fallen = (v * v / g) * math.log(math.cosh(g * dt / v))
        ts.append(release_t + dt)
        alts.append(max(0.0, 30.48 - fallen))
    land_t = ts[-1]
    while t <= land_t + 12.0:
        t = ts[-1] + step
        ts.append(t); alts.append(0.0)
        if t > land_t + 12.0:
            break

    fig, ax = plt.subplots(figsize=(7.2, 3.4))
    ax.plot(ts, alts, color=VEHICLE, lw=2.0)
    ax.fill_between(ts, alts, color=VEHICLE, alpha=0.08)

    ax.axhline(30.48, color=MUTED, ls=":", lw=0.9)
    ax.text(0.4, 31.4, "release altitude 30.48 m (100 ft)", color=MUTED, fontsize=7.5)
    ax.axhline(15.0, color=WARN, ls=":", lw=0.9)
    ax.text(0.4, 15.9, "launch-detect threshold 15 m", color=WARN, fontsize=7.5)

    # Staggered on two rows: several of these transitions are seconds apart and their
    # labels are wider than the gap.
    # Staggered on two rows: several of these transitions are seconds apart and their
    # labels are wider than the gap.
    marks = [
        (0.0, "armed\nST-R11", 0, "left"),
        (pad_s + climb_s * 15.0 / 30.48, "FLIGHT", 0, "center"),
        (release_t, "release", 1, "center"),
        (land_t, "LANDED", 0, "center"),
        (land_t + 5.0, "RECOVERY", 1, "right"),
    ]
    for x, label, row, ha in marks:
        ax.axvline(x, color=MUTED, lw=0.6, alpha=0.5)
        ax.annotate(label, xy=(x, 0), xytext=(x, -4.0 - 6.2 * row), fontsize=7,
                    ha=ha, va="top", color=INK, linespacing=1.25)

    ax.annotate("", xy=(land_t, 3.0), xytext=(release_t, 3.0),
                arrowprops=dict(arrowstyle="<->", color=GOOD, lw=1.2))
    ax.text(release_t - 1.5, 6.0,
            f"descent {r.total_time_s:.2f} s at {r.terminal_mps:.2f} m/s\n"
            f"~{r.packets_in_descent} packets at 3.11 Hz",
            ha="right", fontsize=7.5, color=GOOD, linespacing=1.3)

    ax.set_xlabel("time since arming (s) — arming follows the command window, up to five minutes after power-on")
    ax.set_ylabel("altitude above the pad (m)")
    ax.set_title("Flight profile after arming — 500 g under the fitted 80 cm canopy")
    ax.set_ylim(-19, 36)
    ax.set_xlim(-1, land_t + 13)
    ax.set_yticks([0, 10, 20, 30])
    ax.spines[["top", "right"]].set_visible(False)
    save(fig, "fig-01-mission-profile.png")


def fig_mass_budget() -> None:
    """From the last scale reading to the submitted vehicle.

    The 280 g stack is measured. Everything added after it -- canopy, switch, LED, ballast --
    went in before submission and the result is reported inside the band, but no final
    number is on record. The figure draws that honestly: measured bars solid, reported
    additions hatched, and the submitted mass as a bracket over the band, not a bar.
    """
    pcb, battery = 110.573, 40.726
    structure = 280.0 - pcb - battery
    parts_lo, parts_hi = 35.0, 65.0      # canopy and harness 30-55 g, switch/LED/divider 5-10 g

    fig, ax = plt.subplots(figsize=(7.2, 3.0))

    ax.axvspan(450, 550, color=GOOD, alpha=0.10, zorder=0)
    ax.axvline(450, color=GOOD, lw=1.4)
    ax.axvline(550, color=GOOD, lw=1.4)
    ax.axvline(500, color=GOOD, lw=0.9, ls=":")
    ax.text(450, 1.66, "450 g", ha="center", fontsize=8, color=GOOD, weight="bold")
    ax.text(550, 1.66, "550 g", ha="center", fontsize=8, color=GOOD, weight="bold")
    ax.text(500, 1.66, "500 g", ha="center", fontsize=7.5, color=GOOD)

    segs = [
        ("vehicle PCB", pcb, VEHICLE),
        ("battery", battery, "#1565c0"),
        ("printed structure" + NL + "+ egg chamber", structure, "#5c6bc0"),
    ]
    left = 0.0
    for label, w, colour in segs:
        ax.barh(0.75, w, left=left, height=0.34, color=colour, edgecolor="white", lw=1.0)
        ax.text(left + w / 2, 0.75, f"{w:.1f}", ha="center", va="center",
                color="white", fontsize=7.5, weight="bold")
        ax.text(left + w / 2, 0.50, label, ha="center", va="top", fontsize=7,
                color=INK, linespacing=1.2)
        left += w

    ax.plot([280], [0.75], marker="|", color=INK, ms=14, mew=1.6)
    ax.text(272, 0.28, "280 g" + NL + "last scale reading", ha="center", va="top", fontsize=7.2,
            color=INK, weight="bold", linespacing=1.2)

    # Reported additions: canopy, switch, LED, divider -- then ballast into the band. The
    # ballast amount is not recorded, so its bar runs to the band as a range, not a number.
    ax.barh(0.75, parts_hi, left=280.0, height=0.34, color=WARN, alpha=0.55,
            edgecolor="white", lw=1.0, hatch="///")
    ax.text(280 + parts_hi / 2, 0.96, "canopy, switch," + NL + "LED, divider", ha="center",
            va="bottom", fontsize=6.8, color=WARN, linespacing=1.15)
    ax.barh(0.75, 450 - (280 + parts_lo), left=280 + parts_lo, height=0.34, color=MUTED,
            alpha=0.25, edgecolor=MUTED, lw=0.8, hatch="..", zorder=1)
    ax.text(395, 0.50, "ballast" + NL + "(amount not recorded)", ha="center", va="top",
            fontsize=7, color=MUTED, linespacing=1.2)

    ax.annotate("", xy=(450, 1.30), xytext=(550, 1.30),
                arrowprops=dict(arrowstyle="|-|", color=GOOD, lw=1.4, mutation_scale=4))
    ax.text(500, 1.38, "submitted: inside the band" + NL + "(reported by the team)",
            ha="center", va="bottom", fontsize=7.5, color=GOOD, weight="bold",
            linespacing=1.2)

    ax.set_xlim(0, 600)
    ax.set_ylim(0.0, 1.85)
    ax.set_yticks([])
    ax.set_xlabel("mass (g)")
    ax.set_title("Mass budget — from the last scale reading to the submitted vehicle")
    ax.spines[["top", "right", "left"]].set_visible(False)
    save(fig, "fig-02-mass-budget.png")


def fig_descent_vs_mass() -> None:
    """Why the 80 cm canopy does not have to be re-sized whatever the ballast decision is.

    Descent rate and descent time against flight mass, for the canopy already specified,
    on both an ISA day and the 35 C day the canopy was sized against. The rulebook cap is
    a ceiling on rate; section C scores descent time comparatively. The two panels
    therefore pull in opposite directions, and both matter.
    """
    grams = list(range(250, 601, 5))
    curves = [
        ("ISA, 15 °C", 15.0, VEHICLE, "-"),
        ("hot day, 35 °C — the sizing case", 35.0, WARN, "--"),
    ]

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(7.2, 3.2))
    for label, temp_c, colour, style in curves:
        runs = [descent.descend(mass_kg=g_ / 1000.0, diameter_m=0.80, temperature_c=temp_c)
                for g_ in grams]
        ax1.plot(grams, [r.terminal_mps for r in runs], color=colour, lw=1.9,
                 ls=style, label=label)
        ax2.plot(grams, [r.total_time_s for r in runs], color=colour, lw=1.9,
                 ls=style, label=label)

    ax1.axhline(5.0, color=BAD, lw=1.3, ls=":")
    ax1.text(256, 5.08, "rulebook cap 5 m/s", color=BAD, fontsize=7.5)

    # The three cases the mechanical page tabulates, each on the curve it belongs to.
    cases = [
        (315, 15.0, "unballasted", MUTED, ((-10, -6), "right"), ((10, 4), "left")),
        (450, 15.0, "band floor", GOOD, ((10, -16), "left"), ((-10, -16), "right")),
        (550, 35.0, "sizing case", WARN, ((-7, 9), "right"), ((-8, -22), "right")),
    ]
    for panel, (ax, value_of, fmt) in enumerate(((ax1, lambda r: r.terminal_mps, "{:.2f} m/s"),
                                                 (ax2, lambda r: r.total_time_s, "{:.2f} s"))):
        for g_, temp_c, label, colour, *placements in cases:
            offset, ha = placements[panel]
            r = descent.descend(mass_kg=g_ / 1000.0, diameter_m=0.80, temperature_c=temp_c)
            y = value_of(r)
            ax.plot([g_], [y], "o", color=colour, ms=5, zorder=5)
            ax.annotate(f"{label}\n{fmt.format(y)}", xy=(g_, y), xytext=offset,
                        textcoords="offset points", ha=ha, fontsize=7,
                        color=colour, linespacing=1.2, zorder=6)
        ax.axvspan(450, 550, color=GOOD, alpha=0.08, zorder=0)
        ax.set_xlabel("flight mass (g)")
        ax.set_xlim(250, 600)
        ax.spines[["top", "right"]].set_visible(False)

    ax1.set_ylabel("terminal descent rate (m/s)")
    ax1.set_title("Descent rate")
    ax1.set_ylim(2.9, 5.7)
    ax2.set_ylabel("descent time from 30.48 m (s)")
    ax2.set_title("Descent time  (longer scores better)")
    ax2.set_ylim(5.6, 10.6)
    ax2.legend(fontsize=7, frameon=False, loc="upper right", bbox_to_anchor=(1.02, 1.04))
    ax1.text(452, 3.02, "450–550 g band", fontsize=7, color=GOOD)

    fig.suptitle("The fitted 80 cm canopy across the band the vehicle was ballasted into",
                 fontsize=10, weight="bold", y=1.02)
    save(fig, "fig-03-descent-vs-mass.png")


def fig_safety_factors() -> None:
    """The three static-stress studies, and what anisotropy derating does to them."""
    studies = ["1 – horizontal\n30 N on +Z", "2 – tearing\n30 N on −X", "3 – impact\n100 N on −X"]
    yield_derived = [18.9, 40.9, 23.2]
    derate_40 = [round(v * 0.40, 1) for v in yield_derived]
    derate_70 = [round(v * 0.70, 1) for v in yield_derived]

    fig, ax = plt.subplots(figsize=(7.2, 3.1))
    x = range(len(studies))
    w = 0.26
    ax.bar([i - w for i in x], yield_derived, w, label="isotropic (yield / peak stress)",
           color=VEHICLE)
    ax.bar(list(x), derate_70, w, label="derated x0.70 (best-case interlayer)", color="#5c6bc0")
    ax.bar([i + w for i in x], derate_40, w, label="derated x0.40 (worst-case interlayer)",
           color=WARN)

    for i, (a, b, c) in enumerate(zip(yield_derived, derate_70, derate_40)):
        for off, v in ((-w, a), (0.0, b), (w, c)):
            ax.text(i + off, v + 0.7, f"{v:g}", ha="center", fontsize=7.5, color=INK)

    ax.axhline(1.0, color=BAD, lw=1.4)
    ax.text(2.42, 1.6, "failure, SF = 1", color=BAD, fontsize=7.5, ha="right")
    ax.axhline(15.0, color=MUTED, ls=":", lw=1.0)
    ax.text(-0.44, 15.6, "Fusion's display cap, 15", color=MUTED, fontsize=7.5)

    ax.set_xticks(list(x))
    ax.set_xticklabels(studies, fontsize=8, linespacing=1.3)
    ax.set_ylabel("minimum safety factor")
    ax.set_ylim(0, 52)
    ax.set_title("Structural margin — and what a printed part costs it")
    ax.legend(fontsize=7.2, frameon=False, loc="upper right", bbox_to_anchor=(1.0, 1.0))
    ax.spines[["top", "right"]].set_visible(False)
    save(fig, "fig-04-safety-factors.png")


def _box(ax, x, y, w, h, text, face, fg="white", fs=8, weight="bold", radius=0.10):
    ax.add_patch(FancyBboxPatch((x - w / 2, y - h / 2), w, h,
                                boxstyle=f"round,pad=0.012,rounding_size={radius}",
                                facecolor=face, edgecolor=face, linewidth=1.0, zorder=2))
    ax.text(x, y, text, ha="center", va="center", color=fg, fontsize=fs,
            weight=weight, zorder=3, linespacing=1.3)


def _arrow(ax, p0, p1, label=None, colour=MUTED, rad=0.0, fs=6.8, lx=0.0, ly=0.0,
           style="-|>"):
    ax.add_patch(FancyArrowPatch(p0, p1, arrowstyle=style, mutation_scale=11,
                                 color=colour, lw=1.1, zorder=1,
                                 connectionstyle=f"arc3,rad={rad}",
                                 shrinkA=2, shrinkB=3))
    if label:
        mx, my = (p0[0] + p1[0]) / 2 + lx, (p0[1] + p1[1]) / 2 + ly
        ax.text(mx, my, label, ha="center", va="center", fontsize=fs, color=colour,
                zorder=4, linespacing=1.25,
                bbox=dict(boxstyle="round,pad=0.16", fc="white", ec="none", alpha=0.92))


def fig_state_machine() -> None:
    """The mission state machine, with the guard on every transition.

    Redrawn from StateMachine rather than described, because the guards are the design:
    two of them exist only because a document or a test found a way into the wrong state
    (F-20, and the arming delay).
    """
    fig, ax = plt.subplots(figsize=(7.2, 5.4))
    ax.set_xlim(0, 11)
    ax.set_ylim(0, 10.4)
    ax.axis("off")

    w, h = 1.72, 0.66
    top = 8.15
    states = {
        "INIT": (1.15, top, VEHICLE),
        "SELF_TEST": (3.45, top, VEHICLE),
        "READY": (5.95, top, VEHICLE),
        "FLIGHT": (9.05, top, GOOD),
        "LANDED": (9.05, 6.15, GOOD),
        "RECOVERY": (9.05, 4.20, GROUND),
        "FAULT": (3.45, 4.20, BAD),
    }
    for name, (x, y, colour) in states.items():
        _box(ax, x, y, w, h, name, colour, fs=8.5)

    # ---- the nominal path, left to right then down ----
    _arrow(ax, (1.15 + w / 2, top), (3.45 - w / 2, top))
    _arrow(ax, (3.45 + w / 2, top), (5.95 - w / 2, top))
    ax.text(4.42, top - 0.52, "mandatory" + NL + "sensors OK", ha="center", va="top",
            fontsize=6.8, color=MUTED, linespacing=1.25)

    _arrow(ax, (5.95 + w / 2, top), (9.05 - w / 2, top), colour=GOOD)
    ax.text(7.50, top + 0.52,
            "ARMED (3 s delay AND calibration settled), then" + NL
            + "boost >30 m/s² or climb >15 m, held 300 ms",
            ha="center", va="bottom", fontsize=6.8, color=GOOD, linespacing=1.3)

    _arrow(ax, (9.05, top - h / 2), (9.05, 6.15 + h / 2), colour=GOOD)
    # Kept short on purpose: the full guard is spelled out in the notes box, and a long
    # label here is crossed by the FLIGHT -> FAULT arrow.
    ax.text(8.72, 6.95, "descent gate," + NL + "then at rest 3 s",
            ha="right", va="center", fontsize=6.8, color=GOOD, linespacing=1.3,
            bbox=dict(boxstyle="round,pad=0.2", fc="white", ec="none", alpha=0.94))

    _arrow(ax, (9.05, 6.15 - h / 2), (9.05, 4.20 + h / 2), colour=GROUND)
    ax.text(8.72, 5.18, "5 s post-impact" + NL + "window elapsed", ha="right",
            va="center", fontsize=6.8, color=GROUND, linespacing=1.3)

    # ---- the fault path: three ways in, and it is not an exit ----
    _arrow(ax, (3.45, top - h / 2), (3.45, 4.20 + h / 2), colour=BAD)
    ax.text(3.20, 6.20, "self-test" + NL + "failed", ha="right", va="center",
            fontsize=6.8, color=BAD, linespacing=1.3)

    _arrow(ax, (5.95 - w / 2, top - h / 3), (3.45 + w / 2, 4.20 + h / 3),
           colour=BAD, rad=0.22)
    _arrow(ax, (9.05 - w / 2, top - h / 2.2), (3.45 + w / 2, 4.20 + h / 8),
           colour=BAD, rad=0.16)
    ax.text(5.95, 5.35, "critical fault" + NL + "from READY or FLIGHT", ha="center",
            va="center", fontsize=6.8, color=BAD, linespacing=1.3,
            bbox=dict(boxstyle="round,pad=0.2", fc="white", ec="none", alpha=0.94))

    ax.text(3.45, 3.42,
            "FAULT stops state progression." + NL
            + "It never stops transmission.",
            ha="center", va="top", fontsize=7.2, color=BAD, style="italic",
            linespacing=1.35)

    notes = (
        "Three guards, and each exists because something got past the obvious test:" + NL
        + "• READY → FLIGHT refuses to fire until the vehicle is ARMED, so a startup"
        + " glitch cannot launch it." + NL
        + "• FLIGHT → LANDED refuses to fire until a real descent has been seen. A drone"
        + " hover answers" + NL
        + "   every other landing test, and used to declare a landing 12 s before release"
        + " (F-20)." + NL
        + "• RECOVERY is terminal. The machine never returns to FLIGHT."
    )
    ax.text(0.30, 1.28, notes, ha="left", va="center", fontsize=7.0, color=INK,
            linespacing=1.55,
            bbox=dict(boxstyle="round,pad=0.55", fc="#f5f5f5", ec="#cfd8dc"))

    ax.set_title("Mission state machine — every transition and the guard on it",
                 fontsize=10.5, weight="bold", y=1.0)
    save(fig, "fig-05-state-machine.png")


def fig_architecture() -> None:
    """The whole system on one page: vehicle, link, ground, and the three interfaces."""
    fig, ax = plt.subplots(figsize=(7.2, 4.6))
    ax.set_xlim(0, 12)
    ax.set_ylim(-0.95, 9.0)
    ax.axis("off")

    # ---- vehicle ----
    ax.add_patch(FancyBboxPatch((0.25, 0.62), 4.9, 7.20,
                                boxstyle="round,pad=0.05,rounding_size=0.18",
                                facecolor="#e8eaf6", edgecolor=VEHICLE, lw=1.2, zorder=0))
    ax.text(2.70, 8.16, "CanSat — vehicle", ha="center", fontsize=9.5,
            weight="bold", color=VEHICLE)

    sensors = [
        ("MPU-6500" + NL + "accel + gyro" + NL + "I2C0 @ 0x68", 5.90),
        ("BMP280" + NL + "pressure + temp" + NL + "I2C0 @ 0x76", 4.62),
        ("NEO-6M GNSS" + NL + "UART0 @ 9600", 3.40),
        ("LM393 mic" + NL + "ADC1 / GP27", 2.30),
    ]
    for text, y in sensors:
        _box(ax, 1.42, y, 1.95, 0.84, text, "#5c6bc0", fs=6.5, weight="normal")
        _arrow(ax, (2.42, y), (3.45, 4.30), colour=VEHICLE)

    _box(ax, 4.12, 4.30, 1.58, 1.50,
         "Raspberry" + NL + "Pi Pico" + NL + "RP2040" + NL + "" + NL + "flight" + NL + "computer", VEHICLE, fs=6.6)
    _box(ax, 4.12, 1.60, 1.32, 0.68, "microSD" + NL + "raw block log", "#455a64", fs=6.4,
         weight="normal")
    _arrow(ax, (4.12, 3.55), (4.12, 1.96), colour=VEHICLE)
    ax.text(4.32, 2.72, "SPI0", fontsize=6.2, color=MUTED)

    _box(ax, 4.12, 6.62, 1.32, 0.60, "SX1278" + NL + "RA-02", "#4527a0", fs=6.8)
    _arrow(ax, (4.12, 5.05), (4.12, 6.30), colour=VEHICLE)
    ax.text(4.32, 5.68, "SPI0", fontsize=6.2, color=MUTED)

    # ---- the link ----
    _arrow(ax, (4.80, 6.74), (7.16, 6.74), colour="#4527a0")
    ax.text(5.98, 7.02,
            "433 MHz LoRa · SF7 / 125 kHz / CR 4-5" + NL + ""
            "sync 0xA5 · ≤ 200 B · 1.43 Hz",
            ha="center", va="bottom", fontsize=6.7, color="#4527a0", linespacing=1.3,
            bbox=dict(boxstyle="round,pad=0.18", fc="white", ec="none", alpha=0.9))
    _arrow(ax, (7.16, 6.42), (4.80, 6.42), colour=MUTED)
    ax.text(5.98, 6.26, "uplink: one MAX_RATE command, pre-arm window only",
            ha="center", va="top", fontsize=6.3, color=MUTED,
            bbox=dict(boxstyle="round,pad=0.16", fc="white", ec="none", alpha=0.9))

    # ---- ground ----
    ax.add_patch(FancyBboxPatch((6.88, 0.62), 4.9, 7.20,
                                boxstyle="round,pad=0.05,rounding_size=0.18",
                                facecolor="#e0f2f1", edgecolor=GROUND, lw=1.2, zorder=0))
    ax.text(9.33, 8.16, "Ground station", ha="center", fontsize=9.5,
            weight="bold", color=GROUND)

    _box(ax, 7.86, 6.62, 1.32, 0.60, "SX1278" + NL + "RA-02", "#4527a0", fs=6.8)
    _box(ax, 9.33, 5.15, 2.95, 0.74, "Raspberry Pi Pico" + NL + "USB bridge — CRC framing",
         GROUND, fs=6.4)
    _box(ax, 9.33, 3.62, 3.05, 1.02,
         "Python pipeline" + NL + "transport → CRC → parse →" + NL + "validate → health → log",
         "#00838f", fs=6.6)
    _arrow(ax, (7.86, 6.32), (8.60, 5.52), colour=GROUND)
    _arrow(ax, (9.33, 4.78), (9.33, 4.13), colour=GROUND)
    ax.text(9.52, 4.45, "USB serial", fontsize=6.2, color=MUTED)

    for x, text in ((7.78, "web console" + NL + "single file"),
                    (9.33, "Tk dashboard" + NL + "live numeric"),
                    (10.88, "CSV / TSV" + NL + "export")):
        _box(ax, x, 1.85, 1.42, 0.78, text, "#26a69a", fs=6.4, weight="normal")
        _arrow(ax, (9.33, 3.11), (x, 2.24), colour=GROUND)

    ax.text(6.0, -0.88,
            "The flight core compiles and runs on a laptop: hardware reaches it only through six "
            "abstract interfaces," + NL + ""
            "which is why 5485 C++ assertions can exercise the mission logic with no Pico present.",
            ha="center", va="bottom", fontsize=6.8, color=INK, style="italic",
            linespacing=1.4)

    ax.set_title("System architecture — two Picos, two identical radios, one protocol",
                 fontsize=10.5, weight="bold", y=1.0)
    save(fig, "fig-06-architecture.png")


def fig_verification() -> None:
    """What is actually verified, and by what -- host tests against hardware gates.

    The point of the pairing is that the left bar is large and cheap and the right one is
    small and expensive, and only the right one is evidence about a vehicle.
    """
    suites = [
        ("flight_tests\n143 C++ suites", 4674, VEHICLE),
        ("sd_card_tests", 613, VEHICLE),
        ("sx1278_tests", 168, VEHICLE),
        ("fat_volume_tests", 30, VEHICLE),
        ("Python ground station", 143, GROUND),
        ("Web console (Node)", 71, GROUND),
        ("Python tooling", 49, "#00838f"),
        ("Python simulations", 40, "#00838f"),
        ("Documented claims", 303, MUTED),
    ]
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(7.4, 3.5),
                                   gridspec_kw={"width_ratios": [1.0, 1.15]})

    names = [s[0] for s in suites][::-1]
    vals = [s[1] for s in suites][::-1]
    colours = [s[2] for s in suites][::-1]
    ax1.barh(range(len(names)), vals, color=colours, height=0.66)
    for i, v in enumerate(vals):
        ax1.text(v + 90, i, f"{v:,}", va="center", fontsize=7.2, color=INK)
    ax1.set_yticks(range(len(names)))
    ax1.set_yticklabels(names, fontsize=7.2, linespacing=1.2)
    ax1.set_xlim(0, 5600)
    ax1.set_xlabel("assertions / tests passing")
    ax1.set_title("On the host — 6091 checks, no hardware", fontsize=9.5)
    ax1.spines[["top", "right"]].set_visible(False)

    # ---- the gates, which is the honest half ----
    # Statuses are the ones recorded in documentation/project/timeline.md, not a
    # percentage. Nobody has measured a gate as "70 % done" and inventing one here would
    # be exactly the kind of number this project refuses to print.
    gates = [
        ("1  requirements locked", "partial", "6 organizer questions open"),
        ("2  electrical architecture", "partial", "built; Schottky never fitted"),
        ("3  power system tested", "partial", "rail measured; no power-cycle test"),
        ("4  sensors verified", "partial", "no GPS fix outdoors"),
        ("5  telemetry verified", "partial", "66-packet link, not 500"),
        ("6  ground station verified", "partial", "official station untested"),
        ("7  mechanical + recovery", "partial", "built, canopy fitted; never dropped"),
        ("8  full integration", "partial", "range test; no full rehearsal"),
        ("9  competition readiness", "submitted", "submitted 2026-09-14; launch ahead"),
    ]
    fill = {"partial": WARN, "none": "#90a4ae", "submitted": GOOD}
    word = {"partial": "PARTIAL", "none": "NOT STARTED", "submitted": "SUBMITTED"}
    labels = [g[0] for g in gates][::-1]
    for i, (name, status, note) in enumerate(gates[::-1]):
        ax2.barh(i, 1.0, color=fill[status], height=0.66)
        ax2.text(1.06, i + 0.16, word[status], va="center", fontsize=6.6,
                 color=fill[status], weight="bold")
        ax2.text(1.06, i - 0.20, note, va="center", fontsize=6.2, color=MUTED)
    ax2.set_yticks(range(len(labels)))
    ax2.set_yticklabels(labels, fontsize=7.2)
    ax2.set_xlim(0, 4.6)
    ax2.set_xticks([])
    ax2.set_xlabel("recorded gate status")
    ax2.set_title("On hardware — submitted, no gate closed", fontsize=9.5)
    ax2.spines[["top", "right", "bottom"]].set_visible(False)

    fig.text(0.5, -0.06,
             "A passing test suite is not flight verification. Nothing in the left panel "
             "needs a vehicle;" + NL + ""
             "nothing in the right panel is closed, because the evidence for the last gate is the launch.",
             ha="center", fontsize=7.2, color=INK, style="italic", linespacing=1.4)
    fig.tight_layout()
    save(fig, "fig-07-verification.png")


def fig_packet_budget() -> None:
    """Where 700 ms goes, and where the max-rate pattern goes instead."""
    fig, ax = plt.subplots(figsize=(7.2, 2.7))

    rows = [
        (2.0, "Normal flight — 1.43 Hz", [(323.4, "rich 200 B · 323 ms", "#4527a0"),
                                          (376.6, "idle 377 ms", "#eceff1")], 700.0),
        (1.0, "After MAX_RATE — 3.11 Hz", [(323.4, "rich 323", "#4527a0"),
                                           (50.6, "", "#eceff1"),
                                           (245.2, "lean 245", "#7e57c2"),
                                           (50.8, "", "#eceff1"),
                                           (245.2, "lean 245", "#7e57c2"),
                                           (50.8, "", "#eceff1")], 966.0),
    ]
    for y, label, segs, total in rows:
        left = 0.0
        for w, text, colour in segs:
            ax.barh(y, w, left=left, height=0.42, color=colour,
                    edgecolor="white", lw=0.8)
            if text:
                ax.text(left + w / 2, y, text, ha="center", va="center", fontsize=6.6,
                        color="white" if colour != "#eceff1" else MUTED,
                        weight="bold" if colour != "#eceff1" else "normal")
            left += w
        ax.text(-14, y, label, ha="right", va="center", fontsize=7.6, weight="bold",
                color=INK)
        ax.text(total + 12, y, f"cycle {total:.0f} ms", ha="left", va="center",
                fontsize=7.0, color=MUTED)

    ax.axvline(500, color=BAD, ls=":", lw=1.1)
    ax.text(505, 2.45, "50 % duty cap at the 700 ms period", fontsize=6.8, color=BAD)
    ax.text(0, 0.42,
            "Each 50 ms gap is doing two jobs: one worst-case SD block write (30 ms, F-11) "
            "and the ~35 ms the" + NL + ""
            "organizers' ESP32 receiver spends deaf while it prints the packet it just heard.",
            fontsize=6.9, color=INK, style="italic", va="top", linespacing=1.4)

    ax.set_xlim(-320, 1120)
    ax.set_ylim(0.0, 2.75)
    ax.set_yticks([])
    ax.set_xlabel("time within one transmit cycle (ms)")
    ax.set_xticks([0, 200, 400, 600, 800, 1000])
    ax.set_title("Airtime budget — measured, not modelled")
    ax.spines[["top", "right", "left"]].set_visible(False)
    save(fig, "fig-08-packet-budget.png")


FIGURES = (
    fig_mission_profile,
    fig_mass_budget,
    fig_descent_vs_mass,
    fig_safety_factors,
    fig_state_machine,
    fig_architecture,
    fig_verification,
    fig_packet_budget,
)


def main() -> int:
    print(f"writing {len(FIGURES)} figures to {OUT.relative_to(ROOT)}/")
    for fn in FIGURES:
        fn()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
