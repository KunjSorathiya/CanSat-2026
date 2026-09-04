#include "flight/startup_calibration.hpp"

#include "flight/sensor_math.hpp"

#include <cmath>

namespace flight {

StartupCalibrator::StartupCalibrator(const Configuration& config) : config_(config) {}

void StartupCalibrator::clear_accumulators() {
    for (int i = 0; i < 3; ++i) {
        sum_g_[i] = sumsq_g_[i] = sum_a_[i] = 0.0;
    }
    sum_p_ = sum_t_ = 0.0;
    n_imu_ = n_baro_ = 0;
}

void StartupCalibrator::reset() {
    clear_accumulators();
    started_ = complete_ = failed_ = false;
    started_ms_ = 0;
    result_ = CalibrationResult{};
}

void StartupCalibrator::add_imu(const ImuSample& s) {
    if (complete_ || failed_) return;
    const double g[3] = {s.gx_dps, s.gy_dps, s.gz_dps};
    const double a[3] = {s.ax_mps2, s.ay_mps2, s.az_mps2};
    for (int i = 0; i < 3; ++i) {
        sum_g_[i] += g[i];
        sumsq_g_[i] += g[i] * g[i];
        sum_a_[i] += a[i];
    }
    ++n_imu_;
}

void StartupCalibrator::add_baro(const BaroSample& s) {
    if (complete_ || failed_) return;
    sum_p_ += s.pressure_pa;
    sum_t_ += s.temperature_c;
    ++n_baro_;
}

void StartupCalibrator::finalise(bool best_effort) {
    if (n_imu_ > 0) {
        double mg[3], ma[3];
        for (int i = 0; i < 3; ++i) {
            mg[i] = sum_g_[i] / n_imu_;
            ma[i] = sum_a_[i] / n_imu_;
        }
        for (int i = 0; i < 3; ++i) result_.gyro_bias_dps[i] = mg[i];
        result_.gyro_bias_valid = !best_effort;

        const double amag = sensors::vector_magnitude(ma[0], ma[1], ma[2]);
        result_.accel_magnitude_ref = amag;
        if (amag > 1.0) {
            const double s = sensors::kStandardGravity / amag;
            for (int i = 0; i < 3; ++i) result_.accel_bias_mps2[i] = ma[i] - ma[i] * s;
            result_.accel_reference_valid = !best_effort;
        }
        result_.imu_samples = n_imu_;
    }

    // The barometric ground reference stays usable even if the IMU saw motion.
    if (n_baro_ > 0) {
        result_.ground_pressure_pa = sum_p_ / n_baro_;
        result_.ground_altitude_m = sensors::pressure_altitude_m(
            result_.ground_pressure_pa, config_.reference_pressure_pa);
        result_.baro_reference_valid = std::isfinite(result_.ground_altitude_m);
        result_.baro_samples = n_baro_;
    }
}

void StartupCalibrator::update(std::uint64_t now_ms) {
    if (!started_) {
        started_ = true;
        started_ms_ = now_ms;
    }
    if (complete_ || failed_) return;

    if (n_imu_ >= config_.calib_samples) {
        double mg[3], var_g[3], ma[3];
        for (int i = 0; i < 3; ++i) {
            mg[i] = sum_g_[i] / n_imu_;
            var_g[i] = sumsq_g_[i] / n_imu_ - mg[i] * mg[i];
            if (var_g[i] < 0.0) var_g[i] = 0.0;
            ma[i] = sum_a_[i] / n_imu_;
        }
        const bool steady = std::sqrt(var_g[0]) < config_.calib_gyro_still_dps &&
                            std::sqrt(var_g[1]) < config_.calib_gyro_still_dps &&
                            std::sqrt(var_g[2]) < config_.calib_gyro_still_dps;
        // Low variance means "not shaking", which is not the same as "not turning": a
        // constant rotation is perfectly steady. Bound the mean too, against what the
        // datasheet says a zero-rate offset can actually be.
        const bool plausible_bias =
            std::fabs(mg[0]) < config_.calib_max_gyro_bias_dps &&
            std::fabs(mg[1]) < config_.calib_max_gyro_bias_dps &&
            std::fabs(mg[2]) < config_.calib_max_gyro_bias_dps;
        const bool still = steady && plausible_bias;
        const double amag = sensors::vector_magnitude(ma[0], ma[1], ma[2]);
        const bool accel_ok =
            std::fabs(amag - sensors::kStandardGravity) < config_.calib_accel_tol_mps2;

        if (still && accel_ok) {
            finalise(false);
            complete_ = true;
            return;
        }
        // Not stationary: drop this window and keep trying until the timeout.
        clear_accumulators();
    }

    if (now_ms - started_ms_ >= config_.calib_timeout_ms) {
        finalise(true);
        result_.motion_detected = true;
        failed_ = true;
    }
}

}  // namespace flight
