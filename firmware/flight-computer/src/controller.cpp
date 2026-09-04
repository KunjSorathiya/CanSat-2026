#include "flight/controller.hpp"

#include "flight/sensor_math.hpp"

#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace flight {

namespace {
bool finite3(double a, double b, double c) {
    return std::isfinite(a) && std::isfinite(b) && std::isfinite(c);
}
}  // namespace

Controller::Controller(Configuration config, Imu& imu, Barometer& barometer, Gps& gps,
                       Radio& radio, SdLogger& logger, BoardIo& board)
    : config_(std::move(config)),
      imu_(imu),
      barometer_(barometer),
      gps_(gps),
      radio_(radio),
      logger_(logger),
      board_(board),
      state_machine_(config_),
      orientation_(config_.orientation_alpha),
      builder_(config_),
      calibrator_(config_) {
    sensor_task_.configure(config_.sensor_period_ms, 0);
    telemetry_task_.configure(config_.telemetry_period_ms, 0);
    sd_flush_task_.configure(config_.sd_flush_period_ms, config_.sd_flush_period_ms);
    health_task_.configure(config_.health_period_ms, 0);
    battery_task_.configure(config_.battery_period_ms, 0);
}

void Controller::set_boot_cause(bool from_watchdog) {
    watchdog_reboot_ = from_watchdog;
}

bool Controller::initialize() {
    board_.set_status_led(true);
    if (watchdog_reboot_) {
        // Previous run hung or browned out. Telemetry restarts automatically below;
        // record it so the ground station can see the recovery.
        faults_.report(FaultCode::watchdog_reboot, FaultSeverity::warning, 0);
    }

    std::string why;
    if (!validate_config(config_, why)) {
        last_error_ = "configuration invalid";
        faults_.report(FaultCode::config_invalid, FaultSeverity::critical, 0);
        state_machine_.begin_self_test(0);
        DetectionInputs di;
        di.critical_fault = true;
        state_machine_.update(0, di);
        refresh_health(0);
        return false;
    }

    const bool imu_ok = imu_.initialize();
    const bool baro_ok = barometer_.initialize();
    const bool gps_ok = gps_.initialize();
    logger_enabled_ = logger_.initialize();
    const bool radio_ok = radio_.initialize(sync_word(config_));

    if (!imu_ok) faults_.report(FaultCode::imu_init, FaultSeverity::error, 0);
    if (!baro_ok) faults_.report(FaultCode::baro_init, FaultSeverity::error, 0);
    if (!gps_ok) faults_.report(FaultCode::gps_unavailable, FaultSeverity::warning, 0);
    if (!logger_enabled_) faults_.report(FaultCode::sd_unavailable, FaultSeverity::warning, 0);
    if (!radio_ok) faults_.report(FaultCode::radio_init, FaultSeverity::error, 0);

    // Mandatory telemetry needs acceleration (IMU) and altitude/pressure/temperature
    // (barometer). If both are dead, no compliant packet can ever be produced.
    operational_ = imu_ok && baro_ok;

    state_machine_.begin_self_test(0);
    DetectionInputs di;
    di.self_test_ok = operational_;
    di.sensors_ok = operational_;
    di.critical_fault = !operational_;
    state_machine_.update(0, di);  // self_test -> ready or fault

    if (!operational_) {
        last_error_ = "mandatory sensor initialisation failure";
        // imu_init / baro_init are already recorded above; escalate the surviving one
        // (or both) so the state machine latches FAULT while telemetry keeps trying.
        if (!imu_ok) faults_.report(FaultCode::imu_init, FaultSeverity::critical, 0);
        if (!baro_ok) faults_.report(FaultCode::baro_init, FaultSeverity::critical, 0);
        refresh_health(0);
        return false;
    }

    refresh_health(0);
    return true;
}

void Controller::poll(std::uint64_t now_ms) {
    if (!epoch_set_) {
        epoch_ms_ = now_ms;
        epoch_set_ = true;
    }
    const std::uint64_t mission_ms = now_ms - epoch_ms_;
    mission_ms_ = mission_ms;

    // GPS first: bounded, non-blocking drain of the UART buffer.
    gps_.poll(now_ms);

    if (sensor_task_.due(mission_ms)) {
        acquire_sensors(mission_ms);
    }

    run_calibration(mission_ms);
    feed_state_machine(mission_ms);

    if (telemetry_task_.due(mission_ms)) {
        emit_telemetry(mission_ms);
    }

    if (logger_enabled_ && sd_flush_task_.due(mission_ms)) {
        if (!logger_.flush()) {
            faults_.report(FaultCode::sd_write, FaultSeverity::warning, mission_ms);
        }
    }

    if (battery_task_.due(mission_ms)) {
        sample_battery(mission_ms);
    }

    if (health_task_.due(mission_ms)) {
        refresh_health(mission_ms);
    }

    update_led(mission_ms);
}

