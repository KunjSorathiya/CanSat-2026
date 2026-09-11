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
// Both Pico images now fly the official word, for testing as well as the launch: the
// organizers' ground station listens on 0xA5 and nothing else, and a vehicle on the test
// word reached it only in the few packets the SX127x's sync filter lets through.

// ---- Airtime budget ---------------------------------------------------------------
// The longest packet this vehicle transmits, and why it is 200.
//
// **The organizers' ground station discards any packet over 200 bytes.** Their receiver --
// an ESP32 running the arduino-LoRa library, shared with the team on 2026-09-11 -- reads
// into a 201-byte buffer and throws away anything with packetSize > 200, printing an error
// and scoring nothing. The competition scores what *their* station receives, so 200 is the
// ceiling for every packet, whatever this project's own bridge can hear.
inline constexpr std::size_t kGroundStationMaxPacketBytes = 200;

// The packet is one of two shapes.
//
// **Rich**: the twelve mandatory fields, then `GP-Lat`, `GP-Lon`, `GP-Alt`, then `SN-` for
// the sound level. **Lean**: the twelve mandatory fields only. The five project-local
// diagnostic tags -- `MODE`, `FAULTS`, `CAL`, `ARM`, `YR` -- are not transmitted in any
// mode; they go to the SD log. (The organizers ruled on 2026-09-11 that only *transmitted*
// telemetry counts for extra-sensor points, and the tags are the one part of the packet the
// rulebook does not reward. See documentation/design/max-rate-command.md.)
//
// Every figure is a worst case, and the widths are defended by a test that constructs the
// widest packet of each shape -- packet number 4294967295, a 99:59:59:999 mission clock and
// the extreme values the overflow test uses -- rather than by this arithmetic.
//
// The GPS block is 51 because it is printed to what the receiver resolves and no further:
// latitude and longitude to 5 decimals (1.1 m, against the NEO-6M's ~2.5 m horizontal
// error) and altitude to whole metres (its vertical error is several). At 6 decimals and 1
// it was 55, and mandatory + GPS was 202 -- two bytes over the organizers' ceiling.
inline constexpr std::size_t kMandatoryPacketBytes = 147;
inline constexpr std::size_t kGpsFieldBytes = 51;       // the three GP- fields and separators
inline constexpr std::size_t kSoundFieldBytes = 11;     // " SN-3300.0;" -- the ADC reference
inline constexpr std::size_t kDiagnosticTagBytes = 54;  // the five tags, when they flew
inline constexpr std::size_t kRichPacketBytes = 209;    // all three at their widest together
inline constexpr std::size_t kLeanPacketBytes = 147;

// The budget, and the runtime cap the controller sheds optional content to stay under.
//
// Mandatory + GPS fits it by construction: 198. Sound fits beside them whenever they leave
// 11 bytes, and every rich packet of the 2026-09-10 range test left more than 20 -- the
// widest was 184 bytes, at the old GPS precision. The widest rich packet by construction
// (209) needs a ten-digit packet number, a 99-hour clock and every axis at full scale at
// once. If a packet would pass 200 anyway, the controller drops SN- from that one packet,
// then GP-, and never the mandatory data.
inline constexpr std::size_t kWorstCasePacketBytes = 200;
// A bench build with the diagnostic tags on the air may budget the whole FIFO: it is heard
// by this project's bridge and not scored by anyone. validate_config() allows it only with
// the tags on, so a flight build cannot reach it by accident.
inline constexpr std::size_t kBenchPacketBytes = 255;
// 700 ms, 1.43 Hz. The rulebook's 1 Hz is a *minimum*, and this is as fast as the link can
// be driven without breaking the duty policy below.
//
// The arithmetic, and it is deliberately built on the measured airtime rather than the
// model. The model reads 1.8 % low: a 255-byte packet costs 399.6 ms by the model and
// **406.9 ms measured on this hardware**, twice, on two boards (bring-up rows 5.2 and
// 5.3). Applying that same 1.8 % to the 200-byte budget's 317.7 ms model figure gives
// ~323 ms, so the 50 % duty cap is a floor of ~647 ms. 700 leaves the real duty at 46 %.
//
// **A second reason to be under 1000 rather than on it.** At exactly 1 Hz any jitter puts
// an interval over a second and the vehicle momentarily below the rulebook minimum. 700
// carries 300 ms of margin against a requirement that is checked, not estimated.
//
// Normal flight cannot go much faster without a wider bandwidth (250 kHz halves airtime and
// costs 3 dB of sensitivity, so range) or a higher duty cap. The max-rate schedule below
// goes faster a different way: by sending lean packets between the rich ones.
inline constexpr std::uint32_t kTelemetryPeriodMs = 700;
inline constexpr double kMaxChannelDuty = 0.5;

