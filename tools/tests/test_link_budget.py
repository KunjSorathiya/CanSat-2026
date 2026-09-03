"""Tests for the LoRa time-on-air calculator.

The first two tests are the load-bearing ones: they pin the implementation to published
Semtech reference values. Everything else in this repository that reasons about telemetry
rate depends on these numbers being right.
"""

import contextlib
import io
import math
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from link_budget import (  # noqa: E402
    MAX_LORA_PAYLOAD_BYTES,
    ModemConfig,
    feasible,
    main,
    payload_symbol_count,
    sweep,
    time_on_air,
)


class ReferenceVectorTests(unittest.TestCase):
    """Published SX127x airtime values. These must not move."""

    def test_sf7_bw125_cr45_13_bytes_is_46_336_ms(self):
        config = ModemConfig(spreading_factor=7, bandwidth_hz=125_000,
                             coding_rate_denominator=5, preamble_symbols=8)
        self.assertAlmostEqual(time_on_air(13, config).time_on_air_ms, 46.336, places=3)

    def test_sf12_bw125_cr45_13_bytes_is_1155_072_ms(self):
        config = ModemConfig(spreading_factor=12, bandwidth_hz=125_000,
                             coding_rate_denominator=5, preamble_symbols=8)
        self.assertAlmostEqual(time_on_air(13, config).time_on_air_ms, 1155.072, places=3)

    def test_sf10_bw125_cr45_1_byte(self):
        # 8 + ceil((8 - 40 + 28 + 16) / 40) * 5 = 13 payload symbols, Tsym = 8.192 ms.
        config = ModemConfig(spreading_factor=10, bandwidth_hz=125_000)
        result = time_on_air(1, config)
        self.assertEqual(result.payload_symbols, 13)
        self.assertAlmostEqual(result.time_on_air_ms, (12.25 + 13) * 8.192, places=6)


class SymbolTimeTests(unittest.TestCase):
    def test_symbol_time_is_two_to_the_sf_over_bandwidth(self):
        config = ModemConfig(spreading_factor=9, bandwidth_hz=125_000)
        self.assertAlmostEqual(config.symbol_time_s, 512 / 125_000)

    def test_doubling_bandwidth_halves_symbol_time(self):
        narrow = ModemConfig(spreading_factor=9, bandwidth_hz=125_000).symbol_time_s
        wide = ModemConfig(spreading_factor=9, bandwidth_hz=250_000).symbol_time_s
        self.assertAlmostEqual(narrow / 2.0, wide)

    def test_low_data_rate_optimize_engages_above_16_ms_symbols(self):
        self.assertFalse(ModemConfig(spreading_factor=10).uses_low_data_rate_optimize)  # 8.192 ms
        self.assertTrue(ModemConfig(spreading_factor=11).uses_low_data_rate_optimize)  # 16.384 ms
        self.assertTrue(ModemConfig(spreading_factor=12).uses_low_data_rate_optimize)

    def test_low_data_rate_optimize_can_be_forced(self):
        forced_off = ModemConfig(spreading_factor=12, low_data_rate_optimize=False)
        self.assertFalse(forced_off.uses_low_data_rate_optimize)
        # DE=1 shrinks the formula's denominator, so optimisation never shortens a packet
        # and lengthens most of them.
        self.assertGreaterEqual(
            time_on_air(13, ModemConfig(spreading_factor=12)).time_on_air_s,
            time_on_air(13, forced_off).time_on_air_s,
        )
        self.assertGreater(
            time_on_air(188, ModemConfig(spreading_factor=12)).time_on_air_s,
            time_on_air(188, forced_off).time_on_air_s,
        )


