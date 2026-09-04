#pragma once

#include <cstdint>

namespace flight {

struct OrientationEstimate {
    // Aerospace Z-Y-X Euler angles of the rotation that takes a body vector into the
    // local level frame, in degrees. roll about +X, pitch about +Y, yaw about +Z.
    double roll_deg = 0.0;
    double pitch_deg = 0.0;
    // Yaw is right-handed about the body's +Z (up when the vehicle is upright), so it
    // increases anticlockwise seen from above. It is an absolute MAGNETIC yaw only while
    // `yaw_is_magnetic` is true; otherwise it is a free-running gyro integration whose
    // zero is wherever the vehicle happened to be pointing at reset.
    double yaw_deg = 0.0;
    // The same information as a compass heading: 0..360 degrees clockwise from magnetic
    // north, i.e. wrap360(-yaw_deg). Meaningless unless `yaw_is_magnetic`.
    double heading_deg = 0.0;
    bool valid = false;
    // True only when the magnetometer has been correcting yaw recently AND the
    // calibration supplied to the estimator was marked valid. A vehicle flown without a
    // magnetometer calibration reports false here and its yaw must be read as relative.
    bool yaw_is_magnetic = false;
    // Estimated gyro bias currently being removed, deg/s, body axes. Diagnostic: a value
    // that keeps growing means the pad calibration was taken while the vehicle moved.
    double gyro_bias_dps[3] = {0.0, 0.0, 0.0};
};

// Nonlinear complementary attitude filter (Mahony) on the unit quaternion, with an
// explicit gyro-bias integrator.
//
// Why this rather than the Euler complementary filter it replaces: a CanSat under a
// parachute tumbles, and integrating Euler angles directly (roll += p*dt) is only valid
// for small angles and breaks down entirely as pitch approaches +-90 degrees. The
// quaternion form has no such singularity, costs a few hundred floating-point operations
// per update, and at the 30 Hz acquisition rate uses well under 1 % of the RP2040 --
// which has no FPU, so this is the cheapest formulation that is actually correct.
//
//   * the gyroscope propagates attitude at the full update rate;
//   * the accelerometer corrects roll and pitch, and is ignored whenever the specific
//     force is not close to 1 g (boost, chute snatch, impact), because it is then
//     measuring the manoeuvre rather than gravity;
//   * the magnetometer corrects yaw, and only yaw: the reference field is re-levelled
//     each update so a magnetic disturbance cannot tip the roll/pitch solution.
//
// Frames: body +Z along the can's long axis towards the nose, so the accelerometer reads
// +1 g on Z at rest and upright. The magnetometer must already be rotated into the same
// body frame and hard/soft-iron corrected by the caller -- the AK8963 inside the MPU-9250
// does NOT share the accelerometer's axes, and feeding it raw produces a heading that is
// confidently wrong.
class OrientationEstimator {
public:
    struct Gains {
        // Proportional feedback, in (rad/s) per unit of normalised cross-product error.
        // kp_accel ~2 gives a roll/pitch time constant of a few hundred milliseconds:
        // fast enough to track a tumble, slow enough that vibration does not drive it.
        double kp_accel = 2.0;
        // Yaw is corrected more gently than roll/pitch. The magnetic field is the noisier
        // reference of the two -- currents, the battery, the ferrous parts of the
        // airframe -- and yaw has no other observer, so a hard correction shows up
        // directly in the reported attitude.
        double kp_mag = 0.6;
        // Integral feedback that tracks residual gyro bias left over from, or drifting
        // away from, the pad calibration.
        double ki_bias = 0.05;
        // Hard limit on what the integrator may attribute to bias, deg/s. The MPU-9250
        // specifies a +-5 deg/s zero-rate output over temperature; anything past this
        // bound is a manoeuvre being mistaken for bias, and clamping keeps a bad minute
        // from poisoning the rest of the flight.
        double bias_limit_dps = 10.0;
    };

    OrientationEstimator();
    explicit OrientationEstimator(const Gains& gains);

    void set_gains(const Gains& gains);
    // Drop the attitude solution and the bias estimate. The next accelerometer sample
    // re-seeds roll and pitch directly, and the next magnetometer sample re-seeds yaw.
    void reset();

    // 6-axis update: accelerometer in m/s^2, gyroscope in deg/s, dt in seconds. Yaw is
    // propagated from the gyro alone and stays relative.
    void update(double ax, double ay, double az,
                double gx, double gy, double gz, double dt_s);

    // 9-axis update. Magnetometer in microtesla, body frame, already calibrated.
    // `mag_calibrated` says whether that calibration was a real one: an uncalibrated
    // magnetometer is still used to stop yaw drifting, but the estimate is not reported
    // as an absolute magnetic heading.
    void update(double ax, double ay, double az,
                double gx, double gy, double gz,
                double mx, double my, double mz, bool mag_calibrated, double dt_s);

    OrientationEstimate estimate() const;

    // Tilt-compensated magnetic yaw from one accelerometer + magnetometer pair, in
    // degrees, without touching the filter state. Exposed because it is the natural way
    // to test the frame convention, and because the controller uses it to seed and to
    // cross-check.  Returns false when the inputs cannot define a heading.
    static bool magnetic_yaw_deg(double ax, double ay, double az,
                                 double mx, double my, double mz, double& yaw_deg);

private:
    void apply(double ax, double ay, double az,
               double gx, double gy, double gz,
               const double* mag_ut, bool mag_calibrated, double dt_s);
    void seed(double ax, double ay, double az, const double* mag_ut);

    Gains gains_{};
    double q_[4] = {1.0, 0.0, 0.0, 0.0};  // body -> level frame, w x y z
    double bias_rad_[3] = {0.0, 0.0, 0.0};
    int mag_confidence_ = 0;  // consecutive-ish magnetometer corrections, saturating
    bool mag_calibrated_ = false;
    bool seeded_ = false;
    bool valid_ = false;
};

// Wrap an angle in degrees to (-180, 180].
double wrap_degrees(double degrees);

// Wrap an angle in degrees to [0, 360).
double wrap_degrees_360(double degrees);

// Smallest signed difference a - b, in degrees, wrapped to (-180, 180]. Comparing two
// headings any other way is wrong across the seam.
double angle_difference_deg(double a_deg, double b_deg);

}  // namespace flight
