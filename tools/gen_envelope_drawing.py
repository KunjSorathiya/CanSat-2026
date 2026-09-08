#!/usr/bin/env python3
"""Draws the rulebook envelope to scale, with the vehicle board inside it.

    python tools/gen_envelope_drawing.py

The 2026 guidelines fix the envelope at 21 cm tall (+7 cm for the egg chamber) by 12 cm
across. The vehicle board is a 100 x 100 mm perfboard. Those two numbers do not obviously
fit together, and the arithmetic that decides it -- a 100 mm square has a 141.4 mm diagonal,
while a 120 mm circle inscribes only an 84.9 mm square -- is the kind of thing that is
much easier to believe once drawn.

Everything here is derived from the constants at the top. Nothing is a hand-placed
coordinate, so changing a dimension moves the drawing rather than making it wrong.
"""

from __future__ import annotations

import io
import math

# ---- Dimensions, all in millimetres ------------------------------------------------
# Rulebook, 2026 revision: 21 cm body + 7 cm egg chamber, 12 cm across. See
# documentation/requirements/requirements.md GEN-004.
BODY_HEIGHT_MM = 210.0
CHAMBER_HEIGHT_MM = 70.0
DIAMETER_MM = 120.0

# The perfboard actually held: 2 x 100 x 100 mm, 1.6 mm FR-4. See
# documentation/hardware/receiving-inspection.md C.9.
BOARD_MM = 100.0

# The largest square that fits inside the envelope's circular cross-section, if the board
# is laid flat as a horizontal deck: s = d / sqrt(2).
INSCRIBED_SQUARE_MM = DIAMETER_MM / math.sqrt(2.0)

OUTPUT_PATH = "mechanical/drawings/envelope-and-board-fit.svg"

SCALE = 2.2          # px per mm
MARGIN = 120.0       # px — wide enough on the left for two stacked dimension lines

BLUE = "#0d47a1"
GREEN = "#1b5e20"
RED = "#b71c1c"
AMBER = "#b45309"
SLATE = "#475569"
GREY = "#94a3b8"


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

    def render(self) -> str:
        head = (f'<svg xmlns="http://www.w3.org/2000/svg" '
                f'viewBox="0 0 {self.width:.0f} {self.height:.0f}" '
                f'width="{self.width:.0f}" height="{self.height:.0f}" '
                f'font-family="ui-sans-serif, system-ui, \'Segoe UI\', Roboto, '
                f'Helvetica, Arial, sans-serif">')
        bg = f'<rect width="{self.width:.0f}" height="{self.height:.0f}" fill="#ffffff"/>'
        return "\n".join([head, bg, *self.parts, "</svg>"])


def dimension(canvas: Canvas, x1: float, y1: float, x2: float, y2: float,
              label: str, offset: float = 0.0, vertical: bool = False) -> None:
    """A dimension line with arrow ticks and a label, in the style of a drawing."""
    canvas.add(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" '
               f'stroke="{SLATE}" stroke-width="1.1"/>')
    for px, py in ((x1, y1), (x2, y2)):
        if vertical:
            canvas.add(f'<line x1="{px - 5:.1f}" y1="{py:.1f}" x2="{px + 5:.1f}" '
                       f'y2="{py:.1f}" stroke="{SLATE}" stroke-width="1.1"/>')
        else:
            canvas.add(f'<line x1="{px:.1f}" y1="{py - 5:.1f}" x2="{px:.1f}" '
                       f'y2="{py + 5:.1f}" stroke="{SLATE}" stroke-width="1.1"/>')
    mx, my = (x1 + x2) / 2.0, (y1 + y2) / 2.0
    if vertical:
        canvas.add(f'<g transform="translate({mx + offset:.1f},{my:.1f}) rotate(-90)">'
                   f'<text font-size="12.5" fill="{SLATE}" text-anchor="middle" '
                   f'dy="-4">{esc(label)}</text></g>')
    else:
        canvas.text(mx, my + offset, label, size=12.5, fill=SLATE, anchor="middle")


