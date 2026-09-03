#pragma once

#include "flight/config.hpp"

#include <array>
#include <cstdint>

namespace flight {

// Enumerated fault conditions. The fault store is a fixed array indexed by this enum, so
// it never grows regardless of mission length.
enum class FaultCode : std::uint8_t {
    config_invalid = 0,
    imu_init,
    imu_stale,
    baro_init,
    baro_stale,
    gps_unavailable,
    sd_unavailable,
    sd_write,
    radio_init,
    radio_tx,
    battery_low,
    telemetry_suppressed,
    orientation_invalid,
    calibration,          // startup calibration did not settle cleanly (best-effort used)
    sensor_implausible,   // a reading fell outside datasheet-derived bounds
    watchdog_reboot,      // this boot was caused by the hardware watchdog
    count
};

struct FaultRecord {
    FaultCode code = FaultCode::count;
    FaultSeverity severity = FaultSeverity::warning;
    bool active = false;
    std::uint32_t occurrences = 0;
    std::uint64_t first_ms = 0;
    std::uint64_t last_ms = 0;
};

class FaultManager {
public:
    static constexpr std::size_t kCount = static_cast<std::size_t>(FaultCode::count);

    FaultManager();

    void report(FaultCode code, FaultSeverity severity, std::uint64_t now_ms);
    void clear(FaultCode code);  // mark recovered (inactive); history is kept

    bool active(FaultCode code) const;
    bool has_critical() const;       // a critical fault is currently active
    bool ever_critical() const;      // a critical fault has occurred at least once
    std::uint32_t total_occurrences() const;
    std::uint32_t active_count() const;

    const FaultRecord& record(FaultCode code) const;
    const std::array<FaultRecord, kCount>& records() const { return records_; }

private:
    std::array<FaultRecord, kCount> records_{};
    bool ever_critical_ = false;
};

const char* fault_name(FaultCode code);

}  // namespace flight
