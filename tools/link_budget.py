#!/usr/bin/env python3
"""LoRa time-on-air and telemetry-rate feasibility calculator for CanSat 2026.

The transmitted telemetry rate is bounded by physics, not by the scheduler period.
This tool computes the airtime of one LoRa packet from the modem parameters and the
payload length, using the Semtech SX1276/77/78/79 datasheet formulation (rev. 7,
section 4.1.1.7 "Time on air"), and reports the maximum packet rate the link can
sustain.

Formulation
-----------
    Tsym            = 2^SF / BW                                     (symbol period)
    Tpreamble       = (n_preamble + 4.25) * Tsym
    n_payload       = 8 + max(ceil((8*PL - 4*SF + 28 + 16*CRC - 20*IH)
                                   / (4 * (SF - 2*DE))) * (CR + 4), 0)
    Tpayload        = n_payload * Tsym
    ToA             = Tpreamble + Tpayload

where PL is the payload length in bytes, CRC is 1 when the payload CRC is enabled,
IH is 1 for implicit header mode, CR is 1..4 for coding rate 4/5..4/8, and DE is 1
when low-data-rate optimisation is on. The datasheet mandates DE when the symbol
period exceeds 16 ms, which this module applies automatically unless overridden.

Two published reference vectors are asserted by ``tools/tests/test_link_budget.py``:
SF7/BW125/CR4-5/PL13 = 46.336 ms, SF12/BW125/CR4-5/PL13 = 1155.072 ms.

Usage
-----
    python tools/link_budget.py --payload 188 --sf 9 --bw 125000
    python tools/link_budget.py --sweep --payload 188
    python tools/link_budget.py --sweep --payload 130 --target-rate 2 --format markdown

Nothing here is a competition requirement. The rulebook fixes only the 1 Hz minimum
packet rate and the sync words; every modem parameter is a project engineering choice
and must be confirmed against the RA-02 carrier and local radio regulations.
"""

from __future__ import annotations

import argparse
import math
import sys
from dataclasses import dataclass

# LoRa FIFO limit on the SX127x family. A payload above this cannot be sent in one packet.
MAX_LORA_PAYLOAD_BYTES = 255

# Rulebook CanSat 2026: telemetry must be transmitted at no less than 1 packet per second.
RULEBOOK_MIN_RATE_HZ = 1.0

VALID_SF = (6, 7, 8, 9, 10, 11, 12)
VALID_CR_DENOM = (5, 6, 7, 8)
COMMON_BANDWIDTHS_HZ = (7800, 10400, 15600, 20800, 31250, 41700, 62500, 125000, 250000, 500000)


@dataclass(frozen=True)
class ModemConfig:
    """One LoRa physical-layer configuration."""

    spreading_factor: int = 9
    bandwidth_hz: int = 125_000
    coding_rate_denominator: int = 5  # 5..8 meaning 4/5 .. 4/8
    preamble_symbols: int = 8
    explicit_header: bool = True
    crc_enabled: bool = True
    low_data_rate_optimize: bool | None = None  # None = datasheet rule (Tsym > 16 ms)

    def __post_init__(self) -> None:
        if self.spreading_factor not in VALID_SF:
            raise ValueError(f"spreading factor {self.spreading_factor} not in {VALID_SF}")
        if self.bandwidth_hz <= 0:
            raise ValueError("bandwidth must be positive")
        if self.coding_rate_denominator not in VALID_CR_DENOM:
            raise ValueError(
                f"coding rate denominator {self.coding_rate_denominator} not in {VALID_CR_DENOM}"
            )
        if self.preamble_symbols < 6:
            raise ValueError("preamble must be at least 6 symbols on SX127x")
        if self.spreading_factor == 6 and self.explicit_header:
            raise ValueError("SF6 requires implicit header mode on SX127x")

    @property
    def symbol_time_s(self) -> float:
        return (2**self.spreading_factor) / float(self.bandwidth_hz)

    @property
    def uses_low_data_rate_optimize(self) -> bool:
        if self.low_data_rate_optimize is not None:
            return self.low_data_rate_optimize
        # Datasheet: mandated when the symbol period exceeds 16 ms.
        return self.symbol_time_s > 16.0e-3

    @property
    def label(self) -> str:
        return (
            f"SF{self.spreading_factor}/BW{self.bandwidth_hz // 1000}k"
            f"/CR4-{self.coding_rate_denominator}"
        )


