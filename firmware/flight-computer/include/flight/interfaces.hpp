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

class Imu {
public:
    virtual ~Imu() = default;
    virtual bool initialize() = 0;
    virtual bool read(cansat::TelemetryRecord& record, std::uint64_t now_ms) = 0;
    virtual SensorHealth health() const = 0;
};

class Barometer {
public:
    virtual ~Barometer() = default;
    virtual bool initialize() = 0;
    virtual bool read(cansat::TelemetryRecord& record, std::uint64_t now_ms) = 0;
    virtual SensorHealth health() const = 0;
};

class Gps {
public:
    virtual ~Gps() = default;
    virtual bool initialize() = 0;
    virtual void poll(std::uint64_t now_ms) = 0;
    virtual bool latest(cansat::GpsData& data) const = 0;
    virtual SensorHealth health() const = 0;
};

class Radio {
public:
    virtual ~Radio() = default;
    virtual bool initialize(std::uint8_t sync_word) = 0;
    virtual bool transmit(const std::string& packet) = 0;
    virtual bool healthy() const = 0;
};

class SdLogger {
public:
    virtual ~SdLogger() = default;
    virtual bool initialize() = 0;
    virtual bool append(const cansat::TelemetryRecord& record, const std::string& packet) = 0;
    virtual bool flush() = 0;
    virtual bool healthy() const = 0;
};

class BoardIo {
public:
    virtual ~BoardIo() = default;
    virtual void set_status_led(bool on) = 0;
    virtual float battery_voltage() const = 0;
};

}  // namespace flight
