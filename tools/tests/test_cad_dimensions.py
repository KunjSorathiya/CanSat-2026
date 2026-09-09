"""Tests for the STEP dimension reader.

The mechanical documents quote a bounding box this module returns, and a bounding box that
is quietly too small is worse than no bounding box at all -- it would say a part fits an
envelope it does not fit. So the tests are mostly about the ways the measurement can be
wrong rather than the way it is right.
"""

import math
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from cad_dimensions import Dimensions, format_text, main, read_step  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[2]
DESIGN = REPO_ROOT / "mechanical/CAD/Cansat_D1.step"

HEADER = """ISO-10303-21;
HEADER;
FILE_DESCRIPTION(/* description */ (''),'2;1');
FILE_NAME('t.step','2026-01-01T00:00:00',(''),(''),'','','');
FILE_SCHEMA (('AUTOMOTIVE_DESIGN { 1 0 10303 214 3 1 1 }'));
ENDSEC;
DATA;
#1=(LENGTH_UNIT()NAMED_UNIT(*)SI_UNIT(.MILLI.,.METRE.));
#2=MANIFOLD_SOLID_BREP('Body1',#900);
"""

FOOTER = "ENDSEC;\nEND-ISO-10303-21;\n"


def step_file(body: str, header: str = HEADER) -> Path:
    handle = tempfile.NamedTemporaryFile("w", suffix=".step", delete=False,
                                         encoding="utf-8")
    handle.write(header + body + FOOTER)
    handle.close()
    return Path(handle.name)


def box_points(x0, y0, z0, x1, y1, z1, start=10):
    """Eight corner points of an axis-aligned box, as STEP entities."""
    corners = [(x, y, z) for x in (x0, x1) for y in (y0, y1) for z in (z0, z1)]
    return "".join(
        f"#{start + i}=CARTESIAN_POINT('',({x},{y},{z}));\n"
        for i, (x, y, z) in enumerate(corners))


class TrailingDotTests(unittest.TestCase):
    """ISO 10303-21 writes the integer 8 as `8.` -- dot, no digits after it.

    A regex requiring a digit after the decimal point matches `8` and then fails on the
    `)` it expected, so the radius is silently dropped. That is how this module first
    reported the design's D16 feature as absent, and it is how a bounding box would come
    back too small on a part whose extreme lies on such a circle.
    """

    def test_a_trailing_dot_radius_is_read(self):
        path = step_file(
            box_points(0, 0, 0, 10, 10, 10) +
            "#30=CARTESIAN_POINT('',(5.,5.,0.));\n"
            "#31=DIRECTION('',(0.,0.,1.));\n"
            "#32=DIRECTION('',(1.,0.,0.));\n"
            "#33=AXIS2_PLACEMENT_3D('',#30,#31,#32);\n"
            "#34=CIRCLE('',#33,8.);\n")
        try:
            self.assertIn(8.0, read_step(path).radii)
        finally:
            path.unlink()

    def test_every_step_real_spelling_is_read(self):
        for literal, expected in (("8.", 8.0), ("2.55", 2.55), (".5", 0.5),
                                  ("6", 6.0), ("1.5E1", 15.0)):
            path = step_file(
                box_points(0, 0, 0, 1, 1, 1) +
                "#30=CARTESIAN_POINT('',(0.,0.,0.));\n"
                "#31=DIRECTION('',(0.,0.,1.));\n"
                "#32=DIRECTION('',(1.,0.,0.));\n"
                "#33=AXIS2_PLACEMENT_3D('',#30,#31,#32);\n"
                f"#34=CIRCLE('',#33,{literal});\n")
            try:
                self.assertIn(expected, read_step(path).radii, literal)
            finally:
                path.unlink()


