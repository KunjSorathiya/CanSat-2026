#include "flight/orientation.hpp"

#include "flight/sensor_math.hpp"

#include <cmath>

namespace flight {

namespace {
constexpr double kRadToDeg = 57.29577951308232;
constexpr double kDegToRad = 0.017453292519943295;
constexpr double kGravity = sensors::kStandardGravity;

// How many magnetometer corrections in a row are needed before yaw is reported as an
// absolute magnetic heading, and how long a dropout is tolerated before that claim is
// withdrawn. At 30 Hz these are a fifth of a second up and two thirds of a second down.
constexpr int kMagConfidenceRequired = 6;
constexpr int kMagConfidenceMax = 20;

bool finite3(double a, double b, double c) {
    return std::isfinite(a) && std::isfinite(b) && std::isfinite(c);
}

// Normalise in place. Returns false (leaving the vector untouched) when it has no length
// to speak of, which is what a dead bus reading all zeroes looks like.
bool normalize3(double v[3]) {
    const double n = sensors::vector_magnitude(v[0], v[1], v[2]);
    if (!(n > 1e-9) || !std::isfinite(n)) return false;
    const double inv = 1.0 / n;
    v[0] *= inv;
    v[1] *= inv;
    v[2] *= inv;
    return true;
}

void cross3(const double a[3], const double b[3], double out[3]) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}
}  // namespace

double wrap_degrees(double degrees) {
    if (!std::isfinite(degrees)) return 0.0;
    degrees = std::fmod(degrees + 180.0, 360.0);
    if (degrees <= 0.0) degrees += 360.0;
    return degrees - 180.0;
}

double wrap_degrees_360(double degrees) {
    if (!std::isfinite(degrees)) return 0.0;
    degrees = std::fmod(degrees, 360.0);
    if (degrees < 0.0) degrees += 360.0;
    return degrees;
}

double angle_difference_deg(double a_deg, double b_deg) {
    return wrap_degrees(a_deg - b_deg);
}

OrientationEstimator::OrientationEstimator() = default;

OrientationEstimator::OrientationEstimator(const Gains& gains) { set_gains(gains); }

void OrientationEstimator::set_gains(const Gains& gains) {
    gains_ = gains;
    if (!(gains_.kp_accel >= 0.0) || !std::isfinite(gains_.kp_accel)) gains_.kp_accel = 2.0;
    if (!(gains_.kp_mag >= 0.0) || !std::isfinite(gains_.kp_mag)) gains_.kp_mag = 0.6;
    if (!(gains_.ki_bias >= 0.0) || !std::isfinite(gains_.ki_bias)) gains_.ki_bias = 0.05;
    if (!(gains_.bias_limit_dps > 0.0) || !std::isfinite(gains_.bias_limit_dps)) {
        gains_.bias_limit_dps = 10.0;
    }
}

void OrientationEstimator::reset() {
    q_[0] = 1.0;
    q_[1] = q_[2] = q_[3] = 0.0;
    bias_rad_[0] = bias_rad_[1] = bias_rad_[2] = 0.0;
    mag_confidence_ = 0;
    mag_calibrated_ = false;
    seeded_ = false;
    valid_ = false;
}

bool OrientationEstimator::magnetic_yaw_deg(double ax, double ay, double az,
                                            double mx, double my, double mz,
                                            double& yaw_deg) {
    if (!finite3(ax, ay, az) || !finite3(mx, my, mz)) return false;
    const double amag = sensors::vector_magnitude(ax, ay, az);
    const double mmag = sensors::vector_magnitude(mx, my, mz);
    if (!(amag > 1e-6) || !(mmag > 1e-9)) return false;

    // Roll and pitch from the gravity direction. With +Z up at rest these are the same
    // two expressions the filter uses when it seeds, so a cross-check and the filter
    // cannot disagree about what "level" means.
    const double roll = std::atan2(ay, az);
    const double pitch = std::atan2(-ax, std::sqrt(ay * ay + az * az));

    const double sr = std::sin(roll), cr = std::cos(roll);
    const double sp = std::sin(pitch), cp = std::cos(pitch);

    // De-rotate the field into the local level plane: m_level = Ry(pitch) Rx(roll) m.
    const double y1 = my * cr - mz * sr;
    const double z1 = my * sr + mz * cr;
    const double level_x = mx * cp + z1 * sp;
    const double level_y = y1;
    if (std::fabs(level_x) < 1e-12 && std::fabs(level_y) < 1e-12) return false;

    // At yaw = 0 the body +Y axis points at magnetic north, so the level-frame field is
    // (H sin(yaw), H cos(yaw)) and the yaw that produced it is atan2(x, y) -- not the
    // atan2(y, x) a compass bearing would use.
    yaw_deg = std::atan2(level_x, level_y) * kRadToDeg;
    return true;
}

