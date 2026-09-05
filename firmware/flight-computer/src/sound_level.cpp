#include "flight/sound_level.hpp"

namespace flight {

double sound_peak_to_peak_mv(const SoundWindow& window, double reference_mv,
                             std::uint16_t full_scale_counts) {
    if (window.sample_count == 0 || full_scale_counts == 0 || !(reference_mv > 0.0)) {
        return 0.0;
    }
    // A window whose maximum is below its minimum was never filled; report nothing rather
    // than a negative level.
    if (window.max_counts < window.min_counts) {
        return 0.0;
    }
    const double span_counts = static_cast<double>(window.max_counts - window.min_counts);
    return span_counts * reference_mv / static_cast<double>(full_scale_counts);
}

bool sound_window_clipped(const SoundWindow& window, std::uint16_t full_scale_counts) {
    if (window.sample_count == 0) return false;
    return window.min_counts == 0 || window.max_counts >= full_scale_counts;
}

double sound_gate_duty_pct(std::uint32_t asserted_samples, std::uint32_t total_samples) {
    if (total_samples == 0) return 0.0;
    if (asserted_samples >= total_samples) return 100.0;
    return 100.0 * static_cast<double>(asserted_samples) /
           static_cast<double>(total_samples);
}

}  // namespace flight
