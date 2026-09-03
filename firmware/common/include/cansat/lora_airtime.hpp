#pragma once

#include <cstddef>
#include <cstdint>

// LoRa time-on-air model, Semtech SX1276/77/78/79 datasheet (rev. 7, section 4.1.1.7).
//
// The transmitted telemetry rate is bounded by physics, not by the scheduler period. This
// header lets the flight computer compute, at startup, how long one packet will occupy the
// channel, and refuse a configuration that cannot meet the rulebook's 1 Hz minimum.
//
// The identical formulation lives in `tools/link_budget.py` for design-time analysis. Both
// are pinned to the same two published reference vectors:
//   SF7 / BW125 kHz / CR 4-5 / 13-byte payload  ->   46.336 ms
//   SF12 / BW125 kHz / CR 4-5 / 13-byte payload -> 1155.072 ms
//
// Everything here is arithmetic only: no allocation, no I/O, no floating-point traps. It is
// safe to call on the vehicle, and cheap enough that startup validation costs nothing
// measurable.

namespace cansat {

struct LoraModemParams {
    std::uint8_t spreading_factor = 7;    // 6..12
    std::uint32_t bandwidth_hz = 125000;  // modem bandwidth
    std::uint8_t coding_rate = 5;         // denominator: 5..8 meaning 4/5..4/8
    std::uint16_t preamble_symbols = 8;
    bool explicit_header = true;
    bool crc_enabled = true;
    // -1 = apply the datasheet rule (mandatory when the symbol period exceeds 16 ms).
    int low_data_rate_optimize = -1;
};

// Symbol period in seconds: 2^SF / BW.
constexpr double lora_symbol_time_s(const LoraModemParams& p) {
    return static_cast<double>(std::uint32_t{1} << p.spreading_factor) /
           static_cast<double>(p.bandwidth_hz);
}

constexpr bool lora_uses_low_data_rate_optimize(const LoraModemParams& p) {
    if (p.low_data_rate_optimize >= 0) return p.low_data_rate_optimize != 0;
    return lora_symbol_time_s(p) > 16.0e-3;
}

// Number of symbols carrying the payload, including the 8 fixed header symbols.
constexpr std::uint32_t lora_payload_symbols(std::size_t payload_bytes,
                                             const LoraModemParams& p) {
    const int sf = static_cast<int>(p.spreading_factor);
    const int de = lora_uses_low_data_rate_optimize(p) ? 1 : 0;
    const int ih = p.explicit_header ? 0 : 1;
    const int crc = p.crc_enabled ? 1 : 0;
    const int cr = static_cast<int>(p.coding_rate) - 4;

    const long numerator =
        8L * static_cast<long>(payload_bytes) - 4L * sf + 28L + 16L * crc - 20L * ih;
    const long denominator = 4L * (sf - 2 * de);
    if (denominator <= 0) return 8;  // impossible parameters; degrade to header only

    // Integer ceiling division that is correct for a negative numerator too (short
    // payloads at a high spreading factor drive it below zero).
    const long ceil_div =
        numerator > 0 ? (numerator + denominator - 1) / denominator : 0;
    const long symbols = ceil_div * (cr + 4);
    return 8U + static_cast<std::uint32_t>(symbols > 0 ? symbols : 0);
}

// Airtime of one packet, in seconds.
constexpr double lora_time_on_air_s(std::size_t payload_bytes, const LoraModemParams& p) {
    const double tsym = lora_symbol_time_s(p);
    const double preamble = (static_cast<double>(p.preamble_symbols) + 4.25) * tsym;
    return preamble + static_cast<double>(lora_payload_symbols(payload_bytes, p)) * tsym;
}

constexpr double lora_time_on_air_ms(std::size_t payload_bytes, const LoraModemParams& p) {
    return lora_time_on_air_s(payload_bytes, p) * 1000.0;
}

// Fraction of the channel one packet per `period_ms` occupies. 0.30 means 30 % duty.
constexpr double lora_channel_duty(std::size_t payload_bytes, const LoraModemParams& p,
                                   std::uint32_t period_ms) {
    if (period_ms == 0) return 1.0;
    return lora_time_on_air_ms(payload_bytes, p) / static_cast<double>(period_ms);
}

// Highest packet rate that keeps channel occupancy at or below `duty` (0 < duty <= 1).
constexpr double lora_max_rate_hz(std::size_t payload_bytes, const LoraModemParams& p,
                                  double duty) {
    const double toa = lora_time_on_air_s(payload_bytes, p);
    if (!(toa > 0.0)) return 0.0;
    return duty / toa;
}

// Largest LoRa payload the SX127x FIFO can carry in a single packet.
inline constexpr std::size_t kMaxLoraPayloadBytes = 255;

}  // namespace cansat
