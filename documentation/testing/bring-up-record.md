# Hardware Bring-Up Record

Every number this repository computes but has never observed, paired with how to measure it
and space to write down what you actually got.

**Print it, or fill it in as you go.** The point is not paperwork: this project has made a
long series of predictions — airtime, output rates, bus loads, link margin — from datasheets
and arithmetic. Each one is a place where reality can differ, and the difference is worth
knowing *before* it appears as a lost flight. A prediction that survives measurement becomes
a fact. One that does not is a finding.

**Fill in:** the measured value, the date, and who took it. Leave a row blank rather than
guessing — a blank row is honest, a guessed one is worse than nothing.

---

## Contents

- [How to use this](#how-to-use-this)
- [Gate 1 · Bare Pico](#gate-1--bare-pico)
- [Gate 2 · Power](#gate-2--power)
- [Gate 3 · Sensors](#gate-3--sensors)
- [Gate 4 · GPS](#gate-4--gps)
- [Gate 5 · Radio](#gate-5--radio)
- [Gate 6 · Storage](#gate-6--storage)
- [Gate 7 · Shared bus](#gate-7--shared-bus)
- [Gate 8 · End to end](#gate-8--end-to-end)
- [Gate 9 · Endurance and recovery](#gate-9--endurance-and-recovery)
- [Findings](#findings)
- [Sign-off](#sign-off)

---

## How to use this

Work the gates in order. Each is a stop: if a row fails, fix it before moving on, because a
fault on a shared bus is far harder to find with three devices on it than with one.

| Column | Means |
|---|---|
| **Predicted** | What this repository computes, with the document that computes it |
| **Predicted Hz** | Gate 1 only, where the quantity is a rate and a meter can read it directly |
| **How to measure** | The specific procedure, not "check it works" |
| **Measured** | What you got. Blank if not done |
| **Verdict** | ✅ within tolerance · ⚠️ outside but usable · ❌ needs action |

The sequenced electrical order is in [wiring.md](../design/wiring.md#bring-up-order); the
pass/fail matrix is in [test-plan.md](test-plan.md#hardware-test-plan). This document is the
*numbers* those two ask you to collect.

---

## Gate 1 · Bare Pico

| # | Quantity | Predicted | Predicted Hz | How to measure | Measured | Verdict |
|---|---|---|---|---|---|---|
| 1.1 | Status LED blink, `READY` unarmed | 900 ms on, 900 ms off — **1800 ms full cycle** | **0.56 Hz** | Stopwatch over 10 **full cycles**, divide by 10; or the meter's `Hz` range on GP14 | | |
| 1.2 | Status LED blink, `READY` armed | 400 ms on, 400 ms off — **800 ms full cycle** | **1.25 Hz** | Same, after the arming delay | | |
| 1.3 | Status LED blink, `FLIGHT` | 100 ms on, 100 ms off — **200 ms full cycle** | **5 Hz** | Same | | |
| 1.3a | Status LED blink, `LANDED` / `RECOVERY` | 250 ms on, 250 ms off — **500 ms full cycle** | **2 Hz** | Same, after a landing is declared | | |
| 1.3b | Status LED blink, `FAULT` | 60 ms on, 60 ms off — **120 ms full cycle** | **8.3 Hz** | Same, with a mandatory sensor disconnected so the state machine latches `FAULT` | | |
| 1.4 | USB serial enumerates | Appears as a serial port | — | Device manager / `ls /dev/tty*` | **Yes.** Both Picos enumerate; the ground bridge came up as `COM4`, and the port appears and disappears with the cable | ✅ 2026-09-05 / KS |
| 1.5 | Boot to first telemetry attempt | < 1 s | — | Log timestamps from power-on | **Deferred — not measurable at this gate.** See the note below | — |
| 1.6 | Bridge status cadence, no radio attached | 1000 ms (`STATUS_PERIOD_MS`) | 1 Hz | Watch the `#state=RX` line in a serial monitor | **1 Hz, steady, no gaps** over a continuous run | ✅ 2026-09-05 / KS |
| 1.7 | USB frame integrity | Length and CRC-16/CCITT match the payload | — | Decode one captured frame by hand against `frame_encode()` | **Byte-exact.** `$51,56b5,` against a 51-character payload whose CRC independently computes to `56b5` | ✅ 2026-09-05 / KS |

> Blink rates come from `update_led()` in `controller.cpp`. They are the only diagnostic
> visible on a sealed vehicle, so confirm all three before the structure closes.

> **Read the predicted numbers carefully — they are half-periods, not blink rates.**
> `update_led()` computes `on = (mission_ms / period) % 2 == 0`, so `period` is the time the
> LED spends **on**, and the same again **off**. A "900 ms" state therefore blinks once every
> **1800 ms**. Timing ten blinks and dividing by ten gives 1800, which is correct behaviour
> and would read as a 2× failure against a row that only said "900 ms period". That is why
> these rows now state on-time, full cycle and frequency separately.
>
> The complete map, for whichever state the vehicle is actually in when you measure:
>
> | `MissionState` | On / off | Full cycle | Frequency |
> |---|---:|---:|---:|
> | `init`, `self_test` | **solid on** | — | — |
> | `ready`, unarmed | 900 ms | 1800 ms | 0.56 Hz |
> | `ready`, armed | 400 ms | 800 ms | 1.25 Hz |
> | `flight` | 100 ms | 200 ms | 5 Hz |
> | `landed`, `recovery` | 250 ms | 500 ms | 2 Hz |
> | `fault` | 60 ms | 120 ms | 8.3 Hz |
>
> **A meter's `Hz` range on GP14 beats a stopwatch** for 1.2 and 1.3, and needs no LED. Many
> meters will not lock onto 0.56 Hz, so 1.1 may still want the stopwatch. `solid on` at
> power-up is its own check: it says the firmware reached `update_led()` at all.

> **1.1–1.3 are not yet takeable, and the reason is not a fault.** The status LED is on
> **GP14, an external LED** — the Pico's own LED on GP25 is not driven by this firmware, so a
> bare board shows nothing and that is correct. The rows wait on an LED and a ~330 Ω resistor
> between GP14 (physical pin 19) and GND (physical pin 18).
>
> **1.5 is deferred to Gate 5 or Gate 6, whichever runs first.** The vehicle firmware writes
> nothing to USB — the only `stdout` writer in the tree is the ground-station bridge — so
> "first telemetry attempt" has no observable on a vehicle with no radio and no SD card. It is
> deferred, not skipped, and it is not evidence of a fault.
>
> **What the bridge did prove, on 2026-09-05:** USB CDC enumeration, a main loop running to
> completion, a timebase good enough to hold a 1 Hz cadence with no visible drift, a 3 s
> watchdog being fed (a starved one would show as a gap and a restart), and the framing layer
> byte-correct against an independent implementation of the same CRC. `radio=0` and `frames=0`
> are the correct readings with no RA-02 attached.
>
> Rows 1.6 and 1.7 were added because they are what this gate could actually measure. A gate
> that records only what it planned to measure, and nothing of what it learned, is worth less
> than the afternoon it costs.

---

## Gate 2 · Power

🔴 **This gate is blocked on one thing: no regulator is selected.** See
[electrical-architecture.md](../design/electrical-architecture.md). Complete it before any
battery-powered test.

> **The microSD reader's supply is no longer part of this blocker.** Receiving inspection
> [D.1](../hardware/receiving-inspection.md#d1--the-microsd-reader-sku-11566) closed it on
> 2026-09-04: the delivered board has **no regulator and no level shifter**, its supply pin is
> printed `3V3`, and its entire parts list is four 10 kΩ pull-ups and two capacitors. The
> supplier listing that described a 4.5–5.5 V board described a different product from the one
> that arrived — see [sd-module-analysis.md](../hardware/sd-module-analysis.md) for why this
> question held up the power design for so long.
>
> **The consequence simplifies this gate.** There is one 3.3 V rail, not two, and the boost
> stage the earlier design reserved is not to be built.
>
> **The microSD half is now answered, on 2026-09-05.** Under a 10-second sustained write at
> 100 % duty — 324 writes per second, far beyond anything the mission asks — the 3V3 rail held
> **3.28–3.30 V**. That is a 0.6 % droop. **The Pico's own regulator carries the card without
> strain, and the peripherals do not need a rail of their own on the card's account.**
>
> **What is still open is the combined case.** The radio draws ~120 mA transmitting and the
> GPS ~67 mA, both from this same rail, and nothing has yet run them together. Gate 7 is where
> that is settled. Until then this gate stays amber rather than green: the SD question is
> closed, the total is not.

| # | Quantity | Predicted | How to measure | Measured | Verdict |
|---|---|---|---|---|---|
| 2.1 | Regulator output, no load | — (part not chosen) | Multimeter at the rail | | |
| 2.2 | Regulator output, radio transmitting | — | Scope on the rail during a TX burst | | |
| 2.3 | Rail droop during TX | Must not brown out the Pico | Scope, minimum during TX | | |
| 2.4 | Idle current | — | Inline meter, vehicle in `READY` | | |
| 2.5 | Peak current during TX | — | Inline meter or scope across a shunt | | |
| 2.6 | Battery voltage at the ADC pin | Divider ratio unknown; firmware reports raw pin volts until it is set | Multimeter at GP26 vs battery | | |
| 2.7 | Endurance, full charge to cutoff | — | Log until telemetry stops | | |

**Until 2.6 is measured and `battery_divider_ratio` set, battery telemetry is the raw ADC
pin voltage — not the cell voltage.** That is deliberate: an invented ratio would produce a
confident wrong number.

---

## Gate 3 · Sensors

| # | Quantity | Predicted | Source | How to measure | Measured | Verdict |
|---|---|---|---|---|---|---|
| 3.1 | I2C devices found | 2 before IMU init (0x68, 0x76). A nine-axis part adds `0x0C` after init; **the delivered six-axis part does not**, and the diagnostic now says so rather than asking for it | [wiring.md](../design/wiring.md) | Bus scan — `cansat_bringup_firmware` scans twice | **Both parts answer, tested one at a time: `0x68` and `0x76`**, matching the firmware defaults — so AD0 and SDO are both strapped low. **`0x0C` never appears**, in either scan, with or without the bypass: there is no magnetometer, see 8.8. Not yet run with both on the bus together | ⚠️ 2026-09-05 / KS — identities confirmed; `0x0C` is a real absence, and the shared-bus run is still to come |
| 3.2 | Stationary acceleration magnitude | 9.81 m/s². **Firmware gate is ±1.5** (`calib_accel_tol_mps2`); a healthy part at rest should be an order of magnitude tighter | `calib_accel_tol_mps2` | Read 100 samples, take the mean | **9.8675 m/s², sd 0.0092**, 100 of 100 samples valid. +0.06 against true g — under 1 % scale error | ✅ 2026-09-05 / KS |
| 3.3 | Stationary gyro bias, per axis | Within ±25 dps, typically < 5 | `calib_max_gyro_bias_dps` | Mean of 100 still samples | **X −3.3878, Y +0.9079, Z −0.4720 dps.** All inside ±25, and inside the datasheet's ±5 zero-rate figure. Startup calibration removes these | ✅ 2026-09-05 / KS |
| 3.4 | Gyro noise, per axis | < 2 dps standard deviation | `calib_gyro_still_dps` | Standard deviation of the same samples | **X 0.0984, Y 0.0964, Z 0.1424 dps sd.** Fourteen to twenty times inside the limit — a quiet part | ✅ 2026-09-05 / KS |
| 3.5 | Barometer output rate | **83 Hz** typical, 72 Hz worst case | [sensor-rates.md](../design/sensor-rates.md) | Count falling edges of `STATUS.measuring` (0xF3 bit 3); **not** changed values — see the note | **83.0 Hz.** 166 `STATUS.measuring` falling edges in 2 s, status polled at 9.1 kHz so no completion could be missed. Changed-value counting gave 46.5 Hz on the same run — the undercount described below | ✅ 2026-09-05 / KS — matches the 83.3 Hz prediction, 2.8× margin over 30 Hz confirmed |
| 3.6 | Pressure vs a local reference | Within a few hundred Pa | — | Compare with a weather station or second barometer | | |
| 3.7 | Achieved acquisition rate | **30 Hz** (33 ms period) | `sensor_period_ms` | Log mission time between sensor ticks | **33.289 ms mean → 30.04 Hz**, over 150 ticks | ✅ 2026-09-05 / KS |
| 3.8 | Acquisition jitter | < 6 % of the period (2 ms) | 2 ms loop tick | Standard deviation of the same intervals | **0.453 ms sd** against a 1.98 ms limit — 4.4× inside. Sensor read cost **0.282 ms mean, 0.33–0.35 ms worst across two runs**, i.e. under 1 % of the period (barometer only; the IMU was not wired for this run) | ✅ 2026-09-05 / KS |
| 3.9 | Calibration settle time | Within `calib_samples` at 30 Hz ≈ 2.7 s | `startup_calibration.cpp` | Time from power-on to `CAL-1` in telemetry | | |
| 3.10 | I2C bus utilisation | ~1.6 % at 30 Hz | [sensor-rates.md](../design/sensor-rates.md) | Scope SCL, measure active time per second | | |
| 3.11 | **Microphone quiet-room floor** | A small non-zero span, **not** zero and **not** clipped; `sound_gate_pct` near 0 | `sound_mv_pp` and `sound_gate_pct` in the SD log, vehicle still, room quiet. **A span of exactly zero means `AO` is not connected**, and a span that never moves means it is not connected to a microphone | | |
| 3.12 | **Microphone responds to sound** | Both channels move: the level rises clearly above 3.11 and returns, and `sound_gate_pct` rises from near 0 | Clap, then speak steadily. **A clap should give a high level at low duty and speech a lower level at higher duty** — that difference is the whole reason both channels are carried | | |
| 3.13a | **Board variant recorded** | 3-pin or 4-pin, written down | Count the pins. A 3-pin board has no `AO` at all and needs `sound_analog_connected = false`, or the log fills with a floating ADC pin that looks exactly like a quiet room | **Four-pin: `AO DO GND VCC`**, read off the header silkscreen in the delivery photograph ([D.5](../hardware/receiving-inspection.md#d5--the-lm393-sound-module)). Both channels are real; both config flags are correct at their defaults | ✅ 2026-09-06 / KS |
| 3.13 | **Trimpot position recorded** | A written record exists | The gain is set by an unmarked trimpot and nothing reads it back, so **two flights at different positions produce incomparable numbers.** Set it, mark it, write it down | | |

> **3.5 was re-taken, and the first attempt's shortfall was the method, not the sensor.** The
> first measurement counted *changed* pressure values and got 44 Hz against a predicted 83.
> The edge-counting method on the same board returns **83.0 Hz**. That undercount happens
> because with the IIR filter at x16 the BMP280 deliberately moves its output slowly, so
> consecutive conversions frequently produce the **same** compensated value. Counting distinct
> values measures how often the reading moves, not how often the part converts — a different
> question from the one this row asks.
>
> The diagnostic counts falling edges of **`STATUS.measuring`** (register `0xF3`, bit 3).
> Every 1 → 0 transition is one completed conversion whether or not the result changed, which
> is the output rate as the datasheet defines it. Both figures are printed, so the gap between
> them stays visible rather than being quietly replaced.
>
> **Take the lesson, not just the number.** A measurement that disagrees with a prediction is
> not automatically a finding about the hardware. Here the sensor was right, the datasheet was
> right, and the instrument was wrong — the same shape of error as the multimeter that
> invented a short across the MPU-9250 two days earlier.

> 3.5 and 3.7 are the two that matter most. If the barometer is slower than the loop, the
> vertical-speed estimate degrades — the firmware detects and handles it, but the
> configuration should be corrected rather than relied on to degrade gracefully.

> **Rows 3.1–3.5, 3.7, 3.8, 4.1–4.3, 5.1–5.3, 6.1–6.3, 6.6, 7.1–7.5 and 8.8 are taken with `cansat_bringup_firmware`**, a separate image
> that prints over USB. The flight firmware speaks only over LoRa, so a vehicle with no radio
> attached produces nothing to read — that is why this gate had no observable until the
> diagnostic existed. Flash it exactly like the flight image, open the port at any baud rate,
> and it prints:
>
> - **two bus scans**, before and after IMU initialisation. The AK8963 at `0x0C` must be
>   absent from the first and present in the second — it sits behind the MPU's pass-through
>   bridge and does not answer the outside bus until `INT_PIN_CFG.BYPASS_EN` is set. Seeing
>   the difference is the check; seeing `0x0C` in *both* would mean something else is at that
>   address.
> - the **barometer chip ID** from register `0xD0` — `0x58` BMP280, `0x60` BME280. This is the
>   register read that
>   [C.4.1](../hardware/receiving-inspection.md#c4--gy-bmp280-33) defers to, and it settles
>   [F-4](../hardware/receiving-inspection.md#findings) properly rather than by measuring a
>   package in a photograph.
> - the IMU's **`WHO_AM_I`**, which settles
>   [F-1](../hardware/receiving-inspection.md#findings): `0x71`/`0x73` is a real nine-axis
>   part, `0x70` an MPU-6500 with no magnetometer in the package at all.
> - **100 stationary samples** reduced to the mean and standard deviation that rows 3.2, 3.3
>   and 3.4 ask for, each printed against the limit from `Configuration` and marked PASS or
>   OUT OF RANGE. The tolerances are read from the config at run time, so these can never
>   drift away from what the firmware actually enforces.
>
> - **the barometer's true output rate (3.5)** by counting *changed* pressure values rather
>   than reads. Polling faster than the part converts returns the same bytes again, so
>   counting reads would report the poll rate and call it the output rate.
> - **acquisition interval and jitter (3.7, 3.8)**, plus the sensor read time inside each
>   tick — which is the number that actually matters, being the part of the period the flight
>   loop cannot spend on anything else. Measured on the diagnostic's own loop, not on
>   `controller.cpp`'s scheduler: it **bounds** the flight loop rather than describing it.
> - **five seconds of raw NMEA (4.1)**, echoed verbatim. That is deliberately not the
>   parser's opinion: a wrong baud rate produces a steady stream of plausible-looking
>   garbage, and only looking at the characters tells that apart from silence or from real
>   sentences.
> - **fix status, satellite count, time to first fix and checksum errors (4.2, 4.3)** in the
>   live line, once the parser is running.
>
> **Any subset of the hardware may be connected.** Absent devices are reported and skipped,
> never fatal — which is what makes one-sensor-at-a-time bring-up practical without a
> breadboard.
>
> It drives the same `mpu9250.cpp`, `bmp280.cpp` and `neo6m.cpp` the vehicle flies. A
> diagnostic built on its own copy of the drivers can pass while the flight build fails,
> which is worse than having no diagnostic at all.
>
> **It is not flight software and is never linked into the flight image.** The launch build
> carries no debug output, and no flag that could accidentally enable some.

---

## Gate 4 · GPS

| # | Quantity | Predicted | How to measure | Measured | Verdict |
|---|---|---|---|---|---|
| 4.1 | Raw NMEA arrives | Sentences at 9600 baud | Serial monitor on the UART | **Yes.** `$GPRMC`, `$GPVTG`, `$GPGGA`, `$GPGSA`, `$GPGSV`, `$GPGLL` — one full cycle per second, well formed, at 9600 baud. **818 bytes in 5 s (164 B/s)** against the 960 B/s the line carries | ✅ 2026-09-05 / KS |
| 4.2 | Time to first fix, cold, outdoors | Minutes | Stopwatch from power-on to `GP-Lat` appearing | Not yet taken — needs sky. **Indoors it reached one satellite in view within 5 s** (`$GPGSV,1,1,01,04,,,28`: PRN 04 at 28 dB-Hz), so the receiver and antenna path are live | |
| 4.3 | NMEA checksum errors | ≈ 0 | `gps_checksum_errors` in the health snapshot | **0** over an 18 s indoor run. Re-check with a fix, when the sentences carry populated fields and get longer | ⚠️ 2026-09-05 / KS — provisional, taken without a fix |
| 4.4 | Position accuracy | — | Compare with a known surveyed point or a phone | | |
| 4.5 | Fix held while the radio transmits | No dropouts | Watch the fix flag through 50 transmissions | | |

> **The receiver runs at 1 Hz, not the 5 Hz the supplier listing advertised.** Five complete
> sentence cycles arrived in five seconds. 5 Hz is a NEO-6M *capability*, reached by sending it
> a UBX configuration message; nothing in this firmware sends one, so 1 Hz is what the vehicle
> gets. That is the receiver's default and is fine for a CanSat — but if anyone reconfigures
> it, the `24C32A` EEPROM will remember the change across power cycles, which is a good way to
> confuse the next person to test it.
>
> **The byte rate is a useful margin figure.** 164 B/s indoors with no fix, against the
> 960 B/s a 9600 baud line carries. It will rise once sentences carry populated fields and
> more satellites, but the RP2040's 32-byte FIFO is nowhere near pressure at this rate — it
> fills in about 195 ms rather than the ~33 ms `gps_uart_fifo_bytes` assumes at full line
> rate. The flight loop's tick has more room here than the worst case it was sized against.

> 4.5 is worth its own row: the GPS and the radio have not shared a vehicle before, and RF
> desensitisation of a GPS front end by a nearby 433 MHz transmitter is a real effect.

---

## Gate 5 · Radio

**The most valuable gate in this document.** Everything about the telemetry rate rests on
computed airtime that has never been observed.

> **Taken 2026-09-05: 5.1, 5.2, 5.3 and 5.5 all pass, on the vehicle Pico's own 3.3 V rail.**
> The airtime model this project computes everything from is now checked against a radio and
> agrees to within 1.8 %, with the excess growing slightly with payload — which is what
> filling a larger FIFO over SPI looks like, and is the direction overhead has to move.
>
> **That is the most load-bearing number in the repository.** The 1 Hz telemetry rate, the SF7
> choice, the channel-occupancy budget and the build-time `static_assert` that refuses a
> profile which cannot meet 1 Hz all rest on `lora_time_on_air_ms()`. It was previously
> verified only against published reference vectors — self-consistent arithmetic. It has now
> been asked of hardware.
>
> **An unplanned Gate 2 data point came free.** Ten transmits at +17 dBm ran off the Pico's
> own 3V3 regulator with no reset and no USB dropout. That is not the flight power tree — a
> regulator is still unchosen — but it says the RA-02's transmit transient does not brown out
> a 300 mA-class source, which is useful when sizing one.

> **Row 5.4a exists because five packets cannot tell you what is wrong.** The airtime test
> is over in under three seconds — no use with a handheld meter, and five samples is not
> enough to judge an intermittent fault. The sustained burst runs for 15 seconds and prints
> one character per attempt, so the *shape* of a failure is visible:
>
> | Pattern | Means |
> |---|---|
> | `..............` | healthy |
> | `.....xxxxxxxxx` | worked, then stopped — heat, or a supply falling away |
> | `.x.x..x.x.x.x.` | random — a marginal connection or a marginal rail |
>
> Those three want different investigations and a success *count* cannot separate them.
> Back-to-back transmission is also the harshest load the supply will ever see, far beyond
> the 1 Hz the mission sends — which is the point of measuring there.

> ⚠️ **Never transmit without the antenna connected.** An open RF port reflects the whole
> output back into the power amplifier, and this is the one action in bring-up that can
> destroy a module rather than merely fail. The chain is antenna → SMA joint → 10 cm pigtail
> → u.FL, and [C.7.4](../hardware/receiving-inspection.md#c7--antenna-and-ipex-cable) confirmed
> it mates with no adapter.
>
> `cansat_bringup_firmware` enforces this: it reads the version register unprompted, but
> **will not transmit until someone presses `t`**, after printing the warning. Skipping the
> prompt leaves 5.2 and 5.3 open rather than risking the part.
>
> **Rows 5.1–5.3 need only one radio.** 5.1 is a register read over SPI; 5.2 and 5.3 time
> five transmits each and compare the mean against `lora_time_on_air_ms()` — the same model
> the link budget and the build-time `static_assert` use, called at run time so the two
> cannot drift apart. Measured time includes FIFO fill, mode changes and the DIO0 round trip,
> so it should sit slightly **above** the prediction; well above means the driver is waiting
> on something it should not be.
>
> Everything from 5.4 down needs both radios and the ground station.
>
> **Wiring for Gate 5**, from `BoardPins` — the RA-02's supply pin is third from the end with
> `GND` either side, so mark pin 1 before connecting anything:
>
> ```text
> Pico 3V3  pin 36  ──  3.3V        Pico GP17 pin 22  ──  NSS
> Pico GND  pin 38  ──  GND         Pico GP20 pin 26  ──  RST
> Pico GP18 pin 24  ──  SCK         Pico GP21 pin 27  ──  DIO0
> Pico GP19 pin 25  ──  MOSI
> Pico GP16 pin 21  ──  MISO
> ```

| # | Quantity | Predicted | Source | How to measure | Measured | Verdict |
|---|---|---|---|---|---|---|
| 5.1 | RA-02 version register | 0x12 | `sx1278.cpp` | Read register 0x42 over SPI — `cansat_bringup_firmware` reports it | **0x12** | ✅ 2026-09-05 / KS |
| 5.2 | Airtime, 206-byte packet, SF7/125 kHz | **328 ms** | [link-budget.md](../design/link-budget.md) | Scope DIO0, TX start to TxDone | **333.7 ms**, mean of 5, all 5 sent. +5.8 ms (1.8 %) over the 327.9 ms model. Reproduced to 0.1 ms across three sessions, including one that failed all 5 before [F-5](#findings) was closed | ✅ 2026-09-05 / KS |
| 5.3 | Airtime, full 255-byte packet | **400 ms** | Same | Same, with a padded packet | **406.9 ms**, all 5 sent. +7.3 ms (1.8 %) over the 399.6 ms model. Identical to 0.1 ms across three sessions — the successful transmissions timed the same whether or not their neighbours failed, which is what pointed at supply rather than timing | ✅ 2026-09-05 / KS |
| 5.4 | Achieved telemetry rate | **1.00 Hz** | `telemetry_period_ms` | Packet numbers per second at the ground station | | |
| 5.5 | Channel occupancy at 1 Hz | ~33 % typical, 40 % worst case | [link-budget.md](../design/link-budget.md) | 5.2 ÷ 1000 ms | **33.4 % typical, 40.7 % worst case**, from the measured airtimes | ✅ 2026-09-05 / KS |
| 5.4a | **Sustained transmit, back to back** | Every packet sent; the rail holds | 15 s of continuous transmits, one character printed per attempt; meter on DC volts across the 3V3 rail | **45 attempts in 15.0 s: 45 sent, 0 failed. 3.0 packets/s at 206 bytes — 100 % duty against the 333.7 ms airtime, so the radio is transmitting continuously.** **The 3V3 rail held 3.26–3.27 V** throughout. Taken on the same power cycle in which 5.2 failed all 5 and 5.3 failed 3 of 5, minutes earlier | ✅ 2026-09-05 / KS |
| 5.6 | RSSI at 10 m | −28 dBm free-space | [link-budget.md](../design/link-budget.md) | Bridge status line | | |
| 5.7 | RSSI at 100 m | −48 dBm free-space | Same | Same | | |
| 5.8 | RSSI at 500 m | −62 dBm free-space | Same | Same | | |
| 5.9 | RSSI at 1 km | −68 dBm free-space | Same | Same | | |
| 5.10 | SNR at maximum range | Positive | — | Bridge status line | | |
| 5.11 | Packet loss at maximum range | < 1 % | — | Ground-station loss counter over 200 packets | | |
| 5.12 | Range at which loss reaches 5 % | Predicted well beyond 1 km | [link-budget.md](../design/link-budget.md) | Walk out until loss climbs | | |
| 5.13 | Both sync words verified | 0xF3 and 0xA5 both link | Rulebook | Reflash **both** Picos, confirm each | | |

> **The RSSI predictions are free-space path loss with 17 dBm transmit and 0 dBi antennas,
> and nothing else.** Real readings will be weaker — commonly by 10–20 dB — because of
> antenna efficiency, polarisation mismatch while the vehicle tumbles, the vehicle's own
> structure, and ground reflections. Treat the predicted column as the ceiling, not the
> expectation; what matters is the *shape* of the curve and the margin to the SX1278's
> roughly −123 dBm sensitivity at SF7.
>
> **Record RSSI against distance as a table, not a single number.** It is the only
> measurement that predicts the link's behaviour at ranges you cannot walk to, and this
> project's entire spreading-factor decision rests on having margin.
>
> A measured airtime more than 10 % from 5.2 means the modem is not configured as the
> firmware believes — check the spreading factor and bandwidth on **both** ends first.

---

## Gate 6 · Storage

> **6.6 is the dual-header resume mechanism, proved on hardware.** The log came back from a
> power cycle at the right place: boot count up by exactly one, and the CSV column header
> still written once rather than once per boot. That is `RawBlockLog` reading its header off
> the card, believing it, and continuing — which is the whole reason the header is written
> after *every* record and kept in two alternating copies. A brownout mid-flight must not
> make the next boot overwrite the flight it just recorded.
>
> Note that the boot count tracks **logger initialisations, not power cycles.** A run that
> never reaches the logger does not increment it, which is why the sequence read 1 then 2
> across three sessions.

> **Gate 2's SD question is answered, and the answer is decisive.** The rail was watched on
> DC volts through the 10-second sustained burst — **100 % duty, 324 writes per second** — and
> held **3.28–3.30 V**. A 20 mV droop on a 3.3 V rail is 0.6 %, against a card specified to
> 2.7 V and an RP2040 that browns out far below that. **The Pico's own regulator carries the
> microSD with no strain, and no separate buck-boost rail is needed for it.**
>
> No current figure was taken — the series connection would not hold with hand-held probes —
> so the load budget still cannot be totalled arithmetically. It does not need to be for this
> decision. **What is still open is the combined case**: the radio's ~120 mA transmit burst
> and the GPS's ~67 mA share this rail, and only Gate 7 exercises them together.
>
> **The card is 500× faster than the mission needs.** 324 writes/s sustained, against two
> writes per telemetry record — the record and the header rewrite — so 2 writes/s at 1 Hz, or
> 4 at 2 Hz. Storage throughput will never be the constraint on this vehicle.
>
> **One number to carry into Gate 7: the worst single write took 4.814 ms.** The flight loop
> is single-threaded and its sensor tick is 33 ms, so a write landing inside a tick consumes
> 15 % of it. Row 3.8 measured 0.453 ms of jitter with no card attached; expect occasional
> excursions once the log is live, on roughly one tick in thirty. That is a prediction to
> check, not a fault.

> **The log file was located successfully on 2026-09-05**, which is the first exercise of the
> FAT32 lookup on real hardware: `FLIGHT.CSV` at **LBA 33152, 131072 blocks available**. That
> is exactly the 64 MiB the prep script allocated, so the contiguity check passed across every
> cluster - the file is in one piece and the log can be written linearly into it.
>
> **The card is an HP mx310 64 GB, which is SDXC**, and 64 GB cards ship formatted exFAT.
> `FatVolume` reads FAT32 only, so the card had to be reformatted - Windows will not do that
> above 32 GB through its own dialog, and Rufus's `Large FAT32` mode was used.
>
> **A fragmented log file is no longer a failure.** The first version of `FatVolume`
> demanded one unbroken run and refused anything else. On this card a routine recreate
> produced a 64 MiB file in **two runs with a 31 MB hole between them**, twice, on a freshly
> formatted volume — confirmed independently by `tools/inspect_sd_log.py`, which reads the
> same FAT over a different code path in a different language and reached the same verdict.
>
> Refusing that turns a pre-flight step into "reformat the card and try again", which is
> exactly the step that gets skipped at six in the morning — and skipping it costs the whole
> log. `FatVolume` now returns a list of extents and the logger maps its own block numbers
> through it, so a fragmented file costs nothing but a line of output. Only a file in more
> runs than the sixteen-extent bound is refused.

> ⚠️ **The write test destroys the filesystem on the card.** The vehicle logs raw 512-byte
> blocks with no filesystem at all, and the log starts at **LBA 2048** — exactly where a
> FAT32 partition begins on a card formatted the usual way. After any write test the card
> will not mount on a PC until it is reformatted. **That is by design, not a fault**, and it
> is the same layout the vehicle will use in flight.
>
> No hardware is at risk here — unlike the radio's antenna warning, the only casualty is the
> card's contents. `cansat_bringup_firmware` still puts it behind a `w` prompt, because
> destroying data silently is its own kind of failure. **6.1 and 6.2 run unprompted; they are
> read-only.**
>
> **What Gate 2 needs from this gate is a current, and firmware cannot measure it** — a
> board cannot see its own supply. The diagnostic makes the measurement *takeable*
> instead: after the timed 100-write burst it holds the card writing for a continuous
> **10 seconds** (row 6.3b), because a handheld meter samples two or three times a second
> and would otherwise average a window that is mostly idle.
>
> It prints the **duty cycle** alongside, which is what makes the reading mean anything:
> near 100 % the meter is reading the write current itself rather than an average of
> writes and gaps. Below 80 % the true figure is higher than it reads, by 1/duty.
>
> **Take the idle reading first.** The write cost is the difference, not the absolute.
>
> **The card is the whole variable.** The module has no active component at all — four
> 10 kΩ pull-ups and two capacitors is its entire parts list ([C.6.6](../hardware/receiving-inspection.md#c6--micro-sd-card-reader-sku-11566))
> — so what is being measured is flash programming inside the card. Write current varies
> enormously between cards, so measure the one that will fly.
>
> **If you would rather not break the circuit**, the go/no-go question has a simpler
> answer: meter on DC volts across the 3V3 rail during the burst. If it holds 3.3 V the
> regulator is coping. That does not size a replacement part, but it does decide whether
> one is needed.
>
> **Wiring for Gate 6** — the microSD shares SCK, MOSI and MISO with the RA-02 and has its
> own chip select, so this is the same bus with one more wire:
>
> ```text
> Pico 3V3  pin 36  ──  3V3         Pico GP18 pin 24  ──  CLK
> Pico GND  pin 38  ──  GND         Pico GP19 pin 25  ──  MOSI
> Pico GP6  pin  9  ──  CS          Pico GP16 pin 21  ──  MISO
> ```
>
> The module prints `CLK`, not `SCK`, and its header runs `GND MISO CLK MOSI CS 3V3` — ground
> and supply at opposite ends, so a reversed header is a direct short across the rail. The
> card holder is friction-fit with no positive retention, so a card can sit in it looking
> seated without making contact.

| # | Quantity | Predicted | How to measure | Measured | Verdict |
|---|---|---|---|---|---|
| 6.1 | Card initialises | CMD0/CMD8/ACMD41 succeed | `sd_ok` in the health snapshot, or `cansat_bringup_firmware` | **Complete.** 32 ms in ACMD41, 1 CMD0 attempt, on an HP mx310 64 GB. Reached only after [the CMD0 fix](#gate-6--storage) - the first attempt on this card returned `0x1F` | ✅ 2026-09-05 / KS |
| 6.2 | Card type detected | SDHC (block-addressed) for any modern card | `high_capacity()` | **Block-addressed.** The delivered card is an HP mx310 64 GB, so SDXC rather than SDHC - the same addressing mode, which is what this row actually tests | ✅ 2026-09-05 / KS |
| 6.3 | Single block write time | — | Time 100 `write_block` calls | **2.677 ms mean, 4.814 ms worst**, 100/100 written, on a first pass. **Three sessions: 4.814 ms, 29.756 ms and 4.915 ms worst case**, 100/100 written each time, means of 2.677, 6.160 and 2.678 ms. About 5 ms is typical and ~30 ms has been seen once. **Read-back of the last block matched** on both, so the writes landed rather than merely being acknowledged. The worst case is the number that matters for flight — see [F-11](#findings) | ✅ 2026-09-05 / KS |
| 6.3b | **Sustained write load** | — (this is the Gate 2 input) | Meter in series for a current; **or DC volts across the rail for the go/no-go** | **2979–3672 writes in 10.0 s across three sessions — 297 to 367 writes/s, 148.6 to 183.6 KiB/s, at 100.0 % duty.** Current not taken; the series connection would not hold. **Instead the 3V3 rail was watched under that load and held 3.28–3.30 V** | ✅ 2026-09-05 / KS — decisive for the design question, though not a current figure |
| 6.4 | Records written per telemetry packet | 2 (record + header) | Count blocks after N packets | | |
| 6.5 | Log survives a power cut | Resumes at the right block, no data lost | Pull power mid-flight-test, reboot, read back | | |
| 6.6 | Boot count increments | +1 per power session | `boot_count()` | **1 on the fresh log, 2 after a power cycle — exactly one increment.** `records` stayed at **1** across both, so the CSV column header was written once and not repeated. `truncated = 0` | ✅ 2026-09-05 / KS |
| 6.6a | Records cut to fit a block | **0** — the widest possible row is 402 bytes against a 511-byte limit, 109 to spare | `truncated_records()`, reported by `cansat_bringup_firmware` at 6.6 | | |
| 6.7 | Records recovered after impact | All up to the last write | Read the card after a drop test | | |

> 6.5 is the one to do deliberately and more than once, at different moments — the log's
> two-header design exists precisely for this case, and it has only ever been tested
> against a simulated card.

---

## Gate 7 · Shared bus

The radio and the microSD share SPI0. This gate exists because they have never been on a
bus together.

| # | Quantity | Predicted | How to measure | Measured | Verdict |
|---|---|---|---|---|---|
| 7.1 | Radio works with the card present but idle | No change from Gate 5 | Repeat 5.1 and 5.4 | **Version `0x12`, unchanged**, re-read over the bus after the card had initialised | ✅ 2026-09-05 / KS |
| 7.2 | Card works with the radio present but idle | No change from Gate 6 | Repeat 6.1 and 6.3 | **Card initialised with the radio on the bus**, 42 ms in ACMD41, 2 CMD0 attempts | ✅ 2026-09-05 / KS |
| 7.3 | Radio works *during* card writes | No lost packets | Run both at full rate for 5 minutes | **30 transmit-then-write rounds: 0 transmit failures, 0 write failures, 0 radio misreads.** Each round is a 206-byte transmit, then a block write, then a radio version read — the interleaving the flight loop actually does. Ten seconds, not five minutes. **Passed twice, on separate power cycles** | ⚠️ 2026-09-05 / KS — passes; the duration this row asks for is still owed |
| 7.4 | MISO released when each device is deselected | Line goes high-impedance | Scope MISO during the other device's transaction | **200 interleaved rounds: 0 card-read failures, 0 radio misreads.** Every radio probe returned `0x12`. Measured rather than scoped — a held MISO would have corrupted the radio read | ✅ 2026-09-05 / KS |
| 7.5 | SPI clock after an SD transfer | 4 MHz, unchanged for the radio | Scope SCK during a radio transaction | **Radio reads correctly at the 4 MHz the SD driver raises the bus to.** Confirmed by the same version re-read as 7.1 | ✅ 2026-09-05 / KS |

> **Phase A passed on 2026-09-05, and it is the half that matters most.** 200 interleaved
> rounds with zero card-read failures and zero radio misreads: **the card releases MISO and
> the radio stays readable.** That is the fault this gate was written for, and the delivered
> SD board has nothing that could have saved us from it — no buffer, no active component at
> all.
>
> **7.5 came free with 7.1.** The SD driver raises SPI from 400 kHz to 4 MHz once the card is
> up, and the radio then answered correctly at that clock. Two rows from one register read.
>
> **The highest-risk integration in the BOM is now the one with the most evidence behind it.**
> 7.3 is still open and needs an antenna.

> **`cansat_bringup_firmware` takes this gate in two phases**, because half of it is free
> and half of it is not.
>
> **Phase A is non-destructive and needs no antenna.** It initialises both devices, then
> re-reads the radio's version register *after* the SD driver has raised SPI from 400 kHz to
> 4 MHz — that single read covers 7.1 and 7.5 together. It then runs **200 rounds of
> card-read followed immediately by radio-register-read**. The version register is the ideal
> probe precisely because its correct answer is known in advance: `0x12` or the bus is
> lying. Any other value is unambiguous corruption rather than a judgement call, and that is
> 7.4 measured rather than scoped.
>
> **Phase B is prompted separately**, because it transmits and it overwrites the log: 30
> rounds of transmit-then-write, checking the radio is still readable after each. That is
> 7.3.
>
> The three counters are reported separately — card failures, radio misreads, transmit
> failures — because they point at different faults and a single pass/fail would lose that.

> 7.4 is the specific failure the driver was fixed for: a card that keeps driving MISO
> corrupts the **radio's** next transaction, and the symptom looks like a dead radio.

---

## Gate 8 · End to end

| # | Quantity | Predicted | How to measure | Measured | Verdict |
|---|---|---|---|---|---|
| 8.1 | Packet numbering | Sequential from `P-001`, no gaps | Ground-station validator over 500 packets | | |
| 8.2 | Ground-station packet loss, bench | 0 % | Loss counter | | |
| 8.3 | CRC errors on the USB link | 0 | Link health panel | | |
| 8.4 | Mission clock vs wall clock | Within 1 % | Compare `Ti-` with a stopwatch over 10 minutes | | |
| 8.5 | Altitude at rest | ≈ 0.0 m after calibration | Read `A-` on the bench | | |
| 8.6 | Altitude vs a known height | Within a few metres | Carry the vehicle up a measured staircase | | |
| 8.7 | Attitude vs a known orientation | Within a few degrees | Place on a level surface, then on each face | | |
| 8.8 | `WHO_AM_I` of the IMU | `0x71` or `0x73` | Read at initialisation; the health report carries it | **`0x70` — an MPU-6500.** Not the nine-axis part the module was sold as. Read with `cansat_bringup_firmware` | ❌ 2026-09-05 / KS — see [F-1](../hardware/receiving-inspection.md#findings) |
| 8.9 | Magnetometer present and answering | AK8963 found at `0x0C`, `mag_ok` true | Health snapshot | **Absent.** `0x0C` does not appear in the bus scan taken *after* `BYPASS_EN` is set — there is nothing behind the bridge | ❌ 2026-09-05 / KS |
| 8.10 | Total magnetic field, vehicle assembled | 25–65 µT, and stable as the vehicle is moved | `mag_field_ut` in the health snapshot | **Not takeable on this part** — no magnetometer. 0 of 100 samples carried one | N/A |
| 8.11 | Magnetometer calibration sweep | Every axis spans ≥ 30 µT; calibration accepted | Figure-of-eight with `mag_cal_in_flight` set; watch `mag_cal_span_ut` | **Not takeable on this part** | N/A |
| 8.12 | Yaw drift, stationary, 10 minutes, uncalibrated | Drifts: no magnetic reference is being applied | Record `Ya-` with `YR-G` in the packets | **Now the only yaw row this vehicle has.** Still to take — and it characterises flight behaviour, not a defect | |
| 8.13 | Yaw drift, stationary, 10 minutes, calibrated | Holds: bounded by magnetometer noise, not integrating | Record `Ya-` with `YR-M` in the packets | **Not takeable on this part.** `YR-M` will never appear in this vehicle's telemetry | N/A |
| 8.14 | Yaw against a known bearing | Within a few degrees of a hand compass, four cardinal directions | Point the vehicle, read `Ya-`/heading | **Not takeable on this part** — there is no absolute heading to compare | N/A |
| 8.15 | SD log vs received telemetry | SD complete, radio may have gaps | Diff the two after a run | | |

> **The delivered IMU is an MPU-6500, so 8.9, 8.10, 8.11, 8.13 and 8.14 cannot be taken on
> this vehicle.** `WHO_AM_I` returned `0x70` on 2026-09-05 and `0x0C` never appears after the
> pass-through bridge is enabled: there is no magnetometer in the package. The notes below
> describe what those rows would prove, and stand for the day a genuine nine-axis part is
> fitted — see [F-1](../hardware/receiving-inspection.md#findings).
>
> **What it means for the flight.** Yaw is gyro-integrated, so it drifts without bound, and
> the packets will always say `YR-G`. Roll and pitch are unaffected: they are referenced to
> gravity through the accelerometer, which measured well at Gate 3. Altitude, pressure,
> acceleration, rates and GPS are all untouched. **8.12 becomes the only yaw row**, and it
> characterises the vehicle rather than testing it.
>
> The firmware was written for exactly this: it accepts `0x70` as a six-axis part and reports
> degraded attitude instead of refusing to boot, or worse, inventing a heading from a bus that
> is not answering. That decision is now vindicated on hardware rather than in review.

> 8.10 and 8.11 are the tests that decide whether this vehicle can claim an absolute
> heading. Hard and soft iron are properties of the **assembled airframe** — battery,
> radio, wiring included — so the sweep must be done on the finished vehicle, not on a bare
> breakout, and repeated whenever the layout changes.
>
> 8.12 and 8.13 are the same measurement either side of that calibration, and the pair is
> the evidence. Uncalibrated yaw is expected to drift and the packets say `YR-G`; calibrated
> yaw is expected to hold and the packets say `YR-M`. If 8.13 still drifts, the
> magnetometer is not being believed — check the field magnitude against the 20–70 µT gate
> before suspecting the filter.
>
> 8.14 is the one that catches a frame or sign error. A heading that is mirrored, offset by
> 90°, or that turns the wrong way as the vehicle rotates points at the magnetometer axis
> mapping, not at the calibration.

---

## Gate 9 · Endurance and recovery

| # | Quantity | Predicted | How to measure | Measured | Verdict |
|---|---|---|---|---|---|
| 9.1 | Continuous run without fault | Hours | Leave it running, watch `FAULTS-` | | |
| 9.2 | Watchdog recovery | Reboots and telemetry resumes automatically | Force a hang, observe | | |
| 9.3 | Fault recorded after a watchdog reboot | `watchdog_reboot` present | Health snapshot after 9.2 | | |
| 9.4 | GPS unplugged mid-run | Telemetry continues without GPS fields | Unplug, watch | | |
| 9.5 | SD removed mid-run | Telemetry continues; SD logging disables after 10 failures | Remove, watch | | |
| 9.6 | Barometer disconnected | Packets suppressed, loop alive, resumes on reconnect | Disconnect, watch, reconnect | | |
| 9.7 | Ground PC closed mid-run | Bridge keeps running; reconnect resumes | Close the app, reopen | | |
| 9.8 | Post-impact transmission | ≥ 5 s after impact detection | Drop test, count packets after landing | | |

> Gate 9 is the failure philosophy in [software-architecture.md](../design/software-architecture.md)
> tested rather than asserted. Every row here has a host test behind it; this is where those
> tests meet real hardware.

---

## Findings

Anything that did not match its prediction. One line each: what was expected, what was
measured, what you did about it.

| # | Gate | Expected | Measured | Action taken |
|---|---|---|---|---|
| 1 | 3 · 8 | An MPU-9250: `WHO_AM_I` `0x71`/`0x73`, and an AK8963 answering at `0x0C` once `BYPASS_EN` is set | **`WHO_AM_I` `0x70` — an MPU-6500. Six axes, no magnetometer.** `0x0C` never appears, in either scan, with or without the bypass | Recorded as [F-1](../hardware/receiving-inspection.md#findings). The firmware accepts `0x70` as a six-axis part rather than refusing to boot, and the estimator degrades instead of stopping. **The vehicle has no absolute yaw reference:** yaw is gyro-integrated and drifts, and telemetry reports `YR-G`, never `YR-M`. Rows 8.9–8.11, 8.13 and 8.14 are not takeable on this part |
| 2 | 3 | Barometer output rate ≈ 83 Hz | The first procedure counted *changed values* and reported 44 Hz, then 46.5 Hz on a re-run — a 2× undercount, not a slow part | The procedure was the defect, not the sensor. Row 3.5 now counts falling edges of `STATUS.measuring` and reads **83.0 Hz**, matching the 83.3 Hz prediction with 2.8× margin over the 30 Hz acquisition rate |
| 3 | — | A working multimeter | The resistance range read ~51 Ω between *every* pair of pins, including two with no path between them, and 18 Ω across its own shorted probe tips | **None of those numbers were recorded as measurements.** They describe the instrument, and it is written up in the Part A tools table instead. `C.3.6`/`C.3.7` and `C.4.5`/`C.4.6` stay blank until a working resistance range is available; the Gate 3 bus scan answers the same question from the address each device actually replies at |
| 4 | 4 | GPS NMEA checksum errors ≈ 0 | **0** over an 18 s indoor run, without a fix | Accepted as provisional (row 4.3, ⚠️). Sentences without a fix carry empty fields and are shorter than the ones that matter; the row is to be re-taken with a fix before it counts |
| 5 | 5 | A radio that either works or does not | **The same 206-byte transmit failed 5/5 in row 5.2 and then succeeded 45/45 in row 5.4a, minutes later on one power cycle** — same code, same length, same antenna. The previous power cycle had passed 5.2 and 5.3 outright. At the failures `IRQ_FLAGS = 0x00`: `TxDone` was never set, so the packets did not merely go unreported, they did not complete | **Closed by [F-10](#findings): the RA-02's 3V3 jumper.** Shortening it, exactly as the microSD's had been, gave 5/5 and 5/5 on rows 5.2 and 5.3 and a clean Gate 7. The same fault on the same kind of wire on both modules — a supply that carries an idle part and not a working one. The `RegOpMode`/`RegVersion` capture added while chasing it stays: it is what would have named this in one run instead of five |
| 7 | 6 | A status byte naming why a write was refused | **`R1 = 0xFF`, `R2 = 0xFF`** — and the diagnostic decoded them bit by bit into *write-protect violation, card locked, CC error, ECC failed, out of range*: five unrelated catastrophes at once. **`0xFF` is not a status byte.** Every valid R1 has bit 7 clear; `0xFF` is MISO idling high with nothing driving it | The card had **stopped answering**, not refused. The decode has been corrected to say so before touching a bit — the guard the init path had from the start and the write path never inherited. The reported `data token = 0xE5` was also stale, left by an earlier attempt the write never reached; the capture is cleared per attempt now and a host test holds it. **The write-protect reading was wrong and would have condemned a working card** |
| 8 | 6 | A card that reads is a card that writes | **The card initialised, read its BPB, FAT and directory, then went silent on the very first `CMD24` and stayed silent — the read-back after also failed.** A fresh `SdCard::begin()` in row 6.6 seconds later brought it straight back, boot count incrementing normally | Not closed. A card that is damaged stays damaged; this one recovers on re-initialisation, so it is dropping out rather than failing. `CMD24` is the first moment in the run that the card draws programming current, which points at its supply rather than at the card or the bus. Row 6.3 now re-initialises after a total write failure and reports whether it comes back, so the next run says this outright instead of leaving it to be inferred |
| 9 | 5 · 6 | The radio dragging the rail down would explain the card dropping out | **With the RA-02's VCC wire physically removed, the card still went silent on the first `CMD24` and still came back on a fresh init.** Confirmed by the run of 2026-09-05 with `boot_count = 5` | The radio is cleared of [F-8](#findings). Two faults, not one. The radio's own `0x00` is separately unexplained and confined to NSS (GP17), RESET (GP20) and its VCC/GND pins — the card proves the shared lines carry data |
| 10 | 6 | A card that reads is a card that writes | **The 3V3 jumper from the Pico to the microSD reader.** With a long jumper the card initialised, read its BPB, FAT and directory, then went silent on the very first `CMD24` and stayed silent until re-initialised — five runs in a row. **Replacing it with a short one fixed it outright: 100/100 written, then 3672 in ten seconds.** Nothing else changed | Closed. The reader is a 3.3 V board with no regulator and two capacitors ([F-3](../hardware/receiving-inspection.md#findings)), so the write-current spike arrives down whatever the supply jumper can deliver, and reads never draw enough to expose it. **On the soldered board this becomes a short track and a 470 µF bulk capacitor across the module's own 3V3 and GND**, which is now a requirement rather than a precaution |
| 11 | 6 · 8 | Write latency small against the 33 ms sensor period | **29.756 ms worst case**, against a `sensor_period_ms` of 33. The mean is 6.160 ms and the sustained rate is 2.7 ms/write, so this is an occasional stall — the card's own housekeeping — not the typical cost | Open, and it needs a row. `RawBlockLog` writes the record block **and** rewrites its header, so one telemetry append is **two** writes: a bad second can block the loop for ~60 ms, close to two whole sensor periods. Row 3.8 measured 0.453 ms of loop jitter **with no SD logging running**, so the figure that matters has not been taken. Gate 8 needs a jitter measurement with the logger active before this is called safe. **Not closed by [F-10](#findings):** the ~30 ms was seen once in three sessions and the other two gave ~5 ms, so it is rare rather than absent, and a rare 60 ms stall is exactly the kind that is not found on a bench |
| 6 | 6 · 7 | Nothing — this row exists because the previous run's failure vanished | **The previous power cycle failed every write (row 6.6 `logger init FAILED`, row 7.3 30 write failures) while all 200 reads passed. The next power cycle, with an unchanged write path, wrote 100/100 in 6.3, 2667 in 6.3b and 30/30 in 7.3** | **Closed by [F-10](#findings).** The write path was never changed, only instrumented, so the fault was always outside the code — and it was the microSD's 3V3 jumper. That also explains why it appeared to come and go: the same marginal wire delivers enough on one seating and not the next. [F-5](#findings) turned out to be the identical fault on the RA-02 |

When a finding changes a constant, change it in the source, re-run
`bash tools/build_host.sh` — which will tell you if a document now contradicts it — and add
a row to [CHANGELOG.md](../../CHANGELOG.md).

---

## Sign-off

| Gate | Completed by | Date | All rows passed? |
|---|---|---|---|
| 1 · Bare Pico | | | |
| 2 · Power | | | |
| 3 · Sensors | | | |
| 4 · GPS | | | |
| 5 · Radio | | | |
| 6 · Storage | | | |
| 7 · Shared bus | | | |
| 8 · End to end | | | |
| 9 · Endurance | | | |

**No gate may be signed off with a blank row.** Either measure it or write down why it was
skipped — an unmeasured row that looks measured is exactly the kind of thing this document
exists to prevent.

---

## Related documents

- [Test Plan](test-plan.md) — the pass/fail matrix these measurements feed
- [Bring-up order](../design/wiring.md#bring-up-order) — the sequence to work in
- [Link Budget](../design/link-budget.md) — where the radio predictions come from
- [Sensor Rates](../design/sensor-rates.md) — where the sensor predictions come from
- [Runbook](../operations/runbook.md) — operating the ground station while you measure
- [Quick Start](../quick-start.md) — if you have not built the vehicle yet
