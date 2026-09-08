"""Tests for the descent model.

The load-bearing ones are first: they pin the module to closed-form limits that can be
checked by hand, and to the ISA sea-level air density. Everything the mechanical build
takes from this model -- the canopy diameter above all -- depends on those being right.
"""

import contextlib
import io
import math
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from descent import (  # noqa: E402
    CANOPY_TYPES,
    ISA_SEA_LEVEL_PRESSURE_PA,
    ISA_SEA_LEVEL_TEMPERATURE_C,
    MASS_TARGET_KG,
    RELEASE_ALTITUDE_M,
    RULEBOOK_MAX_DESCENT_MPS,
    STANDARD_GRAVITY,
    air_density,
    circular_area,
    circular_diameter,
    descend,
    fall_time,
    free_fall_distance,
    main,
    required_area,
    sweep,
    terminal_velocity,
    velocity_at,
)


class AtmosphereTests(unittest.TestCase):
    """The gas law, against the one value everybody knows."""

    def test_isa_sea_level_is_1_225_kg_per_m3(self):
        # The standard atmosphere's sea-level density, quoted everywhere to four figures.
        self.assertAlmostEqual(
            air_density(ISA_SEA_LEVEL_PRESSURE_PA, ISA_SEA_LEVEL_TEMPERATURE_C),
            1.2250, places=3)

    def test_a_hot_day_is_less_dense_than_the_standard_one(self):
        hot = air_density(ISA_SEA_LEVEL_PRESSURE_PA, 35.0)
        standard = air_density(ISA_SEA_LEVEL_PRESSURE_PA, ISA_SEA_LEVEL_TEMPERATURE_C)
        self.assertLess(hot, standard)
        # ~6.5 % thinner, which is ~7 % more canopy for the same descent rate. This is the
        # reason the model takes a temperature at all rather than assuming ISA.
        self.assertAlmostEqual(hot / standard, 288.15 / 308.15, places=6)

    def test_density_falls_with_pressure(self):
        self.assertLess(air_density(90_000.0, 15.0), air_density(101_325.0, 15.0))

    def test_impossible_atmospheres_are_refused(self):
        with self.assertRaises(ValueError):
            air_density(0.0, 15.0)
        with self.assertRaises(ValueError):
            air_density(101_325.0, -300.0)


class CanopySizingTests(unittest.TestCase):
    """Sizing and the rate it produces must be exact inverses of each other."""

    def test_a_sized_canopy_descends_at_exactly_its_target(self):
        density = air_density()
        for target in (3.0, 4.0, RULEBOOK_MAX_DESCENT_MPS, 6.5):
            area = required_area(MASS_TARGET_KG, target, 0.75, density)
            self.assertAlmostEqual(
                terminal_velocity(MASS_TARGET_KG, area, 0.75, density), target, places=9)

    def test_the_500_g_five_metre_case_by_hand(self):
        # S = 2 m g / (rho Cd v^2), with rho written out as the gas law rather than
        # taken from air_density(), so the two implementations have to agree.
        rho = 101325.0 / (287.058 * 288.15)
        expected = 2.0 * 0.5 * STANDARD_GRAVITY / (rho * 0.75 * 25.0)
        area = required_area(0.5, 5.0, 0.75, air_density())
        self.assertAlmostEqual(area, expected, places=9)
        # ...which is a canopy somewhere around three quarters of a metre across. If this
        # ever reads in centimetres or in metres-squared, the mechanical build gets a
        # parachute of the wrong order and nothing else here would catch it.
        self.assertTrue(0.6 < circular_diameter(area) < 0.9, circular_diameter(area))

    def test_diameter_and_area_round_trip(self):
        for diameter in (0.3, 0.52, 0.75, 1.2):
            self.assertAlmostEqual(circular_diameter(circular_area(diameter)), diameter,
                                   places=12)

    def test_a_heavier_vehicle_needs_more_canopy(self):
        density = air_density()
        light = required_area(0.45, 5.0, 0.75, density)
        heavy = required_area(0.55, 5.0, 0.75, density)
        self.assertGreater(heavy, light)
        # Area is linear in mass, so the +/-10 % mass tolerance is +/-10 % of area and
        # only ~5 % of diameter. Sizing at the top of the tolerance is nearly free.
        self.assertAlmostEqual(heavy / light, 0.55 / 0.45, places=9)

    def test_nonsense_inputs_are_refused(self):
        for bad in ((0.0, 5.0, 0.75, 1.225), (0.5, 0.0, 0.75, 1.225),
                    (0.5, 5.0, 0.0, 1.225), (0.5, 5.0, 0.75, 0.0)):
            with self.assertRaises(ValueError):
                required_area(*bad)
        with self.assertRaises(ValueError):
            circular_diameter(0.0)
        with self.assertRaises(ValueError):
            circular_area(-1.0)


