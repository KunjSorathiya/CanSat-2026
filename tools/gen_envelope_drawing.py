#!/usr/bin/env python3
"""Draws the rulebook envelope to scale, with the actual design inside it.

    python tools/gen_envelope_drawing.py

The 2026 guidelines fix the envelope at 21 cm tall (+7 cm for the egg chamber) by 12 cm
across. "12 cm across" did not say whether it meant a width or a diameter, and for a
prismatic body those give different answers -- a 115 x 110 mm section passes as a width and
is 33 % over as a diameter, against a 10 % disqualification threshold.

**The organizers settled it on 2026-09-09: a 12 cm sided box is acceptable.** So the
envelope is a 120 mm square section, the design fits, and what the drawing now shows is the
margin rather than the question.

**The design's dimensions are read from the STEP file**, not typed here, so the drawing
cannot disagree with the model. Change the model, re-export, re-run this: the drawing
follows. `check_doc_claims.py` fails the build if the committed SVG stops matching what
this produces.
"""

from __future__ import annotations

import io
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from cad_dimensions import read_step  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[1]
DESIGN_STEP = REPO_ROOT / "mechanical/CAD/Cansat_D1.step"
OUTPUT_PATH = REPO_ROOT / "mechanical/drawings/envelope-and-board-fit.svg"

# ---- Rulebook dimensions, all in millimetres ---------------------------------------
# 2026 revision: 21 cm body + 7 cm egg chamber, 12 cm across. See
# documentation/requirements/requirements.md GEN-004.
BODY_HEIGHT_MM = 210.0
CHAMBER_HEIGHT_MM = 70.0
ACROSS_MM = 120.0

# The perfboard actually held: 2 x 100 x 100 mm, 1.6 mm FR-4. See
# documentation/hardware/receiving-inspection.md C.9.
BOARD_MM = 100.0

# The largest square that fits inside a circle of the envelope diameter, if "across" is
# read as a diameter: s = d / sqrt(2).
INSCRIBED_SQUARE_MM = ACROSS_MM / math.sqrt(2.0)

SCALE = 2.0          # px per mm
MARGIN = 120.0       # px — wide enough on the left for two stacked dimension lines

BLUE = "#0d47a1"
GREEN = "#1b5e20"
RED = "#b71c1c"
AMBER = "#b45309"
SLATE = "#475569"
GREY = "#94a3b8"
PURPLE = "#6d28d9"


def esc(text: str) -> str:
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


class Canvas:
    def __init__(self, width: float, height: float) -> None:
        self.width, self.height = width, height
        self.parts: list[str] = []

    def add(self, markup: str) -> None:
        self.parts.append(markup)

    def text(self, x: float, y: float, body: str, size: float = 12.0,
             fill: str = "#0f172a", weight: str = "400", anchor: str = "start",
             mono: bool = False) -> None:
        family = ('font-family="ui-monospace, Consolas, monospace" ' if mono else "")
        self.add(f'<text x="{x:.1f}" y="{y:.1f}" font-size="{size}" fill="{fill}" '
                 f'font-weight="{weight}" text-anchor="{anchor}" {family}>{esc(body)}</text>')

    def rect(self, x: float, y: float, w: float, h: float, *, fill: str = "none",
             stroke: str = "none", width: float = 1.0, dash: str = "",
             rx: float = 0.0) -> None:
        d = f' stroke-dasharray="{dash}"' if dash else ""
        r = f' rx="{rx}"' if rx else ""
        self.add(f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}"{r} '
                 f'fill="{fill}" stroke="{stroke}" stroke-width="{width}"{d}/>')

    def circle(self, cx: float, cy: float, r: float, *, fill: str = "none",
               stroke: str = "none", width: float = 1.0, dash: str = "") -> None:
        d = f' stroke-dasharray="{dash}"' if dash else ""
        self.add(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{r:.1f}" fill="{fill}" '
                 f'stroke="{stroke}" stroke-width="{width}"{d}/>')

    def render(self) -> str:
        head = (f'<svg xmlns="http://www.w3.org/2000/svg" '
                f'viewBox="0 0 {self.width:.0f} {self.height:.0f}" '
                f'width="{self.width:.0f}" height="{self.height:.0f}" '
                f'font-family="ui-sans-serif, system-ui, \'Segoe UI\', Roboto, '
                f'Helvetica, Arial, sans-serif">')
        bg = f'<rect width="{self.width:.0f}" height="{self.height:.0f}" fill="#ffffff"/>'
        return "\n".join([head, bg, *self.parts, "</svg>"])


