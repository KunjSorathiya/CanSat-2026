#pragma once

#include "cansat/telemetry.hpp"

#include <cstdint>
#include <string>

namespace flight {

struct SensorHealth {
    bool initialized = false;
    bool healthy = false;
    std::uint64_t last_update_ms = 0;
};

// One inertial + magnetic reading, already converted to engineering units and already
// rotated into the project body frame.
//
// Body frame (documentation/design/software-architecture.md): +Z along the can's long
// axis towards the nose, +X and +Y completing a right-handed set across the body. At rest
// and upright the accelerometer therefore reads +1 g on Z, which is what the attitude
// estimator's gravity reference assumes.
//
// The MPU-9250's magnetometer is a separate AK8963 die whose axes do NOT line up with the
// accelerometer and gyroscope axes; the driver rotates them before filling this struct,
// so every field below is in one consistent frame by the time anything else sees it.
struct ImuSample {
    double ax_mps2 = 0.0;
    double ay_mps2 = 0.0;
    double az_mps2 = 0.0;
    double gx_dps = 0.0;
    double gy_dps = 0.0;
    double gz_dps = 0.0;
    // Magnetic flux density in microtesla, body frame, sensitivity-adjusted but NOT
    // hard/soft-iron corrected: that correction belongs to the vehicle, not the sensor,
    // and is applied by the controller from the calibration in use.
    double mx_ut = 0.0;
    double my_ut = 0.0;
    double mz_ut = 0.0;
    double die_temperature_c = 0.0;  // MPU-9250 on-die temperature, diagnostic only
    bool valid = false;              // accelerometer + gyroscope fields are usable
    bool mag_valid = false;          // a fresh, unsaturated magnetometer sample is present
    std::uint64_t timestamp_ms = 0;
};

struct BaroSample {
    double pressure_pa = 0.0;
    double temperature_c = 0.0;
    double altitude_m = 0.0;  // pressure altitude relative to Configuration::reference_pressure_pa
    bool valid = false;
    std::uint64_t timestamp_ms = 0;
};

// Inertial measurement unit (MPU-9250 on this vehicle): accelerometer, gyroscope and,
// on a genuine part, the integrated AK8963 magnetometer.
//
// has_magnetometer() is not decoration. Boards sold as MPU-9250 modules are frequently
// MPU-6500 dies with no magnetometer at all, and the difference is only visible from the
// WHO_AM_I values read during initialize(). The estimator asks rather than assumes, so a
// substituted part degrades to 6-axis attitude instead of producing a heading from a bus
// that is not answering.
class Imu {
public:
    virtual ~Imu() = default;
    virtual bool initialize() = 0;
    virtual bool read(ImuSample& out, std::uint64_t now_ms) = 0;
    virtual bool has_magnetometer() const = 0;
    virtual SensorHealth health() const = 0;
};

// Barometer / temperature (BMP280 on this vehicle).
class Barometer {
public:
    virtual ~Barometer() = default;
    virtual bool initialize() = 0;
    virtual bool read(BaroSample& out, std::uint64_t now_ms) = 0;
    virtual SensorHealth health() const = 0;
};

// GNSS receiver (NEO-6M). Reception must be non-blocking; a missing fix must never
// stall the flight loop.
class Gps {
public:
    virtual ~Gps() = default;
    virtual bool initialize() = 0;
    virtual void poll(std::uint64_t now_ms) = 0;
    virtual bool latest(cansat::GpsData& data) const = 0;
    // Mission-clock reading at which the fix returned by latest() was last refreshed by
    // the receiver; zero if no fix has ever been parsed. A receiver that goes silent --
    // a connector shaken loose, a browned-out module, a severed antenna lead -- leaves
    // its last good fix in the parser forever, so the caller needs this to tell a live
    // position from a frozen one.
    virtual std::uint64_t last_fix_ms() const = 0;
    virtual std::uint32_t checksum_errors() const = 0;
    virtual SensorHealth health() const = 0;
};

// LoRa radio (SX1278 / RA-02).
class Radio {
public:
    virtual ~Radio() = default;
    virtual bool initialize(std::uint8_t sync_word) = 0;
    virtual bool transmit(const std::string& packet) = 0;
    virtual bool healthy() const = 0;
};

// Onboard data log (microSD over SPI, on the shared SPI0 bus with CS on GP6).
// Every method must fail rather than block: losing the card must never cost a packet.
class SdLogger {
public:
    virtual ~SdLogger() = default;
    virtual bool initialize() = 0;
    virtual bool append(const cansat::TelemetryRecord& record, const std::string& packet) = 0;
    virtual bool flush() = 0;
    virtual bool healthy() const = 0;
};

// Board-level I/O: status LED and battery sense.
class BoardIo {
public:
    virtual ~BoardIo() = default;
    virtual void set_status_led(bool on) = 0;
    // Raw voltage at the ADC pin (0 .. Vref). The controller applies the divider ratio;
    // implementations must NOT pre-scale.
    virtual float battery_voltage() const = 0;
};

}  // namespace flight