@dataclass(frozen=True)
class AirtimeResult:
    """Airtime of one packet plus the rate ceilings it implies."""

    config: ModemConfig
    payload_bytes: int
    symbol_time_s: float
    preamble_time_s: float
    payload_symbols: int
    payload_time_s: float
    time_on_air_s: float
    low_data_rate_optimize: bool

    @property
    def time_on_air_ms(self) -> float:
        return self.time_on_air_s * 1000.0

    @property
    def max_rate_hz(self) -> float:
        """Packet rate at 100 % channel occupancy — an unreachable upper bound."""
        return 1.0 / self.time_on_air_s

    def max_rate_at_duty(self, duty_cycle: float) -> float:
        """Packet rate that keeps channel occupancy at or below ``duty_cycle`` (0..1)."""
        if not 0.0 < duty_cycle <= 1.0:
            raise ValueError("duty cycle must be in (0, 1]")
        return duty_cycle / self.time_on_air_s

    def min_period_ms_at_duty(self, duty_cycle: float) -> float:
        return 1000.0 * self.time_on_air_s / duty_cycle

    @property
    def effective_bitrate_bps(self) -> float:
        """Payload bits divided by whole-packet airtime, including preamble and header."""
        return (self.payload_bytes * 8.0) / self.time_on_air_s


def payload_symbol_count(payload_bytes: int, config: ModemConfig) -> int:
    """Number of symbols carrying the payload, per the datasheet formula."""
    if payload_bytes < 0:
        raise ValueError("payload length cannot be negative")
    sf = config.spreading_factor
    de = 1 if config.uses_low_data_rate_optimize else 0
    ih = 0 if config.explicit_header else 1
    crc = 1 if config.crc_enabled else 0
    cr = config.coding_rate_denominator - 4

    denominator = 4 * (sf - 2 * de)
    numerator = 8 * payload_bytes - 4 * sf + 28 + 16 * crc - 20 * ih
    return 8 + max(math.ceil(numerator / denominator) * (cr + 4), 0)


def time_on_air(payload_bytes: int, config: ModemConfig | None = None) -> AirtimeResult:
    """Airtime of a single LoRa packet carrying ``payload_bytes`` of payload."""
    config = config or ModemConfig()
    tsym = config.symbol_time_s
    preamble_time = (config.preamble_symbols + 4.25) * tsym
    n_payload = payload_symbol_count(payload_bytes, config)
    payload_time = n_payload * tsym
    return AirtimeResult(
        config=config,
        payload_bytes=payload_bytes,
        symbol_time_s=tsym,
        preamble_time_s=preamble_time,
        payload_symbols=n_payload,
        payload_time_s=payload_time,
        time_on_air_s=preamble_time + payload_time,
        low_data_rate_optimize=config.uses_low_data_rate_optimize,
    )


def feasible(payload_bytes: int, target_rate_hz: float, config: ModemConfig,
             duty_cycle: float = 0.5) -> bool:
    """True when ``target_rate_hz`` fits inside ``duty_cycle`` of the channel."""
    return time_on_air(payload_bytes, config).max_rate_at_duty(duty_cycle) >= target_rate_hz


def sweep(payload_bytes: int, bandwidths_hz=(125_000, 250_000),
          spreading_factors=(7, 8, 9, 10, 11, 12),
          coding_rate_denominator: int = 5,
          preamble_symbols: int = 8) -> list[AirtimeResult]:
    """Airtime across a grid of spreading factors and bandwidths."""
    results: list[AirtimeResult] = []
    for bw in bandwidths_hz:
        for sf in spreading_factors:
            config = ModemConfig(
                spreading_factor=sf,
                bandwidth_hz=bw,
                coding_rate_denominator=coding_rate_denominator,
                preamble_symbols=preamble_symbols,
            )
            results.append(time_on_air(payload_bytes, config))
    return results


def _verdict(result: AirtimeResult, target_rate_hz: float, duty_cycle: float) -> str:
    if result.max_rate_at_duty(duty_cycle) >= target_rate_hz:
        return "ok"
    if result.max_rate_hz >= target_rate_hz:
        return "over duty"
    return "impossible"


def _format_table(results: list[AirtimeResult], target_rate_hz: float,
                  duty_cycle: float, markdown: bool) -> str:
    header = ["Config", "ToA (ms)", "Max rate (Hz)", f"Rate @ {duty_cycle:.0%} duty (Hz)",
              "Min period (ms)", f"{target_rate_hz:g} Hz"]
    rows = [
        [
            r.config.label,
            f"{r.time_on_air_ms:.1f}",
            f"{r.max_rate_hz:.2f}",
            f"{r.max_rate_at_duty(duty_cycle):.2f}",
            f"{r.min_period_ms_at_duty(duty_cycle):.0f}",
            _verdict(r, target_rate_hz, duty_cycle),
        ]
        for r in results
    ]
    if markdown:
        align = "|" + "|".join(["---"] + ["---:"] * 4 + ["---"]) + "|"
        lines = ["| " + " | ".join(header) + " |", align]
        lines += ["| " + " | ".join(row) + " |" for row in rows]
        return "\n".join(lines)

    widths = [max(len(header[i]), max((len(row[i]) for row in rows), default=0))
              for i in range(len(header))]
    out = ["  ".join(h.ljust(w) for h, w in zip(header, widths))]
    out.append("  ".join("-" * w for w in widths))
    out += ["  ".join(c.ljust(w) for c, w in zip(row, widths)) for row in rows]
    return "\n".join(out)


