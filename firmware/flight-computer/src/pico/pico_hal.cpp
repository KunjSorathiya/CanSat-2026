#include "flight/pico/pico_hal.hpp"

#include "flight/sensor_math.hpp"
#include "flight/sound_level.hpp"

#ifdef PICO_BUILD
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#endif

namespace flight {

#ifdef PICO_BUILD

namespace {
bool g_i2c_ready = false;
bool g_spi_ready = false;
bool g_adc_ready = false;

void ensure_i2c0() {
    if (g_i2c_ready) return;
    i2c_init(i2c0, 400 * 1000);
    gpio_set_function(BoardPins::i2c_sda, GPIO_FUNC_I2C);
    gpio_set_function(BoardPins::i2c_scl, GPIO_FUNC_I2C);
    gpio_pull_up(BoardPins::i2c_sda);
    gpio_pull_up(BoardPins::i2c_scl);
    g_i2c_ready = true;
}

void ensure_spi0() {
    if (g_spi_ready) return;
    spi_init(spi0, 400 * 1000);  // SD-safe; bumped by the SD driver after init
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(BoardPins::spi_sck, GPIO_FUNC_SPI);
    gpio_set_function(BoardPins::spi_mosi, GPIO_FUNC_SPI);
    gpio_set_function(BoardPins::spi_miso, GPIO_FUNC_SPI);

    // Both chip selects belong to the bus, not to the drivers that happen to use them.
    // Whichever device initialises first would otherwise run with the other device's CS
    // still in its reset state -- and an RP2040 pad resets with its pull-down enabled
    // (PADS_BANK0 reset value 0x56, PDE set), which holds the line LOW, which on both of
    // these parts means *selected*. The controller initialises the card before the radio,
    // so the radio sat selected and drove MISO through the whole of the card's
    // initialisation sequence, and every response the card sent came back corrupted.
    //
    // Deselecting both here makes the order the drivers run in stop mattering, which is
    // the property a shared bus needs.
    gpio_init(BoardPins::lora_cs);
    gpio_set_dir(BoardPins::lora_cs, GPIO_OUT);
    gpio_put(BoardPins::lora_cs, 1);
    gpio_init(BoardPins::sd_cs);
    gpio_set_dir(BoardPins::sd_cs, GPIO_OUT);
    gpio_put(BoardPins::sd_cs, 1);

    g_spi_ready = true;
}

void ensure_adc() {
    if (g_adc_ready) return;
    adc_init();
    adc_gpio_init(BoardPins::battery_adc);
    adc_gpio_init(BoardPins::sound_adc);  // harmless if only DO is wired
    g_adc_ready = true;
}
}  // namespace

void pico_buses_init() {
    ensure_i2c0();
    ensure_spi0();
    ensure_adc();
}

// ---- IMU ----
bool PicoImu::initialize() {
    ensure_i2c0();
    gpio_init(BoardPins::imu_int);
    gpio_set_dir(BoardPins::imu_int, GPIO_IN);
    // Bandwidth, internal rate and magnetometer mode all come from the flight
    // configuration: the filter settings must stay compatible with the acquisition rate
    // or airframe vibration aliases into the attitude estimate, and the magnetometer must
    // run fast enough to have a fresh sample for every attitude update (see
    // documentation/design/sensor-rates.md). validate_config() enforces both.
    pico::Mpu9250::Options options;
    options.gyro_dlpf = config_.imu_gyro_dlpf_cfg;
    options.accel_dlpf = config_.imu_accel_dlpf_cfg;
    options.sample_rate_div = config_.imu_sample_rate_div;
    options.mag_resolution = config_.mag_resolution;
    options.mag_mode = config_.mag_mode;
    const bool ok = device_.begin(i2c0, options);
    health_.initialized = ok;
    health_.healthy = ok;
    return ok;
}

bool PicoImu::read(ImuSample& out, std::uint64_t now_ms) {
    const bool ok = device_.read(out, now_ms);
    health_.healthy = ok;
    if (ok) health_.last_update_ms = now_ms;
    return ok;
}

// ---- Barometer ----
bool PicoBarometer::initialize() {
    ensure_i2c0();
    pico::Bmp280::Options options;
    options.reference_pressure_pa = config_.reference_pressure_pa;
    options.osrs_t = config_.baro_osrs_t;
    options.osrs_p = config_.baro_osrs_p;
    options.filter = config_.baro_filter;
    const bool ok = device_.begin(i2c0, options);
    health_.initialized = ok;
    health_.healthy = ok;
    return ok;
}

bool PicoBarometer::read(BaroSample& out, std::uint64_t now_ms) {
    const bool ok = device_.read(out, now_ms);
    health_.healthy = ok;
    if (ok) health_.last_update_ms = now_ms;
    return ok;
}

// ---- GPS ----
bool PicoGps::initialize() {
    // The baud rate is configuration, not a literal: validate_config() sizes the flight
    // loop's tick against the time this rate takes to fill the RP2040's 32-byte UART
    // FIFO, and that reasoning is only sound if the UART is actually opened at the rate
    // it was given.
    const bool ok = device_.begin(uart0, BoardPins::gps_tx, BoardPins::gps_rx,
                                  config_.gps_baud);
    health_.initialized = ok;
    // A UART opens whether or not a receiver is attached to it -- nothing has been heard
    // from the module yet, so nothing is claimed about it.
    health_.healthy = false;
    return ok;
}

void PicoGps::poll(std::uint64_t now_ms) {
    device_.poll(now_ms);
    // Report what the receiver actually did, not that we asked it to. A GPS whose lead
    // is off, whose regulator has browned out, or that never powered up leaves this
    // clock frozen; claiming health here would hide a dead module for a whole flight.
    health_.last_update_ms = device_.last_byte_ms();
    health_.healthy = device_.started() && device_.last_byte_ms() != 0 &&
                      (now_ms - device_.last_byte_ms()) <= config_.gps_silence_after_ms;
}

bool PicoGps::latest(cansat::GpsData& data) const { return device_.latest(data); }

// ---- Board I/O ----
void PicoBoardIo::set_status_led(bool on) {
    static bool configured = false;
    if (!configured) {
        gpio_init(BoardPins::status_led);
        gpio_set_dir(BoardPins::status_led, GPIO_OUT);
        configured = true;
    }
    gpio_put(BoardPins::status_led, on ? 1 : 0);
}

bool PicoSoundSensor::initialize() {
    ensure_adc();
    if (config_.sound_gate_connected) {
        gpio_init(BoardPins::sound_gate);
        gpio_set_dir(BoardPins::sound_gate, GPIO_IN);
        // Pulled down rather than left floating. An unconnected input would otherwise drift
        // and count as sound, which is the one failure that produces plausible-looking data
        // from a wire that is not there.
        gpio_pull_down(BoardPins::sound_gate);
    }
    health_.initialized = true;
    // There is nothing to interrogate. An analogue module has no identity register and no
    // handshake, so unlike the IMU or the radio this cannot report "the part is wrong" -- it
    // can only report what the pin reads. A disconnected input floats and produces a level
    // like any other, which is why the log keeps the raw extremes beside the span: a window
    // whose min and max are both pinned near a rail is a wire, not a sound.
    SoundSample probe{};
    const bool ok = read(probe, 0);
    health_.healthy = ok;
    return ok;
}

bool PicoSoundSensor::read(SoundSample& out, std::uint64_t now_ms) {
    ensure_adc();
    const std::uint32_t wanted = config_.sound_samples_per_window;
    if (wanted == 0) return false;
    if (!config_.sound_analog_connected && !config_.sound_gate_connected) return false;

    if (config_.sound_analog_connected) {
        adc_select_input(static_cast<std::uint32_t>(BoardPins::sound_adc - 26));
    }

    std::uint16_t lo = 0xFFFF;
    std::uint16_t hi = 0;
    std::uint32_t asserted = 0;
    // Both channels come out of ONE pass, interleaved, so the envelope and the threshold
    // duty describe the same half-millisecond of air rather than two neighbouring ones.
    // Comparing a peak from one window against a duty from another is the sort of thing
    // that produces a correlation nobody can reproduce.
    for (std::uint32_t i = 0; i < wanted; ++i) {
        if (config_.sound_analog_connected) {
            const std::uint16_t raw = adc_read();
            if (raw < lo) lo = raw;
            if (raw > hi) hi = raw;
        }
        if (config_.sound_gate_connected && gpio_get(BoardPins::sound_gate)) {
            ++asserted;
        }
    }

    out.samples = wanted;
    out.timestamp_ms = now_ms;

    if (config_.sound_analog_connected) {
        SoundWindow window;
        window.min_counts = lo;
        window.max_counts = hi;
        window.sample_count = wanted;
        out.min_counts = lo;
        out.max_counts = hi;
        out.level_mv_pp = sound_peak_to_peak_mv(window, config_.sound_reference_mv,
                                                config_.sound_full_scale_counts);
        out.clipped = sound_window_clipped(window, config_.sound_full_scale_counts);
        out.valid = true;
    }
    if (config_.sound_gate_connected) {
        out.gate_duty_pct = sound_gate_duty_pct(asserted, wanted);
        out.gate_valid = true;
    }

    health_.healthy = true;
    health_.last_update_ms = now_ms;
    return true;
}

float PicoBoardIo::battery_voltage() const {
    ensure_adc();
    // Derive the ADC channel from the pin rather than hard-coding 0: GP26/27/28 are ADC
    // 0/1/2, and a change to BoardPins::battery_adc must not leave this reading a
    // different pin than the one that was wired.
    adc_select_input(static_cast<std::uint32_t>(BoardPins::battery_adc - 26));
    const std::uint16_t raw = adc_read();
    const float counts = config_.battery_adc_max_counts == 0 ? 4095.0f : config_.battery_adc_max_counts;
    return static_cast<float>(raw) * config_.battery_adc_ref_v / counts;  // raw pin voltage
}

#else  // ------------------------- host stubs -------------------------

void pico_buses_init() {}

bool PicoImu::initialize() { return false; }
bool PicoImu::read(ImuSample&, std::uint64_t) { return false; }

bool PicoBarometer::initialize() { return false; }
bool PicoBarometer::read(BaroSample&, std::uint64_t) { return false; }

bool PicoGps::initialize() { return false; }
void PicoGps::poll(std::uint64_t) { health_.healthy = false; }
bool PicoGps::latest(cansat::GpsData&) const { return false; }

void PicoBoardIo::set_status_led(bool) {}
float PicoBoardIo::battery_voltage() const { return 0.0f; }
bool PicoSoundSensor::initialize() { return false; }
bool PicoSoundSensor::read(SoundSample&, std::uint64_t) { return false; }

#endif

}  // namespace flight
