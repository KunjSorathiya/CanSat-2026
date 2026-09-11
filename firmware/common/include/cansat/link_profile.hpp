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
// The packet this vehicle transmits is one of two shapes, and the budget is the larger.
//
// **Rich**: the twelve mandatory fields, then `GP-Lat`, `GP-Lon`, `GP-Alt`, then `SN-` for
// the sound level. **Lean**: the twelve mandatory fields only. The five project-local
// diagnostic tags -- `MODE`, `FAULTS`, `CAL`, `ARM`, `YR` -- are no longer transmitted in
// any mode; they go to the SD log.
//
// Why the shape changed: the organizers ruled on 2026-09-11 that only *transmitted*
// telemetry is considered for extra-sensor points. GPS and sound had been logged and not
// transmitted, so under that ruling they scored nothing. And there is no budget that holds
// the tags as well: tags plus GPS already fill the 255-byte FIFO, and sound takes the
// total to 266. Something had to leave the air, and the tags are the only part the
// rulebook does not reward. See documentation/design/max-rate-command.md.
//
// Every figure is a worst case, and the widths are defended by a test that constructs the
// widest packet of each shape rather than by this arithmetic -- at packet number
// 4294967295, a 99:59:59:999 mission clock and the extreme values the overflow test uses.
//
// That test is why these read 147 and 55. The first revision of this design carried 145
// and 56 from a commit message, and 147 is not a rounding difference: it crosses a LoRa
// symbol boundary 145 does not (43 blocks against 42), which is 5 ms of airtime on every
// lean packet and 5 ms on the lean slot.
inline constexpr std::size_t kMandatoryPacketBytes = 147;
inline constexpr std::size_t kGpsFieldBytes = 55;       // the three GP- fields and separators
inline constexpr std::size_t kSoundFieldBytes = 11;     // "SN-3300.0; " -- the ADC reference
inline constexpr std::size_t kDiagnosticTagBytes = 54;  // the five tags, when they flew
inline constexpr std::size_t kRichPacketBytes = 213;
inline constexpr std::size_t kLeanPacketBytes = 147;

// The normal-flight budget is the rich packet: every packet in normal flight is rich,
// because at a 700 ms cadence that is the only way to put the sensors on the air at least
// once a second. This is also the runtime cap the controller sheds optional content to
// stay under, rather than let a packet reach the radio's silent 255-byte truncation.
inline constexpr std::size_t kWorstCasePacketBytes = 213;
// Tags plus GPS: exactly the FIFO. validate_config() now computes the floor from what
// is enabled, and this names the one legacy combination that fills the FIFO to the byte.
inline constexpr std::size_t kWorstCasePacketBytesWithGps = 255;
// 700 ms, 1.43 Hz. The rulebook's 1 Hz is a *minimum*, and this is as fast as the link can
// be driven without breaking the duty policy below.
//
// The arithmetic, and it is deliberately built on the measured airtime rather than the
// model. The model reads 1.8 % low: a 255-byte packet costs 399.6 ms by the model and
// **406.9 ms measured on this hardware**, twice, on two boards (bring-up rows 5.2 and
// 5.3). Applying that same 1.8 % to the 213-byte rich packet's 338.2 ms model figure gives
// ~344 ms, so the 50 % duty cap is a floor of ~689 ms. 700 leaves the real duty at 49 % --
// under the limit, though not by much, and that narrowing is what GPS and sound on the air
// cost in normal flight.
//
// **A second reason to be under 1000 rather than on it.** At exactly 1 Hz any jitter puts
// an interval over a second and the vehicle momentarily below the rulebook minimum. 700
// carries 300 ms of margin against a requirement that is checked, not estimated.
//
// Normal flight cannot go faster without a wider bandwidth (250 kHz halves airtime and
// costs 3 dB of sensitivity, so range) or a higher duty cap. The max-rate schedule below
// goes faster a different way: by sending lean packets between the rich ones.
inline constexpr std::uint32_t kTelemetryPeriodMs = 700;
inline constexpr double kMaxChannelDuty = 0.5;

// ---- The max-rate schedule ---------------------------------------------------------
// One ground command, MAX_RATE, moves the vehicle from the fixed 700 ms period to a
// repeating pattern of one rich packet and two lean ones for the rest of the power cycle,
// and closes the uplink behind it. See documentation/design/max-rate-command.md.

