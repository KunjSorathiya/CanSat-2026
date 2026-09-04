# Sensor Rates and the 30 Hz Acquisition Loop

Why the flight loop acquires at 30 Hz, why the barometer had to be reconfigured before it
could, and why sampling faster than a sensor converts makes the data worse rather than
better.

This document is the source of the sensor constants in
[`config.hpp`](../../firmware/flight-computer/include/flight/config.hpp) and the timing
model in [`sensor_timing.hpp`](../../firmware/flight-computer/include/flight/sensor_timing.hpp).

---

## Contents

- [The rate split](#the-rate-split)
- [What each sensor can actually deliver](#what-each-sensor-can-actually-deliver)
- [The barometer: the binding constraint](#the-barometer-the-binding-constraint)
- [The IMU: bandwidth and aliasing](#the-imu-bandwidth-and-aliasing)
- [The magnetometer](#the-magnetometer)
- [Why over-sampling a sensor corrupts vertical speed](#why-over-sampling-a-sensor-corrupts-vertical-speed)
- [Bus and CPU budget](#bus-and-cpu-budget)
- [Loop scheduling](#loop-scheduling)
- [Guards in the code](#guards-in-the-code)
- [Changing the rates](#changing-the-rates)
- [What is not verified](#what-is-not-verified)

---

## The rate split

The radio is the slowest stage in the system by two orders of magnitude, and nothing else
is tied to it. Each stage runs at the rate its own physics allows:

| Stage | Rate | Bounded by | Configured by |
|---|---:|---|---|
| Main loop tick | 500 Hz (2 ms) | Scheduling jitter, and the GPS UART FIFO | `loop_tick_ms` |
| Sensor acquisition, orientation, altitude rate | **30 Hz** (33 ms) | Barometer conversion time | `sensor_period_ms` |
| Mission state machine | 30 Hz, fed each acquisition | — | — |
| Battery, health | 1 Hz | Nothing meaningful changes faster | `battery_period_ms`, `health_period_ms` |
| SD flush | 0.5 Hz (appends are per packet) | SD write latency | `sd_flush_period_ms` |
| **RF telemetry** | **1 Hz** | **LoRa airtime — see [link-budget.md](link-budget.md)** | `telemetry_period_ms` |

30 Hz of *RF telemetry* is not physically possible with this radio and packet; 30 Hz of
*acquisition and state estimation* is, and that is what the loop does.

## What each sensor can actually deliver

| Sensor | Configured output rate | Sampled at | Margin |
|---|---:|---:|---|
| MPU-9250 (accelerometer + gyroscope) | 200 Hz internal; 21.2 Hz accelerometer / 20 Hz gyroscope bandwidth | 30 Hz | 6.7× |
| AK8963 magnetometer (inside the MPU-9250) | 100 Hz continuous mode 2 | 30 Hz | 3.3× |
| BMP280 (pressure + temperature) | ~83 Hz typical, ~72 Hz worst case | 30 Hz | 2.4× |
| NEO-6M (GPS) | 1 Hz NMEA, drained continuously without blocking | every tick | n/a |

## The barometer: the binding constraint

The BMP280 in normal mode converts continuously, and its output rate is set by the
oversampling settings. From the datasheet (BST-BMP280-DS001 rev 1.19, section 3.8.1):

```text
t_measure,typ [ms] = 1.0  + 2.0 * osrs_t + (2.0 * osrs_p + 0.5)
t_measure,max [ms] = 1.25 + 2.3 * osrs_t + (2.3 * osrs_p + 0.575)
output rate        = 1 / (t_measure + t_standby)
```

Both formulas are implemented in `sensor_timing.hpp` and pinned by tests against the
datasheet's own published figures: they reproduce the 43.2 ms worst case quoted for
x2/x16, the 26.3 Hz "indoor navigation" preset, and the 83 Hz "handheld device, dynamic"
preset exactly.

| Setting | osrs_t | osrs_p | Typical conversion | Output rate | Verdict at 30 Hz |
|---|---|---|---:|---:|---|
| Previous (hard-coded) | x2 | x16 | 37.5 ms | **26.3 Hz** | **too slow** |
| Current | x1 | x4 | 11.5 ms | **83.3 Hz** | 2.8× margin |
| Fastest useful | x1 | x1 | 5.5 ms | 166 Hz | noisier, unnecessary |

> **The finding.** The barometer was configured for x2 temperature and x16 pressure
> oversampling — the datasheet's "indoor navigation" preset, which produces **26.3 Hz**.
> A 30 Hz acquisition loop would have re-read unchanged conversions roughly one sample in
> eight. The oversampling now comes from the flight configuration rather than a constant
> buried in the driver, and the startup validator refuses a sampling period the barometer
> cannot keep up with.

The cost of dropping from x16 to x4 pressure oversampling is a modest increase in pressure
noise, which the IIR filter (kept at x16, as in the datasheet preset) largely absorbs. For
a vehicle descending at several metres per second, update rate is worth more than the last
few centimetres of altitude resolution.

## The IMU: bandwidth and aliasing

The MPU-9250's digital low-pass filters are **anti-aliasing** filters, and their cutoffs
have to be chosen against the *acquisition* rate, not the sensor's internal rate. Sampling
at 30 Hz puts the Nyquist limit at 15 Hz: content above that folds down into the attitude
estimate and cannot be removed afterwards.

The MPU-9250 splits what the MPU-6050 did with one register into two. `DLPF_CFG` in
`CONFIG` (register 26) filters the gyroscope; `A_DLPF_CFG` in `ACCEL_CONFIG 2`
(register 29) filters the accelerometer. They are configured separately and their
bandwidth tables are not the same, so quoting one figure for both would be wrong.

One more MPU-9250 trap: `FCHOICE_B` in `GYRO_CONFIG` must be `00`, or `DLPF_CFG` is
bypassed entirely and the part stays on its 8 kHz path. The configured bandwidth is then
silently not applied — the registers read back exactly as written and the filter is simply
not in circuit.

| Setting | Accel bandwidth (`A_DLPF_CFG`) | Gyro bandwidth (`DLPF_CFG`) | At 30 Hz sampling |
|---:|---:|---:|---|
| 3 | 44.8 Hz | 41 Hz | **Aliases** airframe vibration from 15 Hz upward |
| **4 (current)** | **21.2 Hz** | **20 Hz** | Small residual band above 15 Hz, accepted |
| 5 | 10.2 Hz | 10 Hz | Fully anti-aliased, but blurs the launch transient |

Setting 4 is the compromise: it removes the bulk of the vibration band while keeping the
launch and impact transients that the state machine watches. It is **not** fully
anti-aliased — 15 to 21 Hz still folds down — and that residual is accepted knowingly
rather than hidden. `validate_config()` deliberately does not reject it: the right setting
depends on how much this airframe actually vibrates, which is a shake-table measurement,
not something software can assert. `SMPLRT_DIV` stays at 4, so the internal rate is 200 Hz
and every 30 Hz read returns a fresh sample.

All three are configuration fields (`imu_gyro_dlpf_cfg`, `imu_accel_dlpf_cfg`,
`imu_sample_rate_div`), not driver constants, so they can be re-tuned against real
vibration data without touching the driver.

## The magnetometer

The AK8963 inside the MPU-9250 does not follow `SMPLRT_DIV` at all. It free-runs in its own
continuous measurement mode and raises its own data-ready flag, which the driver checks on
every read.

| Mode | Output rate | At 30 Hz acquisition |
|---|---:|---|
| Continuous mode 1 | 8 Hz | Too slow: most reads would return a stale sample |
| **Continuous mode 2 (current)** | **100 Hz** | 3.3× margin, a fresh sample every time |

`validate_config()` rejects a magnetometer mode slower than the acquisition rate, because a
repeated magnetic sample is not merely wasted — it drags the yaw correction toward a
reading the vehicle has already turned away from.

The driver also reads the AK8963's `ST2` register at the end of every burst. That is not
optional: an AK8963 whose `ST2` is never read stops updating, so a driver that reads only
the data registers gets a magnetometer that works exactly once.

## Why over-sampling a sensor corrupts vertical speed

Vertical speed is differentiated from barometric altitude:

```text
rate_inst = (altitude_now - altitude_prev) / dt
rate      = 0.7 * rate + 0.3 * rate_inst
```

Reading the BMP280 faster than it converts returns the *same registers*, so
`altitude_now == altitude_prev` and `rate_inst` is exactly zero. Those false zeros enter
the filter and pull the estimate toward zero — worst during descent, when the vehicle is
moving fastest and the landing detector is watching that number.

Two independent protections, because this is a silent failure:

1. `validate_config()` refuses a `sensor_period_ms` shorter than the barometer's worst-case
   conversion time, so the configuration cannot create the situation.
2. The controller only updates the rate when the pressure reading has actually changed, so
   the estimate holds its last value if the sensor stalls, slows or is reconfigured in the
   field. `test_controller_ignores_repeated_barometer_samples()` covers this.

**The hold is bounded, and that matters as much as the hold itself.** An unchanged pressure
means one of two things, and they need opposite responses:

| Unchanged for | Means | Correct response |
|---|---|---|
| A sample or two | The loop outran the sensor | Hold the estimate |
| Longer than `altitude_rate_hold_ms` (200 ms) | The vehicle genuinely is not moving vertically | Decay to zero |

Getting the second case wrong is not a small error: the landing detector requires
`|vertical speed| < 1 m/s`, so a rate held at its last descent value would **never** let the
mission leave `FLIGHT` — it would sit in the flight state on the ground, and the post-impact
window would never begin. Telemetry would continue regardless, but the mission state would
be wrong for the rest of the recovery.

## Bus and CPU budget

I2C0 runs at 400 kHz. Each transaction is roughly `(bytes + 2) × 9` bits including
addressing and acknowledgement:

| Transaction | Bytes | Bits | Time at 400 kHz |
|---|---:|---:|---:|
| MPU-9250 burst read (accel, temp, gyro) | 14 | ~144 | ~0.36 ms |
| BMP280 burst read (pressure, temperature) | 6 | ~72 | ~0.18 ms |
| **Per 30 Hz acquisition** | 20 | ~216 | **~0.54 ms** |

At 30 Hz that is **~16 ms per second, about 1.6 % of the I2C bus** — no contention risk.
The GPS UART drains at 9600 baud into a bounded buffer and never blocks. SPI is shared
between the radio and the SD card and is exercised at the telemetry rate, not the sensor
rate.

CPU cost per acquisition is one complementary-filter update, one barometric-altitude
evaluation and the Bosch fixed-point compensation — all fixed-cost integer and
double-precision arithmetic with no allocation. This has **not** been measured on hardware;
the claim here is that the work is bounded and small, not that a figure was observed.

## Loop scheduling

The main loop is non-blocking and yields for `loop_tick_ms` — **2 ms** — per tick. Two
independent limits bound that number from above, and the second is easy to miss:

**1. Scheduling jitter.** The shortest scheduled task is the 33 ms acquisition, so the tick
sets the jitter on every task: 2 ms is **under 6 %** of the acquisition period. The previous
5 ms tick would have been 15 %.

**2. The GPS UART FIFO.** The GPS is drained once per tick, and the RP2040's UART FIFO is
**32 bytes deep**. At 9600 baud, 8N1 — ten bits on the wire per byte — that FIFO fills in:

```text
32 bytes × 10 bits ÷ 9600 baud = 33.3 ms
```

A tick at or beyond that silently loses NMEA bytes *before anything reads them*. The symptom
is not an obvious fault: it is truncated sentences, rising checksum errors, and a GPS that
seems unreliable for no visible reason. `validate_config()` therefore refuses a tick above
**half** the fill time, leaving margin for a late tick. At 2 ms the margin is 16×.

This limit tightens if the GPS is ever reconfigured to a faster baud rate: at 115200 baud the
FIFO fills in 2.8 ms, and only a 1 ms tick would pass. Both `gps_baud` and
`gps_uart_fifo_bytes` are configuration fields so the check follows the hardware rather than
a comment.

`PeriodicTask` re-anchors after a stall rather than firing a catch-up burst, so a long tick
delays one acquisition instead of triggering several back to back.

## Guards in the code

| Layer | What it prevents |
|---|---|
| `validate_config()` | An acquisition period shorter than the barometer's worst-case conversion time, and a loop tick too slow to drain the GPS UART before its FIFO overflows |
| Controller pressure-change check | A stalled or slow barometer biasing vertical speed toward zero |
| `test_sensor_timing_model()` | The timing model drifting from the datasheet's published presets |
| `test_config_sensor_rate_guard()` | The default configuration silently becoming unachievable |

## Changing the rates

1. Edit `sensor_period_ms`, `baro_osrs_t`, `baro_osrs_p`, `baro_filter`,
   `imu_gyro_dlpf_cfg`, `imu_accel_dlpf_cfg`, `imu_sample_rate_div` or `mag_mode` in
   `config.hpp`.
2. Rebuild and run `bash tools/build_host.sh`. If the barometer cannot feed the new rate,
   `validate_config()` says so, with the numbers.
3. Check the IMU bandwidth against the new Nyquist limit — the guard does not enforce this
   one, because the right trade-off depends on the vibration environment.
4. Update the tables in this document.

## What is not verified

| Claim | Status |
|---|---|
| BMP280 timing formulas | Verified against three published datasheet figures |
| MPU-9250 bandwidth table | Transcribed from the register map, register 26 |
| 83 Hz barometer output rate | **MEASURED 2026-09-05: 83.0 Hz.** 166 `STATUS.measuring` falling edges in 2 s, on the delivered board at the configured x1/x4 oversampling |
| I2C bus utilisation | Computed from bus speed and transaction length, **never measured** |
| CPU headroom at 30 Hz | **Partly measured 2026-09-05.** A barometer read costs 0.282 ms mean, 0.347 ms worst — under 1 % of the 33 ms period. Not yet measured with the IMU sharing the bus, and not profiled beyond sensor reads |
| Actual achieved loop rate | **MEASURED 2026-09-05: 30.04 Hz**, 33.289 ms mean interval with 0.453 ms standard deviation, over 150 ticks. Taken on the bring-up diagnostic's loop, which bounds `controller.cpp`'s scheduler rather than describing it |
| Vibration spectrum during flight | Unknown; the DLPF choice is a datasheet-informed estimate |

The barometer's real output rate and the achieved loop rate have both now been taken, on
hardware, and both matched their predictions — **83.0 Hz against 83.3 predicted, and 30.04 Hz
against 30**. The 2.8× margin in the table above is confirmed rather than assumed.

**How 3.5 was measured matters, because the obvious method gives the wrong answer.** Counting
*changed* pressure values returns roughly 46 Hz, not 83. That is not a slow sensor: with the
IIR filter at x16 the BMP280 deliberately moves its output slowly, so consecutive conversions
often produce the same compensated value, and counting distinct values measures how often the
reading moves rather than how often the part converts. Count falling edges of
`STATUS.measuring` (register `0xF3`, bit 3) instead — one edge per completed conversion,
regardless of whether the value changed.

What remains unmeasured is the bus utilisation, which needs a scope, and CPU headroom beyond
the sensor-read cost.

---

## Related documents

- [Software Architecture](software-architecture.md) — the flight loop these rates schedule
- [Link Budget](link-budget.md) — why the radio rate is decoupled from all of this
- [Hardware](../hardware/hardware.md) — the sensors themselves
- [Test Plan](../testing/test-plan.md) — the hardware measurements this document asks for