class PayloadSymbolTests(unittest.TestCase):
    def test_symbol_count_never_drops_below_the_header_eight(self):
        # A tiny payload at a high SF drives the formula's max() term to zero.
        config = ModemConfig(spreading_factor=12, bandwidth_hz=125_000, crc_enabled=False)
        self.assertGreaterEqual(payload_symbol_count(0, config), 8)

    def test_symbol_count_is_monotonic_in_payload_length(self):
        config = ModemConfig(spreading_factor=9)
        counts = [payload_symbol_count(n, config) for n in range(0, 200, 7)]
        self.assertEqual(counts, sorted(counts))

    def test_crc_and_header_add_symbols(self):
        with_crc = payload_symbol_count(50, ModemConfig(spreading_factor=9, crc_enabled=True))
        without_crc = payload_symbol_count(50, ModemConfig(spreading_factor=9, crc_enabled=False))
        self.assertGreaterEqual(with_crc, without_crc)

        explicit = payload_symbol_count(50, ModemConfig(spreading_factor=9))
        implicit = payload_symbol_count(
            50, ModemConfig(spreading_factor=9, explicit_header=False))
        self.assertGreaterEqual(explicit, implicit)

    def test_higher_coding_rate_costs_more_symbols(self):
        light = payload_symbol_count(100, ModemConfig(coding_rate_denominator=5))
        heavy = payload_symbol_count(100, ModemConfig(coding_rate_denominator=8))
        self.assertGreater(heavy, light)

    def test_negative_payload_is_rejected(self):
        with self.assertRaises(ValueError):
            payload_symbol_count(-1, ModemConfig())


class RateTests(unittest.TestCase):
    def test_max_rate_is_the_inverse_of_airtime(self):
        result = time_on_air(188, ModemConfig(spreading_factor=9))
        self.assertAlmostEqual(result.max_rate_hz, 1.0 / result.time_on_air_s)

    def test_duty_cycle_scales_the_rate_linearly(self):
        result = time_on_air(188, ModemConfig(spreading_factor=9))
        self.assertAlmostEqual(result.max_rate_at_duty(0.5), result.max_rate_hz * 0.5)
        self.assertAlmostEqual(result.min_period_ms_at_duty(0.5),
                               2000.0 * result.time_on_air_s)

    def test_invalid_duty_cycles_are_rejected(self):
        result = time_on_air(50, ModemConfig())
        for bad in (0.0, -0.1, 1.5):
            with self.assertRaises(ValueError):
                result.max_rate_at_duty(bad)

    def test_effective_bitrate_is_payload_bits_over_airtime(self):
        result = time_on_air(100, ModemConfig(spreading_factor=7))
        self.assertAlmostEqual(result.effective_bitrate_bps, 800.0 / result.time_on_air_s)


class ProjectConfigurationTests(unittest.TestCase):
    """The findings this project acts on. If these change, the radio plan changes."""

    FULL_PACKET = 188   # team + mandatory + GPS + MODE/FAULTS diagnostics
    SLIM_PACKET = 130   # mandatory fields only, no GPS, no diagnostics

    def test_sf9_bw125_leaves_no_usable_margin_over_the_1_hz_minimum(self):
        # The original provisional default. One full packet occupies 943 ms of a 1000 ms
        # slot: nominally 1.06 Hz, but 94 % channel occupancy with no room for a retry.
        result = time_on_air(self.FULL_PACKET, ModemConfig(spreading_factor=9))
        self.assertAlmostEqual(result.time_on_air_ms, 943.1, delta=0.5)
        self.assertLess(result.max_rate_hz, 1.1)
        self.assertFalse(feasible(self.FULL_PACKET, 1.0, ModemConfig(spreading_factor=9),
                                  duty_cycle=0.5))

    def test_sf9_bw125_cannot_carry_a_full_packet_at_2_hz_at_all(self):
        result = time_on_air(self.FULL_PACKET, ModemConfig(spreading_factor=9))
        self.assertLess(result.max_rate_hz, 2.0)

    def test_sf7_bw125_carries_a_full_packet_at_1_hz_with_margin(self):
        # The chosen default: 302 ms airtime, 30 % occupancy at a 1 s period.
        config = ModemConfig(spreading_factor=7, bandwidth_hz=125_000)
        result = time_on_air(self.FULL_PACKET, config)
        self.assertAlmostEqual(result.time_on_air_ms, 302.3, delta=0.5)
        self.assertTrue(feasible(self.FULL_PACKET, 1.0, config, duty_cycle=0.35))

    def test_sf7_bw125_does_not_reach_2_hz_within_half_duty(self):
        # 2 Hz on this packet needs BW250 or a shorter packet, not just SF7.
        config = ModemConfig(spreading_factor=7, bandwidth_hz=125_000)
        self.assertFalse(feasible(self.FULL_PACKET, 2.0, config, duty_cycle=0.5))
        self.assertTrue(feasible(self.FULL_PACKET, 2.0,
                                 ModemConfig(spreading_factor=7, bandwidth_hz=250_000),
                                 duty_cycle=0.5))

    def test_dropping_gps_and_diagnostics_buys_meaningful_airtime(self):
        config = ModemConfig(spreading_factor=8)
        full = time_on_air(self.FULL_PACKET, config).time_on_air_s
        slim = time_on_air(self.SLIM_PACKET, config).time_on_air_s
        self.assertLess(slim, full)
        self.assertGreater(full - slim, 0.05)

    def test_30_hz_is_impossible_for_a_full_packet_on_any_standard_setting(self):
        for result in sweep(self.FULL_PACKET, bandwidths_hz=(125_000, 250_000)):
            self.assertLess(result.max_rate_hz, 30.0, msg=result.config.label)

    def test_full_packet_fits_the_lora_fifo(self):
        self.assertLessEqual(self.FULL_PACKET, MAX_LORA_PAYLOAD_BYTES)