void OrientationEstimator::seed(double ax, double ay, double az, const double* mag_ut) {
    const double roll = std::atan2(ay, az);
    const double pitch = std::atan2(-ax, std::sqrt(ay * ay + az * az));
    double yaw = 0.0;
    if (mag_ut != nullptr) {
        double seeded_yaw = 0.0;
        if (magnetic_yaw_deg(ax, ay, az, mag_ut[0], mag_ut[1], mag_ut[2], seeded_yaw)) {
            yaw = seeded_yaw * kDegToRad;
        }
    }

    const double cy = std::cos(yaw * 0.5), sy = std::sin(yaw * 0.5);
    const double cp = std::cos(pitch * 0.5), sp = std::sin(pitch * 0.5);
    const double cr = std::cos(roll * 0.5), sr = std::sin(roll * 0.5);
    q_[0] = cy * cp * cr + sy * sp * sr;
    q_[1] = cy * cp * sr - sy * sp * cr;
    q_[2] = cy * sp * cr + sy * cp * sr;
    q_[3] = sy * cp * cr - cy * sp * sr;
    bias_rad_[0] = bias_rad_[1] = bias_rad_[2] = 0.0;
    seeded_ = true;
}

void OrientationEstimator::update(double ax, double ay, double az,
                                  double gx, double gy, double gz, double dt_s) {
    apply(ax, ay, az, gx, gy, gz, nullptr, false, dt_s);
}

void OrientationEstimator::update(double ax, double ay, double az,
                                  double gx, double gy, double gz,
                                  double mx, double my, double mz,
                                  bool mag_calibrated, double dt_s) {
    const double mag[3] = {mx, my, mz};
    apply(ax, ay, az, gx, gy, gz, mag, mag_calibrated, dt_s);
}

void OrientationEstimator::apply(double ax, double ay, double az,
                                 double gx, double gy, double gz,
                                 const double* mag_ut, bool mag_calibrated, double dt_s) {
    if (!finite3(ax, ay, az) || !finite3(gx, gy, gz) || !std::isfinite(dt_s)) {
        return;
    }
    if (mag_ut != nullptr && !finite3(mag_ut[0], mag_ut[1], mag_ut[2])) {
        mag_ut = nullptr;
    }
    if (dt_s < 0.0) dt_s = 0.0;
    if (dt_s > 1.0) dt_s = 1.0;  // bound integration against a loop stall

    double accel[3] = {ax, ay, az};
    const double accel_magnitude = sensors::vector_magnitude(ax, ay, az);
    // Gravity is only observable while the specific force is close to 1 g. Under boost,
    // parachute snatch or impact the accelerometer measures the manoeuvre, and correcting
    // attitude towards it would tilt the solution towards the thrust axis.
    const bool accel_usable = accel_magnitude > 0.5 * kGravity &&
                              accel_magnitude < 2.0 * kGravity && normalize3(accel);

    double mag[3] = {0.0, 0.0, 0.0};
    bool mag_usable = false;
    if (mag_ut != nullptr) {
        const double field = sensors::vector_magnitude(mag_ut[0], mag_ut[1], mag_ut[2]);
        if (field > sensors::kEarthFieldMinUt && field < sensors::kEarthFieldMaxUt) {
            mag[0] = mag_ut[0];
            mag[1] = mag_ut[1];
            mag[2] = mag_ut[2];
            mag_usable = normalize3(mag);
        }
    }

    if (!seeded_) {
        if (!accel_usable) {
            return;  // nothing yet defines which way is up
        }
        seed(ax, ay, az, mag_usable ? mag : nullptr);
        if (mag_usable) {
            mag_confidence_ = kMagConfidenceRequired;
            mag_calibrated_ = mag_calibrated;
        }
        valid_ = true;
        return;
    }

    const double q0 = q_[0], q1 = q_[1], q2 = q_[2], q3 = q_[3];

    // Rotation matrix body -> level frame, built once and reused by both corrections.
    const double r00 = 1.0 - 2.0 * (q2 * q2 + q3 * q3);
    const double r01 = 2.0 * (q1 * q2 - q0 * q3);
    const double r02 = 2.0 * (q1 * q3 + q0 * q2);
    const double r10 = 2.0 * (q1 * q2 + q0 * q3);
    const double r11 = 1.0 - 2.0 * (q1 * q1 + q3 * q3);
    const double r12 = 2.0 * (q2 * q3 - q0 * q1);
    const double r20 = 2.0 * (q1 * q3 - q0 * q2);
    const double r21 = 2.0 * (q2 * q3 + q0 * q1);
    const double r22 = 1.0 - 2.0 * (q1 * q1 + q2 * q2);

    double error[3] = {0.0, 0.0, 0.0};       // drives the proportional correction
    double bias_error[3] = {0.0, 0.0, 0.0};  // drives the bias integrator

    if (accel_usable) {
        // Expected direction of "up" in body axes is the third row of R.
        const double up_body[3] = {r20, r21, r22};
        double e[3];
        cross3(accel, up_body, e);
        for (int i = 0; i < 3; ++i) {
            error[i] += gains_.kp_accel * e[i];
            bias_error[i] += e[i];
        }
    }

    bool mag_corrected = false;
    if (mag_usable) {
        // Rotate the measured field into the level frame, then rebuild the reference with
        // its whole horizontal component on +Y (north). Only the yaw component of the
        // field survives that, which is exactly the axis the magnetometer may correct --
        // a magnetic disturbance cannot tip roll or pitch through this path.
        const double hx = r00 * mag[0] + r01 * mag[1] + r02 * mag[2];
        const double hy = r10 * mag[0] + r11 * mag[1] + r12 * mag[2];
        const double hz = r20 * mag[0] + r21 * mag[1] + r22 * mag[2];
        const double horizontal = std::sqrt(hx * hx + hy * hy);
        // With the vehicle's own field dominating, or very close to a magnetic pole,
        // there is no horizontal component left to steer by.
        if (horizontal > 0.10) {
            // Expected field in body axes: R^T * (0, horizontal, hz).
            const double w[3] = {r10 * horizontal + r20 * hz,
                                 r11 * horizontal + r21 * hz,
                                 r12 * horizontal + r22 * hz};
            double e[3];
            cross3(mag, w, e);
            for (int i = 0; i < 3; ++i) {
                error[i] += gains_.kp_mag * e[i];
                bias_error[i] += e[i];
            }
            mag_corrected = true;
        }
    }

    if (mag_corrected) {
        if (mag_confidence_ < kMagConfidenceMax) ++mag_confidence_;
        mag_calibrated_ = mag_calibrated;
    } else if (mag_confidence_ > 0) {
        --mag_confidence_;
    }

    // Integral term: what survives after the proportional correction has settled is gyro
    // bias. Clamped, because a long manoeuvre with no usable gravity reference would
    // otherwise let the integrator wander into the attitude solution.
    const double bias_limit_rad = gains_.bias_limit_dps * kDegToRad;
    for (int i = 0; i < 3; ++i) {
        bias_rad_[i] += gains_.ki_bias * bias_error[i] * dt_s;
        if (bias_rad_[i] > bias_limit_rad) bias_rad_[i] = bias_limit_rad;
        if (bias_rad_[i] < -bias_limit_rad) bias_rad_[i] = -bias_limit_rad;
    }

    const double wx = gx * kDegToRad + bias_rad_[0] + error[0];
    const double wy = gy * kDegToRad + bias_rad_[1] + error[1];
    const double wz = gz * kDegToRad + bias_rad_[2] + error[2];

    const double half_dt = 0.5 * dt_s;
    q_[0] = q0 + (-q1 * wx - q2 * wy - q3 * wz) * half_dt;
    q_[1] = q1 + (q0 * wx + q2 * wz - q3 * wy) * half_dt;
    q_[2] = q2 + (q0 * wy - q1 * wz + q3 * wx) * half_dt;
    q_[3] = q3 + (q0 * wz + q1 * wy - q2 * wx) * half_dt;

    const double norm =
        std::sqrt(q_[0] * q_[0] + q_[1] * q_[1] + q_[2] * q_[2] + q_[3] * q_[3]);
    if (!(norm > 1e-9) || !std::isfinite(norm)) {
        // Numerically dead: re-seed from the next sample rather than propagate a
        // meaningless quaternion.
        seeded_ = false;
        valid_ = false;
        return;
    }
    const double inv = 1.0 / norm;
    for (double& component : q_) component *= inv;
    valid_ = true;
}

