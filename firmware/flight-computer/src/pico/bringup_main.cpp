// Bring-up diagnostic firmware. NOT flight software, and deliberately a separate image.
//
// The flight firmware writes nothing to USB - it speaks only over LoRa - so a vehicle with
// no radio attached produces no observable at all. That makes Gate 3 of
// documentation/testing/bring-up-record.md impossible to take: its first row asks for an
// I2C bus scan, and there was no tool to run one and no path for the answer to reach a
// human.
//
// This image fills that gap. It drives the *real* drivers - the same mpu9250.cpp and
// bmp280.cpp the vehicle flies - and prints what they find over USB CDC. It is not a
// parallel reimplementation: a diagnostic that exercises different code from the flight
// build can pass while the flight build fails, which is worse than having no diagnostic.
//
// Kept out of the flight image on purpose. The launch build has no debug output in it,
// and no flag that could accidentally enable some.

#include "flight/config.hpp"
#include "flight/interfaces.hpp"
#include "flight/pico/pico_hal.hpp"

#include "cansat/link_profile.hpp"
#include "cansat/lora_airtime.hpp"

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace {

std::uint64_t now_ms() { return to_ms_since_boot(get_absolute_time()); }

// A USB CDC port does not exist until the host opens it, and anything printed before then
// is lost. Wait, but never forever: a board left running headless must still reach the
// loop rather than hang here looking identical to a dead one.
void wait_for_host(std::uint32_t timeout_ms) {
    const std::uint64_t deadline = now_ms() + timeout_ms;
    while (!stdio_usb_connected() && now_ms() < deadline) {
        sleep_ms(100);
    }
    sleep_ms(250);  // let the host's terminal attach before the banner goes out
}

const char* i2c_address_name(std::uint8_t addr) {
    switch (addr) {
        // The address labels say what answered, not what the board was sold as. This
        // scan runs before WHO_AM_I is read, so it cannot know which part it is looking
        // at -- and naming a specific one here would contradict the identity report a
        // few lines later on the part this project actually received.
        case 0x0C: return "AK8963 magnetometer, present only on a nine-axis package";
        case 0x68: return "IMU (MPU-9250 family), AD0 low";
        case 0x69: return "IMU (MPU-9250 family), AD0 high";
        case 0x76: return "BMP280, SDO low";
        case 0x77: return "BMP280, SDO high";
        default:   return "unexpected - not a device this vehicle knows about";
    }
}

// Returns the number of devices that answered.
int scan_i2c(const char* when) {
    std::printf("\n-- I2C0 bus scan (%s) --\n", when);
    int found = 0;
    for (std::uint8_t addr = 0x08; addr < 0x78; ++addr) {
        std::uint8_t rx = 0;
        // A device that ACKs its address answers a 1-byte read. Nothing is written, so a
        // scan cannot disturb a device that happens to be mid-conversion.
        if (i2c_read_blocking(i2c0, addr, &rx, 1, false) >= 0) {
            std::printf("   0x%02X  %s\n", addr, i2c_address_name(addr));
            ++found;
        }
    }
    if (found == 0) {
        std::printf("   nothing answered.\n"
                    "   Check 3V3 and GND first, then SDA on GP4 and SCL on GP5. Every\n"
                    "   device on this bus is 3.3 V only - there is no level shifting\n"
                    "   anywhere on this vehicle.\n");
    }
    std::printf("   %d device(s) answered.\n", found);
    return found;
}

bool read_reg8(std::uint8_t addr, std::uint8_t reg, std::uint8_t& out) {
    if (i2c_write_blocking(i2c0, addr, &reg, 1, true) < 0) return false;
    return i2c_read_blocking(i2c0, addr, &out, 1, false) >= 0;
}

// The BMP280 and BME280 are pin- and protocol-compatible and share breakout artwork; the
// only reliable difference from outside is this register. Receiving inspection C.4.1
// identified the delivered part by measuring its package, which is evidence but not proof.
// This is the proof.
// Returns the address the barometer answered at, or 0 if none did.
std::uint8_t report_baro_identity() {
    std::printf("\n-- Barometer identity (register 0xD0) --\n");
    int answered = 0;
    std::uint8_t found = 0;
    for (const std::uint8_t addr : {0x76, 0x77}) {
        std::uint8_t id = 0;
        if (!read_reg8(addr, 0xD0, id)) continue;
        ++answered;
        found = addr;
        const char* part = id == 0x58   ? "BMP280 - matches the BOM and the telemetry format"
                           : id == 0x60 ? "BME280 - HAS HUMIDITY, which the packet format has no field for"
                                        : "unrecognised - do not proceed on this part";
        std::printf("   0x%02X answered chip-ID 0x%02X: %s\n", addr, id, part);
        if (addr == 0x77) {
            std::printf("   NOTE: found at 0x77, not the 0x76 the firmware defaults to.\n"
                        "   SDO is strapped high. Change Bmp280::Options::address in\n"
                        "   config, or re-strap the board - deliberately, and record which.\n");
        }
    }
    // A silent block reads as a broken tool. Say that nothing was there.
    if (answered == 0) {
        std::printf("   no device answered at 0x76 or 0x77.\n"
                    "   Expected if the BMP280 is not wired - this image is happy to run\n"
                    "   with one sensor at a time.\n");
    }
    return found;
}

void report_imu_identity(const flight::PicoImu& imu) {
    const std::uint8_t who = imu.who_am_i();
    std::printf("\n-- IMU identity (WHO_AM_I) --\n");
    std::printf("   WHO_AM_I = 0x%02X  ", who);
    switch (who) {
        case 0x71: std::printf("MPU-9250. Nine axes, magnetometer present.\n"); break;
        case 0x73: std::printf("MPU-9255. Nine axes, magnetometer present.\n"); break;
        case 0x70:
            std::printf("MPU-6500 sold as an MPU-9250.\n"
                        "   SIX axes - there is no magnetometer in this package at all.\n"
                        "   The vehicle flies on gyro-integrated yaw, which drifts.\n");
            break;
        default:
            std::printf("unrecognised. The part did not identify itself.\n");
            break;
    }
    std::printf("   Magnetometer answering: %s\n", imu.has_magnetometer() ? "yes" : "NO");
}

struct Stats {
    double mean = 0.0;
    double sd = 0.0;
};

Stats finish(double sum, double sum_sq, int n) {
    Stats s;
    if (n <= 0) return s;
    s.mean = sum / n;
    const double var = (sum_sq / n) - (s.mean * s.mean);
    s.sd = var > 0.0 ? std::sqrt(var) : 0.0;
    return s;
}

// Fills bring-up rows 3.2, 3.3 and 3.4 with numbers rather than an impression. Keep the
// board still: this measures what the sensor does when nothing is happening to it.
void stationary_statistics(flight::PicoImu& imu, const flight::Configuration& config) {
    constexpr int kSamples = 100;
    std::printf("\n-- Stationary statistics, %d samples --\n", kSamples);
    std::printf("   Hold the board still and level. Starting in 2 s...\n");
    sleep_ms(2000);

    double a_sum = 0.0, a_sq = 0.0;
    double g_sum[3] = {0, 0, 0}, g_sq[3] = {0, 0, 0};
    double m_sum = 0.0;
    int n = 0, mag_n = 0;

    for (int i = 0; i < kSamples; ++i) {
        flight::ImuSample s;
        if (imu.read(s, now_ms()) && s.valid) {
            const double a = std::sqrt(s.ax_mps2 * s.ax_mps2 + s.ay_mps2 * s.ay_mps2 +
                                       s.az_mps2 * s.az_mps2);
            a_sum += a;
            a_sq += a * a;
            const double g[3] = {s.gx_dps, s.gy_dps, s.gz_dps};
            for (int k = 0; k < 3; ++k) {
                g_sum[k] += g[k];
                g_sq[k] += g[k] * g[k];
            }
            if (s.mag_valid) {
                m_sum += std::sqrt(s.mx_ut * s.mx_ut + s.my_ut * s.my_ut + s.mz_ut * s.mz_ut);
                ++mag_n;
            }
            ++n;
        }
        sleep_ms(33);  // ~30 Hz, the flight acquisition rate
    }

    if (n == 0) {
        std::printf("   no valid samples. The IMU is not returning data.\n");
        return;
    }

    const Stats a = finish(a_sum, a_sq, n);
    std::printf("   3.2  |a| mean       = %8.4f m/s^2   (expect 9.81 +/- %.2f)  %s\n",
                a.mean, config.calib_accel_tol_mps2,
                std::fabs(a.mean - 9.81) <= config.calib_accel_tol_mps2 ? "PASS" : "OUT OF RANGE");
    std::printf("        |a| sd         = %8.4f m/s^2\n", a.sd);

    const char* axis[3] = {"X", "Y", "Z"};
    for (int k = 0; k < 3; ++k) {
        const Stats g = finish(g_sum[k], g_sq[k], n);
        std::printf("   3.3  gyro %s bias   = %8.4f dps      (limit +/- %.1f)        %s\n",
                    axis[k], g.mean, config.calib_max_gyro_bias_dps,
                    std::fabs(g.mean) <= config.calib_max_gyro_bias_dps ? "PASS" : "OUT OF RANGE");
        std::printf("   3.4  gyro %s noise  = %8.4f dps sd   (limit %.1f)            %s\n",
                    axis[k], g.sd, config.calib_gyro_still_dps,
                    g.sd <= config.calib_gyro_still_dps ? "PASS" : "OUT OF RANGE");
    }

    if (mag_n > 0) {
        const double mean = m_sum / mag_n;
        std::printf("   8.10 |B| mean       = %8.2f uT       (Earth's field is 25-65)  %s\n",
                    mean, (mean >= 25.0 && mean <= 65.0) ? "PASS" : "OUT OF RANGE");
        std::printf("        magnetometer samples: %d of %d\n", mag_n, n);
        std::printf("   A reading far outside 25-65 uT usually means iron or a magnet\n"
                    "   near the board, not a broken part. Move it and repeat.\n");
    } else {
        std::printf("   no magnetometer samples - see WHO_AM_I above.\n");
    }
    std::printf("   valid samples: %d of %d\n", n, kSamples);
}

// ---- Gate 3 rate rows -------------------------------------------------------
//
// 3.5 is a property of the sensor: how often the BMP280 actually publishes a new
// conversion at the configured oversampling and filter. Counting *changed* values is the
// only honest way to measure it from the outside - polling faster than the part converts
// returns the same bytes again, and counting reads instead of changes would report the
// poll rate and call it the output rate.
void barometer_output_rate(flight::PicoBarometer& baro, std::uint8_t baro_addr) {
    std::printf("\n-- 3.5 Barometer output rate --\n");
    constexpr std::uint32_t kWindowMs = 2000;
    const double secs = static_cast<double>(kWindowMs) / 1000.0;

    // Method A: count changed compensated values. Simple, and it undercounts - see below.
    {
        const std::uint64_t start = now_ms();
        double last_pa = -1.0;
        int changes = 0, reads = 0;
        while (now_ms() - start < kWindowMs) {
            flight::BaroSample b;
            if (baro.read(b, now_ms()) && b.valid) {
                ++reads;
                if (b.pressure_pa != last_pa) {
                    ++changes;
                    last_pa = b.pressure_pa;
                }
            }
        }
        std::printf("   A: polled %d times in %.1f s (%.0f Hz poll rate)\n", reads, secs,
                    reads / secs);
        std::printf("   A: distinct values %d -> %.1f Hz  **LOWER BOUND ONLY**\n", changes,
                    changes / secs);
    }

    // Method B: count falling edges of STATUS.measuring (register 0xF3, bit 3). Each
    // 1 -> 0 transition is one completed conversion, whether or not the result differs
    // from the last one. This is the actual output rate.
    //
    // Method A cannot see that. With the IIR filter at x16 the part deliberately changes
    // its output slowly, so consecutive conversions frequently produce the *same*
    // compensated value - and counting distinct values then reports how often the reading
    // moves, not how often the sensor converts. The two are different questions and only
    // B answers the one 3.5 asks.
    {
        constexpr std::uint8_t kStatusReg = 0xF3;
        constexpr std::uint8_t kMeasuringBit = 0x08;
        const std::uint64_t start = now_ms();
        int completions = 0, polls = 0;
        bool was_measuring = false;
        bool ok = true;
        while (now_ms() - start < kWindowMs) {
            std::uint8_t status = 0;
            if (!read_reg8(baro_addr, kStatusReg, status)) {
                ok = false;
                break;
            }
            ++polls;
            const bool measuring = (status & kMeasuringBit) != 0;
            if (was_measuring && !measuring) ++completions;
            was_measuring = measuring;
        }
        if (!ok) {
            std::printf("   B: STATUS register unreadable - falling back on A\n");
            return;
        }
        std::printf("   B: STATUS.measuring falling edges %d -> **%.1f Hz**  (predicted 83 Hz)\n",
                    completions, completions / secs);
        std::printf("      status polled %d times (%.0f Hz) - must be well above the\n"
                    "      output rate or completions are missed between polls\n",
                    polls, polls / secs);
    }
    std::printf("   What matters for the design is the margin over the 30 Hz acquisition\n"
                "   rate in sensor-rates.md, not the agreement with 83 Hz.\n");
}

// 3.7 and 3.8 measure a paced loop, not the flight controller's scheduler. What this
// bounds is real: if the sensors cannot be read inside the period, the flight loop cannot
// hold it either. Reported as the diagnostic's own loop so nobody mistakes it for
// controller.cpp's.
void acquisition_rate(flight::PicoImu& imu, flight::PicoBarometer& baro,
                      const flight::Configuration& config, bool imu_ok, bool baro_ok) {
    std::printf("\n-- 3.7 / 3.8 Acquisition rate and jitter --\n");
    constexpr int kTicks = 150;
    const std::uint32_t period = config.sensor_period_ms;

    std::uint64_t prev = now_ms();
    double sum = 0.0, sum_sq = 0.0;
    std::uint64_t worst_read_us = 0, read_us_sum = 0;
    int n = 0;

    for (int i = 0; i < kTicks; ++i) {
        sleep_ms(period);
        const std::uint64_t t = now_ms();

        const std::uint64_t r0 = to_us_since_boot(get_absolute_time());
        flight::ImuSample s;
        flight::BaroSample b;
        if (imu_ok) imu.read(s, t);
        if (baro_ok) baro.read(b, t);
        const std::uint64_t read_us = to_us_since_boot(get_absolute_time()) - r0;
        read_us_sum += read_us;
        if (read_us > worst_read_us) worst_read_us = read_us;

        if (i > 0) {  // the first interval includes start-up, so drop it
            const double dt = static_cast<double>(t - prev);
            sum += dt;
            sum_sq += dt * dt;
            ++n;
        }
        prev = t;
    }

    const Stats s = finish(sum, sum_sq, n);
    const double tolerance = 0.06 * static_cast<double>(period);
    std::printf("   3.7  mean interval  = %7.3f ms   (configured %lu ms -> %.2f Hz)\n",
                s.mean, static_cast<unsigned long>(period),
                s.mean > 0.0 ? 1000.0 / s.mean : 0.0);
    std::printf("   3.8  interval sd    = %7.3f ms   (limit %.2f ms, 6%% of the period)  %s\n",
                s.sd, tolerance, s.sd <= tolerance ? "PASS" : "OUT OF RANGE");
    std::printf("        sensor read     = %7.3f ms mean, %.3f ms worst\n",
                (read_us_sum / static_cast<double>(kTicks)) / 1000.0,
                worst_read_us / 1000.0);
    std::printf("   The read time is the number that matters: it is the part of the\n"
                "   period the flight loop cannot spend on anything else. Worst case\n"
                "   must stay well inside %lu ms.\n", static_cast<unsigned long>(period));
    std::printf("   Measured on this diagnostic's loop, NOT on controller.cpp's\n"
                "   scheduler. It bounds the flight loop rather than describing it.\n");
}

// ---- Gate 4 ------------------------------------------------------------------
//
// 4.1 asks whether raw NMEA arrives at all. That is a question about bytes and baud rate,
// so it is answered by echoing the UART verbatim rather than by anything the parser says:
// a wrong baud rate produces a steady stream of plausible-looking garbage, and only
// looking at the characters distinguishes that from silence or from real sentences.
void gps_raw_echo(const flight::Configuration& config, std::uint32_t seconds) {
    std::printf("\n-- 4.1 Raw NMEA, %lu s of whatever the UART carries --\n",
                static_cast<unsigned long>(seconds));
    std::printf("   Expect lines like $GPRMC / $GPGGA. Readable text means the baud rate\n"
                "   is right. Mojibake means it is wrong. Nothing at all means the module\n"
                "   is not talking - check its supply before its wiring.\n");
    std::printf("   ----------------------------------------------------------\n");

    uart_init(uart0, config.gps_baud);
    gpio_set_function(flight::BoardPins::gps_tx, GPIO_FUNC_UART);
    gpio_set_function(flight::BoardPins::gps_rx, GPIO_FUNC_UART);

    const std::uint64_t deadline = now_ms() + seconds * 1000;
    int bytes = 0;
    while (now_ms() < deadline) {
        if (uart_is_readable(uart0)) {
            const char c = uart_getc(uart0);
            std::putchar(c);
            ++bytes;
        }
    }
    std::printf("\n   ----------------------------------------------------------\n");
    std::printf("   %d bytes in %lu s (%.0f bytes/s; 9600 baud carries ~960)\n", bytes,
                static_cast<unsigned long>(seconds),
                bytes / static_cast<double>(seconds));
    if (bytes == 0) {
        std::printf("   SILENCE. The module sent nothing. C.5.5 is the first suspect:\n"
                    "   this board prints no supply range and its regulator is\n"
                    "   unidentified, so 3.3 V may be leaving the NEO-6M below its\n"
                    "   2.7 V floor. Check TX/RX are crossed before assuming a dead part.\n");
    }
}

// ---- Gate 5 ------------------------------------------------------------------
//
// Timing a transmit measures airtime plus the driver's overhead: FIFO fill, mode changes
// and the DIO0 round trip. The predicted figure comes from lora_time_on_air_ms() rather
// than a literal, so this comparison cannot drift away from the model the link budget and
// the build-time static_assert both use.

// A counted snapshot of the radio's failure counters, so a test reports what happened
// during it rather than everything since boot. The counters are cumulative, and printing
// them raw made 5.3 announce "8 timeout(s)" when three of its five failed and the other
// five belonged to 5.2 - a test looking twice as bad as it was.
struct TxFailures {
    std::uint32_t timeouts = 0;
    std::uint32_t impossibly_fast = 0;
};

TxFailures tx_counters(const flight::PicoRadio& radio) {
    TxFailures c;
    c.timeouts = radio.tx_timeouts();
    c.impossibly_fast = radio.tx_impossibly_fast();
    return c;
}

TxFailures tx_failures_since(const flight::PicoRadio& radio, const TxFailures& before) {
    TxFailures d;
    d.timeouts = radio.tx_timeouts() - before.timeouts;
    d.impossibly_fast = radio.tx_impossibly_fast() - before.impossibly_fast;
    return d;
}

// A transmit that fails looks identical whatever caused it. Three registers, read at the
// moment of the failure and before anything is cleared, separate the candidates - and they
// want completely different investigations, so guessing between them wastes an evening.
void describe_tx_failure(const flight::PicoRadio& radio, const TxFailures& here) {
    const std::uint8_t irq = radio.last_tx_irq_flags();
    const std::uint8_t op = radio.last_tx_op_mode();
    const std::uint8_t ver = radio.last_tx_version();
    std::printf("       in this test: %lu timeout(s), %lu impossibly fast\n",
                static_cast<unsigned long>(here.timeouts),
                static_cast<unsigned long>(here.impossibly_fast));
    std::printf("       at the last failure: IRQ_FLAGS = 0x%02X, OP_MODE = 0x%02X, "
                "VERSION = 0x%02X\n", irq, op, ver);

    // Order matters. Each check rules out everything below it, so the first one that fires
    // is the one to act on - and the top two say the RF side is not implicated at all.
    if (ver != 0x12) {
        std::printf("       VERSION is not 0x12: SPI to the radio was broken at that\n"
                    "       instant, so nothing about the transmit is implicated. Look at\n"
                    "       CS on GP%d, SCK, MOSI and MISO - and at the microSD, which\n"
                    "       shares this bus and corrupts the radio rather than itself.\n",
                    flight::BoardPins::lora_cs);
        return;
    }
    if ((op & 0x80) == 0) {
        std::printf("       OP_MODE bit 7 is CLEAR: the modem is no longer in LoRa mode.\n"
                    "       It only leaves LoRa on a reset, so the module lost power or was\n"
                    "       reset mid-transmit and took its configuration with it. Suspect\n"
                    "       its 3V3 and GND, and the RESET line on GP%d, before anything\n"
                    "       about the RF side.\n",
                    flight::BoardPins::lora_reset);
        return;
    }
    if (here.impossibly_fast != 0) {
        std::printf("       Transmits reported done faster than their own airtime.\n"
                    "       DIO0 is floating HIGH - reading done the instant it is\n"
                    "       polled. Same loose wire on GP%d as a timeout, opposite\n"
                    "       symptom. Refused rather than counted as sent.\n",
                    flight::BoardPins::lora_dio0);
        return;
    }
    if (irq & 0x08) {
        std::printf("       TxDone IS set: the radio finished and DIO0 never said so.\n"
                    "       That is the wire on GP%d, not the radio.\n",
                    flight::BoardPins::lora_dio0);
        return;
    }
    std::printf("       TxDone is CLEAR, and the chip is still the one we configured and\n"
                "       still in LoRa mode (0x83 is LoRa TX). It accepted the transmit and\n"
                "       never finished it: the PLL, the PA, or the rail behind them.\n"
                "       Watch 3V3 on DC volts through a burst - the PA pulls about 87 mA\n"
                "       at +17 dBm - and check the antenna is actually on the SMA.\n");
}

void radio_airtime(flight::PicoRadio& radio, std::size_t bytes, const char* label) {
    constexpr int kBursts = 5;
    const std::string payload(bytes, 'A');
    const double predicted = cansat::lora_time_on_air_ms(bytes, cansat::link::kModem);

    const TxFailures before = tx_counters(radio);
    double sum = 0.0;
    int sent = 0;
    for (int i = 0; i < kBursts; ++i) {
        const std::uint64_t t0 = to_us_since_boot(get_absolute_time());
        const bool ok = radio.transmit(payload);
        const std::uint64_t t1 = to_us_since_boot(get_absolute_time());
        if (ok) {
            sum += static_cast<double>(t1 - t0) / 1000.0;
            ++sent;
        }
        sleep_ms(200);
    }
    if (sent == 0) {
        std::printf("   %s: every transmit FAILED after %lu ms\n", label,
                    static_cast<unsigned long>(2000));
        describe_tx_failure(radio, tx_failures_since(radio, before));
        return;
    }
    const double mean = sum / sent;
    std::printf("   %s: %.1f ms measured, %.1f ms predicted (%+.1f ms), %d/%d sent\n", label,
                mean, predicted, mean - predicted, sent, kBursts);
    if (sent < kBursts) describe_tx_failure(radio, tx_failures_since(radio, before));
}

// A long enough burst to watch a meter, and enough samples to see a pattern.
//
// The five-packet airtime test is over in under three seconds, which is no use with a
// handheld meter and gives five samples to judge an intermittent fault from. This transmits
// back to back for long enough to do both, and prints one character per attempt so the
// SHAPE of the failure is visible rather than just its rate:
//
//   ..............................  healthy
//   .....xxxxxxxxxxxxxxxxxxxxxxxxx  works then stops - heat, or a supply sagging as a
//                                   bulk capacitor somewhere gives up
//   .x.x..x.x.x..x.x.x.x.x.x.x.x.x  random - a marginal connection or a marginal rail
//
// Those three want different investigations, and a success count alone cannot tell them
// apart. Back-to-back transmission is also the worst case the supply will ever see: far
// harsher than the 1 Hz the mission actually sends, which is the point.
void radio_sustained(flight::PicoRadio& radio, std::size_t bytes, std::uint32_t window_ms) {
    std::printf("\n-- 5.4 Sustained transmit, %lu s, for a rail measurement --\n",
                static_cast<unsigned long>(window_ms / 1000));
    std::printf("   Put the meter on DC volts across pin 36 (3V3) and pin 38 (GND) and\n"
                "   watch it through the burst. The PA pulls about 87 mA at +17 dBm, so\n"
                "   a rail that sags below ~3.1 V here is the answer to Gate 2.\n");
    std::printf("   Back-to-back transmission - far harsher than the 1 Hz the mission\n"
                "   sends. Starting in 3 s.\n");
    sleep_ms(3000);

    const std::string payload(bytes, 'A');
    const TxFailures before = tx_counters(radio);
    const std::uint64_t start = now_ms();
    int attempts = 0, ok = 0;
    int first_failure_at = -1;
    std::printf("   ");
    while (now_ms() - start < window_ms) {
        const bool sent = radio.transmit(payload);
        ++attempts;
        if (sent) {
            ++ok;
            std::putchar('.');
        } else {
            if (first_failure_at < 0) first_failure_at = attempts;
            std::putchar('x');
        }
        // Keep the line readable rather than letting it wrap wherever it lands.
        if (attempts % 50 == 0) std::printf("\n   ");
    }
    std::printf("\n");

    const double secs = static_cast<double>(now_ms() - start) / 1000.0;
    std::printf("   %d attempts in %.1f s: %d sent, %d failed (%.0f %% success)\n",
                attempts, secs, ok, attempts - ok,
                attempts ? (100.0 * ok / attempts) : 0.0);
    if (ok > 0) {
        std::printf("   %.1f packets/s sustained at %u bytes\n", ok / secs,
                    static_cast<unsigned>(bytes));
    }
    if (attempts != ok) {
        if (first_failure_at > 1) {
            std::printf("   First failure at attempt %d, so it worked before it did not.\n"
                        "   That shape is heat or a supply falling away, not a bad wire -\n"
                        "   a bad wire fails from the first attempt.\n", first_failure_at);
        }
        describe_tx_failure(radio, tx_failures_since(radio, before));
    }
}

void report_radio(const flight::Configuration& config) {
    std::printf("\n-- 5.1 Radio identity --\n");
    flight::PicoRadio radio(config);
    const bool ok = radio.initialize(cansat::link::kTestSyncWord);
    std::printf("   init: %s, sync word 0x%02X (test - the launch word is 0x%02X)\n",
                ok ? "ok" : "FAILED", cansat::link::kTestSyncWord,
                cansat::link::kOfficialSyncWord);
    const std::uint8_t v = radio.chip_version();
    std::printf("   version register 0x42 = 0x%02X  ", v);
    if (v == 0x12) {
        std::printf("SX1276/77/78 family - correct\n");
    } else if (v == 0x00 || v == 0xFF) {
        std::printf("**the SPI transaction failed, not the modem.**\n"
                    "   0x00 and 0xFF are what an unresponsive bus reads as. Check NSS on\n"
                    "   GP%d, SCK GP%d, MOSI GP%d, MISO GP%d, and that the module has 3.3 V.\n",
                    flight::BoardPins::lora_cs, flight::BoardPins::spi_sck,
                    flight::BoardPins::spi_mosi, flight::BoardPins::spi_miso);
    } else {
        std::printf("unexpected - the modem answered, but not as an SX127x\n");
    }
    if (!ok || v != 0x12) return;

    // Transmitting into an unterminated port reflects the whole output back into the power
    // amplifier. This is the one action in this diagnostic that can damage hardware, so it
    // is not run without someone saying so.
    std::printf("\n-- 5.2 / 5.3 Airtime --\n");
    std::printf("   ***  DO NOT RUN THIS WITHOUT THE ANTENNA CONNECTED.  ***\n");
    std::printf("   Transmitting into an open port reflects the output back into the PA\n");
    std::printf("   and can destroy it. The chain is antenna -> SMA -> pigtail -> u.FL.\n");
    std::printf("\n   Antenna fitted? Press 't' within 20 s to transmit. Anything else skips.\n");

    const std::uint64_t deadline = now_ms() + 20000;
    int key = -1;
    while (now_ms() < deadline) {
        key = getchar_timeout_us(0);
        if (key != PICO_ERROR_TIMEOUT) break;
        sleep_ms(50);
    }
    if (key != 't' && key != 'T') {
        std::printf("   skipped. 5.2 and 5.3 stay open - re-run with the antenna fitted.\n");
        return;
    }

    std::printf("   transmitting...\n");
    radio_airtime(radio, 206, "5.2  206-byte packet");
    radio_airtime(radio, 255, "5.3  255-byte packet");
    radio_sustained(radio, 206, 15000);
    std::printf("   Measured time includes FIFO fill, mode changes and the DIO0 round\n"
                "   trip, so it should sit slightly above the predicted airtime. Well\n"
                "   above means the driver is waiting on something it should not be.\n");
}

// ---- Gate 6 ------------------------------------------------------------------
//
// The highest-risk item in the BOM, per documentation/hardware/sd-module-analysis.md. The
// delivered board has no regulator, no level shifter and nothing buffering MISO, so what
// this section proves about the card is also what Gate 7 depends on.
//
// Initialisation and the card-type read are non-destructive. The write test is not, and it
// is behind its own prompt for a different reason from the radio's: it cannot damage
// hardware, but it destroys the filesystem.
// A failed write says one thing from outside and means five. This says which, and decodes
// the CMD13 status byte, because that is the only place a card admits to being write
// protected -- and a card that has gone read-only looks exactly like bad wiring until you
// ask it.
// 0xFF is not a status byte. It is the absence of one: MISO idling high with nothing
// driving it. Every valid R1 has bit 7 clear, and every R2 arrives behind one.
//
// This check exists because the decode below did not have it, and read 0xFF as five
// simultaneous catastrophes - write protection, a lock, a controller fault, an ECC failure
// and an out-of-range address. Five unrelated faults at once is not a card in trouble, it
// is a card that never answered, and the report sent an evening after a healthy card. The
// init path had this guard from the start; the write path did not inherit it.
bool report_no_answer(const char* what, std::uint8_t value) {
    if (value != 0xFF) return false;
    std::printf("       %s = 0xFF: NO RESPONSE, not a status byte - a valid response\n"
                "       always has bit 7 clear. The card drove nothing at all here, so\n"
                "       there is nothing to decode: it was not refusing the write, it\n"
                "       had stopped answering.\n", what);
    return true;
}

void describe_write_failure(const flight::pico::SdCard& card) {
    using WriteStage = flight::pico::SdCard::WriteStage;
    const WriteStage stage = card.write_stage();
    std::printf("       stopped at: %s\n", flight::pico::SdCard::describe(stage));

    // Only report what the write actually reached. A stage that stopped before CMD13 has no
    // R2 to show, and printing the field anyway presents a leftover byte as a measurement.
    if (stage == WriteStage::busy_before) {
        std::printf("       No command was sent, so there is no R1 to report. The card was\n"
                    "       still busy - or not driving MISO high - before the write began.\n");
        return;
    }

    std::printf("       CMD R1 = 0x%02X\n", card.last_write_r1());
    if (report_no_answer("R1", card.last_write_r1())) return;

    if (stage != WriteStage::cmd24_rejected) {
        const std::uint8_t tok = card.last_data_response();
        std::printf("       data token = 0x%02X\n", tok);
        if ((tok & 0x1F) == 0x0B) {
            std::printf("       Data token 0x0B: CRC error on the data block - the bus.\n");
        } else if ((tok & 0x1F) == 0x0D) {
            std::printf("       Data token 0x0D: write error - the card, not the bus.\n");
        }
    }

    if (stage != WriteStage::status_error) return;

    const std::uint8_t r2 = card.last_write_r2();
    std::printf("       CMD13 R2 = 0x%02X\n", r2);
    if (report_no_answer("R2", r2)) return;
    if (r2 & 0x20) {
        std::printf("       R2 bit 5: WRITE PROTECT VIOLATION. The card is refusing to be\n"
                    "       written, and that is the card's decision, not the wiring.\n");
    }
    if (r2 & 0x01) std::printf("       R2 bit 0: the card reports itself LOCKED.\n");
    if (r2 & 0x08) std::printf("       R2 bit 3: CC error - internal controller fault.\n");
    if (r2 & 0x10) std::printf("       R2 bit 4: ECC failed - the card could not correct it.\n");
    if (r2 & 0x80) std::printf("       R2 bit 7: out of range, or CSD overwrite.\n");
    if (r2 == 0x00) {
        std::printf("       R2 clear but R1 was not: re-read the R1 value above.\n");
    }
}

bool sd_probe_read(void* ctx, std::uint32_t lba, std::uint8_t* out512) {
    return static_cast<flight::pico::SdCard*>(ctx)->read_block(lba, out512);
}

void report_sd() {
    std::printf("\n-- 6.1 / 6.2 microSD --\n");
    flight::pico::SdCard card;
    const bool ok = card.begin(spi0, flight::BoardPins::sd_cs);
    std::printf("   init (CMD0/CMD8/ACMD41/CMD58/CMD16): %s\n", ok ? "ok" : "FAILED");
    if (!ok) {
        std::printf("   stopped at: %s\n",
                    flight::pico::SdCard::describe(card.stage()));
        std::printf("   last R1 = 0x%02X", card.last_r1());
        if (card.last_r1() == 0xFF) {
            std::printf("  (0xFF means the card never drove MISO at all)");
        } else if ((card.last_r1() & 0x04) != 0) {
            std::printf("  (illegal command bit set)");
        }
        std::printf("\n");
        if (card.stage() == flight::pico::SdCard::Stage::acmd41_ready) {
            std::printf("   waited %lu ms in ACMD41; a healthy card leaves idle in tens\n",
                        static_cast<unsigned long>(card.init_wait_ms()));
        }
        std::printf("   Check CS on GP%d and that a card is actually seated - the holder\n"
                    "   is friction-fit, so a card can sit in it without making contact.\n"
                    "   MISO and MOSI swapped is the other classic: on this module MISO is\n"
                    "   the module OUTPUT and belongs on GP%d, MOSI its input on GP%d.\n",
                    flight::BoardPins::sd_cs, flight::BoardPins::spi_miso,
                    flight::BoardPins::spi_mosi);
        return;
    }
    std::printf("   reached: %s, %lu ms in ACMD41, %d CMD0 attempt(s)\n",
                flight::pico::SdCard::describe(card.stage()),
                static_cast<unsigned long>(card.init_wait_ms()), card.cmd0_attempts());
    std::printf("   6.2 card type: %s\n",
                card.high_capacity() ? "SDHC/SDXC, block-addressed - as predicted"
                                     : "SDSC, byte-addressed - NOT what Gate 6.2 expects");

    std::printf("\n-- 6.3 Block write time --\n");
    flight::FatVolume::Io fio;
    fio.ctx = &card;
    fio.read_block = sd_probe_read;
    flight::FatVolume::Layout layout;
    const flight::FatVolume::Status st =
        flight::FatVolume::locate(fio, "FLIGHT  CSV", 128, layout);
    if (st != flight::FatVolume::Status::ok) {
        std::printf("   cannot time writes: %s\n", flight::FatVolume::describe(st));
        std::printf("   Run tools/prepare_sd_card.py against this card first. Writing at\n"
                    "   a guessed address would destroy the volume, so this refuses to.\n");
        return;
    }
    std::printf("   Writing inside FLIGHT.CSV: %lu blocks over %d extent(s), first LBA %lu.\n",
                static_cast<unsigned long>(layout.total_blocks), layout.extent_count,
                static_cast<unsigned long>(layout.extents[0].first_lba));
    if (layout.extent_count > 1) {
        std::printf("   The file is in %d pieces. That is fine - the log maps its own\n"
                    "   block numbers through the extent list, so a fragmented file\n"
                    "   costs nothing but this line.\n", layout.extent_count);
    }
    std::printf("   This overwrites the log file CONTENTS, not the filesystem. The card\n"
                "   still mounts afterwards, and no hardware is at risk.\n");
    std::printf("\n   Log contents expendable? Press 'w' within 20 s. Anything else skips.\n");

    const std::uint64_t deadline = now_ms() + 20000;
    int key = -1;
    while (now_ms() < deadline) {
        key = getchar_timeout_us(0);
        if (key != PICO_ERROR_TIMEOUT) break;
        sleep_ms(50);
    }
    const bool do_writes = (key == 'w' || key == 'W');
    if (!do_writes) {
        std::printf("   skipped. 6.3 stays open, and the log file keeps its\n"
                    "   contents. 6.6 below still runs - it is not destructive.\n");
    }

    if (do_writes) {
        constexpr int kWrites = 100;
        const std::uint32_t kBaseLba = layout.extents[0].first_lba;
        std::uint8_t block[flight::pico::SdCard::kBlockSize];
        for (std::size_t i = 0; i < sizeof(block); ++i) {
            block[i] = static_cast<std::uint8_t>(i & 0xFF);
        }

        std::uint64_t total_us = 0, worst_us = 0;
        int written = 0;
        for (int i = 0; i < kWrites; ++i) {
            block[0] = static_cast<std::uint8_t>(i);
            const std::uint64_t t0 = to_us_since_boot(get_absolute_time());
            const bool w = card.write_block(kBaseLba + static_cast<std::uint32_t>(i), block);
            const std::uint64_t dt = to_us_since_boot(get_absolute_time()) - t0;
            if (w) {
                total_us += dt;
                if (dt > worst_us) worst_us = dt;
                ++written;
            }
        }
        if (written == 0) {
            std::printf("   every write FAILED. Skipping the timing.\n");
            describe_write_failure(card);

            // A write can fail and still land. CMD13 reporting an error after the data was
            // programmed returns false while the block on the card is correct, and that is a
            // different fault from a card that refused - one is a report, the other is the
            // storage. Reading the block back is the only thing that separates them, and it
            // costs one transaction.
            std::uint8_t check[flight::pico::SdCard::kBlockSize];
            if (!card.read_block(kBaseLba + kWrites - 1, check)) {
                std::printf("   the read-back of that block ALSO failed: the card has\n"
                            "   stopped answering entirely, so this is not a\n"
                            "   write-specific fault. It read perfectly seconds ago.\n");
                // Whether a fresh initialisation brings it back is the whole question. A
                // damaged card stays damaged. A card that lost its supply for a moment
                // comes back the instant it is re-initialised -- and CMD24 is the first
                // thing in this run that makes it draw programming current, which is
                // exactly when a marginal supply would let go.
                if (card.begin(spi0, flight::BoardPins::sd_cs)) {
                    std::printf("   ...and a fresh init BROUGHT IT BACK. The card is not\n"
                                "   damaged - it dropped out and recovered. Something took\n"
                                "   it away at the first write, the first moment in this\n"
                                "   run that it draws programming current.\n");

                    // Two probes, because "it dropped out" still has two causes and they
                    // want opposite fixes. A write straight after a fresh init, with no
                    // reads in between, asks whether the card is broken by the write itself
                    // or by something before it. If that fails, the same write at 400 kHz
                    // asks whether 4 MHz is the problem: a card that writes slowly and not
                    // quickly has a signal-integrity fault, and one that fails at both has
                    // a supply fault. A handheld meter cannot see a collapse that lasts a
                    // few milliseconds; this can.
                    std::uint8_t one[flight::pico::SdCard::kBlockSize];
                    for (std::size_t k = 0; k < sizeof(one); ++k) {
                        one[k] = static_cast<std::uint8_t>(k & 0xFF);
                    }
                    const bool fast_ok = card.write_block(kBaseLba, one);
                    std::printf("\n   PROBE 1, one write straight after that init, at %lu Hz: %s\n",
                                static_cast<unsigned long>(card.run_baud()),
                                fast_ok ? "OK" : "FAILED");
                    if (fast_ok) {
                        std::printf("   So the card writes when freshly initialised. What\n"
                                    "   breaks it is something between the init and the\n"
                                    "   hundredth write, not the first write itself.\n");
                    } else {
                        describe_write_failure(card);
                        card.begin(spi0, flight::BoardPins::sd_cs);
                        card.set_run_baud(flight::pico::SdCard::kInitBaud);
                        const bool slow_ok = card.write_block(kBaseLba, one);
                        std::printf("\n   PROBE 2, the same write at %lu Hz: %s\n",
                                    static_cast<unsigned long>(card.run_baud()),
                                    slow_ok ? "OK" : "FAILED");
                        card.set_run_baud(flight::pico::SdCard::kRunBaud);
                        if (slow_ok) {
                            std::printf("   It writes at 400 kHz and not at 4 MHz. That is\n"
                                        "   SIGNAL INTEGRITY, not power: shorten the SPI\n"
                                        "   jumpers, keep them off long breadboard runs, and\n"
                                        "   give the card module its own ground wire back to\n"
                                        "   Pico pin 38 rather than sharing a rail.\n");
                        } else {
                            describe_write_failure(card);
                            std::printf("   It fails at both clocks, so the clock is not it.\n"
                                        "   That leaves the module's SUPPLY: this board has\n"
                                        "   no regulator and only two capacitors, so a write\n"
                                        "   current spike arrives down whatever the 3V3 and\n"
                                        "   GND jumpers can deliver. Re-seat both, use short\n"
                                        "   ones, and fit the 470 uF bulk capacitor across\n"
                                        "   the module's own 3V3 and GND.\n");
                        }
                    }
                } else {
                    std::printf("   ...and a fresh init did not bring it back either.\n"
                                "   Power-cycle the board before concluding anything about\n"
                                "   the card itself.\n");
                }
            } else {
                bool landed = check[0] == static_cast<std::uint8_t>(kWrites - 1);
                for (std::size_t i = 1; landed && i < sizeof(check); ++i) {
                    if (check[i] != static_cast<std::uint8_t>(i & 0xFF)) landed = false;
                }
                if (landed) {
                    std::printf("   ...but the data IS on the card. The write happened and\n"
                                "   the report of it failed. Look at the stage above, not\n"
                                "   at the wiring.\n");
                } else {
                    std::printf("   and the block does not hold what was written, so the\n"
                                "   data genuinely did not land.\n");
                }
            }
        } else {
        std::printf("   %d/%d written. mean %.3f ms, worst %.3f ms\n", written, kWrites,
                    (total_us / static_cast<double>(written)) / 1000.0, worst_us / 1000.0);
        if (written < kWrites) describe_write_failure(card);

        // A write that reports success and does not land is the failure mode worth catching:
        // the log would look healthy all the way to a card with nothing on it.
        std::uint8_t check[flight::pico::SdCard::kBlockSize];
        const bool read_ok = card.read_block(kBaseLba + kWrites - 1, check);
        bool match = read_ok;
        for (std::size_t i = 1; match && i < sizeof(check); ++i) {
            if (check[i] != static_cast<std::uint8_t>(i & 0xFF)) match = false;
        }
        if (match && check[0] == static_cast<std::uint8_t>(kWrites - 1)) {
            std::printf("   read-back of the last block matches - the writes landed\n");
        } else {
            std::printf("   READ-BACK MISMATCH. Writes reported success without landing,\n"
                        "   which is worse than an honest failure. Do not fly this card.\n");
        }

        // ---- sustained burst, for the meter ----
        //
        // The burst above finishes in well under a second, and a handheld multimeter samples
        // two or three times a second: pointed at that, it averages a window that is mostly
        // idle and reports a number far below the truth. So the card is kept writing long
        // enough for a needle to settle.
        //
        // What this measures is the SUSTAINED write current, not the instantaneous spike, and
        // that is deliberate - it is the figure a regulator is sized against. The duty cycle
        // is printed alongside because it is what makes the reading meaningful: at a duty near
        // 100 % the meter is reading the write current itself rather than an average of writes
        // and gaps.
        constexpr std::uint32_t kBurstMs = 10000;
        std::printf("\n-- 6.3b Sustained write, %lu s, for a current measurement --\n",
                    static_cast<unsigned long>(kBurstMs / 1000));
        std::printf("   Put the meter in series with the module's 3V3 lead, on a current\n"
                    "   range. Note the IDLE reading first - the write cost is the difference,\n"
                    "   not the absolute. Starting in 3 s; watch the meter.\n");
        sleep_ms(3000);

        const std::uint64_t burst_start = now_ms();
        std::uint64_t busy_us = 0;
        std::uint32_t burst_writes = 0, burst_failures = 0;
        std::uint32_t lba = kBaseLba;
        while (now_ms() - burst_start < kBurstMs) {
            block[0] = static_cast<std::uint8_t>(burst_writes);
            const std::uint64_t t0 = to_us_since_boot(get_absolute_time());
            const bool w = card.write_block(lba, block);
            busy_us += to_us_since_boot(get_absolute_time()) - t0;
            if (w) {
                ++burst_writes;
            } else {
                ++burst_failures;
            }
            // Stay inside the log file. Wrapping is fine: this is a measurement, and the
            // region's contents are already forfeit by the time we are here.
            if (++lba >= kBaseLba + layout.extents[0].block_count) lba = kBaseLba;
        }
        const double elapsed_ms = static_cast<double>(now_ms() - burst_start);
        const double duty = elapsed_ms > 0.0 ? (busy_us / 1000.0) / elapsed_ms : 0.0;
        std::printf("   %lu writes in %.1f s -> %.0f writes/s, %.1f KiB/s\n",
                    static_cast<unsigned long>(burst_writes), elapsed_ms / 1000.0,
                    burst_writes / (elapsed_ms / 1000.0),
                    (burst_writes * 512.0 / 1024.0) / (elapsed_ms / 1000.0));
        std::printf("   duty cycle %.1f %% - the fraction of that time the card was writing\n",
                    duty * 100.0);
        if (burst_failures != 0) {
            std::printf("   %lu writes FAILED during the burst\n",
                        static_cast<unsigned long>(burst_failures));
        }
        if (duty < 0.8) {
            std::printf("   NOTE: below 80 %% duty the meter is averaging writes with gaps, so\n"
                        "   the true write current is higher than it reads. Scale by 1/duty.\n");
        } else {
            std::printf("   At this duty the meter is reading the write current itself, near\n"
                        "   enough, rather than an average of writes and idle.\n");
        }
        std::printf("   Record it in bring-up row 2.5's neighbourhood and in the Gate 2 load\n"
                    "   budget. Under ~100 mA and a 470 uF bulk capacitor covers it; well over,\n"
                    "   and the peripherals want their own buck-boost rail.\n");
        }
    }

    std::printf("\n-- 6.6 Log boot count --\n");
    flight::PicoSdLogger logger;
    if (!logger.initialize()) {
        std::printf("   logger init FAILED: %s\n",
                    flight::FatVolume::describe(logger.locate_status()));
        std::printf("   If the file was located, the failure is a write: the log's\n"
                    "   first act is to put a header down.\n");
        if (logger.locate_status() == flight::FatVolume::Status::ok) {
            describe_write_failure(logger.card());
        }
        return;
    }
    std::printf("   boot_count = %lu, records = %lu, %s\n",
                static_cast<unsigned long>(logger.boot_count()),
                static_cast<unsigned long>(logger.record_count()),
                logger.high_capacity() ? "block-addressed" : "byte-addressed");
    const unsigned long truncated =
        static_cast<unsigned long>(logger.truncated_records());
    std::printf("   truncated = %lu%s\n", truncated,
                truncated == 0 ? "  (no record was cut)"
                               : "  <-- RECORDS WERE CUT: rows longer than 511 bytes");
    std::printf("   Power-cycle and re-run: boot_count must increase by exactly one.\n");
}

// ---- Gate 7 ------------------------------------------------------------------
//
// The radio and the microSD share SPI0, and they have never been on a bus together.
//
// The failure this gate exists for is specific. Nothing on the microSD breakout buffers
// MISO -- C.6.4 found no active component on it at all -- so nothing but the card itself
// releases that line when its chip select goes high. A card that keeps driving it corrupts
// the RADIO's next transaction, and the symptom looks like a dead radio. Someone would
// spend a day on the wrong subsystem.
//
// Phase A is non-destructive and needs no antenna: card reads interleaved with radio
// register reads. The radio's version register is the ideal probe because its correct
// answer is known in advance, so a wrong one is unambiguous corruption rather than a
// judgement call. Phase B adds real transmits and real writes, and is prompted for
// separately because it needs an antenna and it overwrites the log.
void report_shared_bus(const flight::Configuration& config) {
    std::printf("\n-- Gate 7: radio and microSD sharing SPI0 --\n");

    flight::PicoRadio radio(config);
    const bool radio_ok = radio.initialize(cansat::link::kTestSyncWord);
    std::printf("   radio init: %s, version 0x%02X\n", radio_ok ? "ok" : "FAILED",
                radio.chip_version());

    flight::pico::SdCard card;
    const bool card_ok = card.begin(spi0, flight::BoardPins::sd_cs);
    std::printf("   7.2 card init with the radio present: %s\n", card_ok ? "ok" : "FAILED");

    // One of the two working is not nothing. Both devices share SCK, MOSI, MISO and the
    // ground, so whichever one answered has just proved those four are good - and that
    // narrows the other one's fault to the lines it does not share. Saying so here saves
    // an evening spent re-seating wires that have already been shown to work.
    if (!radio_ok || !card_ok) {
        if (card_ok && !radio_ok) {
            std::printf("\n   The card initialised on this same bus and read from it, so\n"
                        "   SCK (GP%d), MOSI (GP%d) and MISO (GP%d) all carry data, and the\n"
                        "   card's supply is sound. (That does not prove MISO reaches a\n"
                        "   clean idle HIGH - only writes need that.) The radio's fault\n"
                        "   is therefore in what it does NOT share: NSS on GP%d, RESET on\n"
                        "   GP%d, or the module's own VCC and GND pins. Check those\n"
                        "   three and nothing else.\n",
                        flight::BoardPins::spi_sck, flight::BoardPins::spi_mosi,
                        flight::BoardPins::spi_miso, flight::BoardPins::lora_cs,
                        flight::BoardPins::lora_reset);
        } else if (radio_ok && !card_ok) {
            std::printf("\n   The radio answered on this same bus, so SCK (GP%d), MOSI\n"
                        "   (GP%d), MISO (GP%d), GND and the 3V3 rail are all working. The\n"
                        "   card's fault is in what it does not share: CS on GP%d, the\n"
                        "   module's own supply, or the card not being seated.\n",
                        flight::BoardPins::spi_sck, flight::BoardPins::spi_mosi,
                        flight::BoardPins::spi_miso, flight::BoardPins::sd_cs);
        }
        std::printf("   Gate 7 itself needs BOTH on the bus. Fix the one above and re-run.\n");
        return;
    }

    // 7.1 and 7.5 together. The SD driver raises SPI from 400 kHz to 4 MHz once the card is
    // initialised, and the radio inherits that clock. Asking the radio to identify itself
    // again is the cheapest way to find out whether it still can.
    const std::uint8_t after = radio.probe_version();
    std::printf("   7.1/7.5 radio version re-read after the card raised SPI to %lu Hz: 0x%02X %s\n",
                static_cast<unsigned long>(flight::pico::SdCard::kRunBaud), after,
                after == 0x12 ? "- unchanged" : "- CHANGED, the faster clock is not safe");

    // Address the log file rather than a remembered LBA. An earlier version had 33152
    // written into it, taken from one card on one day; the number moves with the card, the
    // format and the file, and a stale one would have this gate reading and writing
    // wherever that happened to land.
    flight::FatVolume::Io fio;
    fio.ctx = &card;
    fio.read_block = sd_probe_read;
    flight::FatVolume::Layout layout;
    if (flight::FatVolume::locate(fio, "FLIGHT  CSV", 64, layout) !=
        flight::FatVolume::Status::ok) {
        std::printf("   cannot run Gate 7: the log file could not be located.\n");
        return;
    }
    const std::uint32_t base = layout.extents[0].first_lba;

    // 7.4. Read a block, then immediately ask the radio who it is. If the card is still
    // driving MISO when the radio is selected, this is where it shows.
    constexpr int kRounds = 200;
    std::uint8_t block[flight::pico::SdCard::kBlockSize];
    int card_fail = 0, radio_wrong = 0;
    std::uint8_t worst = 0x12;
    for (int i = 0; i < kRounds; ++i) {
        if (!card.read_block(base + static_cast<std::uint32_t>(i % 32), block)) ++card_fail;
        const std::uint8_t v = radio.probe_version();
        if (v != 0x12) {
            ++radio_wrong;
            worst = v;
        }
    }
    std::printf("   7.4 %d interleaved card-read / radio-read rounds:\n", kRounds);
    std::printf("       card read failures : %d\n", card_fail);
    std::printf("       radio misreads     : %d", radio_wrong);
    if (radio_wrong != 0) std::printf("  (last bad value 0x%02X)", worst);
    std::printf("\n");
    if (card_fail == 0 && radio_wrong == 0) {
        std::printf("       PASS - the card releases MISO and the radio stays readable.\n");
    } else {
        std::printf("       FAIL - this is the shared-bus fault Gate 7 exists to catch.\n"
                    "       A card still driving MISO corrupts the radio, not itself, so\n"
                    "       do not go looking at the radio first.\n");
    }

    // Phase B: real transmits interleaved with real writes.
    std::printf("\n   7.3 needs an antenna on the RA-02 and overwrites the log file.\n");
    std::printf("   Both ready? Press 'g' within 20 s. Anything else skips.\n");
    const std::uint64_t deadline = now_ms() + 20000;
    int key = -1;
    while (now_ms() < deadline) {
        key = getchar_timeout_us(0);
        if (key != PICO_ERROR_TIMEOUT) break;
        sleep_ms(50);
    }
    if (key != 'g' && key != 'G') {
        std::printf("   skipped. 7.3 stays open.\n");
        return;
    }

    constexpr int kBursts = 30;
    const std::string packet(206, 'A');
    const TxFailures before_tx = tx_counters(radio);
    int tx_fail = 0, wr_fail = 0;
    radio_wrong = 0;
    for (int i = 0; i < kBursts; ++i) {
        if (!radio.transmit(packet)) ++tx_fail;
        block[0] = static_cast<std::uint8_t>(i);
        if (!card.write_block(base + static_cast<std::uint32_t>(i), block)) ++wr_fail;
        if (radio.probe_version() != 0x12) ++radio_wrong;
    }
    std::printf("   7.3 %d transmit-then-write rounds: %d TX failures, %d write failures,\n"
                "       %d radio misreads afterwards\n",
                kBursts, tx_fail, wr_fail, radio_wrong);
    if (tx_fail != 0) describe_tx_failure(radio, tx_failures_since(radio, before_tx));
    if (wr_fail != 0) describe_write_failure(card);
    if (tx_fail == 0 && wr_fail == 0 && radio_wrong == 0) {
        std::printf("       PASS - both devices work while the other is active.\n");
    } else {
        std::printf("       FAIL - record which of the three counters moved; they point at\n"
                    "       different faults.\n");
    }
}

void gps_status(flight::PicoGps& gps, std::uint64_t boot_fix_ms) {
    cansat::GpsData d;
    const bool have = gps.latest(d) && d.valid;
    std::printf("gps=");
    if (have) {
        std::printf("FIX sats=%2u lat=%10.6f lon=%11.6f alt=%7.1fm ", d.satellites,
                    d.latitude, d.longitude, d.altitude);
        if (boot_fix_ms > 0) {
            std::printf("ttff=%lus ", static_cast<unsigned long>(boot_fix_ms / 1000));
        }
    } else {
        std::printf("no-fix sats=%2u ", d.satellites);
    }
    std::printf("cksum_err=%lu ", static_cast<unsigned long>(gps.checksum_errors()));
}

}  // namespace