class FallTimeTests(unittest.TestCase):
    """The closed-form fall, checked against both of its own limits."""

    def test_a_very_short_fall_is_free_fall(self):
        # For y << v_t^2 / g the canopy has not begun to matter, and t -> sqrt(2y/g).
        # Asserting "close to free fall" at one height only says the tolerance was chosen
        # generously enough. The leading correction is known -- expanding the closed form
        # gives t/t_freefall = 1 + x/6 + O(x^2) for x = g y / v_t^2 -- so assert that
        # instead: it pins the shape of the departure, not just its size.
        v = 5.0
        for height in (0.1, 0.01, 0.001):
            exact = fall_time(height, v)
            naive = math.sqrt(2.0 * height / STANDARD_GRAVITY)
            x = STANDARD_GRAVITY * height / v ** 2
            self.assertAlmostEqual((exact / naive - 1.0) / (x / 6.0), 1.0, places=2)
            # Drag only ever slows the fall, so the exact time is never the shorter one.
            self.assertGreater(exact, naive)

    def test_a_very_long_fall_is_height_over_terminal_plus_a_fixed_offset(self):
        # For y >> v_t^2 / g, t -> y/v_t + v_t ln2 / g. The offset is the start-up
        # transient, and it does not grow with the height.
        v = 5.0
        offset = v * math.log(2.0) / STANDARD_GRAVITY
        for height in (2000.0, 20_000.0):
            self.assertAlmostEqual(fall_time(height, v), height / v + offset, places=6)

    def test_a_real_descent_takes_longer_than_height_over_rate(self):
        # The naive quotient ignores the acceleration into terminal velocity. Over 30.48 m
        # at 5 m/s that is a third of a second -- small, but in the direction that means
        # "the flight is not shorter than you think".
        t = fall_time(RELEASE_ALTITUDE_M, 5.0)
        self.assertGreater(t, RELEASE_ALTITUDE_M / 5.0)
        self.assertLess(t - RELEASE_ALTITUDE_M / 5.0, 0.5)

    def test_the_solution_is_self_consistent(self):
        # y(t(y)) must return y for the pair of closed forms to describe one motion.
        v = 4.2
        for height in (1.0, 10.0, RELEASE_ALTITUDE_M, 300.0):
            t = fall_time(height, v)
            reconstructed = (v ** 2 / STANDARD_GRAVITY) * math.log(
                math.cosh(STANDARD_GRAVITY * t / v))
            self.assertAlmostEqual(reconstructed, height, places=6)

    def test_an_enormous_fall_does_not_overflow(self):
        # arccosh(exp(x)) computed literally overflows well before this. The asymptote is
        # used instead, and it must still be continuous with the exact branch.
        v = 0.5
        self.assertTrue(math.isfinite(fall_time(50_000.0, v)))
        self.assertAlmostEqual(fall_time(50_000.0, v),
                               50_000.0 / v + v * math.log(2.0) / STANDARD_GRAVITY,
                               places=6)

    def test_velocity_rises_to_terminal_and_stops_there(self):
        self.assertEqual(velocity_at(0.0, 5.0), 0.0)
        self.assertLess(velocity_at(0.2, 5.0), 5.0)
        self.assertAlmostEqual(velocity_at(60.0, 5.0), 5.0, places=9)
        self.assertLessEqual(velocity_at(1e6, 5.0), 5.0)

    def test_zero_height_takes_no_time(self):
        self.assertEqual(fall_time(0.0, 5.0), 0.0)

    def test_free_fall_is_half_g_t_squared(self):
        self.assertAlmostEqual(free_fall_distance(1.0), 0.5 * STANDARD_GRAVITY, places=12)
        self.assertEqual(free_fall_distance(0.0), 0.0)

    def test_negative_arguments_are_refused(self):
        with self.assertRaises(ValueError):
            fall_time(-1.0, 5.0)
        with self.assertRaises(ValueError):
            fall_time(10.0, 0.0)
        with self.assertRaises(ValueError):
            velocity_at(-1.0, 5.0)
        with self.assertRaises(ValueError):
            free_fall_distance(-1.0)