bool Controller::plausible_imu(const ImuSample& s) const {
    const double c = config_.accel_clip_mps2;
    const double g = config_.gyro_clip_dps;
    return std::fabs(s.ax_mps2) < c && std::fabs(s.ay_mps2) < c && std::fabs(s.az_mps2) < c &&
           std::fabs(s.gx_dps) < g && std::fabs(s.gy_dps) < g && std::fabs(s.gz_dps) < g;
}

bool Controller::plausible_baro(const BaroSample& s) const {
    return s.pressure_pa >= config_.baro_min_pa && s.pressure_pa <= config_.baro_max_pa &&
           s.temperature_c >= config_.baro_min_temp_c && s.temperature_c <= config_.baro_max_temp_c;
}

void Controller::acquire_sensors(std::uint64_t mission_ms) {
    // ---- IMU + orientation ----
    bool imu_implausible = false;
    bool baro_implausible = false;

    ImuSample imu{};
    bool imu_read = imu_.read(imu, mission_ms) && imu.valid &&
                    finite3(imu.ax_mps2, imu.ay_mps2, imu.az_mps2) &&
                    finite3(imu.gx_dps, imu.gy_dps, imu.gz_dps);
    if (imu_read && !plausible_imu(imu)) {
        // A reading outside datasheet-derived bounds means the sensor is actively wrong:
        // reject it AND drop the prior value (do not coast on stale data).
        imu_read = false;
        imu_implausible = true;
        snapshot_.imu_valid = false;
        snapshot_.orientation_valid = false;
        faults_.report(FaultCode::sensor_implausible, FaultSeverity::warning, mission_ms);
    }
    if (imu_read) {
        // Feed the raw sample to the pad calibration before any bias correction.
        if (!calibrator_.settled()) {
            calibrator_.add_imu(imu);
        }

        // Bias-corrected values feed both orientation and the telemetry snapshot.
        const double ax = imu.ax_mps2 - accel_bias_mps2_[0];
        const double ay = imu.ay_mps2 - accel_bias_mps2_[1];
        const double az = imu.az_mps2 - accel_bias_mps2_[2];
        const double gx = imu.gx_dps - gyro_bias_dps_[0];
        const double gy = imu.gy_dps - gyro_bias_dps_[1];
        const double gz = imu.gz_dps - gyro_bias_dps_[2];

        snapshot_.ax_mps2 = ax;
        snapshot_.ay_mps2 = ay;
        snapshot_.az_mps2 = az;
        snapshot_.imu_valid = true;

        double dt = (last_sensor_ms_ == 0)
                        ? config_.sensor_period_ms / 1000.0
                        : (mission_ms - last_sensor_ms_) / 1000.0;
        if (!(dt > 0.0) || dt > 1.0) {
            dt = config_.sensor_period_ms / 1000.0;
        }
        orientation_.update(ax, ay, az, gx, gy, gz, dt);
        const OrientationEstimate o = orientation_.estimate();
        snapshot_.roll_deg = o.roll_deg;
        snapshot_.pitch_deg = o.pitch_deg;
        snapshot_.yaw_deg = o.yaw_deg;
        snapshot_.orientation_valid = o.valid;
        if (o.valid) {
            faults_.clear(FaultCode::orientation_invalid);
        }
        faults_.clear(FaultCode::imu_init);
        faults_.clear(FaultCode::imu_stale);
        last_good_imu_ms_ = mission_ms;
    } else if (mission_ms - last_good_imu_ms_ > config_.sensor_stale_after_ms) {
        snapshot_.imu_valid = false;
        snapshot_.orientation_valid = false;
        faults_.report(FaultCode::imu_stale, FaultSeverity::error, mission_ms);
        faults_.report(FaultCode::orientation_invalid, FaultSeverity::error, mission_ms);
    }
    last_sensor_ms_ = mission_ms;

    // ---- Barometer + altitude ----
    BaroSample baro{};
    bool baro_read = barometer_.read(baro, mission_ms) && baro.valid &&
                     std::isfinite(baro.pressure_pa) && std::isfinite(baro.temperature_c) &&
                     std::isfinite(baro.altitude_m);
    if (baro_read && !plausible_baro(baro)) {
        baro_read = false;
        baro_implausible = true;
        snapshot_.baro_valid = false;
        faults_.report(FaultCode::sensor_implausible, FaultSeverity::warning, mission_ms);
    }
    if (baro_read) {
        if (!calibrator_.settled()) {
            calibrator_.add_baro(baro);
        }
        snapshot_.pressure_pa = baro.pressure_pa;
        snapshot_.temperature_c = baro.temperature_c;

        const double abs_alt = baro.altitude_m;
        const double agl = (config_.altitude_relative_to_baseline && baseline_ready_)
                               ? abs_alt - baseline_altitude_m_
                               : abs_alt;

        // Vertical speed is differentiated from altitude, so it is only updated when the
        // barometer has actually produced a new conversion. Reading the same registers
        // twice yields an identical pressure; treating that as a real sample would feed a
        // zero climb rate into the filter and bias the estimate toward zero exactly when
        // the vehicle is moving fastest. validate_config() also refuses a sampling period
        // shorter than the barometer's conversion time, but this stays correct even if the
        // sensor stalls, slows, or is reconfigured in the field.
        const bool pressure_changed =
            last_altitude_ms_ == 0 || baro.pressure_pa != last_baro_pressure_pa_;
        if (last_altitude_ms_ != 0 && pressure_changed) {
            const double dts = (mission_ms - last_altitude_ms_) / 1000.0;
            if (dts > 0.0) {
                const double inst_rate = (agl - last_altitude_agl_m_) / dts;
                altitude_rate_mps_ = 0.7 * altitude_rate_mps_ + 0.3 * inst_rate;
            }
        }
        if (pressure_changed) {
            last_altitude_agl_m_ = agl;
            last_altitude_ms_ = mission_ms;
            last_baro_pressure_pa_ = baro.pressure_pa;
        }

        snapshot_.altitude_m = agl;
        snapshot_.baro_valid = true;
        faults_.clear(FaultCode::baro_init);
        faults_.clear(FaultCode::baro_stale);
        last_good_baro_ms_ = mission_ms;
    } else if (mission_ms - last_good_baro_ms_ > config_.sensor_stale_after_ms) {
        snapshot_.baro_valid = false;
        faults_.report(FaultCode::baro_stale, FaultSeverity::error, mission_ms);
    }

    // ---- GPS snapshot (never blocks) ----
    cansat::GpsData gps{};
    if (gps_.latest(gps)) {
        snapshot_.gps = gps;
        faults_.clear(FaultCode::gps_unavailable);
    } else {
        snapshot_.gps.valid = false;
    }

    if (!imu_implausible && !baro_implausible) {
        faults_.clear(FaultCode::sensor_implausible);
    }
}

