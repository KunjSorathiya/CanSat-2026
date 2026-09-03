#include "flight/controller.hpp"

#include "cansat/telemetry.hpp"

#include <utility>

namespace flight {

Controller::Controller(Configuration config, Imu& imu, Barometer& barometer, Gps& gps,
                       Radio& radio, SdLogger& logger, BoardIo& board)
    : config_(std::move(config)), imu_(imu), barometer_(barometer), gps_(gps),
      radio_(radio), logger_(logger), board_(board), state_machine_(config_) {}

bool Controller::initialize() {
    board_.set_status_led(true);
    const bool imu_ok = imu_.initialize();
    const bool barometer_ok = barometer_.initialize();
    const bool gps_ok = gps_.initialize();
    const bool logger_ok = logger_.initialize();
    const bool radio_ok = radio_.initialize(sync_word(config_));

    const bool required_ok = imu_ok && barometer_ok && radio_ok;
    state_machine_.update(0, required_ok, radio_ok);
    if (!required_ok) {
        last_error_ = "required subsystem initialization failure";
        state_machine_.report_fault(FaultSeverity::error, last_error_);
    }
    if (!gps_ok) last_error_ = "GPS unavailable";
    if (!logger_ok) last_error_ = "SD logger unavailable";
    return required_ok;
}

void Controller::poll(std::uint64_t now_ms) {
    gps_.poll(now_ms);
    if (state_machine_.state() == MissionState::fault) return;
    if (now_ms >= next_telemetry_ms_) {
        if (!emit_telemetry(now_ms)) {
            last_error_ = "telemetry emission failure";
            state_machine_.report_fault(FaultSeverity::error, last_error_);
        }
        next_telemetry_ms_ = now_ms + config_.telemetry_period_ms;
    }
}

bool Controller::emit_telemetry(std::uint64_t now_ms) {
    cansat::TelemetryRecord record;
    record.team_id = config_.team_id;
    record.packet_number = ++packet_number_;
    record.timestamp_ms = now_ms;
    bool ok = imu_.read(record, now_ms);
    ok = barometer_.read(record, now_ms) && ok;
    cansat::GpsData gps_data;
    if (gps_.latest(gps_data)) record.gps = gps_data;
    auto packet = cansat::format_packet(record);
    if (!packet) return false;
    if (!radio_.transmit(*packet)) return false;
    if (logger_.healthy() && !logger_.append(record, *packet)) {
        last_error_ = "SD logger write failure";
    }
    return true;
}

MissionState Controller::state() const { return state_machine_.state(); }
const char* Controller::last_error() const { return last_error_; }

}  // namespace flight
