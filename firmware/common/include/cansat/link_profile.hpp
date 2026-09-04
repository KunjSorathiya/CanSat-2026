#pragma once

#include <cstddef>
#include <cstdint>

#include "cansat/lora_airtime.hpp"

// THE radio link profile. One definition, used by both ends of the link.
//
// The flight computer and the ground-station bridge must agree on every modem parameter
// or no packet is ever received — a mismatch produces a silent, total link failure that
// looks identical to broken hardware. Before this header existed the two ends carried
// separate copies of the same numbers and agreed only by coincidence.
//
// Only the sync words are fixed by the competition rulebook. Every other value is a
// project engineering choice, justified in documentation/design/link-budget.md, and the
// static assertions at the bottom of this file refuse to compile a profile that cannot
// meet the rulebook's 1 Hz telemetry minimum.

namespace cansat::link {

// ---- Modem ----------------------------------------------------------------------
// PROVISIONAL: 433 MHz band; the exact channel is an open question for the organisers.
inline constexpr std::uint32_t kFrequencyHz = 433'000'000;
inline constexpr std::int8_t kTxPowerDbm = 17;  // PA_BOOST; RA-02 maximum without PA_DAC

// SF7 is airtime-driven, not range-driven. A ~190-byte packet needs 943 ms at SF9/125 kHz
// and 302 ms at SF7/125 kHz; only the latter meets 1 Hz with margin for retries. The link
// budget still closes with tens of dB to spare at the mission's ~1 km range.
inline constexpr std::uint8_t kSpreadingFactor = 7;
inline constexpr std::uint32_t kBandwidthHz = 125'000;
inline constexpr std::uint8_t kCodingRate = 5;  // 4/5
inline constexpr std::uint16_t kPreambleSymbols = 8;
inline constexpr bool kEnableCrc = true;

// ---- Rulebook-fixed identities ----------------------------------------------------
inline constexpr std::uint8_t kTestSyncWord = 0xF3;      // pre-launch testing
inline constexpr std::uint8_t kOfficialSyncWord = 0xA5;  // official launch

// ---- Airtime budget ---------------------------------------------------------------
// Budget the full LoRa FIFO, not a typical packet. Measured sizes from the formatter:
// 118 bytes mandatory-only, 167 with GPS, 206 with GPS and all four diagnostic tags, and
// 247 for the absolute worst case (longest team id, widest packet number, extreme values).
// An earlier 200-byte budget was below the *typical* in-flight packet, which would have
// under-estimated airtime on every transmission. 255 is the only figure that cannot be
// exceeded, so it is the only honest basis for the budget — and at SF7/125 kHz it still
// costs only 400 ms, 40 % of a 1 Hz slot.
//
// This is also the runtime cap: the controller drops its optional diagnostic tags rather
// than let a packet reach the radio's silent 255-byte truncation. Lower it to buy airtime
// margin, at the cost of dropping diagnostics from the longest packets.
inline constexpr std::size_t kWorstCasePacketBytes = 255;
inline constexpr std::uint32_t kTelemetryPeriodMs = 1000;  // 1 Hz rulebook minimum
inline constexpr double kMaxChannelDuty = 0.5;

// The profile as the airtime model sees it.
inline constexpr LoraModemParams kModem = [] {
    LoraModemParams p;
    p.spreading_factor = kSpreadingFactor;
    p.bandwidth_hz = kBandwidthHz;
    p.coding_rate = kCodingRate;
    p.preamble_symbols = kPreambleSymbols;
    p.explicit_header = true;
    p.crc_enabled = kEnableCrc;
    return p;
}();

inline constexpr double kWorstCaseAirtimeMs =
    lora_time_on_air_ms(kWorstCasePacketBytes, kModem);
inline constexpr double kChannelDuty = kWorstCaseAirtimeMs / kTelemetryPeriodMs;

// A profile that cannot transmit its own worst-case packet on schedule is a build error,
// not a flight-day discovery.
static_assert(kWorstCasePacketBytes <= kMaxLoraPayloadBytes,
              "worst-case packet exceeds the 255-byte LoRa FIFO");
static_assert(kTelemetryPeriodMs <= 1000,
              "telemetry period must satisfy the rulebook 1 Hz minimum");
static_assert(kChannelDuty <= kMaxChannelDuty,
              "link profile cannot sustain its telemetry period: reduce the spreading "
              "factor, widen the bandwidth, shorten the packet, or slow the schedule "
              "(see documentation/design/link-budget.md)");

}  // namespace cansat::link
