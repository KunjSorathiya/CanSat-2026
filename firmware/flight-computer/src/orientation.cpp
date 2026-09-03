#include "flight/orientation.hpp"

#include "flight/sensor_math.hpp"

#include <cmath>

namespace flight {

namespace {
constexpr double kRadToDeg = 57.29577951308232;
constexpr double kGravity = sensors::kStandardGravity;

bool finite3(double a, double b, double c) {
    return std::isfinite(a) && std::isfinite(b) && std::isfinite(c);
}
}  // namespace

double wrap_degrees(double degrees) {
    while (degrees > 180.0) degrees -= 360.0;
    while (degrees <= -180.0) degrees += 360.0;
    return degrees;
}

OrientationEstimator::OrientationEstimator(double alpha) : alpha_(alpha) {
    set_alpha(alpha);
}

void OrientationEstimator::set_alpha(double alpha) {
    if (!(alpha >= 0.0) || !(alpha <= 1.0)) {
        alpha = 0.98;
    }
    alpha_ = alpha;
}

void OrientationEstimator::reset() {
    roll_deg_ = pitch_deg_ = yaw_deg_ = 0.0;
    have_reference_ = false;
    valid_ = false;
}

void OrientationEstimator::update(double ax, double ay, double az,
                                  double gx, double gy, double gz, double dt_s) {
    if (!finite3(ax, ay, az) || !finite3(gx, gy, gz) || !std::isfinite(dt_s)) {
        return;
    }
    if (dt_s < 0.0) dt_s = 0.0;
    if (dt_s > 1.0) dt_s = 1.0;  // bound integration step against loop stalls

    // Gyro-only prediction.
    double roll_pred = roll_deg_ + gx * dt_s;
    double pitch_pred = pitch_deg_ + gy * dt_s;
    yaw_deg_ = wrap_degrees(yaw_deg_ + gz * dt_s);

    // Use the accelerometer as a gravity reference only when its magnitude is close to
    // 1 g; during boost or impact the specific force is dominated by non-gravity terms.
    const double accel_mag = sensors::vector_magnitude(ax, ay, az);
    const bool accel_usable = accel_mag > 0.5 * kGravity && accel_mag < 2.0 * kGravity;

    if (accel_usable) {
        const double roll_acc = std::atan2(ay, az) * kRadToDeg;
        const double pitch_acc = std::atan2(-ax, std::sqrt(ay * ay + az * az)) * kRadToDeg;
        if (!have_reference_) {
            roll_deg_ = roll_acc;
            pitch_deg_ = pitch_acc;
            have_reference_ = true;
        } else {
            roll_deg_ = alpha_ * roll_pred + (1.0 - alpha_) * roll_acc;
            pitch_deg_ = alpha_ * pitch_pred + (1.0 - alpha_) * pitch_acc;
        }
    } else {
        roll_deg_ = roll_pred;
        pitch_deg_ = pitch_pred;
    }

    roll_deg_ = wrap_degrees(roll_deg_);
    pitch_deg_ = wrap_degrees(pitch_deg_);
    valid_ = true;
}

OrientationEstimate OrientationEstimator::estimate() const {
    OrientationEstimate out;
    out.roll_deg = roll_deg_;
    out.pitch_deg = pitch_deg_;
    out.yaw_deg = yaw_deg_;
    out.valid = valid_;
    return out;
}

}  // namespace flight