class BoundingBoxTests(unittest.TestCase):
    def test_a_plain_box_measures_its_own_extents(self):
        path = step_file(box_points(-5, 0, 2, 15, 30, 12))
        try:
            d = read_step(path)
            self.assertEqual(d.min_xyz, (-5.0, 0.0, 2.0))
            self.assertEqual(d.max_xyz, (15.0, 30.0, 12.0))
            self.assertEqual(d.extents, (20.0, 30.0, 10.0))
            self.assertEqual(d.sorted_extents, (30.0, 20.0, 10.0))
        finally:
            path.unlink()

    def test_a_circle_bulging_past_every_vertex_expands_the_box(self):
        # Vertices confined to a 2 mm cube, plus a radius-50 circle in the XY plane. A box
        # taken over points alone would report 2 mm and be wrong by a factor of fifty.
        path = step_file(
            box_points(-1, -1, -1, 1, 1, 1) +
            "#30=CARTESIAN_POINT('',(0.,0.,0.));\n"
            "#31=DIRECTION('',(0.,0.,1.));\n"
            "#32=DIRECTION('',(1.,0.,0.));\n"
            "#33=AXIS2_PLACEMENT_3D('',#30,#31,#32);\n"
            "#34=CIRCLE('',#33,50.);\n")
        try:
            d = read_step(path)
            self.assertAlmostEqual(d.extents[0], 100.0, places=6)   # X: +/-50
            self.assertAlmostEqual(d.extents[1], 100.0, places=6)   # Y: +/-50
            # ...and NOT in Z, because the circle lies flat. A naive expansion that added
            # the radius on every axis would report a 100 mm cube.
            self.assertAlmostEqual(d.extents[2], 2.0, places=6)
        finally:
            path.unlink()

    def test_a_tilted_circle_expands_by_its_projection(self):
        # Normal at 45 degrees in the XZ plane: the circle reaches r/sqrt(2) along X and Z
        # and the full r along Y.
        n = 1.0 / math.sqrt(2.0)
        path = step_file(
            box_points(0, 0, 0, 1, 1, 1) +
            "#30=CARTESIAN_POINT('',(0.,0.,0.));\n"
            f"#31=DIRECTION('',({n},0.,{n}));\n"
            "#32=DIRECTION('',(0.,1.,0.));\n"
            "#33=AXIS2_PLACEMENT_3D('',#30,#31,#32);\n"
            "#34=CIRCLE('',#33,10.);\n")
        try:
            d = read_step(path)
            self.assertAlmostEqual(d.max_xyz[0], 10.0 * n, places=4)
            self.assertAlmostEqual(d.max_xyz[1], 10.0, places=4)
            self.assertAlmostEqual(d.max_xyz[2], 10.0 * n, places=4)
        finally:
            path.unlink()

    def test_the_cross_section_diagonal_uses_the_two_smallest_extents(self):
        path = step_file(box_points(0, 0, 0, 30, 200, 40))
        try:
            # 30 x 40 cross-section under a 200 mm length: a 50 mm diagonal.
            self.assertAlmostEqual(read_step(path).footprint_diagonal, 50.0, places=6)
        finally:
            path.unlink()


class RefusalTests(unittest.TestCase):
    """Refusing to answer beats answering wrongly."""

    def test_a_b_spline_file_is_refused_rather_than_under_measured(self):
        path = step_file(
            box_points(0, 0, 0, 1, 1, 1) +
            "#40=B_SPLINE_SURFACE_WITH_KNOTS('',3,3,((#10,#11)),.UNSPECIFIED.);\n")
        try:
            with self.assertRaises(ValueError) as caught:
                read_step(path)
            self.assertIn("B-spline", str(caught.exception))
        finally:
            path.unlink()

    def test_a_file_in_metres_is_refused(self):
        header = HEADER.replace("SI_UNIT(.MILLI.,.METRE.)", "SI_UNIT($,.METRE.)")
        path = step_file(box_points(0, 0, 0, 1, 1, 1), header=header)
        try:
            with self.assertRaises(ValueError) as caught:
                read_step(path)
            self.assertIn("millimetres", str(caught.exception))
        finally:
            path.unlink()

    def test_a_file_with_no_geometry_is_refused(self):
        path = step_file("#40=DIRECTION('',(0.,0.,1.));\n")
        try:
            with self.assertRaises(ValueError):
                read_step(path)
        finally:
            path.unlink()


class TheActualDesignTests(unittest.TestCase):
    """The file the mechanical documents describe.

    These pin the figures those documents quote. If the model is edited and re-exported,
    they fail here first and check_doc_claims.py fails second -- which is the intended
    order: the tool's own arithmetic before the prose that depends on it.
    """

    def setUp(self):
        if not DESIGN.exists():
            self.skipTest(f"{DESIGN} is not present")
        self.d = read_step(DESIGN)

    def test_it_is_one_solid_in_millimetres(self):
        self.assertEqual(self.d.solids, ("Body1",))
        self.assertIn(".MILLI.,.METRE.", self.d.units)

    def test_the_bounding_box(self):
        self.assertEqual(tuple(round(v, 1) for v in self.d.sorted_extents),
                         (118.5, 115.0, 110.0))

    def test_it_is_prismatic_rather_than_a_cylinder(self):
        # No surface anywhere near the 60 mm radius a 120 mm circular section would need.
        # This is the fact that makes the envelope reading an open question rather than a
        # settled one, so it is worth asserting rather than remembering.
        self.assertLess(max(self.d.radii), 30.0)

    def test_the_cross_section_diagonal_exceeds_120_mm(self):
        # 115 x 110 gives 159.1 mm corner to corner. Both faces are inside the rulebook's
        # 120 mm, and the diagonal is not -- which is the whole of open question 10.
        self.assertAlmostEqual(self.d.footprint_diagonal, 159.1, places=1)
        self.assertGreater(self.d.footprint_diagonal, 120.0)

    def test_the_report_names_the_figures_the_documents_quote(self):
        text = format_text(self.d)
        self.assertIn("118.5", text)
        self.assertIn("159.1", text)


class CommandLineTests(unittest.TestCase):
    def test_the_default_path_is_the_design(self):
        if not DESIGN.exists():
            self.skipTest("design absent")
        cwd = os.getcwd()
        os.chdir(REPO_ROOT)
        try:
            self.assertEqual(main([]), 0)
        finally:
            os.chdir(cwd)

    def test_a_missing_file_exits_two(self):
        self.assertEqual(main(["no-such-file.step"]), 2)


if __name__ == "__main__":
    unittest.main()
