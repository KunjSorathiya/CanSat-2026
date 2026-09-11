#include "flight/controller.hpp"

#include "cansat/command.hpp"

#include "flight/orientation.hpp"
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
                       Radio& radio, SdLogger& logger, BoardIo& board,
                       SoundSensor* sound)
    : config_(std::move(config)),
      imu_(imu),
      barometer_(barometer),
      gps_(gps),
      radio_(radio),
      logger_(logger),
      board_(board),
      sound_(sound),
      state_machine_(config_),
      builder_(config_),
      calibrator_(config_),
      mag_calibrator_(config_) {
    OrientationEstimator::Gains gains;
    gains.kp_accel = config_.orientation_kp_accel;
    gains.kp_mag = config_.orientation_kp_mag;
    gains.ki_bias = config_.orientation_ki_bias;
    gains.bias_limit_dps = config_.orientation_bias_limit_dps;
    orientation_.set_gains(gains);
    // A calibration carried in the configuration was measured on this airframe on the
    // bench; one produced in flight is adopted later, if the vehicle is ever swept
    // through enough attitudes to earn it.
    mag_calibration_ = config_.mag_calibration;
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
    // The pre-arm command window opens here, and only on a clean power-on. A watchdog reset
    // may have happened mid-flight, and five minutes unarmed and listening would then be five
    // minutes with no launch or landing detection -- so a reset closes the uplink instead.
    window_open_ = config_.allow_ground_commands && !watchdog_reboot_;
    window_closed_ms_ = 0;
    if (config_.allow_ground_commands && watchdog_reboot_) uplink_closed_ = true;
    if (watchdog_reboot_) {
        // Previous run hung or browned out. Telemetry restarts automatically below;
        // record it so the ground station can see the recovery.
        faults_.report(FaultCode::watchdog_reboot, FaultSeverity::warning, 0);
    }

    std::string why;
    if (!validate_config(config_, why)) {
        // Keep the reason. validate_config() has thirty ways to refuse and says exactly
        // which one applied; replacing that with "configuration invalid" leaves an
        // operator with a vehicle that will not fly and no way to find out why.
        config_error_ = why;
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
    // An additional sensor that fails to start is a warning and nothing more. It is not
    // counted towards the self-test result and it can never hold up a launch.
    if (sound_ != nullptr && !sound_->initialize()) {
        faults_.report(FaultCode::sound_unavailable, FaultSeverity::warning, 0);
    }

    if (!imu_ok) faults_.report(FaultCode::imu_init, FaultSeverity::error, 0);
    // A module sold as an MPU-9250 that answers WHO_AM_I with an MPU-6500 has no
    // magnetometer. That is a degraded vehicle, not a broken one: roll and pitch are
    // unaffected and yaw falls back to relative gyro integration.
    if (imu_ok && !imu_.has_magnetometer()) {
        faults_.report(FaultCode::mag_unavailable, FaultSeverity::warning, 0);
        last_error_ = "no magnetometer detected; yaw is relative only";
    }
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

    // GPS first: bounded, non-blocking drain of the UART buffer. It is polled on the
    // mission clock, like every other sensor, so that fix ages and health timestamps are
    // all comparable against one another.
    gps_.poll(mission_ms);

    // The packet on the air, if any. The radio sends it on its own; this only notices it has
    // finished. Nothing in this loop waits for a transmission any more -- the sensor task
    // below runs at its own 33 ms through every packet, where it used to stand still for the
    // whole airtime.
    service_transmit(mission_ms);

    if (sensor_task_.due(mission_ms)) {
        acquire_sensors(mission_ms);
    }

    if (window_open_ && mission_ms >= config_.command_window_ms) {
        close_command_window(mission_ms);
    }
    run_calibration(mission_ms);
    feed_state_machine(mission_ms);

    // A packet still on the air holds the next one back rather than being cut off by it:
    // due() is not consulted, so the next packet stays pending and goes the moment the radio
    // is free. Every slot outlasts its packet's airtime, so only a radio that has hung --
    // bounded by its 1000 ms timeout -- ever holds one back.
    if (!tx_in_flight_ && telemetry_task_.due(mission_ms)) {
        emit_telemetry(mission_ms);
        service_transmit(mission_ms);  // a radio that finished at once settles in this poll
    }

    // One slice of any background scrub, every poll. It costs nothing when none is
    // running, and it deliberately does not gate on logger_enabled_ alone changing: a
    // scrub that has started must finish or be abandoned by the logger, not left half done
    // because a write failed once.
    if (logger_enabled_) {
        logger_.erase_step();
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

bool Controller::plausible_mag(const ImuSample& s) const {
    const double c = config_.mag_clip_ut;
    return std::isfinite(s.mx_ut) && std::isfinite(s.my_ut) && std::isfinite(s.mz_ut) &&
           std::fabs(s.mx_ut) < c && std::fabs(s.my_ut) < c && std::fabs(s.mz_ut) < c;
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
    // The magnetometer is validated on its own terms. It shares a die package with the
    // accelerometer and gyroscope but not their failure modes: a saturated or absent
    // magnetometer must cost yaw only, never roll, pitch or acceleration telemetry.
    bool mag_read = imu_read && imu.mag_valid && plausible_mag(imu);
    if (imu_read && imu.mag_valid && !mag_read) {
        faults_.report(FaultCode::sensor_implausible, FaultSeverity::warning, mission_ms);
    }

    if (imu_read) {
        // Feed the raw sample to the pad calibration before any correction is applied.
        if (!calibrator_.settled()) {
            calibrator_.add_imu(imu);
        }

        // Corrected values feed both orientation and the telemetry snapshot. The
        // accelerometer correction is a scalar scale rather than a subtracted vector,
        // because a vector measured in one attitude stops being right in any other.
        const double ax = imu.ax_mps2 * accel_scale_;
        const double ay = imu.ay_mps2 * accel_scale_;
        const double az = imu.az_mps2 * accel_scale_;
        const double gx = imu.gx_dps - gyro_bias_dps_[0];
        const double gy = imu.gy_dps - gyro_bias_dps_[1];
        const double gz = imu.gz_dps - gyro_bias_dps_[2];

        snapshot_.ax_mps2 = ax;
        snapshot_.ay_mps2 = ay;
        snapshot_.az_mps2 = az;
        snapshot_.imu_valid = true;

        double mx = imu.mx_ut;
        double my = imu.my_ut;
        double mz = imu.mz_ut;
        if (mag_read) {
            // The sweep calibrator sees the field as the sensor reports it; applying the
            // correction first would fit a correction to already-corrected data.
            if (config_.mag_cal_in_flight && !mag_calibration_.valid) {
                mag_calibrator_.add(mx, my, mz);
            }
            sensors::apply_mag_calibration(mag_calibration_, mx, my, mz);
            snapshot_.mx_ut = mx;
            snapshot_.my_ut = my;
            snapshot_.mz_ut = mz;
            snapshot_.mag_valid = true;
            last_good_mag_ms_ = mission_ms;
            faults_.clear(FaultCode::mag_unavailable);
        } else if (imu_.has_magnetometer() &&
                   mission_ms - last_good_mag_ms_ > config_.sensor_stale_after_ms) {
            snapshot_.mag_valid = false;
            faults_.hold(FaultCode::mag_unavailable, FaultSeverity::warning, mission_ms);
        }

        double dt = (last_sensor_ms_ == 0)
                        ? config_.sensor_period_ms / 1000.0
                        : (mission_ms - last_sensor_ms_) / 1000.0;
        if (!(dt > 0.0) || dt > 1.0) {
            dt = config_.sensor_period_ms / 1000.0;
        }
        if (mag_read) {
            orientation_.update(ax, ay, az, gx, gy, gz, mx, my, mz,
                                mag_calibration_.valid, dt);
        } else {
            // No usable field this tick: yaw propagates on the gyro alone, which is
            // exactly what the 6-axis entry point does.
            orientation_.update(ax, ay, az, gx, gy, gz, dt);
        }
        const OrientationEstimate o = orientation_.estimate();
        snapshot_.roll_deg = o.roll_deg;
        snapshot_.pitch_deg = o.pitch_deg;
        snapshot_.yaw_deg = o.yaw_deg;
        snapshot_.heading_deg = o.heading_deg;
        snapshot_.yaw_is_magnetic = o.yaw_is_magnetic;
        snapshot_.orientation_valid = o.valid;
        if (o.valid) {
            faults_.clear(FaultCode::orientation_invalid);
        }
        faults_.clear(FaultCode::imu_init);
        faults_.clear(FaultCode::imu_stale);
        last_good_imu_ms_ = mission_ms;
    } else if (mission_ms - last_good_imu_ms_ > config_.sensor_stale_after_ms) {
        snapshot_.imu_valid = false;
        snapshot_.mag_valid = false;
        snapshot_.orientation_valid = false;
        snapshot_.yaw_is_magnetic = false;
        faults_.hold(FaultCode::imu_stale, FaultSeverity::error, mission_ms);
        faults_.hold(FaultCode::orientation_invalid, FaultSeverity::error, mission_ms);
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
        //
        // The hold is bounded. An unchanged pressure means one of two things, and they
        // need opposite responses: for a sample or two it means the loop outran the
        // sensor, and the estimate should hold. For longer than that it means the vehicle
        // genuinely is not moving vertically — and holding a stale descent rate would stop
        // the landing detector from ever firing, leaving the mission stuck in FLIGHT after
        // it had already landed.
        const bool pressure_changed =
            last_altitude_ms_ == 0 || baro.pressure_pa != last_baro_pressure_pa_;
        if (last_altitude_ms_ != 0 && pressure_changed) {
            const double dts = (mission_ms - last_altitude_ms_) / 1000.0;
            if (dts > 0.0) {
                const double inst_rate = (agl - last_altitude_agl_m_) / dts;
                altitude_rate_mps_ = 0.7 * altitude_rate_mps_ + 0.3 * inst_rate;
            }
        } else if (last_altitude_ms_ != 0 &&
                   mission_ms - last_altitude_ms_ > config_.altitude_rate_hold_ms) {
            // Settled: the pressure has genuinely stopped moving, so the rate is zero.
            altitude_rate_mps_ = 0.7 * altitude_rate_mps_;
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
        faults_.hold(FaultCode::baro_stale, FaultSeverity::error, mission_ms);
    }

    // ---- GPS snapshot (never blocks) ----
    // A fix is only used while the receiver keeps refreshing it. The NMEA parser has no
    // clock of its own and holds its last good fix forever, so without this age check a
    // receiver that stopped talking mid-descent would keep publishing the position it
    // last saw as though the vehicle were still there.
    cansat::GpsData gps{};
    // latest() reports that a fix exists; last_fix_ms() reports when it was last renewed.
    // A driver that never stamps leaves the age equal to the mission clock, so an
    // unstamped fix expires on its own rather than being trusted indefinitely.
    const std::uint64_t fix_ms = gps_.last_fix_ms();
    const bool fix_fresh = mission_ms >= fix_ms &&
                           (mission_ms - fix_ms) <= config_.gps_fix_timeout_ms;
    if (gps_.latest(gps) && fix_fresh) {
        snapshot_.gps = gps;
        faults_.clear(FaultCode::gps_unavailable);
    } else {
        snapshot_.gps.valid = false;
        faults_.hold(FaultCode::gps_unavailable, FaultSeverity::warning, mission_ms);
    }

    // ---- Analogue microphone (additional sensor) ----
    //
    // Last, and deliberately so. Everything above feeds mandatory telemetry; this feeds a
    // column in the log. It is read inside the same task rather than on a timer of its own
    // because a level that is not synchronous with the altitude and acceleration beside it
    // is far less useful for the correlations it exists to support.
    if (sound_ != nullptr) {
        SoundSample sound{};
        // Either channel on its own is a working sensor. A three-pin LM393 board has
        // no analogue output at all, and refusing its threshold duty because the level is
        // missing would discard the only measurement it can make.
        const bool got = sound_->read(sound, mission_ms) &&
                         ((sound.valid && std::isfinite(sound.level_mv_pp)) ||
                          (sound.gate_valid && std::isfinite(sound.gate_duty_pct)));
        if (got) {
            snapshot_.sound_valid = sound.valid && std::isfinite(sound.level_mv_pp);
            if (snapshot_.sound_valid) {
                snapshot_.sound_mv_pp = sound.level_mv_pp;
                snapshot_.sound_clipped = sound.clipped;
            }
            snapshot_.sound_gate_valid = sound.gate_valid &&
                                         std::isfinite(sound.gate_duty_pct);
            if (snapshot_.sound_gate_valid) {
                snapshot_.sound_gate_pct = sound.gate_duty_pct;
            }
            last_good_sound_ms_ = mission_ms;
            faults_.clear(FaultCode::sound_unavailable);
        } else if (mission_ms - last_good_sound_ms_ > config_.sound_stale_after_ms) {
            // Warning, never error. A dead microphone costs a column and nothing else, and
            // an additional sensor must not be able to move the mission state.
            snapshot_.sound_valid = false;
            snapshot_.sound_gate_valid = false;
            faults_.hold(FaultCode::sound_unavailable, FaultSeverity::warning, mission_ms);
        }
    }

    if (!imu_implausible && !baro_implausible) {
        faults_.clear(FaultCode::sensor_implausible);
    }

    update_mag_calibration(mission_ms);
    check_yaw_reference(mission_ms);
}

void Controller::update_mag_calibration(std::uint64_t mission_ms) {
    (void)mission_ms;
    if (!config_.mag_cal_in_flight || mag_calibration_.valid) {
        return;
    }
    if (!mag_calibrator_.coverage_met()) {
        return;
    }
    // Adopt without resetting the filter. The correction is a few tens of microtesla and
    // the estimator converges onto it within a second; re-seeding mid-flight would throw
    // away a good attitude solution to save that second.
    mag_calibration_ = mag_calibrator_.result();
}

void Controller::check_yaw_reference(std::uint64_t mission_ms) {
    if (!config_.yaw_cog_cross_check) {
        return;
    }
    // Every one of these conditions is a reason the comparison would be meaningless
    // rather than a reason to distrust the heading, so they reset the counter instead of
    // accumulating towards a warning.
    if (!snapshot_.orientation_valid || !snapshot_.yaw_is_magnetic ||
        !snapshot_.gps.valid || !snapshot_.gps.course_valid ||
        snapshot_.gps.speed_mps < config_.yaw_cog_min_speed_mps) {
        cog_disagreements_ = 0;
        return;
    }

    // Course over ground is referenced to true north and the estimator's heading to
    // magnetic north, so one of them has to be moved before they can be subtracted.
    const double heading_true =
        wrap_degrees_360(snapshot_.heading_deg + config_.magnetic_declination_deg);
    const double difference =
        std::fabs(angle_difference_deg(heading_true, snapshot_.gps.course_deg));

    if (difference > config_.yaw_cog_tolerance_deg) {
        if (++cog_disagreements_ >= config_.yaw_cog_confirm_samples) {
            // A warning, and only a warning. The two quantities are allowed to differ --
            // wind, crab and a spinning payload all separate them legitimately -- so this
            // says "suspect the magnetometer calibration", not "the heading is wrong".
            faults_.report(FaultCode::yaw_reference_disagreement, FaultSeverity::warning,
                           mission_ms);
            cog_disagreements_ = config_.yaw_cog_confirm_samples;
        }
    } else {
        cog_disagreements_ = 0;
        faults_.clear(FaultCode::yaw_reference_disagreement);
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
        accel_scale_ = r.accel_scale;
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
    // Never while the command window is open, and the arming delay runs from when it closed
    // -- zero on a build without the uplink, so that build arms exactly as it always has.
    return operational_ && snapshot_.imu_valid && snapshot_.baro_valid && !window_open_ &&
           mission_ms >= window_closed_ms_ + config_.arming_delay_ms &&
           (!config_.require_calibration_to_arm || calibrator_.settled());
}

// Closes the window, once. The uplink closes with it, and the power-on calibration is thrown
// away: it gave the window a working altitude reference, but the reference the vehicle flies
// on should be taken where it sits now, on the pad, after the operator has finished with it.
// Arming follows when the new calibration settles and the arming delay has run.
void Controller::close_command_window(std::uint64_t mission_ms) {
    if (!window_open_) return;
    window_open_ = false;
    window_closed_ms_ = mission_ms;
    uplink_closed_ = true;
    calibrator_.reset();
    calibration_applied_ = false;
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

    // Which shape this packet is. Normal flight: always rich, because at 700 ms that is the
    // only way to put GPS and sound on the air at least once a second. After MAX_RATE: the
    // first of every three is rich and the other two are lean.
    const bool rich = !max_rate_ || max_rate_slot_ == 0;

    std::vector<std::string> extra;
    if (config_.append_diagnostic_fields) {
        extra.push_back(std::string("MODE-") + to_string(state_machine_.state()));
        extra.push_back("FAULTS-" + std::to_string(faults_.active_count()));
        extra.push_back(std::string("CAL-") + (calibrator_.complete() ? "1" : "0"));
        extra.push_back(std::string("ARM-") + (is_armed(mission_ms) ? "1" : "0"));
        // What the yaw field means in this packet: M = magnetometer-referenced (absolute
        // magnetic yaw, calibration applied), G = gyro-only (relative, arbitrary zero).
        // Four characters, because the ground station must never have to guess which of
        // the two it is looking at and the airtime budget has no room for a longer tag.
        extra.push_back(std::string("YR-") + (snapshot_.yaw_is_magnetic ? "M" : "G"));
    }
    auto built = builder_.build(candidate, mission_ms, snapshot_, extra, rich);

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
        built = builder_.build(candidate, mission_ms, snapshot_, extra, rich);
    }
    if (too_long(built)) {
        // 2. Sound. The budget is the organizers' 200-byte ceiling, and mandatory + GPS fit it
        // by construction while mandatory + GPS + sound do not quite (209 at their widest). So
        // SN- leaves the one packet whose other fields are wide enough to need its bytes --
        // a combination no flight produces together, but a ceiling is not a typical case.
        faults_.report(FaultCode::packet_oversize, FaultSeverity::warning, mission_ms);
        built = builder_.build(candidate, mission_ms, snapshot_, extra, rich, /*air_sound=*/false);
    }
    if (too_long(built)) {
        // 3. GPS. Unreachable in a valid configuration -- validate_config() refuses a budget
        // that cannot hold mandatory + GPS -- so this is the backstop for a value wider than
        // the budget was sized for.
        //
        // It used to rebuild from a snapshot with GPS marked invalid, which also took the
        // fix out of this packet's SD row. air_sensors = false shortens only what is sent.
        faults_.report(FaultCode::packet_oversize, FaultSeverity::warning, mission_ms);
        built = builder_.build(candidate, mission_ms, snapshot_, extra, /*air_sensors=*/false);
    }
    if (built && built->packet.size() > cansat::kMaxLoraPayloadBytes) {
        // 4. Mandatory fields alone still overflow the radio. Transmitting a truncated
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

    // Before the transmit, and that ordering is load-bearing rather than tidy. The receive
    // window is the gap between two packets, and transmitting clears the flags of anything
    // that arrived in it -- so the only moment a command can be read is immediately before
    // the next transmission ends its window. Polling afterwards reads the window that has
    // just been wiped, every time.
    service_ground_commands(mission_ms);

    // Started, not finished. The radio sends the packet on its own from here, and
    // service_transmit() counts it sent or failed when it ends; the SD row below is written
    // while it is still on the air.
    if (start_with_recovery(built->packet, mission_ms)) {
        tx_in_flight_ = true;
    } else {
        ++health_.packets_tx_failed;
    }

    // The max-rate pattern: the next packet is due one slot after this one, and the slot is
    // this packet's shape. due() cannot do that alone -- it advances by the period it held
    // when it fired -- so the next due time is set here, where the shape is known. A failed
    // transmit still used its slot, so the cadence does not stretch around it.
    if (max_rate_) {
        telemetry_task_.reschedule(mission_ms, rich ? cansat::link::kMaxRateRichSlotMs
                                                    : cansat::link::kMaxRateLeanSlotMs);
        max_rate_slot_ = (max_rate_slot_ + 1) % (cansat::link::kMaxRateLeanPerRich + 1);
    }

    if (logger_enabled_) {
        if (logger_.append(builder_.sd_line(*built, state_machine_.state(),
                                            faults_.total_occurrences()))) {
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

// The whole of the uplink. It is short on purpose, and every line of it is a refusal.
//
// The vehicle flies with `allow_ground_commands` false, so on a flight build this returns
// before touching the radio and there is no uplink to reason about at all. On the bench,
// the window is READY with ARM-0 -- on the ground, before launch. It is closed for FLIGHT,
// LANDED, RECOVERY and FAULT, which is every state in which a log exists that cannot be
// recreated.
//
// Ordering matters, and not in the direction it first appears. This runs immediately
// *before* the packet is transmitted, because the radio is half duplex: the transmit takes
// the part out of RX and clears the flags, so a command that arrived during the previous
// gap is destroyed by the next transmission unless it is read first. Polling after the
// transmit -- which reads better, and which this did until the bug was found -- reads a
// window that has just been wiped, and the uplink never receives anything at all.
//
// The cost is a non-blocking register read before each packet, and, on the one cycle where
// a command is actually accepted, the erase itself. Both are small against a 700 ms period,
// and the accepted-command case happens on the ground, in READY, with ARM-0.
void Controller::service_ground_commands(std::uint64_t mission_ms) {
    // The latch, checked before anything else. After a MAX_RATE command this function is
    // dead for the rest of the power cycle: no poll, no parse, no RX. That is the property
    // the operator was promised when they pressed a button labelled irreversible.
    if (uplink_closed_) return;
    if (!config_.allow_ground_commands) return;
    if (state_machine_.state() != MissionState::ready) return;
    if (is_armed(mission_ms)) return;

    std::string received;
    if (!radio_.poll_receive(received)) return;

    std::uint32_t command_pn = 0;
    const cansat::CommandKind kind = cansat::parse_command(
        received, config_.team_id, config_.command_password, command_pn);
    switch (kind) {
        case cansat::CommandKind::erase_log: {
            // The replay check, and it is the reason the packet number is in the command at
            // all. A token is only ever valid for the one packet number it was computed
            // against, so refusing numbers we have already accepted, or have not yet
            // reached, makes a frame recorded off the air useless the moment it is used
            // once. The window keeps an old recording from working at all.
            const bool already_used = command_pn <= last_command_pn_;
            const bool from_the_future = command_pn > packet_number_;
            const bool stale = packet_number_ - command_pn > config_.command_replay_window;
            if (already_used || from_the_future || stale) {
                ++health_.ground_commands_ignored;
                break;
            }
            last_command_pn_ = command_pn;

            // A refusal is reported, not swallowed. An operator who pressed the button and
            // saw nothing happen must be able to tell "erased" from "declined".
            const bool erased = logger_.erase();
            ++health_.ground_commands_accepted;
            if (!erased) {
                faults_.report(FaultCode::sd_write, FaultSeverity::warning, mission_ms);
            }
            break;
        }
        case cansat::CommandKind::max_rate: {
            // The same replay rules as the erase, and for the same reason: a token is worth
            // one use at one packet number. It matters more here -- an erase costs a log
            // that can be re-recorded, and this cannot be undone at all without a power
            // cycle the vehicle may not get.
            const bool already_used = command_pn <= last_command_pn_;
            const bool from_the_future = command_pn > packet_number_;
            const bool stale = packet_number_ - command_pn > config_.command_replay_window;
            if (already_used || from_the_future || stale) {
                ++health_.ground_commands_ignored;
                break;
            }
            last_command_pn_ = command_pn;
            ++health_.ground_commands_accepted;
            engage_max_rate();
            close_command_window(mission_ms);
            break;
        }
        case cansat::CommandKind::none:
            // Anything else on the air: another team's command, a corrupted frame, our own
            // telemetry looped back. Counted so a link that is delivering junk is visible.
            ++health_.ground_commands_ignored;
            break;
    }
}

// One accepted MAX_RATE. The packet shapes do not change here -- both schedules send rich
// packets and carry no tags -- only how often, and whether two lean packets follow each rich
// one. The pattern counts the packet during which the command was accepted as its first,
// rich, slot.
//
// The diagnostic tags are forced off even though that is the default. The slots are sized
// for the rich and lean shapes; a bench build that had turned the tags on would otherwise
// transmit packets its own slots were never measured for.
void Controller::engage_max_rate() {
    config_.append_diagnostic_fields = false;
    max_rate_ = true;
    max_rate_slot_ = 0;
    health_.rate_maxed = true;
    uplink_closed_ = true;
}

bool Controller::start_with_recovery(const std::string& packet, std::uint64_t mission_ms) {
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

    if (radio_.start_transmit(packet)) {
        return true;
    }
    note_tx_failure(mission_ms);
    return false;
}

void Controller::service_transmit(std::uint64_t mission_ms) {
    if (!tx_in_flight_) return;
    const TxState state = radio_.poll_transmit();
    if (state == TxState::busy) return;
    tx_in_flight_ = false;

    if (state == TxState::sent) {
        ++health_.packets_sent;
        radio_retry_after_ms_ = 0;
        radio_consecutive_failures_ = 0;
        faults_.clear(FaultCode::radio_tx);
        return;
    }
    // Failed -- or a radio that forgot the packet it was given (idle), which is the same
    // thing from the ground: a packet number that never arrived.
    ++health_.packets_tx_failed;
    note_tx_failure(mission_ms);
}

void Controller::note_tx_failure(std::uint64_t mission_ms) {
    if (++radio_consecutive_failures_ >= config_.radio_max_consecutive_failures) {
        faults_.report(FaultCode::radio_tx, FaultSeverity::error, mission_ms);
        radio_.initialize(sync_word(config_));  // one bounded re-init attempt
        radio_retry_after_ms_ = mission_ms + config_.radio_recovery_backoff_ms;
        radio_consecutive_failures_ = 0;
    }
}

void Controller::sample_battery(std::uint64_t mission_ms) {
    const float raw = board_.battery_voltage();
    const float ratio = battery_divider_ratio(config_);
    const float voltage = ratio > 0.0f ? raw * ratio : raw;
    health_.battery_voltage = voltage;
    health_.battery_voltage_is_scaled = ratio > 0.0f;

    if (config_.battery_low_voltage > 0.0f && ratio > 0.0f) {
        if (voltage < config_.battery_low_voltage) {
            faults_.hold(FaultCode::battery_low, FaultSeverity::warning, mission_ms);
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
    health_.mag_present = imu_.has_magnetometer();
    health_.mag_ok = snapshot_.mag_valid;
    health_.mag_calibrated = mag_calibration_.valid;
    health_.yaw_is_magnetic = snapshot_.yaw_is_magnetic;
    health_.heading_deg = snapshot_.heading_deg;
    health_.mag_field_ut = snapshot_.mag_valid
                               ? sensors::vector_magnitude(snapshot_.mx_ut, snapshot_.my_ut,
                                                           snapshot_.mz_ut)
                               : 0.0;
    for (int i = 0; i < 3; ++i) health_.mag_cal_span_ut[i] = mag_calibrator_.span_ut(i);
    health_.gps_fix = snapshot_.gps.valid;
    health_.radio_ok = radio_.healthy();
    health_.sd_ok = logger_enabled_;
    // "Not fitted" and "fitted but silent" are both false here, which is correct: neither
    // is a reason to do anything, and the fault log distinguishes them if anyone asks.
    health_.sound_ok = sound_ != nullptr &&
                       (snapshot_.sound_valid || snapshot_.sound_gate_valid);
    health_.calibrated = calibrator_.complete();
    health_.calibration_settled = calibrator_.settled();
    health_.armed = is_armed(mission_ms);
    health_.command_window_open = window_open_;
    health_.command_window_left_ms =
        (window_open_ && mission_ms < config_.command_window_ms)
            ? static_cast<std::uint32_t>(config_.command_window_ms - mission_ms)
            : 0;
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
