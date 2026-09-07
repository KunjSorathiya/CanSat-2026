#pragma once

#include "flight/interfaces.hpp"

#include <string>
#include <vector>

namespace flight::test {

// A stationary, upright, level vehicle facing magnetic north.
//
// The default magnetic sample is a real northern-hemisphere field rather than a tidy
// unit vector: about 40 uT horizontal and 17 uT downward, which in this project's
// body frame (+Z up) is +Y north and -Z down, for a total of 43.5 uT. That total has to
// land inside the estimator's earth-field gate, so a made-up field would quietly disable
// every magnetometer path in the tests that use it.
inline constexpr double kMockFieldNorthUt = 40.0;
inline constexpr double kMockFieldDownUt = 17.0;

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
        // A driver never reports a magnetometer sample from a part that has none, so
        // neither does the mock: `magnetometer` gates the field the same way the real
        // WHO_AM_I result does.
        out.mag_valid = magnetometer && sample.mag_valid;
        out.timestamp_ms = now_ms;
        return true;
    }
    bool has_magnetometer() const override { return magnetometer; }
    SensorHealth health() const override { return health_; }

    ImuSample sample{0.1,  0.2, 9.8,  0.0,   0.0,
                     0.0,  0.0, kMockFieldNorthUt, -kMockFieldDownUt,
                     25.0, true, true, 0};
    bool magnetometer = true;
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
    // A live receiver refreshes its fix on every poll. Set `silent` to simulate one that
    // has stopped talking -- an unplugged lead, a browned-out module -- which leaves the
    // last fix in the parser but stops the clock that says how old it is.
    void poll(std::uint64_t now_ms) override {
        health_.last_update_ms = now_ms;
        if (silent) {
            health_.healthy = false;
            return;
        }
        health_.healthy = true;
        if (fix.valid) last_fix_ms_ = now_ms;
    }
    bool latest(cansat::GpsData& data) const override {
        data = fix;
        return fix.valid;
    }
    std::uint64_t last_fix_ms() const override { return last_fix_ms_; }
    std::uint32_t checksum_errors() const override { return checksum_error_count; }
    SensorHealth health() const override { return health_; }

    cansat::GpsData fix{18.0, 73.0, 20.0, true, 0.0, false, 8};
    bool fail_init = false;
    bool silent = false;
    std::uint32_t checksum_error_count = 0;

private:
    SensorHealth health_;
    std::uint64_t last_fix_ms_ = 0;
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
    bool poll_receive(std::string& out) override {
        if (inbox.empty()) return false;
        out = inbox.front();
        inbox.erase(inbox.begin());
        ++receive_polls;
        return true;
    }
    bool healthy() const override { return healthy_; }

    std::uint8_t sync_word_ = 0;
    std::vector<std::string> packets;
    std::vector<std::string> inbox;   // what the ground is transmitting at us
    int receive_polls = 0;
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
    bool append(const std::string& line) override {
        if (fail_append) return false;
        lines.push_back(line);
        return true;
    }
    bool flush() override { return !fail_flush; }
    bool erase() override {
        if (fail_erase) return false;
        lines.clear();
        ++erases;
        return true;
    }
    bool healthy() const override { return healthy_; }

    std::vector<std::string> lines;
    int erases = 0;
    bool fail_erase = false;
    bool fail_init = false;
    bool fail_append = false;
    bool fail_flush = false;

private:
    bool healthy_ = false;
};

class MockSound final : public SoundSensor {
public:
    bool initialize() override {
        health_.initialized = !fail_init;
        health_.healthy = !fail_init;
        return !fail_init;
    }
    bool read(SoundSample& out, std::uint64_t now_ms) override {
        ++reads;
        if (fail_read) return false;
        out.level_mv_pp = level_mv_pp;
        out.clipped = clipped;
        out.valid = analog_connected;
        out.gate_duty_pct = gate_duty_pct;
        out.gate_valid = gate_connected;
        out.timestamp_ms = now_ms;
        health_.last_update_ms = now_ms;
        return true;
    }
    SensorHealth health() const override { return health_; }

    double level_mv_pp = 120.0;
    double gate_duty_pct = 0.0;
    bool clipped = false;
    bool analog_connected = true;
    bool gate_connected = true;
    bool fail_init = false;
    bool fail_read = false;
    int reads = 0;

private:
    SensorHealth health_{};
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
