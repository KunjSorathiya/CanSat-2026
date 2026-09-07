#pragma once

#include "flight/config.hpp"
#include "flight/fault_manager.hpp"
#include "flight/health.hpp"
#include "flight/interfaces.hpp"
#include "flight/orientation.hpp"
#include "flight/scheduler.hpp"
#include "flight/startup_calibration.hpp"
#include "flight/state_machine.hpp"
#include "flight/telemetry_builder.hpp"

#include <cstdint>
#include <string>

namespace flight {

// Orchestrates the flight loop: sensor acquisition, orientation estimation, mission state,
// telemetry generation/transmission, SD logging, fault handling and battery monitoring.
//
// poll() is non-blocking and bounded. No peripheral failure (GPS, SD, radio, a single
// sensor) stops the loop or suppresses mandatory telemetry that can still be produced.
class Controller {
public:
    // `sound` is optional and may be null. It is a pointer rather than a reference for
    // exactly that reason: the microphone is an additional sensor, a vehicle built without
    // one is a legitimate build, and nothing mandatory may depend on its presence.
    Controller(Configuration config, Imu& imu, Barometer& barometer, Gps& gps,
               Radio& radio, SdLogger& logger, BoardIo& board,
               SoundSensor* sound = nullptr);

    // Record why this power session started, before the loop begins. `from_watchdog`
    // true means the previous run hung or browned out and was reset.
    void set_boot_cause(bool from_watchdog);

    // Initialise peripherals and run self-test. Returns false only on a condition that
    // prevents any compliant telemetry (bad config, or both IMU and barometer dead).
    bool initialize();

    // Advance the flight loop. now_ms is a monotonic millisecond clock; the mission epoch
    // is captured on the first call.
    void poll(std::uint64_t now_ms);

    MissionState state() const { return state_machine_.state(); }
    const HealthSnapshot& health() const { return health_; }
    const FaultManager& faults() const { return faults_; }
    const char* last_error() const { return last_error_; }
    // Why validate_config() refused, verbatim, when it did. Empty otherwise. The generic
    // last_error() says the configuration is invalid; this says which setting, out of the
    // thirty rules that could have rejected it.
    const char* config_error() const { return config_error_.c_str(); }
    std::uint32_t packet_count() const { return packet_number_; }
    std::uint64_t mission_ms() const { return mission_ms_; }

private:
    void acquire_sensors(std::uint64_t mission_ms);
    void run_calibration(std::uint64_t mission_ms);
    bool is_armed(std::uint64_t mission_ms) const;
    void service_ground_commands(std::uint64_t mission_ms);
    void feed_state_machine(std::uint64_t mission_ms);
    void update_mag_calibration(std::uint64_t mission_ms);
    // Compares the magnetometer-referenced heading against the GPS course over ground.
    // A cross-check only: the course is never fed back into the attitude estimator.
    void check_yaw_reference(std::uint64_t mission_ms);
    void emit_telemetry(std::uint64_t mission_ms);
    bool transmit_with_recovery(const std::string& packet, std::uint64_t mission_ms);
    void sample_battery(std::uint64_t mission_ms);
    void refresh_health(std::uint64_t mission_ms);
    void update_led(std::uint64_t mission_ms) const;
    bool plausible_imu(const ImuSample& s) const;
    bool plausible_mag(const ImuSample& s) const;
    bool plausible_baro(const BaroSample& s) const;

    Configuration config_;
    Imu& imu_;
    Barometer& barometer_;
    Gps& gps_;
    Radio& radio_;
    SdLogger& logger_;
    BoardIo& board_;
    SoundSensor* sound_ = nullptr;   // optional; null on a vehicle without a microphone

    StateMachine state_machine_;
    OrientationEstimator orientation_;
    TelemetryBuilder builder_;
    FaultManager faults_;
    StartupCalibrator calibrator_;
    MagCalibrator mag_calibrator_;

    PeriodicTask sensor_task_;
    PeriodicTask telemetry_task_;
    PeriodicTask sd_flush_task_;
    PeriodicTask health_task_;
    PeriodicTask battery_task_;

    SensorSnapshot snapshot_;
    HealthSnapshot health_;

    bool epoch_set_ = false;
    std::uint64_t epoch_ms_ = 0;
    std::uint64_t mission_ms_ = 0;
    std::uint64_t last_sensor_ms_ = 0;
    std::uint64_t last_good_imu_ms_ = 0;   // last plausible + valid IMU read
    std::uint64_t last_good_baro_ms_ = 0;  // last plausible + valid barometer read
    std::uint64_t last_good_sound_ms_ = 0;

    std::uint32_t packet_number_ = 0;

    double baseline_altitude_m_ = 0.0;
    bool baseline_ready_ = false;

    double gyro_bias_dps_[3] = {0.0, 0.0, 0.0};
    // Scalar accelerometer scale correction (see CalibrationResult::accel_scale). A
    // rotation-invariant scale, not a body-frame offset: the vehicle tumbles.
    double accel_scale_ = 1.0;
    sensors::MagCalibration mag_calibration_{};
    bool calibration_applied_ = false;
    std::uint64_t last_good_mag_ms_ = 0;
    std::uint32_t cog_disagreements_ = 0;
    bool watchdog_reboot_ = false;

    double last_altitude_agl_m_ = 0.0;
    std::uint64_t last_altitude_ms_ = 0;
    double last_baro_pressure_pa_ = 0.0;  // detects a re-read of an unchanged conversion
    double altitude_rate_mps_ = 0.0;

    std::uint8_t radio_consecutive_failures_ = 0;
    std::uint64_t radio_retry_after_ms_ = 0;

    bool logger_enabled_ = false;
    std::uint8_t sd_consecutive_failures_ = 0;

    bool operational_ = false;  // past self-test with mandatory sensors available
    const char* last_error_ = "";
    std::string config_error_;  // set once at initialise, never reassigned
};

}  // namespace flight