class ValidationTests(unittest.TestCase):
    def test_invalid_spreading_factor_is_rejected(self):
        with self.assertRaises(ValueError):
            ModemConfig(spreading_factor=13)

    def test_invalid_coding_rate_is_rejected(self):
        with self.assertRaises(ValueError):
            ModemConfig(coding_rate_denominator=9)

    def test_non_positive_bandwidth_is_rejected(self):
        with self.assertRaises(ValueError):
            ModemConfig(bandwidth_hz=0)

    def test_short_preamble_is_rejected(self):
        with self.assertRaises(ValueError):
            ModemConfig(preamble_symbols=4)

    def test_sf6_requires_implicit_header(self):
        with self.assertRaises(ValueError):
            ModemConfig(spreading_factor=6, explicit_header=True)
        ModemConfig(spreading_factor=6, explicit_header=False)  # accepted


class SweepAndCliTests(unittest.TestCase):
    def test_sweep_covers_the_requested_grid(self):
        results = sweep(100, bandwidths_hz=(125_000, 250_000), spreading_factors=(7, 9, 12))
        self.assertEqual(len(results), 6)
        labels = {r.config.label for r in results}
        self.assertIn("SF7/BW125k/CR4-5", labels)
        self.assertIn("SF12/BW250k/CR4-5", labels)

    def test_airtime_falls_as_spreading_factor_falls(self):
        results = sweep(188, bandwidths_hz=(125_000,), spreading_factors=(7, 8, 9, 10, 11, 12))
        airtimes = [r.time_on_air_s for r in results]
        self.assertEqual(airtimes, sorted(airtimes))

    def test_cli_reports_a_single_configuration(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(main(["--payload", "188", "--sf", "9"]), 0)
        self.assertIn("Time on air", out.getvalue())
        self.assertIn("PHYSICALLY IMPOSSIBLE", out.getvalue())  # 2 Hz at SF9

    def test_cli_sweep_runs_in_both_formats(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(main(["--sweep", "--payload", "188"]), 0)
            self.assertEqual(main(["--sweep", "--payload", "188", "--format", "markdown"]), 0)
        self.assertIn("SF7/BW125k/CR4-5", out.getvalue())

    def test_airtime_is_finite_and_positive_across_the_grid(self):
        for result in sweep(255, bandwidths_hz=(125_000, 250_000, 500_000)):
            self.assertTrue(math.isfinite(result.time_on_air_s))
            self.assertGreater(result.time_on_air_s, 0.0)


if __name__ == "__main__":
    unittest.main()