void Controller::run_calibration(std::uint64_t mission_ms) {
    if (calibration_applied_) {
        return;
    }
    calibrator_.update(mission_ms);
    if (!calibrator_.settled()) {
        return;
    }

    const CalibrationResult& r = calibrator_.result();
    // Gyro / accel bias are applied ONLY from a clean (stationary) calibration; a
    // best-effort estimate taken while moving would be worse than no correction.
    if (r.gyro_bias_valid) {
        for (int i = 0; i < 3; ++i) gyro_bias_dps_[i] = r.gyro_bias_dps[i];
    }
    if (r.accel_reference_valid) {
        for (int i = 0; i < 3; ++i) accel_bias_mps2_[i] = r.accel_bias_mps2[i];
    }
    // The barometric ground reference is trustworthy even if the IMU saw motion.
    if (r.baro_reference_valid) {
        baseline_altitude_m_ = r.ground_altitude_m;
        baseline_ready_ = true;
    }
    if (calibrator_.failed()) {
        faults_.report(FaultCode::calibration, FaultSeverity::warning, mission_ms);
        last_error_ = "startup calibration did not settle; best-effort reference used";
    } else {
        faults_.clear(FaultCode::calibration);
    }
    orientation_.reset();  // re-seed the estimator now that bias is known
    calibration_applied_ = true;
}

