#pragma once

#include <cstdint>
#include <string>

namespace flight {

struct BoardPins {
    static constexpr int i2c_sda = 4;
    static constexpr int i2c_scl = 5;
    static constexpr int spi_sck = 18;
    static constexpr int spi_mosi = 19;
    static constexpr int spi_miso = 16;
    static constexpr int lora_cs = 17;
    static constexpr int lora_reset = 20;
    static constexpr int lora_dio0 = 21;
    static constexpr int lora_dio1 = 22;
    static constexpr int gps_tx = 12;
    static constexpr int gps_rx = 13;
    static constexpr int imu_int = 7;
    static constexpr int status_led = 14;
    static constexpr int battery_adc = 26;
    static constexpr int sd_cs = 6;
};

enum class RadioMode { test, official };

enum class MissionState { init, self_test, ready, flight, landed, recovery, fault };

enum class FaultSeverity { warning, error, critical };

struct Configuration {
    std::string team_id = "CAN-Team-XX";
    std::uint32_t telemetry_period_ms = 500;
    RadioMode radio_mode = RadioMode::test;
    std::uint8_t test_sync_word = 0xF3;
    std::uint8_t official_sync_word = 0xA5;
    std::uint32_t post_impact_transmission_ms = 5000;
    std::uint32_t sensor_stale_after_ms = 2000;
    float battery_adc_scale = 0.0F;
};

inline std::uint8_t sync_word(const Configuration& config) {
    return config.radio_mode == RadioMode::official ? config.official_sync_word : config.test_sync_word;
}

}  // namespace flight