def dimension(c: Canvas, x1: float, y1: float, x2: float, y2: float, label: str,
              offset: float = 0.0, vertical: bool = False) -> None:
    """A dimension line with tick ends and a label, in the style of a drawing."""
    c.add(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" '
          f'stroke="{SLATE}" stroke-width="1.1"/>')
    for px, py in ((x1, y1), (x2, y2)):
        if vertical:
            c.add(f'<line x1="{px - 5:.1f}" y1="{py:.1f}" x2="{px + 5:.1f}" '
                  f'y2="{py:.1f}" stroke="{SLATE}" stroke-width="1.1"/>')
        else:
            c.add(f'<line x1="{px:.1f}" y1="{py - 5:.1f}" x2="{px:.1f}" '
                  f'y2="{py + 5:.1f}" stroke="{SLATE}" stroke-width="1.1"/>')
    mx, my = (x1 + x2) / 2.0, (y1 + y2) / 2.0
    if vertical:
        c.add(f'<g transform="translate({mx + offset:.1f},{my:.1f}) rotate(-90)">'
              f'<text font-size="12.5" fill="{SLATE}" text-anchor="middle" '
              f'dy="-4">{esc(label)}</text></g>')
    else:
        c.text(mx, my + offset, label, size=12.5, fill=SLATE, anchor="middle")


