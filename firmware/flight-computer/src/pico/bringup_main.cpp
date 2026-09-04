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

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"

#include <cmath>
#include <cstdint>
#include <cstdio>

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
        case 0x0C: return "AK8963 magnetometer, inside the MPU-9250 package";
        case 0x68: return "MPU-9250, AD0 low";
        case 0x69: return "MPU-9250, AD0 high";
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
        scan_i2c("after IMU init - AK8963 at 0x0C SHOULD now appear");
    }

    if (imu_ok) stationary_statistics(imu, config);
    if (baro_ok && baro_addr != 0) barometer_output_rate(baro, baro_addr);
    if (imu_ok || baro_ok) acquisition_rate(imu, baro, config, imu_ok, baro_ok);

    // GPS last: it is the only subsystem here whose supply is still unverified (C.5.5),
    // so everything that can be measured without it is already on the record by now.
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
