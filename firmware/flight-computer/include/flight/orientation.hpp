#pragma once

namespace flight {

struct OrientationEstimate {
    double roll_deg = 0.0;
    double pitch_deg = 0.0;
    double yaw_deg = 0.0;  // relative, gyro-integrated heading (no magnetometer on this vehicle)
    bool valid = false;
};

// Complementary-filter attitude estimator for an accelerometer + gyroscope (MPU6050).
//
// Roll and pitch fuse the accelerometer gravity vector with integrated body rates.
// Yaw is a pure relative integration of the body Z rate: it is NOT an absolute magnetic
// heading and must be reported/consumed as a relative angle only.
class OrientationEstimator {
public:
    explicit OrientationEstimator(double alpha = 0.98);

    void set_alpha(double alpha);
    void reset();

    // accel in m/s^2, gyro in deg/s, dt in seconds. Non-finite inputs are ignored.
    void update(double ax, double ay, double az,
                double gx, double gy, double gz, double dt_s);

    OrientationEstimate estimate() const;

private:
    double alpha_;
    double roll_deg_ = 0.0;
    double pitch_deg_ = 0.0;
    double yaw_deg_ = 0.0;
    bool have_reference_ = false;
    bool valid_ = false;
};

// Wrap an angle in degrees to (-180, 180].
double wrap_degrees(double degrees);

}  // namespace flight
