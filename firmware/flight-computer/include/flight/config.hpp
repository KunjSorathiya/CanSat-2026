#pragma once

#include <cstdint>
#include <string>

namespace flight {

// Physical Pico GPIO assignment. Mirrors documentation/hardware/pico-gpio-map.md.
// These are logical resource reservations; electrical validation remains a hardware task.
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
    static constexpr int gps_tx = 12;   // Pico TX -> GPS RX
    static constexpr int gps_rx = 13;   // Pico RX <- GPS TX
    static constexpr int imu_int = 7;
    static constexpr int status_led = 14;
    static constexpr int battery_adc = 26;  // ADC0
    static constexpr int sd_cs = 6;
};

enum class RadioMode { test, official };

enum class MissionState { init, self_test, ready, flight, landed, recovery, fault };

enum class FaultSeverity { warning, error, critical };

// Centralised radio parameters. Only the sync words are fixed by the rulebook; every
// other LoRa parameter below is a provisional engineering default and must be confirmed
// against the RA-02 carrier and competition guidance before flight.
struct RadioConfig {
    std::uint32_t frequency_hz = 433000000;  // PROVISIONAL: 433 MHz band, exact channel TBD
    std::int8_t tx_power_dbm = 17;            // PROVISIONAL
    std::uint8_t spreading_factor = 9;        // PROVISIONAL (SF7..SF12)
    std::uint32_t bandwidth_hz = 125000;      // PROVISIONAL
    std::uint8_t coding_rate = 5;             // PROVISIONAL denominator 4/5..4/8 -> 5..8
    std::uint16_t preamble_length = 8;        // PROVISIONAL
    bool enable_crc = true;                   // PROVISIONAL
    std::uint8_t test_sync_word = 0xF3;       // rulebook: pre-launch testing
    std::uint8_t official_sync_word = 0xA5;   // rulebook: official launch
};

struct Configuration {
    // ---- Identity -------------------------------------------------------------
    // MUST be replaced with the registered team identifier before any launch or
    // official test. "CAN-Team-XX" is rejected by the telemetry formatter on purpose.
    std::string team_id = "CAN-Team-XX";

    // ---- Scheduling (milliseconds) -----------------------------------------
    std::uint32_t telemetry_period_ms = 500;  // 2 Hz target; hard cap 1000 (1 Hz minimum)
    std::uint32_t sensor_period_ms = 100;     // sensor acquisition + orientation update
    std::uint32_t sd_flush_period_ms = 2000;
    std::uint32_t health_period_ms = 1000;
    std::uint32_t battery_period_ms = 1000;

    // ---- Radio -------------------------------------------------------------
    RadioMode radio_mode = RadioMode::test;
    RadioConfig radio;
    std::uint32_t radio_recovery_backoff_ms = 1000;   // spacing between bounded re-init attempts
    std::uint8_t radio_max_consecutive_failures = 5;  // TX failures before a radio fault + re-init

    // Legacy flat aliases kept for older call sites / tests.
    std::uint8_t test_sync_word = 0xF3;
    std::uint8_t official_sync_word = 0xA5;

    // ---- Sensor validity -------------------------------------------------------
    std::uint32_t sensor_stale_after_ms = 2000;

    // ---- Barometric altitude ------------------------------------------------
    double reference_pressure_pa = 101325.0;   // PROVISIONAL sea-level reference; set from field baro
    bool altitude_relative_to_baseline = true; // report altitude above the power-on ground baseline
    std::uint32_t baseline_samples = 20;       // barometer samples averaged while in READY

    // ---- Orientation (complementary filter) --------------------------------
    double orientation_alpha = 0.98;  // gyro weight; (1 - alpha) is the accelerometer correction

    // ---- Startup calibration (vehicle stationary on the pad) --------------
    // Gyro bias + accelerometer offset + barometric ground reference are estimated
    // while in INIT/SELF_TEST/READY. A calibration that never settles still resolves
    // (best-effort) at calib_timeout_ms so the mission is never blocked.
    std::uint32_t calib_samples = 80;             // still IMU samples required to accept
    std::uint32_t calib_timeout_ms = 20000;       // resolve best-effort after this
    double calib_gyro_still_dps = 2.0;            // per-axis gyro std-dev gate for "stationary"
    double calib_accel_tol_mps2 = 1.5;            // |mean accel| must be within this of 1 g