def build() -> str:
    total_height = BODY_HEIGHT_MM + CHAMBER_HEIGHT_MM
    elevation_w = DIAMETER_MM * SCALE
    elevation_h = total_height * SCALE
    section_d = DIAMETER_MM * SCALE

    left = MARGIN
    top = 150.0
    gap = 190.0
    section_x = left + elevation_w + gap
    # The right-hand column carries the arithmetic and the two mounting options as text;
    # 340 px is what the longest of those lines needs at 12.5 px.
    canvas_w = section_x + section_d + MARGIN + 340.0
    canvas_h = top + elevation_h + 190.0
    c = Canvas(canvas_w, canvas_h)

    c.text(40, 48, "CanSat 2026 — rulebook envelope and board fit", size=26, weight="700")
    c.text(40, 74,
           "2026 guidelines: 21 cm body (+7 cm egg chamber) × 12 cm across. "
           f"Drawn to scale at {SCALE} px/mm.", size=13.5, fill="#64748b")
    c.text(40, 96,
           "The envelope is fixed. Nothing mechanical is built, so every part of this "
           "drawing except the two dimensions is a proposal.", size=12.5, fill=AMBER)

    # ---- Elevation ---------------------------------------------------------------
    chamber_h = CHAMBER_HEIGHT_MM * SCALE
    body_h = BODY_HEIGHT_MM * SCALE
    c.text(left, top - 16, "ELEVATION", size=13, weight="700", fill=BLUE)

    c.add(f'<rect x="{left:.1f}" y="{top:.1f}" width="{elevation_w:.1f}" '
          f'height="{chamber_h:.1f}" fill="#fff7ed" stroke="{AMBER}" '
          f'stroke-width="2" stroke-dasharray="7 5"/>')
    c.text(left + elevation_w / 2, top + chamber_h / 2 - 4, "egg chamber allowance",
           size=12, fill=AMBER, anchor="middle")
    c.text(left + elevation_w / 2, top + chamber_h / 2 + 13, "70 mm — optional",
           size=11.5, fill=AMBER, anchor="middle")

    body_top = top + chamber_h
    c.add(f'<rect x="{left:.1f}" y="{body_top:.1f}" width="{elevation_w:.1f}" '
          f'height="{body_h:.1f}" rx="4" fill="#eef4ff" stroke="{BLUE}" '
          f'stroke-width="2.5"/>')

    # The board as a vertical spine: 100 mm wide inside a 120 mm envelope, 100 mm of the
    # 210 mm height. This is the orientation that fits without cutting anything.
    board_px = BOARD_MM * SCALE
    spine_x = left + (elevation_w - board_px) / 2.0
    spine_y = body_top + (body_h - board_px) / 2.0
    c.add(f'<rect x="{spine_x:.1f}" y="{spine_y:.1f}" width="{board_px:.1f}" '
          f'height="{board_px:.1f}" fill="#e8f2ea" stroke="{GREEN}" stroke-width="2.5"/>')
    c.text(spine_x + board_px / 2, spine_y + board_px / 2 - 8,
           "vehicle board, edge-mounted", size=12.5, weight="600", fill=GREEN,
           anchor="middle")
    c.text(spine_x + board_px / 2, spine_y + board_px / 2 + 10, "100 × 100 mm",
           size=12, fill=GREEN, anchor="middle", mono=True)
    c.text(spine_x + board_px / 2, spine_y + board_px / 2 + 28, "FITS AS DRAWN",
           size=12, weight="700", fill=GREEN, anchor="middle")

    dimension(c, left - 26, top, left - 26, body_top + body_h, "280 mm max overall",
              offset=-8, vertical=True)
    dimension(c, left - 52, body_top, left - 52, body_top + body_h, "210 mm body",
              offset=-8, vertical=True)
    dimension(c, left, body_top + body_h + 30, left + elevation_w,
              body_top + body_h + 30, "120 mm across", offset=18)

    # ---- Section ------------------------------------------------------------------
    cx = section_x + section_d / 2.0
    cy = top + chamber_h + body_h / 2.0
    r = section_d / 2.0
    c.text(section_x, top - 16, "SECTION — looking down", size=13, weight="700", fill=BLUE)

    c.add(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{r:.1f}" fill="#eef4ff" '
          f'stroke="{BLUE}" stroke-width="2.5"/>')

    # The 100 mm square laid flat: its diagonal is 141.4 mm and it does not fit.
    flat = BOARD_MM * SCALE
    c.add(f'<rect x="{cx - flat / 2:.1f}" y="{cy - flat / 2:.1f}" width="{flat:.1f}" '
          f'height="{flat:.1f}" fill="none" stroke="{RED}" stroke-width="2.2" '
          f'stroke-dasharray="8 5"/>')
    diag = BOARD_MM * math.sqrt(2.0) * SCALE
    c.add(f'<line x1="{cx - flat / 2:.1f}" y1="{cy - flat / 2:.1f}" '
          f'x2="{cx + flat / 2:.1f}" y2="{cy + flat / 2:.1f}" stroke="{RED}" '
          f'stroke-width="1.2" stroke-dasharray="3 3"/>')
    c.add(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{diag / 2:.1f}" fill="none" '
          f'stroke="{RED}" stroke-width="1" stroke-dasharray="2 6"/>')

    # The largest square that does fit.
    ins = INSCRIBED_SQUARE_MM * SCALE
    c.add(f'<rect x="{cx - ins / 2:.1f}" y="{cy - ins / 2:.1f}" width="{ins:.1f}" '
          f'height="{ins:.1f}" fill="#e8f2ea" stroke="{GREEN}" stroke-width="2.2"/>')
    c.text(cx, cy + 5, f"{INSCRIBED_SQUARE_MM:.1f} mm", size=12.5, weight="700",
           fill=GREEN, anchor="middle", mono=True)
    c.text(cx, cy + 22, "largest flat deck", size=11.5, fill=GREEN, anchor="middle")

    key_y = cy + r + 46
    c.text(section_x, key_y, "100 × 100 mm board laid flat — 141.4 mm diagonal",
           size=12.5, weight="600", fill=RED)
    c.text(section_x, key_y + 18,
           "does NOT fit a 120 mm section. Cut to 84.9 mm, or mount it edge-on.",
           size=12, fill=RED)

    # ---- The arithmetic, written out ------------------------------------------------
    tx = section_x + section_d + 46
    ty = top + 6
    c.text(tx, ty, "The arithmetic", size=14, weight="700")
    lines = [
        ("Envelope section", f"{DIAMETER_MM:.0f} mm across"),
        ("Board", f"{BOARD_MM:.0f} × {BOARD_MM:.0f} mm"),
        ("Board diagonal", f"{BOARD_MM * math.sqrt(2.0):.1f} mm"),
        ("Largest inscribed square", f"{INSCRIBED_SQUARE_MM:.1f} mm"),
        ("Board edge-on, width", f"{BOARD_MM:.0f} mm ≤ {DIAMETER_MM:.0f} mm  ✓"),
        ("Board edge-on, height", f"{BOARD_MM:.0f} mm ≤ {BODY_HEIGHT_MM:.0f} mm  ✓"),
        ("Board flat, diagonal", f"{BOARD_MM * math.sqrt(2.0):.1f} mm "
                                 f"> {DIAMETER_MM:.0f} mm  ✗"),
    ]
    for i, (label, value) in enumerate(lines):
        y = ty + 30 + i * 24
        colour = GREEN if "✓" in value else (RED if "✗" in value else "#0f172a")
        c.text(tx, y, label, size=12.5, fill="#475569")
        c.text(tx, y + 15, value, size=13, weight="600", fill=colour, mono=True)
        ty += 14

    concl_y = ty + 30 + len(lines) * 24 + 10
    c.text(tx, concl_y, "Two ways out", size=14, weight="700")
    for i, line in enumerate([
        "1 · Mount the board edge-on as a spine.",
        "    Costs nothing. Puts the RA-02 and its",
        "    antenna along the axis, which is where",
        "    they want to be anyway.",
        "",
        "2 · Cut the board to 84.9 mm square and",
        "    stack two decks. Costs a rebuild, and",
        "    the current floorplan uses the full",
        "    100 mm in both axes.",
    ]):
        c.text(tx, concl_y + 24 + i * 17, line, size=12.5, fill="#334155")

    c.text(40, canvas_h - 26,
           "Generated by tools/gen_envelope_drawing.py — do not hand-edit. "
           "Dimensions from documentation/requirements/requirements.md (GEN-004).",
           size=11.5, fill=GREY)
    return c.render()


def main() -> int:
    # Written to the file rather than to stdout, as the other two generators are: a
    # Windows console encodes as cp1252 and would fail on the first non-ASCII glyph.
    io.open(OUTPUT_PATH, "w", encoding="utf-8", newline="\n").write(build())
    print(f"written {OUTPUT_PATH}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