// ---- The max-rate schedule ---------------------------------------------------------
// One ground command, MAX_RATE, moves the vehicle from the fixed 700 ms period to a
// repeating pattern of one rich packet and two lean ones for the rest of the power cycle,
// and closes the uplink behind it. See documentation/design/max-rate-command.md.

// The quiet time between two packets, and what it is for.
//
// On the vehicle, less than it used to be. Since 2026-09-11 the transmit no longer blocks,
// so the SD block write -- 30 ms at its measured worst, on two boards, in two of five
// sessions (bring-up F-11) -- happens while the packet is on the air, not after it. What is
// left in the gap is noticing the packet has ended, one 2 ms loop tick, and building the next.
//
// On the organizers' station: reading the packet out of the radio, then printing about 374
// characters of it to a 115200-baud serial port -- roughly 35 ms in which that receiver is
// in standby and deaf -- before it listens again. A packet whose preamble starts inside that
// is lost on their side and heard on ours. 50 ms covers that with 15 ms to spare; 40 did not.
//
// A slot is airtime + this, NOT airtime / duty. The rulebook scores consistency on the same
// five points as rate ("provided transmissions remain consistent", "packet loss will reduce
// the score"), so a slot that fits a duty target but not the receivers buys rate by losing
// packets, on the very line the rate was bought for.
inline constexpr std::uint32_t kMaxRateGuardMs = 50;

// This hardware transmits 1.8 % slower than the model (bring-up rows 5.2 and 5.3, 406.9 ms
// measured against 399.6 modelled, twice, on two boards). A period that clears the model
// and not the measurement is a period that fails on the bench rather than in the build.
inline constexpr double kAirtimeMeasuredFactor = 1.018;

// The max-rate schedule: a repeating pattern of one rich packet then two lean ones, each in
// its own slot. A slot is the shape's measured airtime plus kMaxRateGuardMs, rounded up; the
// rich slot is sized for the budget, the longest rich packet that is ever sent.
//
// Three is not a preference. The requirement is GPS and sound on the air at least once a
// second, so the cycle -- one rich slot and N lean -- must fit in 1000 ms: N = 2 gives 966,
// N = 3 gives 1262. The static_asserts below hold both halves of that.
inline constexpr std::uint32_t kMaxRateRichSlotMs = 374;   // 317.70 x 1.018 + 50 = 373.41
inline constexpr std::uint32_t kMaxRateLeanSlotMs = 296;   // 240.90 x 1.018 + 50 = 295.23
inline constexpr std::uint32_t kMaxRateLeanPerRich = 2;
inline constexpr std::uint32_t kMaxRateCycleMs = 966;      // 3.11 Hz, sensors at 1.04 Hz

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
static_assert(kWorstCasePacketBytes <= kGroundStationMaxPacketBytes,
              "the packet budget is over the 200 bytes the organizers' ground station accepts");

// The longest rich packet actually sent is the budget, not the widest the builder can make:
// the controller sheds SN- and then GP- rather than exceed it.
inline constexpr double kRichAirtimeMs = lora_time_on_air_ms(kWorstCasePacketBytes, kModem);
inline constexpr double kLeanAirtimeMs = lora_time_on_air_ms(kLeanPacketBytes, kModem);

static_assert(kRichPacketBytes == kMandatoryPacketBytes + kGpsFieldBytes + kSoundFieldBytes,
              "the rich packet is not mandatory + GPS + sound");
static_assert(kLeanPacketBytes == kMandatoryPacketBytes, "the lean packet is not mandatory-only");
static_assert(kMandatoryPacketBytes + kGpsFieldBytes <= kWorstCasePacketBytes,
              "the mandatory fields and GPS no longer fit the budget together at their widest");
static_assert(kBenchPacketBytes <= kMaxLoraPayloadBytes, "the bench budget exceeds the FIFO");
// The design's reason for taking the tags off the air, held as a fact about the bytes: if
// tags, GPS and sound ever fit the budget together, the reason is gone and the tags should
// come back.
static_assert(kMandatoryPacketBytes + kGpsFieldBytes + kSoundFieldBytes + kDiagnosticTagBytes >
                  kWorstCasePacketBytes,
              "tags + GPS + sound now fit the budget -- revisit dropping the tags");
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
