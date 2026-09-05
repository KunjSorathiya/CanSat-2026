#pragma once

#include "cansat/sx1278.hpp"
#include "flight/config.hpp"
#include "flight/fat_volume.hpp"
#include "flight/interfaces.hpp"
#include "flight/pico/bmp280.hpp"
#include "flight/pico/mpu9250.hpp"
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
    bool has_magnetometer() const override { return device_.has_magnetometer(); }
    SensorHealth health() const override { return health_; }

    // WHO_AM_I read at initialisation. 0x71/0x73 is a real MPU-9250/9255; 0x70 is an
    // MPU-6500 sold as one, with no magnetometer. Worth reporting on the bench.
    std::uint8_t who_am_i() const { return device_.who_am_i(); }

private:
    const Configuration& config_;
    pico::Mpu9250 device_;
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
    explicit PicoGps(const Configuration& config) : config_(config) {}
    bool initialize() override;
    void poll(std::uint64_t now_ms) override;
    bool latest(cansat::GpsData& data) const override;
    std::uint64_t last_fix_ms() const override { return device_.last_fix_ms(); }
    std::uint32_t checksum_errors() const override { return device_.checksum_errors(); }
    SensorHealth health() const override { return health_; }

private:
    const Configuration& config_;
    pico::Neo6mGps device_;
    SensorHealth health_;
};

class PicoRadio final : public Radio {
public:
    explicit PicoRadio(const Configuration& config) : config_(config) {}
    bool initialize(std::uint8_t sync_word) override;
    bool transmit(const std::string& packet) override;
    bool healthy() const override { return healthy_; }

    // Version register 0x42, read during begin(). 0x12 is the SX1276/77/78 family; a 0x00
    // or 0xFF means the SPI transaction itself failed rather than the modem answering
    // wrongly. Worth reporting on the bench, like the IMU's WHO_AM_I.
    std::uint8_t chip_version() const { return radio_.chip_version(); }
    // A fresh read of register 0x42 over the bus. Used by Gate 7 to check that the
    // radio is still readable while the microSD is active on the same SPI0.
    std::uint8_t probe_version() { return radio_.probe_version(); }
    // Why a transmit failed: TxDone set means the radio finished and DIO0 did not say
    // so; clear means it never finished. Different faults, identical symptom.
    std::uint8_t last_tx_irq_flags() const { return radio_.last_tx_irq_flags(); }
    std::uint32_t tx_timeouts() const { return radio_.tx_timeouts(); }

private:
    Configuration config_;
    cansat::Sx1278 radio_;
    bool healthy_ = false;
};

// microSD over the shared SPI0 bus, chip-select on GP6. The breakout is a 3.3 V module
// (2.6 to 3.6 V), so it sits on the same rail as everything else on this vehicle and
// needs no level shifting or boost stage of its own.
class PicoSdLogger final : public SdLogger {
public:
    bool initialize() override;
    bool append(const cansat::TelemetryRecord& record, const std::string& packet) override;
    bool flush() override;
    bool healthy() const override { return healthy_; }

    // Reported on the bench, like the IMU's WHO_AM_I and the radio's version register.
    // boot_count() is bring-up row 6.6 and is the cheapest evidence that the log survived
    // a power cycle rather than being silently restarted.
    bool high_capacity() const { return card_.high_capacity(); }
    std::uint32_t boot_count() const { return log_.boot_count(); }
    std::uint32_t record_count() const { return log_.record_count(); }
    // A record too long for one 512-byte block is cut. The log is evidence, so a cut
    // record has to be visible as one -- and it was counted by RawBlockLog and read by
    // nothing, which is the same as not counting it.
    std::uint32_t truncated_records() const { return log_.truncated_records(); }
    // Why initialisation failed, when it did. A missing file and an unformatted card need
    // different fixes, and "SD failed" does not tell an operator which they are looking at.
    FatVolume::Status locate_status() const { return locate_status_; }
    // How many separate runs the log file occupies. One is ideal; more is
    // ordinary and costs nothing but a line in the bring-up record.
    int log_extents() const { return layout_.extent_count; }

private:
    pico::SdCard card_;
    RawBlockLog log_;
    FatVolume::Layout layout_{};
    FatVolume::Status locate_status_ = FatVolume::Status::ok;
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