class MissionCaseTests(unittest.TestCase):
    """The case this project actually flies."""

    def test_the_default_case_is_the_rulebook_case(self):
        d = descend()
        self.assertEqual(d.mass_kg, MASS_TARGET_KG)
        self.assertEqual(d.release_altitude_m, RELEASE_ALTITUDE_M)
        self.assertAlmostEqual(d.terminal_mps, RULEBOOK_MAX_DESCENT_MPS, places=9)
        self.assertTrue(d.compliant())

    def test_the_descent_is_shorter_than_ten_seconds(self):
        # This is the finding that matters for telemetry, not for the parachute: a 100 ft
        # release at the 5 m/s cap is a descent of well under ten seconds, so the whole
        # descent dataset is a handful of packets.
        d = descend()
        self.assertLess(d.total_time_s, 10.0)
        self.assertGreater(d.total_time_s, 5.0)

    def test_the_packet_count_follows_the_telemetry_period(self):
        fast = descend(telemetry_period_ms=700.0)
        slow = descend(telemetry_period_ms=1000.0)
        self.assertGreater(fast.packets_in_descent, slow.packets_in_descent)
        # At the flight profile's 700 ms the descent yields packets in single figures.
        # If that ever reads as dozens, the release altitude or the rate has been changed.
        self.assertLess(fast.packets_in_descent, 20)
        self.assertGreaterEqual(fast.packets_in_descent, 5)

    def test_a_deployment_delay_costs_altitude_and_arrives_faster(self):
        prompt = descend(deployment_delay_s=0.0)
        delayed = descend(deployment_delay_s=1.0)
        self.assertGreater(delayed.free_fall_m, 4.0)   # ~4.9 m in the first second
        self.assertLess(delayed.canopy_fall_m, prompt.canopy_fall_m)
        # The canopy is sized for the same terminal rate either way, so the impact rate is
        # unchanged; what a slow deployment costs is the height it eats.
        self.assertAlmostEqual(delayed.terminal_mps, prompt.terminal_mps, places=9)

    def test_a_canopy_that_opens_too_late_is_an_error_not_a_number(self):
        # 2.5 s of free fall is ~30.6 m, which is the whole release altitude. Returning a
        # negative descent would be worse than refusing.
        with self.assertRaises(ValueError):
            descend(deployment_delay_s=2.5)

    def test_an_undersized_canopy_is_reported_as_non_compliant(self):
        d = descend(diameter_m=0.30)
        self.assertGreater(d.impact_mps, RULEBOOK_MAX_DESCENT_MPS)
        self.assertFalse(d.compliant())

    def test_a_hot_launch_day_needs_a_bigger_canopy(self):
        cool = descend(temperature_c=15.0)
        hot = descend(temperature_c=40.0)
        self.assertGreater(hot.diameter_m, cool.diameter_m)
        self.assertTrue(hot.compliant())

    def test_the_mass_tolerance_band_stays_compliant_when_sized_for_it(self):
        for mass in (0.450, 0.500, 0.550):
            self.assertTrue(descend(mass_kg=mass).compliant())

    def test_a_canopy_sized_for_500_g_is_too_small_at_550(self):
        # Sizing at the nominal mass and flying at the top of the tolerance breaks the cap.
        # This is the argument for sizing at 550 g and accepting a slower descent.
        nominal = descend(mass_kg=0.500)
        heavy = descend(mass_kg=0.550, diameter_m=nominal.diameter_m)
        self.assertGreater(heavy.impact_mps, RULEBOOK_MAX_DESCENT_MPS)

    def test_sizing_at_the_top_of_the_tolerance_covers_the_whole_band(self):
        sized = descend(mass_kg=0.550)
        for mass in (0.450, 0.500, 0.550):
            flown = descend(mass_kg=mass, diameter_m=sized.diameter_m)
            self.assertTrue(flown.compliant(), (mass, flown.impact_mps))

    def test_an_unknown_canopy_is_refused(self):
        with self.assertRaises(ValueError):
            descend(canopy="ram-air")

    def test_an_explicit_cd_overrides_an_unknown_canopy(self):
        d = descend(canopy="ram-air", drag_coefficient=0.9)
        self.assertEqual(d.drag_coefficient, 0.9)

    def test_a_non_positive_telemetry_period_is_refused(self):
        with self.assertRaises(ValueError):
            descend(telemetry_period_ms=0.0)


