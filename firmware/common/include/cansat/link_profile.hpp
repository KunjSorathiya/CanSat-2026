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

// ---- The telemetry-rate floor, and why it is not 1000 ms ---------------------------
//
// The rulebook states 1 packet per second as a MINIMUM, and the 2026 revision separately
// scores rates above it. A period of exactly 1000 ms satisfies the letter of that and is
// still the wrong number to build, for two independent reasons:
//
//   1. It sits ON the requirement. The interval a ground station measures is the
//      transmit period plus whatever jitter the loop, the radio and the receiver add. At
//      exactly 1000 ms every one of those pushes an interval past a second, and the
//      vehicle is momentarily below a requirement that is checked rather than estimated.
//      Measured mission-clock jitter is under 4 ms (bring-up row 5.4), so 50 ms of margin
//      is more than an order of magnitude of headroom -- cheap, and it means the vehicle
//      is never at the line.
//
//   2. It scores nothing. Rate above 1 Hz is a scored line, and 1.00 Hz is the floor of
//      it.
//
// So the ceiling below is a HARD LIMIT, not a default. It is enforced three times over,
// because a flight build silently running at 1 Hz is exactly the failure this is for:
// a static_assert at the bottom of this file refuses to compile a profile above it,
// validate_config() refuses to run a Configuration above it, and check_doc_claims.py
// refuses to pass a repository whose documentation disagrees with it.
inline constexpr std::uint32_t kRulebookMinRatePeriodMs = 1000;  // 1 Hz, the rulebook floor
inline constexpr std::uint32_t kTelemetryJitterMarginMs = 50;    // >12x the measured jitter
inline constexpr std::uint32_t kMaxTelemetryPeriodMs =
    kRulebookMinRatePeriodMs - kTelemetryJitterMarginMs;          // 950 ms, 1.053 Hz

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
// The longest packet **this configuration** can transmit, measured rather than guessed.
//
// With the GPS fields on the air it is exactly 255 — the LoRa FIFO limit — which is a
// ceiling the format grew into rather than a coincidence: 201 bytes of mandatory fields and
// position, plus 54 bytes of worst-case `MODE`/`FAULTS`/`CAL`/`ARM`/`YR` tags. With
// `Configuration::transmit_gps` false, which is the default, the three `GP-` fields are
// logged instead of transmitted and the worst case is **199**.
//
// That 56-byte difference is worth having. Airtime scales with length, the telemetry period
// is computed from the worst case, and 199 bytes buys 1.43 Hz where 255 bytes allows 1.18 —
// on a scoring line that rewards rates above 1 Hz. The position itself loses nothing: it is
// in every SD row, and SEN-011 asks for data "transmitted or logged".
//
// This is also the runtime cap: the controller drops its optional diagnostic tags rather
// than let a packet reach the radio's silent 255-byte truncation. Raise it back to 255
// alongside `transmit_gps`; `validate_config()` refuses the two settings apart.
inline constexpr std::size_t kWorstCasePacketBytes = 199;
// The FIFO's own limit, and the figure the budget must return to if GPS is transmitted.
inline constexpr std::size_t kWorstCasePacketBytesWithGps = 255;
// 700 ms, 1.43 Hz. The rulebook's 1 Hz is a *minimum*, and this is as fast as the link can
// be driven without breaking the duty policy below.
//
// The arithmetic, and it is deliberately built on the measured airtime rather than the
// model. The model reads 1.8 % low: a 255-byte packet costs 399.6 ms by the model and
// **406.9 ms measured on this hardware**, twice, on two boards (bring-up rows 5.2 and
// 5.3). Applying that same 1.8 % to the 199-byte packet's 317.7 ms model figure gives
// ~323 ms, so the 50 % duty cap is a floor of ~647 ms; 700 leaves the real duty at 46 %
// instead of sitting on the limit.
//
// **A second reason to be under 1000 rather than on it.** At exactly 1 Hz any jitter puts
// an interval over a second and the vehicle momentarily below the rulebook minimum. 700
// carries 300 ms of margin against a requirement that is checked, not estimated.
//
// Going faster now needs a wider bandwidth (250 kHz halves airtime and costs 3 dB of
// sensitivity, so range) or a higher duty cap (a regulatory and courtesy question on a
// band shared with every other team, not an engineering one). The packet itself has
// already given up the only 56 bytes it had to give.
inline constexpr std::uint32_t kTelemetryPeriodMs = 700;
inline constexpr double kMaxChannelDuty = 0.5;

