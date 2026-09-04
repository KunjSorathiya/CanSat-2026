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
> stage the earlier design reserved is not to be built. What is still needed is a load budget
> the regulator can be chosen against — and the number nobody has is the **microSD
> write-transient current**, which no photograph and no datasheet can supply. Measure it at
> Gate 6 and bring it back here.

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
| 3.1 | I2C devices found | 2, at 0x68 and 0x76 | [wiring.md](../design/wiring.md) | Bus scan | | |
| 3.2 | Stationary acceleration magnitude | 9.81 ± 0.15 m/s² | `calib_accel_tol_mps2` | Read 100 samples, take the mean | | |
| 3.3 | Stationary gyro bias, per axis | Within ±25 dps, typically < 5 | `calib_max_gyro_bias_dps` | Mean of 100 still samples | | |
| 3.4 | Gyro noise, per axis | < 2 dps standard deviation | `calib_gyro_still_dps` | Standard deviation of the same samples | | |
| 3.5 | Barometer output rate | **83 Hz** typical | [sensor-rates.md](../design/sensor-rates.md) | Poll continuously; count changed pressure values per second | | |
| 3.6 | Pressure vs a local reference | Within a few hundred Pa | — | Compare with a weather station or second barometer | | |
| 3.7 | Achieved acquisition rate | **30 Hz** (33 ms period) | `sensor_period_ms` | Log mission time between sensor ticks | | |
| 3.8 | Acquisition jitter | < 6 % of the period (2 ms) | 2 ms loop tick | Standard deviation of the same intervals | | |
| 3.9 | Calibration settle time | Within `calib_samples` at 30 Hz ≈ 2.7 s | `startup_calibration.cpp` | Time from power-on to `CAL-1` in telemetry | | |
| 3.10 | I2C bus utilisation | ~1.6 % at 30 Hz | [sensor-rates.md](../design/sensor-rates.md) | Scope SCL, measure active time per second | | |

> 3.5 and 3.7 are the two that matter most. If the barometer is slower than the loop, the
> vertical-speed estimate degrades — the firmware detects and handles it, but the
> configuration should be corrected rather than relied on to degrade gracefully.

---

## Gate 4 · GPS

| # | Quantity | Predicted | How to measure | Measured | Verdict |
|---|---|---|---|---|---|
| 4.1 | Raw NMEA arrives | Sentences at 9600 baud | Serial monitor on the UART | | |
| 4.2 | Time to first fix, cold, outdoors | Minutes | Stopwatch from power-on to `GP-Lat` appearing | | |
| 4.3 | NMEA checksum errors | ≈ 0 | `gps_checksum_errors` in the health snapshot | | |
| 4.4 | Position accuracy | — | Compare with a known surveyed point or a phone | | |
| 4.5 | Fix held while the radio transmits | No dropouts | Watch the fix flag through 50 transmissions | | |

> 4.5 is worth its own row: the GPS and the radio have not shared a vehicle before, and RF
> desensitisation of a GPS front end by a nearby 433 MHz transmitter is a real effect.

---

## Gate 5 · Radio

**The most valuable gate in this document.** Everything about the telemetry rate rests on
computed airtime that has never been observed.

| # | Quantity | Predicted | Source | How to measure | Measured | Verdict |
|---|---|---|---|---|---|---|
| 5.1 | RA-02 version register | 0x12 | `sx1278.cpp` | Read register 0x42 over SPI | | |
| 5.2 | Airtime, 206-byte packet, SF7/125 kHz | **328 ms** | [link-budget.md](../design/link-budget.md) | Scope DIO0, TX start to TxDone | | |
| 5.3 | Airtime, full 255-byte packet | **400 ms** | Same | Same, with a padded packet | | |
| 5.4 | Achieved telemetry rate | **1.00 Hz** | `telemetry_period_ms` | Packet numbers per second at the ground station | | |
| 5.5 | Channel occupancy at 1 Hz | ~33 % typical, 40 % worst case | [link-budget.md](../design/link-budget.md) | 5.2 ÷ 1000 ms | | |
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

| # | Quantity | Predicted | How to measure | Measured | Verdict |
|---|---|---|---|---|---|
| 6.1 | Card initialises | CMD0/CMD8/ACMD41 succeed | `sd_ok` in the health snapshot | | |
| 6.2 | Card type detected | SDHC (block-addressed) for any modern card | `high_capacity()` | | |
| 6.3 | Single block write time | — | Time 100 `write_block` calls | | |
| 6.4 | Records written per telemetry packet | 2 (record + header) | Count blocks after N packets | | |
| 6.5 | Log survives a power cut | Resumes at the right block, no data lost | Pull power mid-flight-test, reboot, read back | | |
| 6.6 | Boot count increments | +1 per power session | `boot_count()` | | |
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
| 7.1 | Radio works with the card present but idle | No change from Gate 5 | Repeat 5.1 and 5.4 | | |
| 7.2 | Card works with the radio present but idle | No change from Gate 6 | Repeat 6.1 and 6.3 | | |
| 7.3 | Radio works *during* card writes | No lost packets | Run both at full rate for 5 minutes | | |
| 7.4 | MISO released when each device is deselected | Line goes high-impedance | Scope MISO during the other device's transaction | | |
| 7.5 | SPI clock after an SD transfer | 4 MHz, unchanged for the radio | Scope SCK during a radio transaction | | |

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
| 8.8 | `WHO_AM_I` of the IMU | `0x71` or `0x73` | Read at initialisation; the health report carries it | | |
| 8.9 | Magnetometer present and answering | AK8963 found at `0x0C`, `mag_ok` true | Health snapshot | | |
| 8.10 | Total magnetic field, vehicle assembled | 25–65 µT, and stable as the vehicle is moved | `mag_field_ut` in the health snapshot | | |
| 8.11 | Magnetometer calibration sweep | Every axis spans ≥ 30 µT; calibration accepted | Figure-of-eight with `mag_cal_in_flight` set; watch `mag_cal_span_ut` | | |
| 8.12 | Yaw drift, stationary, 10 minutes, uncalibrated | Drifts: no magnetic reference is being applied | Record `Ya-` with `YR-G` in the packets | | |
| 8.13 | Yaw drift, stationary, 10 minutes, calibrated | Holds: bounded by magnetometer noise, not integrating | Record `Ya-` with `YR-M` in the packets | | |
| 8.14 | Yaw against a known bearing | Within a few degrees of a hand compass, four cardinal directions | Point the vehicle, read `Ya-`/heading | | |
| 8.15 | SD log vs received telemetry | SD complete, radio may have gaps | Diff the two after a run | | |

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
| 1 | | | | |
| 2 | | | | |
| 3 | | | | |
| 4 | | | | |
| 5 | | | | |

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
