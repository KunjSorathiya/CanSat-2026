#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "cansat/link_profile.hpp"
#include "cansat/lora_airtime.hpp"
#include "flight/sensor_timing.hpp"

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
//
// The spreading factor is NOT a free choice: a ~190-byte telemetry packet takes 943 ms of
// airtime at SF9/125 kHz, which cannot meet the 1 Hz rulebook minimum with any margin. SF7
// brings the same packet to 302 ms (~30 % channel occupancy at 1 Hz). `validate_config()`
// recomputes this from `cansat/lora_airtime.hpp` and refuses an impossible combination —
// see documentation/design/link-budget.md.
struct RadioConfig {
    std::uint32_t frequency_hz = cansat::link::kFrequencyHz;
    std::int8_t tx_power_dbm = cansat::link::kTxPowerDbm;
    std::uint8_t spreading_factor = cansat::link::kSpreadingFactor;  // 6..12
    std::uint32_t bandwidth_hz = cansat::link::kBandwidthHz;
    std::uint8_t coding_rate = cansat::link::kCodingRate;  // denominator 4/5..4/8 -> 5..8
    std::uint16_t preamble_length = cansat::link::kPreambleSymbols;
    bool enable_crc = cansat::link::kEnableCrc;
    std::uint8_t test_sync_word = cansat::link::kTestSyncWord;
    std::uint8_t official_sync_word = cansat::link::kOfficialSyncWord;
};

struct Configuration {
    // ---- Identity -------------------------------------------------------------
    // MUST be replaced with the registered team identifier before any launch or
    // official test. "CAN-Team-XX" is rejected by the telemetry formatter on purpose.
    std::string team_id = "CAN-Team-XX";

    // ---- Scheduling (milliseconds) -----------------------------------------
    // Radio and sensor rates are deliberately decoupled. Sensors and the state estimator
    // run fast enough for flight dynamics; the radio runs as fast as its airtime allows.
    //
    // 1000 ms = 1 Hz, the rulebook minimum, at ~32 % channel occupancy with the default
    // SF7/125 kHz modem. 2 Hz is only reachable with a wider bandwidth or a shorter
    // packet — validate_config() rejects the combinations that are not physically
    // achievable rather than letting the scheduler silently under-run.
    std::uint32_t telemetry_period_ms = cansat::link::kTelemetryPeriodMs;  // cap 1000
    // 33 ms = ~30 Hz acquisition, state estimation and altitude rate. Both sensors can
    // supply data faster than this: the barometer's configured preset runs at 83 Hz and
    // the IMU at 200 Hz. validate_config() refuses a period the barometer cannot keep up
    // with, because re-reading an unchanged conversion reads as zero climb rate.
    std::uint32_t sensor_period_ms = 33;       // sensor acquisition + orientation update
    // How long main() sleeps between poll() calls. Two independent limits bound it:
    //
    //  * it sets the scheduling jitter on every task above (2 ms is under 6 % of the 33 ms
    //    sensor tick), and
    //  * the GPS is drained once per tick from the RP2040's 32-byte UART FIFO. At 9600
    //    baud, 8N1, that FIFO fills in 33 ms — so a tick at or above that silently loses
    //    NMEA bytes and truncates sentences. validate_config() enforces the margin.
    std::uint32_t loop_tick_ms = 2;
    std::uint32_t sd_flush_period_ms = 2000;
    std::uint32_t health_period_ms = 1000;
    std::uint32_t battery_period_ms = 1000;

    // ---- Radio -------------------------------------------------------------
    RadioMode radio_mode = RadioMode::test;
    RadioConfig radio;
    std::uint32_t radio_recovery_backoff_ms = 1000;   // spacing between bounded re-init attempts
    std::uint8_t radio_max_consecutive_failures = 5;  // TX failures before a radio fault + re-init

    // Airtime budget and runtime packet cap. Defaults to the 255-byte LoRa FIFO limit,
    // the only size a packet cannot exceed (measured: 118 bytes mandatory-only, 206 with
    // GPS and diagnostics, 247 absolute worst case). The controller drops its optional
    // diagnostic tags rather than exceed this, because the radio would otherwise truncate
    // the packet silently. `max_channel_duty` is the largest fraction of the channel one
    // packet per telemetry period may occupy — the rest is margin for radio recovery,
    // retries and the receiver's own timing.
    std::size_t worst_case_packet_bytes = cansat::link::kWorstCasePacketBytes;
    double max_channel_duty = cansat::link::kMaxChannelDuty;

