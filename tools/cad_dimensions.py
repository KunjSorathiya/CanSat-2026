#!/usr/bin/env python3
"""Reads the overall dimensions of a STEP solid, so documents can be held to the CAD.

    python tools/cad_dimensions.py mechanical/CAD/Cansat_D1.step

The mechanical documents quote a bounding box, and a bounding box typed by hand goes stale
the first time somebody edits the model and exports again. This reads it out of the file,
and `check_doc_claims.py` fails the build when a document disagrees with what it returns.

Why this can be done at all: STEP (ISO 10303-21) is ASCII, and the entities that matter
here are simple enough to read without a geometry kernel.

    #123=CARTESIAN_POINT('',(1.0,2.0,3.0));
    #124=DIRECTION('',(0.0,0.0,1.0));
    #125=AXIS2_PLACEMENT_3D('',#123,#124,#126);
    #127=CIRCLE('',#125,22.5);

**What this is not.** It is not a CAD kernel and it does not compute volume, mass or
inertia -- for those, read them off Fusion's own properties panel. It measures the extent
of the geometry, which is the quantity the rulebook's envelope limit is about.

**The one place it could be wrong**, stated because a silently-wrong dimension is worse
than none: a bounding box taken over B-rep control points is exact for a solid whose
extremes fall on vertices or on circular edges, which covers planar and cylindrical
geometry -- and circles are expanded properly here, by projecting the radius onto each
global axis through the circle's own axis direction. A NURBS surface bulging past all of
its control points would be under-measured, so the tool refuses to report a box for a file
containing B-spline surfaces rather than quietly returning a number that is too small.
"""

from __future__ import annotations

import argparse
import math
import re
import sys
from dataclasses import dataclass
from pathlib import Path

Vec = tuple[float, float, float]

# A STEP real. The trailing-dot form is the trap: ISO 10303-21 writes 8 as `8.`, with the
# dot and NO digits after it, and a pattern that requires a digit after the decimal point
# matches `8` and then fails on the `)` it expected. That silently drops those radii -- and
# dropping a radius from the bounding-box expansion below under-measures the part, which is
# the exact failure this module's docstring promises not to have. Covered by
# test_cad_dimensions.py.
#
# Accepts: 8.   2.55   .5   6   1.0E-7   -76.059
REAL = r"[-+]?(?:[0-9]+\.[0-9]*|\.[0-9]+|[0-9]+)(?:[eE][-+]?[0-9]+)?"


@dataclass(frozen=True)
class Dimensions:
    """What a STEP file says about the size of what it contains."""

    path: str
    units: str
    solids: tuple[str, ...]
    faces: int
    min_xyz: Vec
    max_xyz: Vec
    radii: tuple[float, ...]

    @property
    def extents(self) -> Vec:
        return tuple(hi - lo for lo, hi in zip(self.min_xyz, self.max_xyz))  # type: ignore

    @property
    def sorted_extents(self) -> Vec:
        """Extents largest first, so a document can quote them without caring about axes."""
        return tuple(sorted(self.extents, reverse=True))  # type: ignore

    @property
    def footprint_diagonal(self) -> float:
        """Diagonal of the two smallest extents.

        For a prismatic body standing on its longest axis this is the diagonal of its
        cross-section -- the dimension that decides whether it passes through a circular
        opening, and the one a rulebook limit expressed as a diameter is really about.
        """
        a, b = sorted(self.extents)[:2]
        return math.hypot(a, b)


def _floats(text: str) -> tuple[float, ...]:
    return tuple(float(v) for v in re.findall(REAL, text))