int main() {
    stdio_init_all();
    flight::pico_buses_init();

    // Solid on: this image never blinks, so a lit LED means the firmware is running and a
    // dark one means it is not. GP14 is an external LED - a bare Pico shows nothing here,
    // and that is correct.
    gpio_init(flight::BoardPins::status_led);
    gpio_set_dir(flight::BoardPins::status_led, GPIO_OUT);
    gpio_put(flight::BoardPins::status_led, 1);

    wait_for_host(10000);

    std::printf("\n\n=====================================================\n");
    std::printf(" CanSat 2026 - bring-up diagnostic\n");
    std::printf(" NOT flight firmware. Gates 1 and 3 of the bring-up record.\n");
    std::printf("=====================================================\n");
    std::printf("\nExpected wiring for this image (nothing else connected):\n");
    std::printf("   Pico 3V3  pin 36  ->  MPU-9250 VCC, BMP280 VCC\n");
    std::printf("   Pico GND  pin 38  ->  MPU-9250 GND, BMP280 GND\n");
    std::printf("   Pico GP%-2d pin  6  ->  MPU-9250 SDA, BMP280 SDA\n", flight::BoardPins::i2c_sda);
    std::printf("   Pico GP%-2d pin  7  ->  MPU-9250 SCL, BMP280 SCL\n", flight::BoardPins::i2c_scl);
    std::printf("   Pico GP%-2d pin 16  ->  NEO-6M RX   (the labels cross)\n", flight::BoardPins::gps_tx);
    std::printf("   Pico GP%-2d pin 17  <-  NEO-6M TX\n", flight::BoardPins::gps_rx);
    std::printf("\nAny subset may be connected. Absent devices are reported, not fatal.\n");

    // Before the IMU is initialised the AK8963 is invisible: it sits behind the MPU's
    // pass-through bridge and does not answer the outside bus until BYPASS_EN is set.
    // Scanning twice makes that visible rather than something to take on trust.
    scan_i2c("before IMU init - AK8963 at 0x0C should NOT appear");
    const std::uint8_t baro_addr = report_baro_identity();

    const flight::Configuration config;
    flight::PicoImu imu(config);
    flight::PicoBarometer baro(config);

    std::printf("\n-- Driver initialisation --\n");
    const bool imu_ok = imu.initialize();
    std::printf("   IMU       : %s\n", imu_ok ? "ok" : "FAILED");
    const bool baro_ok = baro.initialize();
    std::printf("   Barometer : %s\n", baro_ok ? "ok" : "FAILED");

    if (imu_ok) {
        report_imu_identity(imu);
        // What the second scan should show depends on which part answered. Telling an
        // operator that 0x0C "SHOULD now appear" on a six-axis package sends them looking
        // for a wiring fault that is not there -- two lines after this program has just
        // told them the package has no magnetometer in it.
        scan_i2c(imu.has_magnetometer()
                     ? "after IMU init - AK8963 at 0x0C SHOULD now appear"
                     : "after IMU init - six-axis part, so 0x0C will NOT appear, which is "
                       "correct");
    }

    if (imu_ok) stationary_statistics(imu, config);
    if (baro_ok && baro_addr != 0) barometer_output_rate(baro, baro_addr);
    if (imu_ok || baro_ok) acquisition_rate(imu, baro, config, imu_ok, baro_ok);

    // GPS last: it is the only subsystem here whose supply is still unverified (C.5.5),
    // so everything that can be measured without it is already on the record by now.
    report_radio(config);
    report_sd();
    report_shared_bus(config);

    gps_raw_echo(config, 5);
    flight::PicoGps gps(config);
    const bool gps_ok = gps.initialize();
    std::printf("   Parser: %s\n", gps_ok ? "started" : "FAILED to start");
    std::uint64_t first_fix_ms = 0;

    std::printf("\n-- Live readings, 2 Hz. Ctrl-C or unplug to stop. --\n");
    while (true) {
        const std::uint64_t t = now_ms();
        if (gps_ok) {
            gps.poll(t);
            if (first_fix_ms == 0) {
                cansat::GpsData d;
                if (gps.latest(d) && d.valid) first_fix_ms = t;
            }
        }
        flight::ImuSample s;
        flight::BaroSample b;
        const bool has_imu = imu_ok && imu.read(s, t) && s.valid;
        const bool has_baro = baro_ok && baro.read(b, t) && b.valid;

        std::printf("t=%6lus  ", static_cast<unsigned long>(t / 1000));
        if (has_imu) {
            std::printf("a=[%7.3f %7.3f %7.3f] g=[%8.3f %8.3f %8.3f] ",
                        s.ax_mps2, s.ay_mps2, s.az_mps2, s.gx_dps, s.gy_dps, s.gz_dps);
            if (s.mag_valid) {
                std::printf("m=[%7.2f %7.2f %7.2f] ", s.mx_ut, s.my_ut, s.mz_ut);
            } else {
                std::printf("m=[   --      --      -- ] ");
            }
            std::printf("Tdie=%5.1fC ", s.die_temperature_c);
        } else {
            std::printf("imu=--  ");
        }
        if (has_baro) {
            std::printf("P=%9.2fPa T=%5.2fC alt=%7.2fm ", b.pressure_pa, b.temperature_c,
                        b.altitude_m);
        } else {
            std::printf("baro=-- ");
        }
        if (gps_ok) gps_status(gps, first_fix_ms);
        std::printf("\n");

        // The GPS UART must be drained faster than 2 Hz or the RP2040's 32-byte FIFO
        // overruns and sentences are lost mid-line. Poll while waiting rather than
        // sleeping through it - this is the same reasoning that sizes the flight loop's
        // tick against gps_uart_fifo_bytes.
        const std::uint64_t until = now_ms() + 500;
        while (now_ms() < until) {
            if (gps_ok) gps.poll(now_ms());
            sleep_ms(5);
        }
    }
}