class SweepTests(unittest.TestCase):
    def test_the_sweep_covers_every_canopy_type(self):
        results = sweep(MASS_TARGET_KG, RULEBOOK_MAX_DESCENT_MPS,
                        ISA_SEA_LEVEL_PRESSURE_PA, ISA_SEA_LEVEL_TEMPERATURE_C,
                        RELEASE_ALTITUDE_M, 700.0)
        self.assertEqual({d.canopy for d in results}, set(CANOPY_TYPES))

    def test_a_higher_drag_coefficient_needs_less_cloth(self):
        results = {d.canopy: d for d in
                   sweep(MASS_TARGET_KG, RULEBOOK_MAX_DESCENT_MPS,
                         ISA_SEA_LEVEL_PRESSURE_PA, ISA_SEA_LEVEL_TEMPERATURE_C,
                         RELEASE_ALTITUDE_M, 700.0)}
        self.assertLess(results["hemispherical"].diameter_m,
                        results["vented-flat"].diameter_m)

    def test_every_swept_canopy_meets_the_cap_it_was_sized_for(self):
        for d in sweep(MASS_TARGET_KG, RULEBOOK_MAX_DESCENT_MPS,
                       ISA_SEA_LEVEL_PRESSURE_PA, ISA_SEA_LEVEL_TEMPERATURE_C,
                       RELEASE_ALTITUDE_M, 700.0):
            self.assertTrue(d.compliant(), d.canopy)


class CommandLineTests(unittest.TestCase):
    def _run(self, argv):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = main(argv)
        return code, out.getvalue(), err.getvalue()

    def test_the_default_run_succeeds_and_reports_a_diameter(self):
        code, out, _ = self._run([])
        self.assertEqual(code, 0)
        self.assertIn("Flat diameter", out)
        self.assertIn("Packets in descent", out)

    def test_markdown_output_is_a_table(self):
        code, out, _ = self._run(["--format", "markdown"])
        self.assertEqual(code, 0)
        self.assertTrue(out.startswith("| Quantity | Value |"))

    def test_the_sweep_prints_every_canopy(self):
        code, out, _ = self._run(["--sweep"])
        self.assertEqual(code, 0)
        for name in CANOPY_TYPES:
            self.assertIn(name, out)

    def test_a_markdown_sweep_is_a_table(self):
        code, out, _ = self._run(["--sweep", "--format", "markdown"])
        self.assertEqual(code, 0)
        self.assertIn("| Canopy | Cd |", out.replace(" Diameter | Area | Rate | Descent |",
                                                     ""))

    def test_an_undersized_canopy_exits_non_zero_with_a_warning(self):
        code, _, err = self._run(["--diameter", "0.25"])
        self.assertEqual(code, 1)
        self.assertIn("exceeds the rulebook", err)

    def test_an_impossible_case_exits_two(self):
        code, _, err = self._run(["--deployment-delay", "5"])
        self.assertEqual(code, 2)
        self.assertIn("error:", err)


if __name__ == "__main__":
    unittest.main()