def read_step(path: Path) -> Dimensions:
    body = path.read_text(encoding="utf-8", errors="replace")

    if re.search(r"B_SPLINE_SURFACE", body):
        raise ValueError(
            f"{path} contains B-spline surfaces, whose extremes need not lie on any "
            "control point. This tool would under-measure the box, so it refuses to "
            "report one -- take the dimensions from the CAD package instead.")

    units = ", ".join(sorted(set(re.findall(r"SI_UNIT\(([^)]*)\)", body))))
    length = re.search(r"SI_UNIT\(\.(\w+)\.,\.METRE\.\)", body)
    if not length or length.group(1) != "MILLI":
        raise ValueError(f"{path} is not in millimetres (found {units!r}); "
                         "every dimension in this project is mm")

    points: dict[str, Vec] = {}
    for pid, coords in re.findall(r"#(\d+)=CARTESIAN_POINT\('[^']*',\(([^)]*)\)\)", body):
        values = _floats(coords)
        if len(values) == 3:
            points[pid] = values  # type: ignore

    directions: dict[str, Vec] = {}
    for did, coords in re.findall(r"#(\d+)=DIRECTION\('[^']*',\(([^)]*)\)\)", body):
        values = _floats(coords)
        if len(values) == 3:
            directions[did] = values  # type: ignore

    # AXIS2_PLACEMENT_3D('',#location,#axis,#ref_direction)
    placements: dict[str, tuple[str, str]] = {}
    for aid, loc, axis in re.findall(
            r"#(\d+)=AXIS2_PLACEMENT_3D\('[^']*',#(\d+),#(\d+)", body):
        placements[aid] = (loc, axis)

    if not points:
        raise ValueError(f"{path} contains no cartesian points")

    lo = [min(p[i] for p in points.values()) for i in range(3)]
    hi = [max(p[i] for p in points.values()) for i in range(3)]

    # A circular edge sweeps past its own centre. Its extent along each global axis is
    # r * sqrt(1 - n_i^2), where n is the circle's unit normal: a circle whose normal is
    # the Z axis reaches +-r in X and Y and nothing in Z.
    radii: set[float] = set()
    for placement_id, radius_text in re.findall(
            rf"CIRCLE\('[^']*',#(\d+),({REAL})\)", body):
        radius = float(radius_text)
        radii.add(round(radius, 4))
        placement = placements.get(placement_id)
        if not placement:
            continue
        centre, normal = points.get(placement[0]), directions.get(placement[1])
        if centre is None or normal is None:
            continue
        length_n = math.sqrt(sum(c * c for c in normal)) or 1.0
        for i in range(3):
            reach = radius * math.sqrt(max(0.0, 1.0 - (normal[i] / length_n) ** 2))
            lo[i] = min(lo[i], centre[i] - reach)
            hi[i] = max(hi[i], centre[i] + reach)

    # Cylindrical surfaces too, for the feature list. A hole whose edges are not full
    # circles -- a counterbore blended into a pocket, say -- appears only here, so a list
    # built from CIRCLE alone silently omits real features. The bounding box deliberately
    # does NOT use these: a cylindrical surface is unbounded in STEP and its extent is set
    # by the edges that trim it, which are already accounted for above.
    for radius_text in re.findall(rf"CYLINDRICAL_SURFACE\('[^']*',#\d+,({REAL})\)", body):
        radii.add(round(float(radius_text), 4))

    return Dimensions(
        path=path.as_posix(),
        units=units,
        solids=tuple(re.findall(r"MANIFOLD_SOLID_BREP\('([^']*)'", body)),
        faces=len(re.findall(r"#\d+=ADVANCED_FACE\(", body)),
        min_xyz=tuple(lo),  # type: ignore
        max_xyz=tuple(hi),  # type: ignore
        radii=tuple(sorted(radii)),
    )


def format_text(d: Dimensions) -> str:
    x, y, z = d.extents
    lines = [
        f"{d.path}",
        f"  solids            {', '.join(d.solids) or '(none)'}",
        f"  faces             {d.faces}",
        f"  bounding box      {x:.1f} x {y:.1f} x {z:.1f} mm",
        f"  largest first     {' x '.join(f'{v:.1f}' for v in d.sorted_extents)} mm",
        f"  cross-section     {d.footprint_diagonal:.1f} mm diagonal "
        f"(the two smallest extents)",
        f"  circular features {', '.join(f'D{2 * r:.1f}' for r in d.radii) or '(none)'} mm",
    ]
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Read overall dimensions out of a STEP solid.")
    parser.add_argument("step", type=Path, nargs="?",
                        default=Path("mechanical/CAD/Cansat_D1.step"))
    args = parser.parse_args(argv)

    try:
        print(format_text(read_step(args.step)))
    except (OSError, ValueError) as problem:
        print(f"error: {problem}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
