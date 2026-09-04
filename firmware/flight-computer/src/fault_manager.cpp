#include "flight/fault_manager.hpp"

namespace flight {

FaultManager::FaultManager() {
    for (std::size_t i = 0; i < kCount; ++i) {
        records_[i].code = static_cast<FaultCode>(i);
    }
}

void FaultManager::report(FaultCode code, FaultSeverity severity, std::uint64_t now_ms) {
    const std::size_t index = static_cast<std::size_t>(code);
    if (index >= kCount) {
        return;
    }
    FaultRecord& record = records_[index];
    if (record.occurrences == 0) {
        record.first_ms = now_ms;
    }
    record.severity = severity;
    record.active = true;
    record.last_ms = now_ms;
    if (record.occurrences < 0xFFFFFFFFu) {
        ++record.occurrences;
    }
    if (severity == FaultSeverity::critical) {
        ever_critical_ = true;
    }
}

void FaultManager::clear(FaultCode code) {
    const std::size_t index = static_cast<std::size_t>(code);
    if (index < kCount) {
        records_[index].active = false;
    }
}

bool FaultManager::active(FaultCode code) const {
    const std::size_t index = static_cast<std::size_t>(code);
    return index < kCount && records_[index].active;
}

bool FaultManager::has_critical() const {
    for (const auto& record : records_) {
        if (record.active && record.severity == FaultSeverity::critical) {
            return true;
        }
    }
    return false;
}

bool FaultManager::ever_critical() const { return ever_critical_; }

std::uint32_t FaultManager::total_occurrences() const {
    std::uint32_t total = 0;
    for (const auto& record : records_) {
        total += record.occurrences;
    }
    return total;
}

std::uint32_t FaultManager::active_count() const {
    std::uint32_t count = 0;
    for (const auto& record : records_) {
        if (record.active) {
            ++count;
        }
    }
    return count;
}

const FaultRecord& FaultManager::record(FaultCode code) const {
    const std::size_t index = static_cast<std::size_t>(code);
    return records_[index < kCount ? index : 0];
}

const char* fault_name(FaultCode code) {
    switch (code) {
        case FaultCode::config_invalid: return "config_invalid";
        case FaultCode::imu_init: return "imu_init";
        case FaultCode::imu_stale: return "imu_stale";
        case FaultCode::baro_init: return "baro_init";
        case FaultCode::baro_stale: return "baro_stale";
        case FaultCode::gps_unavailable: return "gps_unavailable";
        case FaultCode::sd_unavailable: return "sd_unavailable";
        case FaultCode::sd_write: return "sd_write";
        case FaultCode::radio_init: return "radio_init";
        case FaultCode::radio_tx: return "radio_tx";
        case FaultCode::battery_low: return "battery_low";
        case FaultCode::telemetry_suppressed: return "telemetry_suppressed";
        case FaultCode::orientation_invalid: return "orientation_invalid";
        case FaultCode::calibration: return "calibration";
        case FaultCode::sensor_implausible: return "sensor_implausible";
        case FaultCode::watchdog_reboot: return "watchdog_reboot";
        case FaultCode::packet_oversize: return "packet_oversize";
        case FaultCode::count: return "count";
    }
    return "unknown";
}

}  // namespace flight