bool Controller::is_armed(std::uint64_t mission_ms) const {
    return operational_ && snapshot_.imu_valid && snapshot_.baro_valid &&
           mission_ms >= config_.arming_delay_ms &&
           (!config_.require_calibration_to_arm || calibrator_.settled());
}

void Controller::feed_state_machine(std::uint64_t mission_ms) {
    DetectionInputs di;
    di.self_test_ok = operational_;
    di.sensors_ok = snapshot_.imu_valid && snapshot_.baro_valid;
    di.armed = is_armed(mission_ms);
    di.accel_magnitude_mps2 =
        sensors::vector_magnitude(snapshot_.ax_mps2, snapshot_.ay_mps2, snapshot_.az_mps2);
    di.altitude_agl_m = last_altitude_agl_m_;
    di.altitude_rate_mps = altitude_rate_mps_;

    // Only a genuine loss of ALL mandatory sensing is treated as critical; anything the
    // vehicle can still partially do keeps the mission running (graceful degradation).
    di.critical_fault = !operational_ || faults_.active(FaultCode::config_invalid) ||
                        (faults_.active(FaultCode::imu_stale) && faults_.active(FaultCode::baro_stale));

    state_machine_.update(mission_ms, di);
}

void Controller::emit_telemetry(std::uint64_t mission_ms) {
    const std::uint32_t candidate = packet_number_ + 1;

    std::vector<std::string> extra;
    if (config_.append_diagnostic_fields) {
        extra.push_back(std::string("MODE-") + to_string(state_machine_.state()));
        extra.push_back("FAULTS-" + std::to_string(faults_.active_count()));
        extra.push_back(std::string("CAL-") + (calibrator_.complete() ? "1" : "0"));
        extra.push_back(std::string("ARM-") + (is_armed(mission_ms) ? "1" : "0"));
    }
    auto built = builder_.build(candidate, mission_ms, snapshot_, extra);

    // The airtime budget assumes packets never exceed worst_case_packet_bytes, and the
    // radio silently clamps anything past the 255-byte LoRa FIFO — a truncated packet the
    // ground station can only read as corruption. The rulebook makes the priority
    // explicit: mandatory fields first, optional fields "only if bandwidth allows". So
    // shed optional content in order of value rather than let the radio cut the packet.
    const auto too_long = [&](const std::optional<TelemetryBuilder::Built>& b) {
        return b && b->packet.size() > config_.worst_case_packet_bytes;
    };

    if (too_long(built) && !extra.empty()) {
        // 1. Diagnostic tags: project-local, the least valuable.
        faults_.report(FaultCode::packet_oversize, FaultSeverity::warning, mission_ms);
        extra.clear();
        built = builder_.build(candidate, mission_ms, snapshot_, extra);
    }
    if (too_long(built) && snapshot_.gps.valid) {
        // 2. GPS: optional under the rulebook, and recoverable from the SD log.
        faults_.report(FaultCode::packet_oversize, FaultSeverity::warning, mission_ms);
        SensorSnapshot trimmed = snapshot_;
        trimmed.gps.valid = false;
        built = builder_.build(candidate, mission_ms, trimmed, extra);
    }
    if (built && built->packet.size() > cansat::kMaxLoraPayloadBytes) {
        // 3. Mandatory fields alone still overflow the radio. Transmitting a truncated
        // packet would present as corruption; suppress it and say so instead.
        faults_.report(FaultCode::packet_oversize, FaultSeverity::error, mission_ms);
        ++health_.packets_suppressed;
        last_error_ = "packet exceeds the LoRa payload limit; suppressed";
        return;
    }
    if (!built) {
        // Mandatory data invalid: produce no telemetry point and do NOT consume the
        // packet number, so transmitted packets stay strictly sequential.
        faults_.report(FaultCode::telemetry_suppressed, FaultSeverity::error, mission_ms);
        ++health_.packets_suppressed;
        last_error_ = "mandatory data invalid; packet suppressed";
        return;
    }
    faults_.clear(FaultCode::telemetry_suppressed);
    packet_number_ = candidate;

    const bool tx_ok = transmit_with_recovery(built->packet, mission_ms);
    if (tx_ok) {
        ++health_.packets_sent;
    } else {
        ++health_.packets_tx_failed;
    }

    if (logger_enabled_) {
        if (logger_.append(built->record, built->packet)) {
            sd_consecutive_failures_ = 0;
        } else {
            faults_.report(FaultCode::sd_write, FaultSeverity::warning, mission_ms);
            if (++sd_consecutive_failures_ >= config_.sd_max_failures) {
                logger_enabled_ = false;
                faults_.report(FaultCode::sd_unavailable, FaultSeverity::warning, mission_ms);
            }
        }
    }
}