    // Legacy flat aliases kept for older call sites / tests.
    std::uint8_t test_sync_word = cansat::link::kTestSyncWord;
    std::uint8_t official_sync_word = cansat::link::kOfficialSyncWord;

    // ---- Sensor validity -------------------------------------------------------
    std::uint32_t sensor_stale_after_ms = 2000;

    // How long the vertical-speed estimate holds its value while the barometer reports an
    // unchanged pressure. Over-sampling produces at most a sample or two of repeats — the
    // barometer runs at 83 Hz against a 30 Hz loop — so a short hold rejects those false
    // zeros. Beyond it, an unchanged pressure means the vehicle really is not moving
    // vertically, and holding a stale descent rate would stop the landing detector from
    // ever firing.
    std::uint32_t altitude_rate_hold_ms = 200;

    // ---- Barometer configuration (BMP280 datasheet section 3.8, table 14) ------
    // osrs_t x1 + osrs_p x4 + IIR filter x16 is the datasheet's "handheld device,
    // dynamic" preset: 83 Hz output, low noise, and fast enough that 30 Hz sampling
    // always sees a fresh conversion. The previous x2/x16 setting produced only 26 Hz
    // and could not have supported the acquisition rate.
    sensors::Oversampling baro_osrs_t = sensors::Oversampling::x1;
    sensors::Oversampling baro_osrs_p = sensors::Oversampling::x4;
    sensors::BaroFilter baro_filter = sensors::BaroFilter::x16;
    double baro_standby_ms = 0.5;

    // ---- IMU configuration (MPU-6050 register map, registers 25 and 26) --------
    // DLPF_CFG 4 gives a 21 Hz accelerometer / 20 Hz gyroscope bandwidth. At a 30 Hz
    // sampling rate the Nyquist limit is 15 Hz, so a wider filter would alias airframe
    // vibration into the attitude estimate; a narrower one would blur the launch
    // transient. SMPLRT_DIV 4 leaves the internal rate at 200 Hz, well above sampling.
    std::uint8_t imu_dlpf_cfg = 4;
    std::uint8_t imu_sample_rate_div = 4;

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
    // A steady rotation has near-zero variance, so the std-dev gate alone accepts it and
    // absorbs a real body rate into the "bias". The MPU-6050 datasheet gives a zero-rate
    // output of +-20 deg/s over temperature, so a mean beyond this cannot be bias: the
    // vehicle is turning, and the calibration must be refused rather than silently
    // cancelling the rotation for the rest of the flight.
    double calib_max_gyro_bias_dps = 25.0;
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

    // ---- GPS ---------------------------------------------------------------
    std::uint32_t gps_baud = 9600;          // NEO-6M default
    std::uint32_t gps_uart_fifo_bytes = 32; // RP2040 UART FIFO depth

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

// Time to fill the RP2040's UART FIFO at the GPS baud rate, in milliseconds. The GPS is
// drained once per loop tick, so the tick must stay comfortably below this or NMEA bytes
// are lost before anything reads them.
inline constexpr double uart_fifo_fill_ms(std::uint32_t baud, std::uint32_t fifo_bytes = 32) {
    if (baud == 0) return 0.0;
    // 8N1 framing: 10 bits on the wire per byte.
    return 1000.0 * static_cast<double>(fifo_bytes) * 10.0 / static_cast<double>(baud);
}

// The configured modem, in the shared airtime model's terms.
inline cansat::LoraModemParams modem_params(const Configuration& config) {
    cansat::LoraModemParams p;
    p.spreading_factor = config.radio.spreading_factor;
    p.bandwidth_hz = config.radio.bandwidth_hz;
    p.coding_rate = config.radio.coding_rate;
    p.preamble_symbols = config.radio.preamble_length;
    p.explicit_header = true;
    p.crc_enabled = config.radio.enable_crc;
    return p;
}

// Airtime of the longest packet this configuration can transmit, in milliseconds.
inline double worst_case_airtime_ms(const Configuration& config) {
    return cansat::lora_time_on_air_ms(config.worst_case_packet_bytes, modem_params(config));
}

// Fraction of the channel the telemetry schedule occupies (1.0 = continuously talking).
inline double channel_duty(const Configuration& config) {
    return cansat::lora_channel_duty(config.worst_case_packet_bytes, modem_params(config),
                                     config.telemetry_period_ms);
}

// Cheap sanity check for a configuration before the mission loop starts. Returns false
// and fills `why` when the configuration cannot produce compliant telemetry.
bool validate_config(const Configuration& config, std::string& why);

}  // namespace flight