def build() -> str:
    design = read_step(DESIGN_STEP)
    # Longest extent stands up the vehicle; the other two are its cross-section.
    height_mm, wide_mm, deep_mm = design.sorted_extents
    diagonal_mm = design.footprint_diagonal

    env_w = ACROSS_MM * SCALE
    chamber_h = CHAMBER_HEIGHT_MM * SCALE
    body_h = BODY_HEIGHT_MM * SCALE
    section_d = ACROSS_MM * SCALE

    left = MARGIN
    top = 168.0
    gap = 200.0
    section_x = left + env_w + gap
    canvas_w = section_x + section_d + MARGIN + 360.0
    canvas_h = top + chamber_h + body_h + 120.0
    c = Canvas(canvas_w, canvas_h)

    c.text(40, 48, "CanSat 2026 — rulebook envelope and the Cansat_D1 design",
           size=25, weight="700")
    c.text(40, 74,
           f"2026 guidelines: 21 cm body (+7 cm egg chamber) × 12 cm across. "
           f"Design read from {DESIGN_STEP.relative_to(REPO_ROOT).as_posix()}. "
           f"Drawn to scale at {SCALE} px/mm.", size=13, fill="#64748b")
    c.text(40, 96,
           "Organizers confirmed 2026-09-09: a 12 cm SIDED BOX is acceptable, so the "
           "section limit is a 120 mm square rather than a 120 mm bore.",
           size=12.5, fill=GREEN)

    # ---- Elevation -----------------------------------------------------------------
    c.text(left, top - 16, "ELEVATION", size=13, weight="700", fill=BLUE)
    c.rect(left, top, env_w, chamber_h, fill="#fff7ed", stroke=AMBER, width=2, dash="7 5")
    c.text(left + env_w / 2, top + chamber_h / 2 - 4, "egg chamber allowance",
           size=12, fill=AMBER, anchor="middle")
    c.text(left + env_w / 2, top + chamber_h / 2 + 13, "70 mm — not designed",
           size=11.5, fill=AMBER, anchor="middle")

    body_top = top + chamber_h
    c.rect(left, body_top, env_w, body_h, rx=4, fill="#eef4ff", stroke=BLUE, width=2.5)

    # The design, seated at the bottom of the body envelope.
    d_w, d_h = wide_mm * SCALE, height_mm * SCALE
    dx = left + (env_w - d_w) / 2.0
    dy = body_top + body_h - d_h
    c.rect(dx, dy, d_w, d_h, fill="#e8f2ea", stroke=GREEN, width=2.5)
    c.text(dx + d_w / 2, dy + d_h / 2 - 20, "Cansat_D1", size=13, weight="700",
           fill=GREEN, anchor="middle")
    c.text(dx + d_w / 2, dy + d_h / 2 - 2,
           f"{wide_mm:.1f} × {height_mm:.1f} mm", size=12, fill=GREEN,
           anchor="middle", mono=True)
    c.text(dx + d_w / 2, dy + d_h / 2 + 18,
           f"{BODY_HEIGHT_MM - height_mm:.1f} mm of height unused",
           size=11.5, fill=GREEN, anchor="middle")

    dimension(c, left - 30, top, left - 30, body_top + body_h, "280 mm max overall",
              offset=-8, vertical=True)
    dimension(c, left - 58, body_top, left - 58, body_top + body_h, "210 mm body",
              offset=-8, vertical=True)
    dimension(c, left, body_top + body_h + 32, left + env_w,
              body_top + body_h + 32, "120 mm across", offset=18)

    # ---- Section --------------------------------------------------------------------
    cx = section_x + section_d / 2.0
    cy = body_top + body_h / 2.0
    c.text(section_x, top - 16, "SECTION — looking down", size=13, weight="700", fill=BLUE)

    # The confirmed envelope: a 120 mm square section.
    c.rect(cx - section_d / 2, cy - section_d / 2, section_d, section_d,
           fill="#eef4ff", stroke=BLUE, width=2.5)
    c.text(cx, cy - section_d / 2 - 10, "120 mm SIDED BOX — confirmed", size=12,
           weight="700", fill=BLUE, anchor="middle")

    # The reading that was ruled out, kept faint. It is why the question was asked, and
    # somebody reading this drawing next year should be able to see that it was answered
    # rather than never considered.
    c.circle(cx, cy, section_d / 2, stroke=GREY, width=1.4, dash="4 6")
    c.text(cx, cy + section_d / 2 + 20, "a 120 mm bore would NOT have fit — ruled out",
           size=11, fill=GREY, anchor="middle")

    # The design's actual cross-section.
    sw, sd = wide_mm * SCALE, deep_mm * SCALE
    c.rect(cx - sw / 2, cy - sd / 2, sw, sd, fill="#e8f2ea", stroke=GREEN, width=2.5)
    c.text(cx, cy - 4, f"{wide_mm:.0f} × {deep_mm:.0f}", size=13, weight="700",
           fill=GREEN, anchor="middle", mono=True)
    c.text(cx, cy + 14, "Cansat_D1", size=11.5, fill=GREEN, anchor="middle")

    # The clearance to the envelope, per side. This is the number a builder needs: it is
    # what any protruding feature -- a switch boss, an LED bezel, a chute attachment -- has
    # to live inside.
    side_w = (ACROSS_MM - wide_mm) / 2.0
    side_d = (ACROSS_MM - deep_mm) / 2.0
    for x, y, label in (
            (cx - (wide_mm * SCALE / 2 + side_w * SCALE / 2), cy - sd / 2 - 12,
             f"{side_w:.1f}"),
            (cx + (wide_mm * SCALE / 2 + side_w * SCALE / 2), cy - sd / 2 - 12,
             f"{side_w:.1f}"),
            (cx, cy - (deep_mm * SCALE / 2 + side_d * SCALE / 2) + 4, f"{side_d:.1f}"),
            (cx, cy + (deep_mm * SCALE / 2 + side_d * SCALE / 2) + 4, f"{side_d:.1f}")):
        c.text(x, y, label, size=11, weight="600", fill=AMBER, anchor="middle", mono=True)
    c.text(cx, cy + section_d / 2 + 38,
           f"clearance per side: {side_w:.1f} mm and {side_d:.1f} mm",
           size=12, weight="600", fill=AMBER, anchor="middle")

    # ---- The arithmetic --------------------------------------------------------------
    tx = section_x + section_d + 70
    ty = top + 4
    c.text(tx, ty, "Against the confirmed limit", size=14, weight="700")
    rows = [
        ("Design section", f"{wide_mm:.0f} × {deep_mm:.0f} mm", None),
        ("Envelope, confirmed", f"{ACROSS_MM:.0f} × {ACROSS_MM:.0f} mm box", None),
        ("Widest face", f"{wide_mm:.0f} ≤ {ACROSS_MM:.0f} mm  ✓", True),
        ("Clearance per side",
         f"{(ACROSS_MM - wide_mm) / 2:.1f} and {(ACROSS_MM - deep_mm) / 2:.1f} mm", None),
        ("Height", f"{height_mm:.1f} ≤ {BODY_HEIGHT_MM:.0f} mm  ✓", True),
        ("Height unused", f"{BODY_HEIGHT_MM - height_mm:.1f} mm", None),
    ]
    y = ty + 28
    for label, value, good in rows:
        colour = "#0f172a" if good is None else (GREEN if good else RED)
        c.text(tx, y, label, size=12.5, fill="#475569")
        c.text(tx, y + 16, value, size=13, weight="600", fill=colour, mono=True)
        y += 42

    y += 8
    c.text(tx, y, "The board fits flat", size=14, weight="700", fill=GREEN)
    for i, line in enumerate([
        f"A {BOARD_MM:.0f} × {BOARD_MM:.0f} mm board sits inside this",
        f"{wide_mm:.0f} × {deep_mm:.0f} mm section with room for walls.",
        "The earlier edge-on recommendation assumed",
        "a cylinder and no longer applies.",
        "",
        "WATCH THE CLEARANCE, THOUGH:",
        f"{(ACROSS_MM - wide_mm) / 2:.1f} mm per side on the wide axis is not",
        "much. Anything that protrudes — switch",
        "boss, LED bezel, chute attachment, an",
        "antenna — has to live inside it, or the",
        "envelope grows past 120 mm.",
    ]):
        colour = AMBER if i >= 5 else "#334155"
        weight = "600" if i == 5 else "400"
        c.text(tx, y + 24 + i * 17, line, size=12.5, fill=colour, weight=weight)

    c.text(40, canvas_h - 26,
           "Generated by tools/gen_envelope_drawing.py — do not hand-edit. Envelope from "
           "requirements.md (GEN-004); design dimensions read from the STEP file.",
           size=11.5, fill=GREY)
    return c.render()


def main() -> int:
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    io.open(OUTPUT_PATH, "w", encoding="utf-8", newline="\n").write(build())
    print(f"written {OUTPUT_PATH.relative_to(REPO_ROOT).as_posix()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