bool Controller::transmit_with_recovery(const std::string& packet, std::uint64_t mission_ms) {
    if (radio_retry_after_ms_ != 0 && mission_ms < radio_retry_after_ms_) {
        return false;  // inside bounded back-off window; skip this cycle
    }

    if (!radio_.healthy()) {
        if (!radio_.initialize(sync_word(config_))) {
            radio_retry_after_ms_ = mission_ms + config_.radio_recovery_backoff_ms;
            faults_.report(FaultCode::radio_init, FaultSeverity::error, mission_ms);
            return false;
        }
        faults_.clear(FaultCode::radio_init);
    }

    if (radio_.transmit(packet)) {
        radio_retry_after_ms_ = 0;
        radio_consecutive_failures_ = 0;
        faults_.clear(FaultCode::radio_tx);
        return true;
    }

    if (++radio_consecutive_failures_ >= config_.radio_max_consecutive_failures) {
        faults_.report(FaultCode::radio_tx, FaultSeverity::error, mission_ms);
        radio_.initialize(sync_word(config_));  // one bounded re-init attempt
        radio_retry_after_ms_ = mission_ms + config_.radio_recovery_backoff_ms;
        radio_consecutive_failures_ = 0;
    }
    return false;
}

void Controller::sample_battery(std::uint64_t mission_ms) {
    const float raw = board_.battery_voltage();
    const float ratio = battery_divider_ratio(config_);
    const float voltage = ratio > 0.0f ? raw * ratio : raw;
    health_.battery_voltage = voltage;
    health_.battery_voltage_is_scaled = ratio > 0.0f;

    if (config_.battery_low_voltage > 0.0f && ratio > 0.0f) {
        if (voltage < config_.battery_low_voltage) {
            faults_.report(FaultCode::battery_low, FaultSeverity::warning, mission_ms);
        } else {
            faults_.clear(FaultCode::battery_low);
        }
    }
}

void Controller::refresh_health(std::uint64_t mission_ms) {
    health_.state = state_machine_.state();
    health_.mission_ms = mission_ms;
    health_.fault_total = faults_.total_occurrences();
    health_.fault_active = faults_.active_count();
    health_.imu_ok = snapshot_.imu_valid;
    health_.baro_ok = snapshot_.baro_valid;
    health_.orientation_ok = snapshot_.orientation_valid;
    health_.gps_fix = snapshot_.gps.valid;
    health_.radio_ok = radio_.healthy();
    health_.sd_ok = logger_enabled_;
    health_.calibrated = calibrator_.complete();
    health_.calibration_settled = calibrator_.settled();
    health_.armed = is_armed(mission_ms);
    health_.watchdog_reboot = watchdog_reboot_;
    for (int i = 0; i < 3; ++i) health_.gyro_bias_dps[i] = gyro_bias_dps_[i];
    health_.ground_pressure_pa = calibrator_.result().ground_pressure_pa;
    health_.altitude_agl_m = last_altitude_agl_m_;
    health_.altitude_rate_mps = altitude_rate_mps_;
    health_.gps_checksum_errors = gps_.checksum_errors();
}

void Controller::update_led(std::uint64_t mission_ms) const {
    // Base state: LED on (vehicle powered / running). Distinct blink rate per state.
    std::uint32_t period = 0;
    switch (state_machine_.state()) {
        case MissionState::init:
        case MissionState::self_test:
            period = 0;  // solid on
            break;
        case MissionState::ready:
            period = is_armed(mission_ms) ? 400 : 900;  // fast once armed, slow while calibrating
            break;
        case MissionState::flight:
            period = 100;
            break;
        case MissionState::landed:
        case MissionState::recovery:
            period = 250;
            break;
        case MissionState::fault:
            period = 60;
            break;
    }
    const bool on = period == 0 ? true : ((mission_ms / period) % 2) == 0;
    board_.set_status_led(on);
}

}  // namespace flight