// ---- Commanded maximum rates -------------------------------------------------------
// Two ground commands raise the rate for the rest of the power cycle, each with its own
// packet budget. Neither can be undone: the same command closes the uplink behind itself.
// The full reasoning is in documentation/design/max-rate-command.md; what matters here is
// where the numbers come from.
//
// Both shed the five project-local diagnostic tags -- MODE, FAULTS, CAL, ARM and YR --
// which the rulebook's mandated packet does not contain. The GPS variant spends what they
// cost on the three GP- fields instead, and that is free: LoRa quantises the payload into
// symbol blocks, so 201 bytes and 199 both come to 298 symbols at SF7/125 kHz and cost the
// same 317.70 ms. Position on the air costs nothing but the tags it replaces.
inline constexpr std::size_t kMaxRatePacketBytesGps = 201;
inline constexpr std::size_t kMaxRatePacketBytesLean = 145;

// What the vehicle must be able to do between two transmits: one SD block write at its
// measured worst case -- 30 ms, on two different boards, in two of five sessions (bring-up
// F-11), which is a healthy card's internal housekeeping rather than a fault and does not
// go away because recent runs were clean -- plus the sensor loop and the watchdog feed.
//
// The period is airtime + this, NOT airtime / duty. The rulebook scores consistency on the
// same five points as rate ("provided transmissions remain consistent", "packet loss will
// reduce the score"), so a period that fits the duty policy but not the card buys rate by
// making packets late, on the very line the rate was bought for.
inline constexpr std::uint32_t kMaxRateGuardMs = 40;

// Rounded up, not to taste: 323.41 + 40 is 363.41 and 240.02 + 40 is 280.02, so 363 and 280
// are both inside the guard by a fraction of a millisecond. The static_asserts below refused
// them, which is the whole reason those asserts exist rather than a comment saying "check
// the arithmetic".
inline constexpr std::uint32_t kMaxRatePeriodGpsMs = 364;   // 2.75 Hz
inline constexpr std::uint32_t kMaxRatePeriodLeanMs = 281;  // 3.56 Hz

// This hardware transmits 1.8 % slower than the model (bring-up rows 5.2 and 5.3, 406.9 ms
// measured against 399.6 modelled, twice, on two boards). A period that clears the model
// and not the measurement is a period that fails on the bench rather than in the build.
inline constexpr double kAirtimeMeasuredFactor = 1.018;

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

inline constexpr double kMaxRateAirtimeGpsMs =
    lora_time_on_air_ms(kMaxRatePacketBytesGps, kModem);
inline constexpr double kMaxRateAirtimeLeanMs =
    lora_time_on_air_ms(kMaxRatePacketBytesLean, kModem);

static_assert(kMaxRatePacketBytesGps <= kMaxLoraPayloadBytes,
              "the GPS max-rate budget exceeds the 255-byte LoRa FIFO");
static_assert(kMaxRatePacketBytesLean < kMaxRatePacketBytesGps,
              "the lean max-rate budget is not leaner than the GPS one");
static_assert(kMaxRatePeriodLeanMs < kMaxRatePeriodGpsMs,
              "the lean max-rate period is not faster than the GPS one");
static_assert(kMaxRatePeriodGpsMs < kTelemetryPeriodMs,
              "a commanded maximum that is not faster than normal flight is not a maximum");
// The guard, enforced. A packet cannot be transmitted in less than its airtime, and the
// vehicle cannot log it in no time at all.
static_assert(static_cast<double>(kMaxRatePeriodGpsMs) >=
                  kMaxRateAirtimeGpsMs * kAirtimeMeasuredFactor + kMaxRateGuardMs,
              "the MAX_RATE_GPS period is inside its own measured airtime plus the guard");
static_assert(static_cast<double>(kMaxRatePeriodLeanMs) >=
                  kMaxRateAirtimeLeanMs * kAirtimeMeasuredFactor + kMaxRateGuardMs,
              "the MAX_RATE_LEAN period is inside its own measured airtime plus the guard");

static_assert(kTelemetryPeriodMs <= kMaxTelemetryPeriodMs,
              "telemetry period must be STRICTLY faster than the rulebook's 1 Hz minimum, "
              "with jitter margin: see kMaxTelemetryPeriodMs above");
static_assert(kTelemetryPeriodMs > 0, "telemetry period must be non-zero");
static_assert(kChannelDuty <= kMaxChannelDuty,
              "link profile cannot sustain its telemetry period: reduce the spreading "
              "factor, widen the bandwidth, shorten the packet, or slow the schedule "
              "(see documentation/design/link-budget.md)");

}  // namespace cansat::link
