# Sensors

Everything the vehicle measures, and what each one has actually read on hardware.

**Status: 2026-09-08.** All four sensors are fitted to the soldered board and answer.

---

## The sensor set

| Device | Interface | Address / pins | Measures | State |
|---|---|---|---|---|
| **MPU-6500** IMU | I2C0, GP4/GP5 | `0x68` | Acceleration (3 axis), angular rate (3 axis) | 🟢 Verified. **Sold as an MPU-9250; it is not one** |
| **GY-BMP280** | I2C0, GP4/GP5 | `0x76` | Pressure, temperature, altitude | 🟢 Verified |
| **NEO-6M** GPS | UART0, GP12/GP13 | 9600 8N1 | Latitude, longitude, altitude, satellites, HDOP | 🟠 Talking; **no fix acquired yet** |
| **LM393** sound module | ADC1 GP27, digital GP15 | — | Relative acoustic level, and a threshold duty | 🟠 Fitted; its data reaches neither the card nor the radio yet ([F-15](../../documentation/testing/bring-up-record.md#findings)) |

Both I2C devices share one bus, and `0x0C` is **correctly absent** — that address would be
the AK8963 magnetometer inside a real MPU-9250.

---

## What has been measured

From [bring-up-record.md](../../documentation/testing/bring-up-record.md), gates 3 and 4.

| Quantity | Measured | Limit | Margin |
|---|---|---|---|
| Stationary acceleration magnitude | 9.81 m/s² | ±1.5 of 1 g | Far inside |
| Gyro bias, per axis | X −3.39, Y +0.91, Z −0.47 dps | ±25 dps | Inside the datasheet's own ±5 |
| Gyro noise, per axis | X 0.098, Y 0.096, Z 0.142 dps sd | < 2 dps sd | 14–20× inside |
| Barometer output rate | **83.0 Hz** | 83 Hz predicted | Exact |
| Acquisition rate | **33.289 ms → 30.04 Hz** | 33 ms | 0.9 % |
| Acquisition jitter | 0.453 ms sd | < 1.98 ms | 4.4× inside |
| Sensor read cost | 0.282 ms mean, 0.833 ms worst | 33 ms period | 2.5 % of a tick |
| GPS output | All six NMEA sentences, 164 B/s | 9600 baud | Well formed |
| GPS checksum errors | **0** over 18 s | ≈ 0 | Provisional — taken indoors |

**The acquisition loop is not the constraint.** A worst-case sensor read costs 0.833 ms
against a 33 ms period. Whatever eventually limits this vehicle, it is not the time spent
talking to sensors.

---

## The IMU is a six-axis part

This is the single most consequential hardware fact in the project.

**What was ordered:** an MPU-9250 — accelerometer, gyroscope and an AK8963 magnetometer.
**What arrived:** an MPU-6500. `WHO_AM_I` reads `0x70`, and `0x0C` never answers even after
`BYPASS_EN` is set ([F-1](../../documentation/hardware/receiving-inspection.md#findings),
bring-up rows 8.8 and 8.9).

**The consequence is yaw.** Roll and pitch are absolutely referenced by gravity and are
unaffected. Yaw has no absolute reference at all: it is a gyroscope integration whose zero
is wherever the vehicle happened to be pointing at power-on. Every packet declares which
kind it is — `YR-M` for magnetic, `YR-G` for relative — and **this vehicle will always send
`YR-G`.**

**How badly it drifts, measured rather than estimated.** From a 36.8-minute stationary SD
log of 2209 records ([F-17](../../documentation/testing/bring-up-record.md#findings)): yaw
made **more than a full revolution**. It held to ±0.8° for the first 15 minutes before
settling into roughly 0.4 dps — which over a 3-minute flight is about **70°**.

**The nine-axis path is implemented, tested, and dormant.** The AK8963 driver, the axis
mapping into the body frame, the hard- and soft-iron calibration and the tilt-compensated
magnetic yaw all exist and are covered by tests. They would run on a real MPU-9250. Fixing
this is a procurement decision, not a software one — or an organizer ruling that a declared
relative yaw is acceptable ([open question 6](../../README.md)).

---

## Open items

| Item | Blocked on | Note |
|---|---|---|
| **GPS fix outdoors** | Sky view | Row 4.2. Indoors it reached one satellite in 5 s, which is the receiver working, not a fix |
| **Gyro drift across temperature** | A thermal soak | [F-13](../../documentation/testing/bring-up-record.md#findings). The bias measured on a warm bench is not the bias at altitude |
| **IMU `INT` on GP7** | Firmware | The pin is wired. Nothing enables the interrupt, so it sits static — which is correct behaviour, not a fault |
| **Microphone data reaching the log** | [F-15](../../documentation/testing/bring-up-record.md#findings) | The module is fitted and the columns exist; the values do not arrive |
| **Trimpot position recorded** | Nobody has written it down | The LM393's gain is an unmarked trimpot that nothing reads back, so **two flights at different positions produce incomparable numbers** |
| **Barometer against a reference** | A second barometer | Row 3.6 |

---

## How the firmware treats them

Three rules, all tested, all in [controller.cpp](../../firmware/flight-computer/src/controller.cpp):

1. **An implausible reading is worse than no reading.** A value outside datasheet-derived
   bounds (30–115 kPa, ±170 m/s², ±2200 °/s) drops the previously held value too, so the
   vehicle never coasts on data from a sensor that is actively wrong.
2. **Staleness is time-based.** Two seconds without a good read raises the fault; one failed
   read changes nothing.
3. **Only mandatory sensing is critical.** GPS missing means omitted optional fields. The
   microphone is held as a pointer that may be null — a failed init does not fail the
   self-test and a failed read costs one warning. Losing **both** the IMU and the barometer
   is the only sensor condition that is critical, and even then telemetry continues.

Related: [bring-up-record.md](../../documentation/testing/bring-up-record.md) ·
[sensor-rates.md](../../documentation/design/sensor-rates.md) ·
[receiving-inspection.md](../../documentation/hardware/receiving-inspection.md) ·
[avionics/README.md](../README.md)