def _describe(result: AirtimeResult, target_rate_hz: float, duty_cycle: float) -> str:
    c = result.config
    lines = [
        f"Configuration        {c.label}, preamble {c.preamble_symbols} symbols, "
        f"{'explicit' if c.explicit_header else 'implicit'} header, "
        f"CRC {'on' if c.crc_enabled else 'off'}",
        f"Payload              {result.payload_bytes} bytes",
        f"Symbol period        {result.symbol_time_s * 1000:.3f} ms",
        f"Low-data-rate opt.   {'on' if result.low_data_rate_optimize else 'off'}",
        f"Preamble airtime     {result.preamble_time_s * 1000:.2f} ms",
        f"Payload symbols      {result.payload_symbols}",
        f"Payload airtime      {result.payload_time_s * 1000:.2f} ms",
        f"Time on air          {result.time_on_air_ms:.2f} ms",
        f"Effective bitrate    {result.effective_bitrate_bps:.0f} bit/s payload",
        f"Ceiling @100% duty   {result.max_rate_hz:.3f} Hz",
        f"Ceiling @{duty_cycle:.0%} duty    {result.max_rate_at_duty(duty_cycle):.3f} Hz "
        f"(min period {result.min_period_ms_at_duty(duty_cycle):.0f} ms)",
    ]
    verdict = _verdict(result, target_rate_hz, duty_cycle)
    if verdict == "ok":
        lines.append(f"Target {target_rate_hz:g} Hz         achievable within the duty budget")
    elif verdict == "over duty":
        lines.append(
            f"Target {target_rate_hz:g} Hz         NOT achievable within {duty_cycle:.0%} duty "
            f"(needs {result.time_on_air_s * target_rate_hz:.0%} of the channel)"
        )
    else:
        lines.append(
            f"Target {target_rate_hz:g} Hz         PHYSICALLY IMPOSSIBLE — one packet takes "
            f"{result.time_on_air_ms:.0f} ms"
        )
    if result.payload_bytes > MAX_LORA_PAYLOAD_BYTES:
        lines.append(
            f"WARNING              payload exceeds the {MAX_LORA_PAYLOAD_BYTES}-byte LoRa "
            "FIFO limit and cannot be sent as one packet"
        )
    if target_rate_hz < RULEBOOK_MIN_RATE_HZ:
        lines.append(
            f"WARNING              target is below the rulebook minimum of "
            f"{RULEBOOK_MIN_RATE_HZ:g} Hz"
        )
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="LoRa time-on-air and telemetry-rate feasibility calculator.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--payload", type=int, default=188,
                        help="payload length in bytes (default: 188, a full GPS+diagnostic packet)")
    parser.add_argument("--sf", type=int, default=9, choices=VALID_SF, help="spreading factor")
    parser.add_argument("--bw", type=int, default=125_000, help="bandwidth in Hz")
    parser.add_argument("--cr", type=int, default=5, choices=VALID_CR_DENOM,
                        help="coding rate denominator: 5 means 4/5")
    parser.add_argument("--preamble", type=int, default=8, help="preamble length in symbols")
    parser.add_argument("--implicit-header", action="store_true", help="implicit header mode")
    parser.add_argument("--no-crc", action="store_true", help="disable the payload CRC")
    parser.add_argument("--target-rate", type=float, default=2.0,
                        help="target packet rate in Hz to test for feasibility (default: 2)")
    parser.add_argument("--duty-cycle", type=float, default=0.5,
                        help="fraction of the channel the link may occupy (default: 0.5)")
    parser.add_argument("--sweep", action="store_true",
                        help="print a spreading-factor / bandwidth table instead")
    parser.add_argument("--format", choices=("text", "markdown"), default="text",
                        help="sweep table format")
    args = parser.parse_args(argv)

    if args.sweep:
        results = sweep(args.payload, coding_rate_denominator=args.cr,
                        preamble_symbols=args.preamble)
        print(f"Payload {args.payload} bytes, CR 4/{args.cr}, preamble {args.preamble} symbols, "
              f"target {args.target_rate:g} Hz, duty budget {args.duty_cycle:.0%}\n")
        print(_format_table(results, args.target_rate, args.duty_cycle,
                            markdown=args.format == "markdown"))
        return 0

    config = ModemConfig(
        spreading_factor=args.sf,
        bandwidth_hz=args.bw,
        coding_rate_denominator=args.cr,
        preamble_symbols=args.preamble,
        explicit_header=not args.implicit_header,
        crc_enabled=not args.no_crc,
    )
    print(_describe(time_on_air(args.payload, config), args.target_rate, args.duty_cycle))
    return 0


if __name__ == "__main__":
    sys.exit(main())
