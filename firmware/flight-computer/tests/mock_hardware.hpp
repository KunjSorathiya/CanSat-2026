#pragma once

#include "flight/interfaces.hpp"

#include <string>
#include <vector>

namespace flight::test {

class MockImu final : public Imu {
public:
    bool initialize() override {
        if (fail_init) return false;
        health_.initialized = health_.healthy = true;
        return true;
    }
    bool read(ImuSample& out, std::uint64_t now_ms) override {
        if (fail_read) return false;  // a real driver only advances health on success
        health_.last_update_ms = now_ms;
        out = sample;
        out.valid = true;
        out.timestamp_ms = now_ms;
        return true;
    }
    SensorHealth health() const override { return health_; }

    ImuSample sample{0.1, 0.2, 9.8, 0.0, 0.0, 0.0, 25.0, true, 0};
    bool fail_init = false;
    bool fail_read = false;

private:
    SensorHealth health_;
};

class MockBarometer final : public Barometer {
public:
    bool initialize() override {
        if (fail_init) return false;
        health_.initialized = health_.healthy = true;
        return true;
    }
    bool read(BaroSample& out, std::uint64_t now_ms) override {
        if (fail_read) return false;  // a real driver only advances health on success
        health_.last_update_ms = now_ms;
        out = sample;
        out.valid = true;
        out.timestamp_ms = now_ms;
        return true;
    }
    SensorHealth health() const override { return health_; }

    BaroSample sample{101325.0, 25.0, 0.0, true, 0};
    bool fail_init = false;
    bool fail_read = false;

private:
    SensorHealth health_;
};

class MockGps final : public Gps {
public:
    bool initialize() override {
        if (fail_init) return false;
        health_.initialized = health_.healthy = true;
        return true;
    }
    void poll(std::uint64_t now_ms) override { health_.last_update_ms = now_ms; }
    bool latest(cansat::GpsData& data) const override {
        data = fix;
        return fix.valid;
    }
    std::uint32_t checksum_errors() const override { return checksum_error_count; }
    SensorHealth health() const override { return health_; }

    cansat::GpsData fix{18.0, 73.0, 20.0, true, 0.0, false, 8};
    bool fail_init = false;
    std::uint32_t checksum_error_count = 0;

private:
    SensorHealth health_;
};

class MockRadio final : public Radio {
public:
    bool initialize(std::uint8_t sync_word) override {
        sync_word_ = sync_word;
        ++init_calls;
        healthy_ = !fail_init;
        return healthy_;
    }
    bool transmit(const std::string& packet) override {
        if (fail_tx) return false;
        packets.push_back(packet);
        return true;
    }
    bool healthy() const override { return healthy_; }

    std::uint8_t sync_word_ = 0;
    std::vector<std::string> packets;
    int init_calls = 0;
    bool fail_init = false;
    bool fail_tx = false;

private:
    bool healthy_ = false;
};

class MockLogger final : public SdLogger {
public:
    bool initialize() override {
        healthy_ = !fail_init;
        return healthy_;
    }
    bool append(const cansat::TelemetryRecord&, const std::string& packet) override {
        if (fail_append) return false;
        packets.push_back(packet);
        return true;
    }
    bool flush() override { return !fail_flush; }
    bool healthy() const override { return healthy_; }

    std::vector<std::string> packets;
    bool fail_init = false;
    bool fail_append = false;
    bool fail_flush = false;

private:
    bool healthy_ = false;
};

class MockBoard final : public BoardIo {
public:
    void set_status_led(bool on) override {
        led_on = on;
        ++led_writes;
    }
    float battery_voltage() const override { return pin_voltage; }

    bool led_on = false;
    int led_writes = 0;
    float pin_voltage = 1.65f;
};

}  // namespace flight::test
