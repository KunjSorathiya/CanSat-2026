#pragma once

#include <cstdint>

namespace flight {

// Reduces a window of raw ADC samples from an analogue microphone to one number.
//
// WHY A WINDOW AND NOT A SAMPLE. An analogue microphone module's output is AC-coupled: it
// rests at a bias voltage, usually about half its supply, and swings either side of that
// with the sound pressure. A single ADC reading of that signal is meaningless -- it says
// where in the waveform you happened to look, not how loud anything was. Sampling once per
// flight-loop tick would produce a number that varies wildly with nothing but timing.
//
// What carries the level is the ENVELOPE. So the window is reduced to its peak-to-peak
// span: sample fast for a short burst, keep the smallest and largest codes seen, and report
// the difference. That is stable, cheap, needs no floating-point filter, and is the standard
// way these modules are read.
//
// WHAT THIS IS NOT. It is not a sound pressure level. Converting to dB SPL needs a
// calibrated reference -- a known source at a known distance, measured with an instrument
// this project does not have -- plus the module's own gain, which is set by a trimpot and
// is not recorded anywhere. Every value this produces is a RELATIVE level in millivolts,
// comparable only with other readings from the same module at the same gain setting. That
// is enough for the mission questions (did the canopy open, when did it land, how did the
// acoustic level track descent rate) and it is not enough to state a decibel figure. The
// telemetry and the documentation say millivolts for exactly this reason.
struct SoundWindow {
    std::uint16_t min_counts = 0;
    std::uint16_t max_counts = 0;
    std::uint32_t sample_count = 0;
};

// Peak-to-peak span in millivolts.
//
// `reference_mv` is the converter's full-scale voltage and `full_scale_counts` its top
// code -- 3300 mV and 4095 on a Pico reading its 12-bit ADC against the 3.3 V rail.
//
// Returns 0 for a window that holds no samples or a nonsensical scale, rather than
// dividing by zero or reporting a level nothing measured.
double sound_peak_to_peak_mv(const SoundWindow& window, double reference_mv,
                             std::uint16_t full_scale_counts);

// True when the window touched either end of the converter's range.
//
// A clipped window is not a wrong reading, it is a LOWER BOUND: the real peak-to-peak was
// at least this and may have been much more. That distinction matters for the loudest
// events in the flight -- canopy inflation and touchdown are exactly the moments a gain
// set for ambient noise will saturate -- so it is carried through to the log rather than
// silently folded into the number.
bool sound_window_clipped(const SoundWindow& window, std::uint16_t full_scale_counts);

// Percentage of a window that sat above the module's comparator threshold.
//
// A sharp transient and a sustained noise can reach the same peak level and mean completely
// different things -- canopy inflation against the flow noise of a fast descent. The
// analogue envelope cannot tell them apart within one window; this can, and it is the only
// measurement a three-pin LM393 board can make at all.
//
// Returns 0 for an empty window, and never exceeds 100 even if the counter is nonsense.
double sound_gate_duty_pct(std::uint32_t asserted_samples, std::uint32_t total_samples);

}  // namespace flight
