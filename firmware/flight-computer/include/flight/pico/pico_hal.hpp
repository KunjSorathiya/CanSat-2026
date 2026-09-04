#pragma once

#include "cansat/sx1278.hpp"
#include "flight/config.hpp"
#include "flight/interfaces.hpp"
#include "flight/pico/bmp280.hpp"
#include "flight/pico/mpu6050.hpp"
#include "flight/pico/neo6m.hpp"
#include "flight/pico/sd_card.hpp"
#include "flight/raw_block_log.hpp"

#include <cstdint>

namespace flight {

// Bring up the shared I2C0 / SPI0 / ADC hardware exactly once. Safe to call repeatedly.
void pico_buses_init();

// ---- interface adapters wiring the concrete drivers to the flight core ----

class PicoImu final : public Imu {
public:
    explicit PicoImu(const Configuration& config) : config_(config) {}
    bool initialize() override;
    bool read(ImuSample& out, std::uint64_t now_ms) override;
    SensorHealth health() const override { return health_; }

private:
    const Configuration& config_;
    pico::Mpu6050 device_;
    SensorHealth health_;
};

class PicoBarometer final : public Barometer {
public:
    explicit PicoBarometer(const Configuration& config) : config_(config) {}
    bool initialize() override;
    bool read(BaroSample& out, std::uint64_t now_ms) override;
    SensorHealth health() const override { return health_; }

private:
    const Configuration& config_;
    pico::Bmp280 device_;
    SensorHealth health_;
};

class PicoGps final : public Gps {
public:
    bool initialize() override;
    void poll(std::uint64_t now_ms) override;
    bool latest(cansat::GpsData& data) const override;
    std::uint32_t checksum_errors() const override { return device_.checksum_errors(); }
    SensorHealth health() const override { return health_; }

private:
    pico::Neo6mGps device_;
    SensorHealth health_;
};

class PicoRadio final : public Radio {
public:
    explicit PicoRadio(const Configuration& config) : config_(config) {}
    bool initialize(std::uint8_t sync_word) override;
    bool transmit(const std::string& packet) override;
    bool healthy() const override { return healthy_; }

private:
    Configuration config_;
    cansat::Sx1278 radio_;
    bool healthy_ = false;
};

class PicoSdLogger final : public SdLogger {
public:
    bool initialize() override;
    bool append(const cansat::TelemetryRecord& record, const std::string& packet) override;
    bool flush() override;
    bool healthy() const override { return healthy_; }

private:
    pico::SdCard card_;
    RawBlockLog log_;
    bool healthy_ = false;
};

class PicoBoardIo final : public BoardIo {
public:
    explicit PicoBoardIo(const Configuration& config) : config_(config) {}
    void set_status_led(bool on) override;
    float battery_voltage() const override;

private:
    Configuration config_;
};

}  // namespace flight
