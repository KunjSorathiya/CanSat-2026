#pragma once

#include "flight/config.hpp"
#include "flight/interfaces.hpp"

#include <cstdint>

namespace flight {

struct CalibrationResult {
    bool gyro_bias_valid = false;
    double gyro_bias_dps[3] = {0.0, 0.0, 0.0};

    bool accel_reference_valid = false;
    double accel_bias_mps2[3] = {0.0, 0.0, 0.0};  // residual after removing gravity at rest
    double accel_magnitude_ref = 0.0;             // measured |a| at rest (~9.81 if scale is good)

    bool baro_reference_valid = false;
    double ground_pressure_pa = 0.0;
    double ground_altitude_m = 0.0;

    bool motion_detected = false;  // vehicle was not stationary long enough to accept
    std::uint32_t imu_samples = 0;
    std::uint32_t baro_samples = 0;
};

// Estimates gyro bias, accelerometer offset and the barometric ground reference while
// the vehicle is stationary on the pad. Feed it IMU and barometer samples during
// INIT/SELF_TEST/READY; it settles once enough stationary samples are seen, or resolves
// best-effort at calib_timeout_ms so the mission is never blocked.
class StartupCalibrator {
public:
    explicit StartupCalibrator(const Configuration& config);

    void reset();
    void add_imu(const ImuSample& sample);
    void add_baro(const BaroSample& sample);
    void update(std::uint64_t now_ms);  // evaluates completion / timeout

    bool complete() const { return complete_; }
    bool failed() const { return failed_; }
    bool settled() const { return complete_ || failed_; }
    const CalibrationResult& result() const { return result_; }

private:
    void clear_accumulators();
    void finalise(bool best_effort);

    const Configuration& config_;
    double sum_g_[3] = {0, 0, 0};
    double sumsq_g_[3] = {0, 0, 0};
    double sum_a_[3] = {0, 0, 0};
    double sum_p_ = 0.0;
    double sum_t_ = 0.0;
    std::uint32_t n_imu_ = 0;
    std::uint32_t n_baro_ = 0;
    std::uint64_t started_ms_ = 0;
    bool started_ = false;
    bool complete_ = false;
    bool failed_ = false;
    CalibrationResult result_{};
};

}  // namespace flight