OrientationEstimate OrientationEstimator::estimate() const {
    OrientationEstimate out;
    const double q0 = q_[0], q1 = q_[1], q2 = q_[2], q3 = q_[3];

    double sin_pitch = 2.0 * (q0 * q2 - q1 * q3);
    if (sin_pitch > 1.0) sin_pitch = 1.0;
    if (sin_pitch < -1.0) sin_pitch = -1.0;

    out.roll_deg =
        std::atan2(2.0 * (q2 * q3 + q0 * q1), 1.0 - 2.0 * (q1 * q1 + q2 * q2)) * kRadToDeg;
    out.pitch_deg = std::asin(sin_pitch) * kRadToDeg;
    out.yaw_deg =
        std::atan2(2.0 * (q1 * q2 + q0 * q3), 1.0 - 2.0 * (q2 * q2 + q3 * q3)) * kRadToDeg;
    // A compass bearing runs clockwise from north; this yaw runs anticlockwise about the
    // up axis. They are the same angle with opposite sign, and conflating them is the
    // classic way to fly a heading that is exactly mirrored.
    out.heading_deg = wrap_degrees_360(-out.yaw_deg);
    out.valid = valid_;
    out.yaw_is_magnetic =
        valid_ && mag_calibrated_ && mag_confidence_ >= kMagConfidenceRequired;
    for (int i = 0; i < 3; ++i) {
        // The integrator holds a correction that is ADDED to the gyro, so the bias it has
        // identified is its negation. Reporting the correction as though it were the bias
        // would put the wrong sign in the health telemetry.
        out.gyro_bias_dps[i] = -bias_rad_[i] * kRadToDeg;
    }
    return out;
}

}  // namespace flight
