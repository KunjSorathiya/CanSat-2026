#pragma once

#include "flight/interfaces.hpp"

#include <vector>

namespace flight::test {

class MockImu final : public Imu {
public:
    bool initialize() override { health_.initialized = health_.healthy = true; return true; }
    bool read(cansat::TelemetryRecord& record, std::uint64_t now_ms) override {
        health_.last_update_ms = now_ms;
        record.roll_deg = 1.0; record.pitch_deg = 2.0; record.yaw_deg = 3.0;
        record.acceleration_x_mps2 = 0.1; record.acceleration_y_mps2 = 0.2; record.acceleration_z_mps2 = 9.8;
        record.validity.roll = record.validity.pitch = record.validity.yaw = true;
        record.validity.acceleration_x = record.validity.acceleration_y = record.validity.acceleration_z = true;
        return true;
    }
    SensorHealth health() const override { return health_; }
private: SensorHealth health_;
};

class MockBarometer final : public Barometer {
public:
    bool initialize() override { health_.initialized = health_.healthy = true; return true; }
    bool read(cansat::TelemetryRecord& record, std::uint64_t now_ms) override {
        health_.last_update_ms = now_ms;
        record.altitude_m = 10.0; record.pressure_pa = 101325.0; record.temperature_c = 25.0;
        record.validity.altitude = record.validity.pressure = record.validity.temperature = true;
        return true;
    }
    SensorHealth health() const override { return health_; }
private: SensorHealth health_;
};

class MockGps final : public Gps {
public:
    bool initialize() override { health_.initialized = health_.healthy = true; return true; }
    void poll(std::uint64_t now_ms) override { health_.last_update_ms = now_ms; }
    bool latest(cansat::GpsData& data) const override { data.valid = true; data.latitude = 18.0; data.longitude = 73.0; data.altitude = 20.0; return true; }
    SensorHealth health() const override { return health_; }
private: SensorHealth health_;
};

class MockRadio final : public Radio {
public:
    bool initialize(std::uint8_t sync_word) override { sync_word_ = sync_word; healthy_ = true; return true; }
    bool transmit(const std::string& packet) override { packets.push_back(packet); return true; }
    bool healthy() const override { return healthy_; }
    std::uint8_t sync_word_ = 0; std::vector<std::string> packets;
private: bool healthy_ = false;
};

class MockLogger final : public SdLogger {
public:
    bool initialize() override { healthy_ = true; return true; }
    bool append(const cansat::TelemetryRecord&, const std::string& packet) override { packets.push_back(packet); return true; }
    bool flush() override { return healthy_; }
    bool healthy() const override { return healthy_; }
    std::vector<std::string> packets;
private: bool healthy_ = false;
};

class MockBoard final : public BoardIo {
public:
    void set_status_led(bool on) override { led_on = on; }
    float battery_voltage() const override { return 0.0F; }
    bool led_on = false;
};

}  // namespace flight::test
