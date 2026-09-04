#pragma once

#include "flight/config.hpp"
#include "flight/interfaces.hpp"
#include "flight/sensor_math.hpp"

#include <cstdint>

namespace flight {

struct CalibrationResult {
    bool gyro_bias_valid = false;
    double gyro_bias_dps[3] = {0.0, 0.0, 0.0};

    // Scalar accelerometer scale correction: multiply every axis by this so that the
    // measured magnitude at rest is one standard gravity.
    //
    // This deliberately replaces the per-axis offset vector an earlier version stored. A
    // single stationary orientation gives one equation, which cannot separate offset from
    // scale; subtracting the difference as a fixed body-frame vector is only correct while
    // the vehicle stays in the attitude it was calibrated in, and becomes an error of the
    // same size pointing the wrong way once it tumbles. A scalar scale is rotation
    // invariant, so it stays correct through the whole flight.
    bool accel_reference_valid = false;
    double accel_scale = 1.0;
    double accel_magnitude_ref = 0.0;  // measured |a| at rest (~9.81 if the scale is good)

    bool baro_reference_valid = false;
    double ground_pressure_pa = 0.0;
    double ground_altitude_m = 0.0;

    bool motion_detected = false;  // vehicle was not stationary long enough to accept
    std::uint32_t imu_samples = 0;
    std::uint32_t baro_samples = 0;
};

// Estimates gyro bias, the accelerometer scale correction and the barometric ground
// reference while the vehicle is stationary on the pad. Feed it IMU and barometer samples
// during INIT/SELF_TEST/READY; it settles once enough stationary samples are seen, or
// resolves best-effort at calib_timeout_ms so the mission is never blocked.
//
// It does NOT calibrate the magnetometer: hard and soft iron are only observable while
// the vehicle rotates, which is the opposite of the condition every measurement here
// requires. That job belongs to MagCalibrator below.
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

// Hard-iron and soft-iron estimation for the AK8963, from a rotation sweep.
//
// The method is the min/max ellipsoid-bounding-box one: rotate the vehicle through every
// attitude you can (the usual figure-of-eight, plus a slow roll about each axis), record
// the extremes each axis reaches, and take the centre of the resulting box as the hard-
// iron offset and the ratio of the box's half-widths as the diagonal soft-iron scale.
//
// It is chosen over an ellipsoid least-squares fit on purpose: it needs six numbers of
// state instead of a 9x9 solve, runs in constant time per sample, and its failure mode is
// obvious rather than subtle -- an axis that was never swept simply never meets its span
// requirement, and the calibration is refused instead of quietly fitted to noise.
//
// The result is only reported valid once EVERY axis has swept a real range. This is what
// stops the vehicle from claiming an absolute magnetic heading it has not earned.
class MagCalibrator {
public:
    explicit MagCalibrator(const Configuration& config);

    void reset();
    // One magnetometer sample in microtesla, in the body frame, already sensitivity
    // adjusted by the driver but not yet hard/soft-iron corrected.
    void add(double x_ut, double y_ut, double z_ut);

    std::uint32_t samples() const { return n_; }
    double span_ut(int axis) const;  // max - min observed on `axis`, 0..2
    bool coverage_met() const;

    // The calibration implied by what has been seen so far. `valid` is false until the
    // coverage requirement is met, and an invalid calibration is never applied.
    sensors::MagCalibration result() const;

private:
    const Configuration& config_;
    double min_[3] = {0.0, 0.0, 0.0};
    double max_[3] = {0.0, 0.0, 0.0};
    std::uint32_t n_ = 0;
};

}  // namespace flight