    // ---- Arming / launch lockout ----------------------------------------
    std::uint32_t arming_delay_ms = 3000;         // no launch detection before this since boot
    bool require_calibration_to_arm = true;       // wait for calibration to settle before arming

    // ---- Sensor plausibility (datasheet-derived, not invented mission limits) ----
    double baro_min_pa = 30000.0;                 // BMP280 spec floor ~300 hPa (margin)
    double baro_max_pa = 115000.0;                // BMP280 spec ceiling ~1100 hPa (margin)
    double baro_min_temp_c = -50.0;               // BMP280 operating range -40..+85 (margin)
    double baro_max_temp_c = 95.0;
    double accel_clip_mps2 = 170.0;              // MPU6050 +-16 g full-scale ~157 m/s^2 (margin)
    double gyro_clip_dps = 2200.0;               // MPU6050 +-2000 dps full-scale (margin)

    // ---- Launch / landing detection --------------------------------------
    // PROVISIONAL: the rulebook does not fix these thresholds. Tune against test data.
    double launch_accel_mps2 = 30.0;              // |a| above this implies boost
    double launch_altitude_gain_m = 15.0;        // OR climb this far above the ground baseline
    std::uint32_t launch_confirm_ms = 300;       // launch condition must hold this long
    std::uint32_t min_flight_ms = 3000;          // suppress landing detection until this into flight
    double landing_accel_epsilon_mps2 = 2.5;     // ||a| - g| below this implies at rest
    double landing_altitude_rate_max_mps = 1.0;  // |vertical speed| below this implies not descending
    std::uint32_t landing_confirm_ms = 3000;     // rest condition must hold this long

    // ---- Post-impact --------------------------------------------------------
    std::uint32_t post_impact_transmission_ms = 5000;  // >= rulebook 5 s minimum after impact

    // ---- Battery monitor --------------------------------------------------
    // The resistor-divider ratio is unknown. Leave divider_ratio at 0 to report the raw
    // ADC pin voltage unscaled; never assume a direct LiPo-to-ADC connection.
    float battery_adc_ref_v = 3.3f;
    std::uint16_t battery_adc_max_counts = 4095;
    float battery_divider_ratio = 0.0f;   // (R1 + R2) / R2
    float battery_low_voltage = 0.0f;     // 0 disables the low-battery fault
    float battery_adc_scale = 0.0f;       // deprecated alias for battery_divider_ratio

    // ---- SD logging -------------------------------------------------------
    std::uint8_t sd_max_failures = 10;    // consecutive write failures before SD logging is disabled

    // ---- GPS optional-field precision (rulebook unspecified) --------------
    int gps_latlon_decimals = 6;
    int gps_alt_decimals = 1;

    // Append non-mandatory diagnostic fields ("MODE-<state>", "FAULTS-<n>") AFTER every
    // mandatory (and GPS) field. The official parser ignores unknown optional fields; set
    // this false if a stricter receiver is confirmed.
    bool append_diagnostic_fields = true;
};

inline std::uint8_t sync_word(const Configuration& config) {
    return config.radio_mode == RadioMode::official ? config.radio.official_sync_word
                                                    : config.radio.test_sync_word;
}

// Effective battery divider ratio, honouring the deprecated alias.
inline float battery_divider_ratio(const Configuration& config) {
    if (config.battery_divider_ratio > 0.0f) return config.battery_divider_ratio;
    if (config.battery_adc_scale > 0.0f) return config.battery_adc_scale;
    return 0.0f;
}

// Cheap sanity check for a configuration before the mission loop starts. Returns false
// and fills `why` when the configuration cannot produce compliant telemetry.
bool validate_config(const Configuration& config, std::string& why);

}  // namespace flight