// What the vehicle must be able to do between two transmits: one SD block write at its
// measured worst case -- 30 ms, on two different boards, in two of five sessions (bring-up
// F-11), which is a healthy card's internal housekeeping rather than a fault and does not
// go away because recent runs were clean -- plus the sensor loop and the watchdog feed.
//
// A slot is airtime + this, NOT airtime / duty. The rulebook scores consistency on the same
// five points as rate ("provided transmissions remain consistent", "packet loss will reduce
// the score"), so a slot that fits a duty target but not the card buys rate by making
// packets late, on the very line the rate was bought for.
inline constexpr std::uint32_t kMaxRateGuardMs = 40;

// This hardware transmits 1.8 % slower than the model (bring-up rows 5.2 and 5.3, 406.9 ms
// measured against 399.6 modelled, twice, on two boards). A period that clears the model
// and not the measurement is a period that fails on the bench rather than in the build.
inline constexpr double kAirtimeMeasuredFactor = 1.018;

// The max-rate schedule: a repeating pattern of one rich packet then two lean ones, each in
// its own slot. A slot is the shape's measured airtime plus kMaxRateGuardMs, rounded up.
//
// Three is not a preference. The requirement is GPS and sound on the air at least once a
// second, so the cycle -- one rich slot and N lean -- must fit in 1000 ms: N = 2 gives 957,
// N = 3 gives 1243. The static_asserts below hold both halves of that.
inline constexpr std::uint32_t kMaxRateRichSlotMs = 385;   // 338.18 x 1.018 + 40 = 384.26
inline constexpr std::uint32_t kMaxRateLeanSlotMs = 286;   // 240.90 x 1.018 + 40 = 285.24
inline constexpr std::uint32_t kMaxRateLeanPerRich = 2;
inline constexpr std::uint32_t kMaxRateCycleMs = 957;      // 3.13 Hz, sensors at 1.04 Hz

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


inline constexpr double kRichAirtimeMs = lora_time_on_air_ms(kRichPacketBytes, kModem);
inline constexpr double kLeanAirtimeMs = lora_time_on_air_ms(kLeanPacketBytes, kModem);

static_assert(kRichPacketBytes == kMandatoryPacketBytes + kGpsFieldBytes + kSoundFieldBytes,
              "the rich packet is not mandatory + GPS + sound");
static_assert(kLeanPacketBytes == kMandatoryPacketBytes, "the lean packet is not mandatory-only");
static_assert(kWorstCasePacketBytes == kRichPacketBytes,
              "normal flight sends every packet rich, so its budget is the rich packet");
static_assert(kRichPacketBytes <= kMaxLoraPayloadBytes, "the rich packet exceeds the FIFO");
// The design's reason for taking the tags off the air, held as a fact about the bytes: if
// tags, GPS and sound ever fit together, the reason is gone and the tags should come back.
static_assert(kMandatoryPacketBytes + kGpsFieldBytes + kSoundFieldBytes + kDiagnosticTagBytes >
                  kMaxLoraPayloadBytes,
              "tags + GPS + sound now fit the FIFO -- revisit dropping the tags");
static_assert(kRichAirtimeMs / kTelemetryPeriodMs <= kMaxChannelDuty,
              "a rich packet every 700 ms breaks the duty cap on the model");
static_assert(static_cast<double>(kMaxRateRichSlotMs) >=
                  kRichAirtimeMs * kAirtimeMeasuredFactor + kMaxRateGuardMs,
              "the rich slot is inside its own measured airtime plus the guard");
static_assert(static_cast<double>(kMaxRateLeanSlotMs) >=
                  kLeanAirtimeMs * kAirtimeMeasuredFactor + kMaxRateGuardMs,
              "the lean slot is inside its own measured airtime plus the guard");
static_assert(kMaxRateCycleMs == kMaxRateRichSlotMs + kMaxRateLeanPerRich * kMaxRateLeanSlotMs,
              "the cycle is not one rich slot and kMaxRateLeanPerRich lean ones");
static_assert(kMaxRateCycleMs <= 1000, "the max-rate cycle puts GPS and sound below 1 Hz");
static_assert(kMaxRateRichSlotMs + (kMaxRateLeanPerRich + 1) * kMaxRateLeanSlotMs > 1000,
              "one more lean slot would still keep the sensors at 1 Hz -- the pattern is not "
              "the largest it can be");

static_assert(kTelemetryPeriodMs <= kMaxTelemetryPeriodMs,
              "telemetry period must be STRICTLY faster than the rulebook's 1 Hz minimum, "
              "with jitter margin: see kMaxTelemetryPeriodMs above");
static_assert(kTelemetryPeriodMs > 0, "telemetry period must be non-zero");
static_assert(kChannelDuty <= kMaxChannelDuty,
              "link profile cannot sustain its telemetry period: reduce the spreading "
              "factor, widen the bandwidth, shorten the packet, or slow the schedule "
              "(see documentation/design/link-budget.md)");

}  // namespace cansat::link
