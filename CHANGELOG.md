# Changelog

All notable changes to the CanSat 2026 project.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). This project has
no released versions — it tracks a competition build, so entries are grouped by
development cycle.

---

## [Unreleased] — 2026-09-05 (cycle 33)

### Verified — headers fitted, and six Part C rows closed on the bench

Headers were bought separately and soldered to both Picos, both sensor breakouts, the
microSD reader, the GPS and the RA-02 carriers on 2026-09-04. Joints were inspected and
adjacent-pin isolation checked before anything saw power. `F-6` is closed, and `C.1.5` now
records both what arrived and what was done to it.

- **Mounting the vehicle Pico flat to the prototype board is no longer available.** Both
  Picos carry headers, so the height budget must accommodate the stack. Socketed against
  soldered-through is still open, and it is a vibration question: a socket can walk loose
  under launch loads.
- **microSD supply confirmed direct (`C.6.9`).** The `3V3` header pin reaches exactly one
  socket leg, `GND` a different single leg, nothing else responds. Nothing sits between the
  header and the card, which is what the absent regulator implied and now no longer assumes.
- **RF pigtail proved good (`C.7.6`, `C.7.7`).** Centre-to-centre and shield-to-shield both
  continuous, centre-to-shield open at both ends. The radio will not be asked to drive a
  shorted line.
- **Battery characterised (`C.8.3`, `C.8.5`).** Red is positive, read on the meter rather
  than taken from the insulation colour, and the pack is at **3.92 V** open-circuit — a
  normal storage voltage, well clear of the ~3.0 V set-aside threshold.
- **The prototype board has no rail anywhere (`C.9.5`).** Adjacent pads are isolated, and so
  are the elongated pads along the top and bottom edges — the one place a ready-made bus
  could have been hiding. Both the 3.3 V and GND runs are hand-built with no exceptions.

`D.4` moves from "the least advanced of the four" to blocked on procurement rather than on
measurement: a 1S balance charger and a mating JST-RCY pigtail are what stand between this
pack and a powered test.

### Recorded — a faulty multimeter, and the readings it produced

The meter's resistance range is broken. It read ~51 Ω between *every* pair of pins on the
MPU-9250 — including `FSYNC` to `GND`, which have no path between them — and 18 Ω across its
own shorted probe tips, climbing steadily from zero on a fresh battery.

Those numbers describe the instrument, not the board. **None of them are recorded in Part C,
and the MPU-9250 is not implicated by them.** The episode is written into the Part A tools
table instead, because an instrument that produces confident wrong numbers is a hazard to
the record and the next person to pick this up deserves to know it happened.

Continuity and DC volts were separately verified working and carried the six rows above.
`C.3.6`/`C.3.7` and `C.4.5`/`C.4.6` — the AD0 and SDO straps — need a true resistance range
and **stay blank**. They are not on the critical path: the Gate 3 bus scan answers the same
question from the address a device actually replies at, which is better evidence than a
strap measurement.

### Fixed — the documented test counts were four runs out of date, and nothing was checking them

Every figure below was true when it was written and had quietly stopped being true. The
README badge advertised 1351 C++ assertions against an actual 4222; the test plan described
41 flight-core suites where the source calls 57, and one of them — `test_shared_protocol_fixtures`,
the suite that holds the C++, Python and Node parsers to a single fixture file — had no row
at all.

| Claim | Documented | Actual |
|---|---:|---:|
| C++ assertions (README badge, quick start) | 1351 | **4222** |
| `flight_tests` suites / assertions | 41 / 676 | **57 / 3547** |
| Python tests | 126 | **131** |
| Node tests | 30 / 36 | **37** |
| Claims checked by `check_doc_claims.py` | 61 | **82** |

**The counts are now a build gate, not a promise.** `tools/build_host.sh` tees what every
suite reports about itself to `build/host/test-output.log`, and `tools/check_doc_claims.py`
reads that log and holds the README, the quick start, the test plan, the architecture
document and the timeline to those numbers. Assertion totals cannot be counted statically —
a table-driven test runs one `CHECK` many times — so the only honest source for them is the
suites' own output on the run that just happened.

Two structural changes follow from that. The doc-claims check moved to the end of
`build_host.sh`, after the Node suite, because it now reads a log the Node suite writes to.
And every suite `flight_tests` calls must have a row in the test plan explaining what it
proves, which is what surfaced the missing one: a test nobody documented is a test nobody
can explain when it fails.

Run standalone, without the log, the script skips the count checks and reports 69/69 rather
than inventing a number.

### Added — Gate 6 on the diagnostic, with the destructive half behind a prompt

`cansat_bringup_firmware` now brings up the microSD reader — the item
`sd-module-analysis.md` calls the highest-risk in the BOM.

- **6.1** runs the full `CMD0/CMD8/ACMD41/CMD58/CMD16` sequence and reports it. On failure it
  points at the friction-fit holder first, because a card can sit in that socket looking
  seated without making contact.
- **6.2** reports block- against byte-addressing from `high_capacity()`, which is what
  settles SDHC rather than the capacity printed on the card.
- **6.3** times 100 single-block writes, **then reads the last one back and compares it**.
  A write that reports success without landing is the failure worth catching: the log would
  look healthy all the way to a card with nothing on it.
- **6.6** reports `boot_count()` and `record_count()` through two new `PicoSdLogger`
  accessors, mirroring `PicoImu::who_am_i()` and `PicoRadio::chip_version()`.

**The write test destroys the filesystem, and says so before it runs.** The log starts at
LBA 2048 — exactly where a FAT32 partition begins — so afterwards the card will not mount on
a PC until reformatted. That is the layout the vehicle flies; it is not a fault. No hardware
is at risk, unlike the radio's antenna warning, but destroying data silently is its own kind
of failure, so it sits behind a `w` prompt. 6.1 and 6.2 are read-only and run unprompted.

**What Gate 2 still needs from Gate 6 is a current, and firmware cannot measure it.** The
diagnostic reports write *duration*, which gives the transient's length. Its magnitude needs
a meter in series with the module's `3V3` lead — and it is the last number standing between
this project and a chosen regulator.

### Verified — the airtime model holds on a real radio, to within 1.8 %

The RA-02 answered `0x12` on the version register, and then transmitted.

| Row | Predicted | Measured | |
|---|---:|---:|---|
| 5.2 · 206-byte packet | 327.9 ms | **333.7 ms** | +1.7 % |
| 5.3 · 255-byte packet | 399.6 ms | **406.9 ms** | +1.8 % |
| 5.5 · channel occupancy | ~33 % / 40 % | **33.4 % / 40.7 %** | |

Five of five sent in both cases, no TxDone timeouts. The excess grows slightly with payload,
which is what filling a larger FIFO over SPI looks like — overhead in the only direction it
can move.

**This is the most load-bearing number in the repository.** The 1 Hz telemetry rate, the SF7
spreading-factor choice, the channel-occupancy budget and the build-time `static_assert` that
refuses a profile which cannot meet 1 Hz all rest on `lora_time_on_air_ms()`. Until today that
function was verified only against published reference vectors — rigorous, but
self-consistent. It has now been asked of a radio, and `link-budget.md` loses its
"never measured on a radio" entry.

**An unplanned Gate 2 data point came free.** Ten transmits at +17 dBm ran off the Pico's own
3V3 regulator with no reset and no USB dropout. That is not the flight power tree, which still
has no regulator chosen, but it says the RA-02's transmit transient does not brown out a
300 mA-class source — useful when sizing one.

### Added — Gate 5 on the diagnostic, with the transmit tests behind a prompt

`cansat_bringup_firmware` now reads the RA-02's version register and can time its airtime.

- **5.1** reads register `0x42` and interprets it. `0x12` is the SX1276/77/78 family; `0x00`
  and `0xFF` are called out as **SPI failures rather than wrong answers**, because that is
  what an unresponsive bus reads as and it is the difference between a wiring fault and a
  dead modem.
- **5.2 and 5.3** time five transmits each at 206 and 255 bytes and compare the mean against
  `lora_time_on_air_ms()` — the same model the link budget and the build-time `static_assert`
  use, called at run time so the diagnostic and the design cannot drift apart.

**The transmit tests will not run without someone pressing `t`.** Transmitting into an
unterminated port reflects the whole output back into the power amplifier, and this is the
only action in the entire diagnostic that can destroy hardware rather than merely fail.
Skipping the prompt leaves 5.2 and 5.3 open, which is the right trade.

`PicoRadio` gains a `chip_version()` accessor, mirroring `PicoImu::who_am_i()` and for the
same reason: the identity a part reports about itself is worth having on the bench.

### Verified — the GPS runs on 3.3 V, and C.5.5 is closed by operation

The NEO-6M was powered from the Pico's own 3V3 output on 2026-09-05 and produced clean,
well-formed NMEA at 9600 baud within seconds: `$GPRMC`, `$GPVTG`, `$GPGGA`, `$GPGSA`,
`$GPGSV`, `$GPGLL`, one full cycle per second. A module browning out below its 2.7 V floor
does not emit correct sentences at the right baud rate for minutes on end.

**It proved the antenna path too**, which no supply test had to. Within five seconds
`$GPGSV,1,1,01,04,,,28` reported one satellite in view — PRN 04 at 28 dB-Hz — from indoors.
The receiver is not merely talking, it is hearing.

`C.5.5` is closed **by observation, not by the listing** that claimed the same thing. `C.5.4`
stays open and is now only a curiosity: the regulator's part number is still unread, but its
rated input range only mattered while it decided whether the module could take 3.3 V at all.

**Every module's supply is now confirmed 3.3 V** — the RA-02 and microSD by inspection, the
BMP280 by the absence of a regulator, the IMU and GPS by operation. **What Gate 2 still needs
is current, not volts.**

Two smaller facts worth having: the receiver delivers **1 Hz, not the 5 Hz the listing
advertised** — that is a capability reached by sending a UBX message, and nothing in this
firmware sends one. And the line carries **164 B/s** against 960 B/s capacity, so the
RP2040's 32-byte FIFO fills in roughly 195 ms rather than the ~33 ms `gps_uart_fifo_bytes`
assumes at full line rate. The flight loop's tick has more room than the worst case it was
sized against.

### Verified — the barometer runs at 83.0 Hz, exactly as computed

`STATUS.measuring` falling edges: **166 in 2 s → 83.0 Hz**, against the 83.3 Hz
`sensor-rates.md` derived from the datasheet's timing formulas. The status register was
polled at 9.1 kHz, 110× the output rate, so no completion could have been missed between
polls.

Two long-standing "never measured" entries in `sensor-rates.md` are now measured: the
barometer's output rate, and the achieved loop rate at **30.04 Hz**. **The 2.8× margin over
the 30 Hz acquisition rate is confirmed rather than assumed**, which is what the whole
oversampling argument in that document rested on.

### Fixed — 3.5 was measuring the wrong thing, and reported 44 Hz for it

The first run of the barometer rate test returned 44 Hz against a predicted 83, which looked
like a sensor finding. It was a method finding.

Counting *changed* pressure values undercounts on this part. With the IIR filter at x16 the
BMP280 deliberately moves its output slowly, so consecutive conversions frequently produce
the **same** compensated value. Distinct-value counting therefore measures how often the
reading moves, not how often the part converts — a different question from the one 3.5 asks,
and the wrong one.

The diagnostic now also counts falling edges of **`STATUS.measuring`** (register `0xF3`, bit
3). Each 1 → 0 transition is one completed conversion whether or not the result changed,
which is the output rate as the datasheet defines it. Both methods are printed, with A
labelled a lower bound.

Both methods now print, so the gap stays visible: the same run that reported **83.0 Hz** by
edge counting reported **46.5 Hz** by distinct values. The sensor was right, the datasheet was
right, and the instrument was wrong — the same shape of error as the multimeter that invented
a short across the MPU-9250 two days earlier. A measurement that disagrees with a prediction
is not automatically a finding about the hardware.

### Verified — Gate 3 timing, on hardware

- **3.7** mean interval **33.289 ms → 30.04 Hz** over 150 ticks, against a configured 33 ms.
- **3.8** interval sd **0.453 ms** against a 1.98 ms limit — 4.4× inside.
- Sensor read cost **0.282 ms mean, 0.328 ms worst**: under 1 % of the period. That is the
  number that bounds the flight loop, and it has an enormous amount of room. Measured with
  the barometer only, so it will rise once the IMU shares the bus.

### Added — the diagnostic now covers the rate rows and Gate 4

`cansat_bringup_firmware` gains four measurements it could not take before:

- **3.5, barometer output rate**, by counting *changed* pressure values over a fixed window
  rather than counting reads. Polling faster than the part converts returns the same bytes
  again, so counting reads would report the poll rate and call it the output rate. When
  nearly every poll changes, the figure is reported as a lower bound rather than a
  measurement, because the sensor is then faster than the loop asking it.
- **3.7 and 3.8, acquisition interval and jitter**, plus the sensor read time inside each
  tick. The read time is the number that matters: it is the part of the period the flight
  loop cannot spend on anything else. Reported explicitly as the diagnostic's own loop, not
  `controller.cpp`'s scheduler — it bounds the flight loop rather than describing it.
- **4.1, raw NMEA**, echoed verbatim for five seconds. Deliberately not the parser's opinion:
  a wrong baud rate produces a steady stream of plausible-looking garbage, and only looking
  at the characters distinguishes that from silence or from real sentences. Silence prints a
  pointer to C.5.5 rather than to the wiring, because the unverified supply is the first
  suspect on this board.
- **4.2 and 4.3**, fix status, satellite count, time to first fix and NMEA checksum errors,
  carried in the live line.

The live loop now drains the GPS UART every 5 ms instead of sleeping through the half-second
between prints. The RP2040's UART FIFO is 32 bytes and 9600 baud fills it in about 33 ms, so
a 2 Hz poll would overrun it and lose sentences mid-line. That is the same reasoning
`gps_uart_fifo_bytes` uses to size the flight loop's tick.

Any subset of the hardware may be connected — absent devices are reported and skipped, never
fatal. That is what makes one-sensor-at-a-time bring-up practical without a breadboard.

### Verified — the prototype board is 1.6 mm, and Part C is complete

The last blank row. 1.6 mm is the standard FR-4 thickness, so ordinary M2 and M3 standoffs
and spacers fit, and the mass budget can use the usual figure for a 100 × 100 mm
single-sided board rather than an estimate.

**Every row in Part C is now either measured or carries a written reason it could not be.**

### Verified — the barometer is a BMP280, and Part C's last four straps are closed

Chip ID register `0xD0` returned **`0x58`** on 2026-09-05: a BMP280, not a BME280. `F-4` is
closed, and closed by the register read `C.4.1` explicitly deferred to rather than by the
package measurement that stood in for it.

**The geometric identification held.** `C.4.1` identified this part by measuring its lid at
2.04 × 2.50 mm — an aspect ratio of 0.82 against a BME280's 1.00 — and the register agrees.
That is worth putting beside `F-1`, where reading four characters of marginal laser text off
the same class of photograph produced the wrong answer. **Measuring a shape the camera
resolves beat reading text it did not.**

The barometer also reads sensibly: ~100 822 Pa and 33.9 °C, drifting by about 7 Pa peak to
peak, which is roughly 0.55 m of altitude noise. Plausible values mean the Bosch fixed-point
compensation and the calibration coefficient read are both working — a broken compensation
does not produce numbers this ordinary.

**All four strap rows are now closed, and none of them needed a meter.** `C.3.6`/`C.3.7` and
`C.4.5`/`C.4.6` were answered by the address each part replies at on a live bus: `0x68`
requires AD0 low, `0x76` requires SDO low. Both match the firmware defaults, so no config
change is needed. That is better evidence than a resistance reading — the strap's effect
rather than its cause, measured through the same bus the firmware will use.

### Finding — the IMU is an MPU-6500. This vehicle has no magnetometer

`WHO_AM_I` returned **`0x70`** on 2026-09-05. That is an MPU-6500: pin-compatible with the
MPU-9250, electrically identical for the accelerometer and gyroscope, and **with no
magnetometer in the package at all**. The second bus scan confirms it — `0x0C` never appears
after `INT_PIN_CFG.BYPASS_EN` is set, because there is nothing behind the bridge to answer.

**This overturns C.3.2**, which read the die as `MP92` — the MPU-9250 marking — from a
photograph and concluded the part was genuinely nine-axis. The row carried the right caveat
("a photograph of a package is not a register read") and the wrong conclusion. Whether the
die text was misread at that resolution or the die is remarked cannot be settled from here,
and does not matter: the silicon answers `0x70`, and what the silicon answers is what flies.

**What it costs.** Yaw is gyro-integrated, so it drifts without bound, and telemetry will
always report `YR-G` — `YR-M` will never appear from this vehicle. Bring-up rows 8.9, 8.10,
8.11, 8.13 and 8.14 are not takeable and are marked N/A with the reason; 8.12 becomes the
only yaw row. Roll and pitch are unaffected, being referenced to gravity through the
accelerometer. Altitude, pressure, acceleration, rates and GPS are untouched.

**What it vindicates.** The driver accepts `0x70` as a six-axis part and reports degraded
attitude rather than refusing to boot or inventing a heading from a bus that is not
answering. That was argued for in review when the nine-axis rework landed; it is now the only
reason this vehicle runs at all.

**Nothing is wrong with the part that arrived.** It is simply not the part the listing
described, and one of its three sensors does not exist.

### Verified — Gate 3 accelerometer and gyroscope, on hardware

Measured with `cansat_bringup_firmware`, 100 of 100 samples valid:

- **3.2** |a| mean **9.8675 m/s²**, sd 0.0092 — under 1 % scale error against true g.
- **3.3** gyro bias X −3.3878, Y +0.9079, Z −0.4720 dps — all inside ±25, and inside the
  datasheet's ±5 zero-rate figure. Startup calibration removes them.
- **3.4** gyro noise 0.0984, 0.0964, 0.1424 dps sd — fourteen to twenty times inside the
  2 dps limit.
- **3.1** partial: `0x68` answers, AD0 low, matching the firmware default. The BMP280 is not
  yet wired.

The inertial half of this vehicle measures well. The diagnostic also now says so out loud
when no barometer answers, rather than printing an empty block that reads like a broken tool.

### Added — a bring-up diagnostic image, because the vehicle cannot talk

Gate 3's first row asks for an I2C bus scan. There was no tool to run one, and no path for
its answer to reach a human: the flight firmware speaks only over LoRa and writes nothing to
USB, so a vehicle with sensors wired and no radio attached produces no observable at all.
Wiring the sensors up would have bought a blinking nothing.

`cansat_bringup_firmware` is a third image that prints over USB CDC:

- **Two bus scans**, before and after IMU initialisation. The AK8963 at `0x0C` must be absent
  from the first and present in the second — it sits behind the MPU's pass-through bridge and
  does not answer the outside bus until `INT_PIN_CFG.BYPASS_EN` is set. The difference is the
  check. `0x0C` in both would mean something else is at that address.
- **The barometer's chip ID** from register `0xD0`. `C.4.1` identified the delivered part by
  measuring its package in a photograph and explicitly deferred to this register; `0x58` or
  `0x60` settles `F-4` properly.
- **The IMU's `WHO_AM_I`**, which settles `F-1`. `0x71`/`0x73` is a real nine-axis part;
  `0x70` is an MPU-6500 with no magnetometer in the package at all.
- **100 stationary samples** reduced to the mean and standard deviation that rows 3.2–3.4
  ask for, each printed against its limit and marked PASS or OUT OF RANGE. The limits are
  read from `Configuration` at run time, so they cannot drift away from what the firmware
  actually enforces.

It drives the same `mpu9250.cpp` and `bmp280.cpp` the vehicle flies. A diagnostic built on
its own copy of the drivers can pass while the flight build fails, which is worse than having
no diagnostic. It is a **separate executable and never linked into the flight image** — the
launch build carries no debug output and no flag that could enable any. On the vehicle the
two are told apart at a glance: the bring-up image holds the status LED solid and never
blinks.

`tools/check_pico_syntax.sh` covers it, with a new `pico/stdio_usb.h` stub.

### Fixed — bring-up row 3.2 predicted a tolerance ten times tighter than the firmware's

Row 3.2 read "9.81 ± 0.15 m/s²" and cited `calib_accel_tol_mps2` as its source. That constant
is **1.5**, not 0.15. A board reading 9.6 m/s² would have failed the row as written while
passing the gate the firmware actually enforces.

The row now states the firmware's real limit, and separately that a healthy part sitting
still should be an order of magnitude tighter than it — which is the useful bench
expectation, and was probably what 0.15 was reaching for. The diagnostic prints against the
config value rather than a literal, so the two cannot diverge again.

### Changed — Gate 2 is blocked on one thing, not two

The bring-up record still said Gate 2 was held up by "no regulator selected, and the microSD
reader's supply requirement unresolved". The second half stopped being true on 2026-09-04,
when `D.1` established that the delivered reader has no regulator, no level shifter and a
supply pin printed `3V3`.

The gate now names its single real blocker — regulator selection — and records what the
closed question bought: one 3.3 V rail rather than two, and no boost stage. What it still
needs is a load budget, and the missing number is the **microSD write-transient current**,
which no photograph and no datasheet can supply. Measured at Gate 6, brought back to Gate 2.

Part A is also counted and complete: **every line matches the quantity ordered**, the five
items ordered in pairs included. The microSD card is 32 GB — the top of the SDHC range, so
Gate 6.2's block-addressed prediction should hold, though `high_capacity()` is what settles
it rather than the number printed on the card.

### Verified — the first firmware this project has ever run on hardware

Both Picos are flashed and labelled. Gate 1 is partly measured, and for the first time a
row in the bring-up record contains a number taken from a running board rather than a
prediction.

- **1.4 USB serial enumerates** — both boards enumerate; the bridge came up as `COM4`.
- **1.6 Bridge status cadence** — the `#state=RX` line arrives at a steady 1 Hz with no
  gaps, matching `STATUS_PERIOD_MS`. A starved 3 s watchdog would show as a gap and a
  restart; there is neither.
- **1.7 USB frame integrity** — a captured frame decodes byte-exact. `$51,56b5,` against a
  51-character payload whose CRC-16/CCITT independently computes to `56b5`, confirming
  `frame_encode()` against a separate implementation of the same algorithm.

Rows 1.6 and 1.7 are new. They are what this gate could actually measure, and a gate that
records only what it planned to measure is worth less than the afternoon it costs.

**1.1–1.3 wait on hardware, not on a fault.** The status LED is on GP14, an external LED;
the Pico's own GP25 LED is not driven by this firmware, so a bare board correctly shows
nothing.

**1.5 is deferred to Gate 5 or 6, whichever runs first.** The vehicle firmware writes
nothing to USB — the only `stdout` writer in the tree is the ground-station bridge — so
"boot to first telemetry attempt" has no observable on a vehicle with no radio and no SD
card. Recorded as deferred rather than left blank, because a blank row invites someone to
fill it in later from the pattern around it.

### Fixed — the Pico cross-build configured host tests it could never link

`cmake --build build/pico --parallel` has never worked. The host test executables —
`flight_tests`, `sd_card_tests`, `sx1278_tests`, `ground_framing_test`, `flight_smoke_test`
and `emit_mission` — were added to CMake unconditionally, so a tree configured with the Pico
SDK built them with the ARM cross-compiler and failed at link with `undefined reference to
_write`, `_sbrk`, `_getpid`. Those symbols are host syscalls; a bare-metal newlib has none.

The firmware itself was never implicated. It was the tests being asked to run somewhere they
were never meant to.

- Every host-test block in `firmware/common`, `firmware/flight-computer` and
  `firmware/ground-station` is now guarded with `if (NOT CMAKE_CROSSCOMPILING)`. The firmware
  images are the only useful output of a cross configure.
- Verified both ways from clean: the Pico tree configures and builds 221/221 with both
  `.uf2` images produced, and `tools/build_host.sh` still passes 37/37.

### Changed — the firmware build is executed, not just written

`documentation/quick-start.md` section 15 moves from 🟡 to ✅. It now carries the invocation
that actually produced the images — **SDK 2.3.0, arm-none-eabi-gcc 15.2.1, 2026-09-05** —
including the PowerShell environment block, because the VS Code extension installs its
toolchain under `%USERPROFILE%\.pico-sdk\` and puts none of it on `PATH`.

Two traps are recorded there rather than left to be rediscovered: **`-G Ninja` is not optional
on Windows**, and **the extension's `Import Pico Project` must not be run on this repository**
— it rewrites project files, and this tree's `CMakeLists.txt` is hand-written to build the
host tests and the firmware from one source tree.

---

## [Unreleased] — 2026-09-04 (cycle 32)

### Changed — the IMU is now an MPU-9250, and the vehicle has a magnetometer

The MPU-6050 has been replaced with an MPU-9250. This is not a renaming: the MPU-9250 is a
different part with a different `WHO_AM_I`, a different temperature transfer function, a
second filter register the MPU-6050 does not have, and a third die — an AKM AK8963
magnetometer — behind an I2C pass-through bridge.

- New driver `firmware/flight-computer/src/pico/mpu9250.cpp`, replacing `mpu6050.cpp`.
  Resets the part and waits out its start-up before configuring anything; clears
  `FCHOICE_B` so `DLPF_CFG` is actually in circuit; configures the accelerometer filter
  from `ACCEL_CONFIG 2`, a register the MPU-6050 does not have; enables
  `INT_PIN_CFG.BYPASS_EN` and brings up the AK8963 through the fuse-ROM sensitivity read.
- `WHO_AM_I` `0x71`/`0x73` is a real MPU-9250/9255; `0x70` is an MPU-6500 sold as one, with
  no magnetometer. Both are accepted. A vehicle with a substituted part flies on six axes
  and says so, rather than refusing to boot or inventing a heading.
- The AK8963's `ST2` register is read at the end of every measurement burst. It is not
  optional: a driver that reads only the data registers gets a magnetometer that updates
  exactly once.
- **The magnetometer's axes are not the accelerometer's.** The AK8963 die is mounted
  rotated inside the package — its X lies along the MPU's Y, its Y along the MPU's X, and
  its Z is inverted. `mag_axes_to_body()` corrects this before any other code sees a
  sample. Getting it wrong yields a heading that moves smoothly as the vehicle turns and is
  completely wrong, which is exactly the kind of fault a dashboard cannot show you.
- Temperature now uses the MPU-9250's transfer function (`raw`/333.87 + 21 °C). Carrying
  the MPU-6050's `raw`/340 + 36.53 across would have reported room temperature as ~36 °C.

### Changed — nine-axis attitude, and yaw that says what it is

The Euler complementary filter has been replaced with a Mahony complementary filter on the
unit quaternion.

- The old filter integrated Euler angles directly (`roll += p·dt`), which is only valid for
  small angles and loses its solution entirely as pitch passes ±90°. A CanSat under a
  parachute tumbles through exactly that. The quaternion form has no such singularity, and
  a regression test now tumbles the estimator at 100 °/s about all three axes for 20 s.
- The gyroscope propagates; the accelerometer corrects roll and pitch and is ignored
  whenever the specific force is not near 1 g; the magnetometer corrects yaw and only yaw —
  the reference field is re-levelled every update, so a magnetic disturbance cannot tip the
  roll/pitch solution.
- A bias integrator removes gyro drift the pad calibration did not catch, clamped so a long
  manoeuvre cannot let it wander into the attitude.
- **Yaw is absolute only when it has been earned.** A magnetometer with no hard/soft-iron
  calibration still stops yaw drifting, but the estimate is not reported as a magnetic
  heading. Every packet carries a `YR-M` or `YR-G` tag saying which of the two the
  mandatory `Ya-` field holds. Six bytes of airtime to stop a receiver mistaking a relative
  angle for a bearing.
- GPS course over ground is used as a **cross-check only**, never as an input. It is
  referenced to true north rather than magnetic, and a payload crabbing under a parachute
  or spinning on its axis has a course that legitimately differs from its yaw. A gross,
  sustained disagreement while moving raises a warning that says "suspect the magnetometer
  calibration" — it does not move the heading.

### Added — magnetometer calibration architecture

- `MagCalibrator` estimates hard iron and diagonal soft iron from a rotation sweep, using
  min/max bounding rather than an ellipsoid fit: six numbers of state, constant time per
  sample, and a failure mode that is obvious rather than subtle. It refuses to certify
  itself until **every** axis has swept a real range, which is what stops the vehicle from
  claiming an absolute heading it has not earned.
- `Configuration::mag_calibration` ships invalid on purpose. A bench-measured calibration
  for the assembled airframe is pasted in; until then the vehicle reports `YR-G`.
- Hard and soft iron describe the **vehicle**, not the sensor. The battery, the radio and
  the wiring bias the field by tens of microtesla — the same order as the field being
  measured — so the sweep must be done on the finished airframe and repeated when the
  layout changes.

### Fixed — the accelerometer calibration was only correct in one attitude

The pad calibration stored the residual between the measured gravity vector and 1 g as a
body-frame offset vector, and subtracted it from every later sample.

That is right exactly while the vehicle stays in the attitude it was calibrated in. Once it
rotates, the same vector is an error of the same size pointing the wrong way. A single
stationary orientation gives one equation and cannot separate offset from scale, so the
honest correction is the one that is rotation invariant: a scalar scale, `g / |a_rest|`,
applied multiplicatively. A new test applies it across five attitudes and checks the
magnitude still comes back to one standard gravity in all of them.

### Changed — the microSD module is a 3.3 V board

Receiving inspection identified the delivered SKU 11566 as a **2.6–3.6 V SPI module**. Every
document in this project that said 4.5–5.5 V was repeating a supplier listing that does not
describe the board that arrived.

- The second rail and the boost converter are removed from the power tree. Every peripheral
  on this vehicle now runs from one 3.3 V rail.
- `sd-module-analysis.md` is rewritten: the supply question is answered, and what remains is
  measurement — write-transient current against a regulator shared with the radio, and MISO
  release behaviour on the SPI0 bus shared with the RA-02.
- The SD driver itself needed no functional change: it was already 3.3 V-agnostic, already
  asks CMD8 for the 2.7–3.6 V range, and already fails rather than blocks. Comments and
  documentation now state the supply rather than hedging about it.
- SD failure still cannot cost a telemetry packet. That was true before and is retested.

### Changed — ground station and console

- `YR-` is parsed into `yaw_reference` (`"magnetic"` / `"gyro"` / `None`) and a derived
  `heading` in degrees clockwise from magnetic north, offered only when the yaw is
  magnetic. A bearing derived from a relative yaw would be wrong by an unknown constant, so
  none is offered at all.
- The web console shows the yaw reference beside the yaw, and drives its compass card from
  the heading rather than from the Euler yaw — the two run in opposite directions, and
  conflating them mirrors the display.
- The CSV log and export carry both new columns, so a log read months later still says
  which kind of yaw its numbers are.

### Documentation

- `sensor-rates.md`: the MPU-9250's two separate filter registers, the `FCHOICE_B` trap,
  the AK8963's free-running rate, and an explicit statement that DLPF 4 is **not** fully
  anti-aliased at 30 Hz — 15 to 21 Hz still folds down, and that residual is accepted
  knowingly rather than hidden.
- `link-budget.md`: measured packet sizes are now 118 / 167 / 212 bytes, asserted by a
  test. The old "absolute worst case" row is removed as misleading — the team identifier has
  no length limit, so the worst case is bounded by the flight computer's shedding logic
  rather than by the format.
- `bring-up-record.md` gate 8 gains the tests that actually decide whether this vehicle can
  claim a heading: `WHO_AM_I`, field magnitude on the assembled airframe, the calibration
  sweep, yaw drift either side of that calibration, and yaw against a known bearing.

### Added — the delivered boards are photographed, and the photographs are transcribed

Nineteen photographs of the delivered hardware are stored in
`documentation/hardware/photos/`, named by SKU, and every board-level fact a camera can
establish has been read off them into `receiving-inspection.md` Part C: silkscreen text,
header order, component packages, fitted passives and physical fit.

What that closed:

- **The RA-02's header order**, transcribed from the board rather than assumed —
  `GND GND 3.3V RST DIO0 DIO1 DIO2 DIO3` and `GND NSS MOSI MISO SCK DIO5 DIO4 GND`. Every
  pin the GPIO map reserves for the radio exists, so its half of the map can be frozen. The
  supply pin sits third from one end with `GND` either side of it, which is a one-pin offset
  away from putting 3.3 V onto `RST`.
- **The microSD reader has no regulator and no level shifter at all** — four 10 kΩ pull-ups
  and two capacitors are its entire parts list, and its supply pin is printed `3V3`. The
  power tree loses its second rail. It also loses its buffer: nothing but the card releases
  MISO, which promotes bring-up row 7.4 from a precaution to the most important row in the
  shared-bus gate.
- **The IMU die is marked `MP92`**, so the delivered part is a real MPU-9250 rather than the
  MPU-6500 that is frequently sold as one. `WHO_AM_I` remains the final word.
- **The RF chain fits end to end** — antenna, pigtail and module mated with no adapter, in
  `1150780-ra02-antenna-mated.jpg`. Assembly is unblocked.
- **Module header pinouts** for all five breakouts are now in `wiring.md`, as printed.

What it found:

- The battery is **Pro-Range, not the Orange pack the BOM named**. Capacity, cell count,
  voltage and C-rating match; the brand does not, and the label states no charge current and
  no cutoff voltage.
- The barometer is the shared-artwork `GY-BM ☐E/☐P 280` board with **neither variant box
  legibly marked**, so BMP280 versus BME280 is unresolved — a `0x58`/`0x60` chip-ID read
  settles it.
- **No Pico headers, no microSD card and no 1S charger** were supplied, and none is on the
  BOM. The charger is on the critical path.
- The antenna's shell is female and the cable's male, which **agrees with the supplier
  listing's gender and contradicts the BOM's**. SMA versus RP-SMA still turns on the centre
  contacts, and neither mating face was photographed straight on.

### Fixed — three documented values that a photograph does not support

The microSD module's supply was recorded across four documents as "DC 2.6–3.6 V", marked
`VERIFIED FROM HARDWARE`. The delivered board prints `3V3` on its supply pin and states no
range anywhere; the range came from a generic listing for the module type, which is the same
class of source that produced the 4.5–5.5 V claim it replaced. The board's 3.3 V identity is
now derived from what is actually visible — a `3V3` pin and no regulator to step anything
down — and the tolerated range is back to `TBD`.

`sd-module-analysis.md` also listed the interface as `GND, VCC, MISO, MOSI, SCK, CS`. The
delivered header reads `GND MISO CLK MOSI CS 3V3`: a different order, and `CLK` rather than
`SCK`.

The power-tree diagram in `wiring.md` referenced an `SDRAIL` node that had been deleted,
which Mermaid renders as a stray empty box.

### Not done

Nothing here has been run on hardware. The parts are in hand and photographed, but a
photograph establishes silkscreen text, component packages and physical fit — not voltages,
strap directions, continuity or current. Every row in the receiving record that needs a meter
is still blank, deliberately: the AD0 and SDO strap directions, battery polarity and
open-circuit voltage, cable continuity, and the two unidentified SOT-23-5 regulators on the
MPU-9250 and NEO-6M boards. Axis orientation, magnetometer health, calibration quality and
heading accuracy remain bench measurements, written up as bring-up gates rather than as
results.

---

## [Unreleased] — 2026-09-04 (cycle 31)

### Fixed — a portability defect that only a non-Windows build could reveal

The first Linux CI run failed in all three host jobs. The cause was not the toolchain: it
was a genuine defect this project had no way to see.

`std::uint64_t` is `unsigned long long` on Windows and `unsigned long` on 64-bit Linux.
A range-`for` in `flight_tests.cpp` iterated a braced list mixing `ULL` literals with
`std::uint64_t` values. On Windows both spellings name the same type, so the element type
deduces cleanly. On Linux they are two types, and the deduction is ambiguous:

```
error: unable to deduce 'std::initializer_list<auto>&&' from
       '{0, 1, 999, 3600000, last, (((long unsigned int)last) + 1), 1234567890}'
note: deduced conflicting types for parameter 'auto'
      ('long long unsigned int' and 'long unsigned int')
```

The list is now written as `std::initializer_list<std::uint64_t>`: the element type is
stated rather than deduced, so there is nothing left to conflict. Both the failure and
the fix were reproduced on Windows before pushing, by compiling the same construct
against Linux's spelling of the type — the error message matched CI's exactly.

Every other range-`for` over a braced list in the tree was checked; the rest use uniform
literal types and are unaffected.

### Changed — the strict warning gate blocks again

It was made advisory in cycle 30 while the Linux failure was unexplained. The explanation
turned out to be a real bug rather than a compiler disagreement, so the reason for the
downgrade is gone and the gate is enforced again. A quality gate weakened to hide an
unknown stops being a gate.

---

## [Unreleased] — 2026-09-04 (cycle 30)

### Fixed — a 41 MB Windows CMake wheel was being tracked

`cmake-4.4.3-py3-none-win_amd64.whl` sat in the repository root, committed while the
CMake path was being verified. It is a downloaded installer: a build artifact,
machine-specific to `win_amd64`, and useless to CI, Linux and macOS alike, yet every
clone paid 41 MB for it. Untracked, with `*.whl` and `*.tar.gz` added to `.gitignore` so
the next download cannot repeat it. The blob remains reachable through the commit that
added it; purging it needs a history rewrite and is recorded rather than done quietly.

### Added — line-ending policy, enforced

A `.gitattributes` normalises every text file to LF in the repository while leaving
Windows working copies native. The tree was already clean — all 141 tracked files verified
LF in the index — but nothing was stopping the next one. A CRLF that reaches the index
turns a shell script into a file Linux reports as a missing interpreter, which is a
confusing failure to debug in CI.

### Changed — CI reports instead of guessing

The first CI run this project has ever had failed in two of its three jobs. The workflow
made that harder to act on than it needed to be: the strict-warning gate sent its build
to `/dev/null`, so the one thing it existed to surface was thrown away, and nothing was
kept after a job ended.

The workflow now: discards no output; prints `g++`, `python`, `node` and `cmake` versions
in every job; uploads `host.log`, `strict.log` and the CTest results as artifacts whether
the job passes or fails; and pins `ubuntu-24.04` rather than `ubuntu-latest`, so the
toolchain changes when we decide it does and not when a runner image rolls.

The strict warning set moves to its own `continue-on-error` job until it has been observed
passing on Linux once. The tree is clean under that set on the developer's GCC 15.2; a
different GCC may reasonably disagree, and a quality gate that fails on a toolchain nobody
has run blocks work without explaining itself. It must be made blocking again as soon as a
green Linux run exists — recorded in `documentation/testing/test-plan.md`.

### Documentation — the Linux baseline stated plainly

Every test result this project quotes was measured on Windows. The test plan now says so,
lists what has been ruled out by direct check (CMake configure and test registration
against CMake 4.4.3, include casing, LF in the index, committed sources, foreign working
directory, transitive standard headers), and names what remains untested: the runner's own
compiler and C library, which cannot be reproduced on the development machine.

---

## [Unreleased] — 2026-09-04 (cycle 29)

### Documentation — the test plan and the defect register caught up with the code

An audit of the documentation against the source, prompted by a review of six earlier
findings. All six were confirmed fixed in code with named regression tests that run —
the loop tick bounded by the GPS UART FIFO, battery voltage that says whether it is
scaled, the ADC channel derived from the configured pin, the bounded vertical-speed hold,
the steady-descent landing case, and monotonic fault severity. What had drifted was the
writing about them.

* `documentation/testing/test-plan.md` listed 24 of the 41 `flight_tests` suites and gave
  the count as 43. All 41 are now listed, in the order the runner invokes them, and a
  parity check confirms the table and the runner name exactly the same set.
* The fault model in `software-architecture.md` described severity, occurrence counts and
  timestamps but never stated the rule that severity cannot fall while a fault is active —
  the whole point of the cycle-25 fix. It says so now, with the reason.
* The defect register had no entry for that fix. It is recorded as **F-42**, with its
  actual closure cycle and a note that the register entry came later.

No code changed in this cycle.

---

## [Unreleased] — 2026-09-04 (cycle 28)

### Fixed — a silent GPS no longer transmits its last position as a live one

The NEO-6M's NMEA parser holds the last fix it decoded and has no clock of its own, so it
cannot tell a receiver that has stopped talking from one that simply has nothing new to
say. If the GPS lead came off — at parachute deployment, at impact — or the module browned
out, the vehicle would have gone on transmitting that frozen position in every remaining
packet. The recovery team would have been sent to where the payload was, not where it is.
That is the single worst failure mode for the field this telemetry exists to provide.

The driver now stamps the clock each time a sentence renews the fix, and the controller
uses a fix only while that stamp is younger than `gps_fix_timeout_ms` — 3000 ms by
default, three NEO-6M navigation periods, long enough to ride out a sentence lost to a
checksum error. Past it the GPS fields are withdrawn from the packet and
`gps_unavailable` is raised, so the loss is announced rather than hidden behind a
plausible-looking coordinate. A driver that never stamps at all leaves the age equal to
the mission clock, which expires on its own: the failure is closed, not open.

### Fixed — the GPS health record said what we asked for, not what happened

`PicoGps::poll()` marked the receiver healthy and freshly updated on every tick regardless
of whether a byte had ever arrived, so a GPS that was never plugged in looked exactly like
a working one. Health is now taken from the clock of the last byte actually received,
against `gps_silence_after_ms`. A receiver with no sky view still emits sentences
continuously, so silence on the wire means the module is gone rather than merely unfixed.

### Fixed — the GPS UART opened at a rate the configuration had not agreed to

The Pico adapter hard-coded 9600 baud while `config.gps_baud` existed and
`validate_config()` sized the flight loop's tick against it — the guard that keeps the
32-byte UART FIFO from overflowing between polls. Changing the configured rate would have
moved the guard without moving the hardware. The adapter now opens the UART at the rate it
is validated against.

### Fixed — the GPS was polled on a different clock from every other sensor

`gps.poll()` received the raw boot clock while the rest of the loop ran on the mission
clock, putting two time bases in one health structure. All sensors are now polled on the
mission clock, so ages and timestamps are comparable.

### Fixed — a torn frame header no longer costs the frame behind it

In all three frame decoders a `$` arriving inside a corrupt header was discarded as part
of the resync. That byte is the start of the *next* frame, so a single corrupted header
lost two packets. The header now restarts on it, in `framing.cpp`, `transport.py` and the
web console, with a regression test in each. The `len`-state resync counter, which only
C++ incremented, now agrees across all three.

### Fixed — a literal NUL byte in the test source

`flight_tests.cpp` carried a real 0x00 where `'\0'` was meant, left by an earlier
escape-mangled edit. It compiled, but the file registered as binary — `grep` and `file`
both refused to treat it as source — and it emitted the repository's only warning. The
build is back to zero warnings under the extended set.

---

## [Unreleased] — 2026-09-04 (cycle 27)

### Changed — duplicate detection is bounded

The validator remembered every packet number it had ever seen, in a set that grew for as
long as the ground station ran. A range test or a bench session is hours; the web console
in particular may sit open in a browser tab all day. It was never going to exhaust a PC,
but it was unbounded, and a "duplicate" of a packet from two hours ago is not a useful
thing to report anyway.

Both implementations now keep a bounded window of 32768 recent numbers — over eight hours
of flight at 1 Hz — evicting the oldest as new ones arrive. A restart clears it entirely.
Five tests cover the bound, that recent duplicates are still caught, and that the default
window is long enough for any mission.

---

## [Unreleased] — 2026-09-04 (cycle 26)

### Fixed — a vehicle reboot made the ground station's statistics meaningless

The vehicle's watchdog is **designed** to reboot it: the firmware records the reboot as a
fault and resumes transmitting automatically, and that path has its own tests. What nobody
had followed through was what the ground station does next.

After a reboot the vehicle's packet counter restarts at `P-001` and its mission clock at
zero. The validator, seeing numbers it had already recorded, would have marked **every
remaining packet of the flight** as a duplicate *and* out of order — and the loss
statistics, the numbers an operator judges the link by, would have been meaningless from
that point on. Exactly when they matter most, because the vehicle has just rebooted.

The validator now recognises a restart and resets its sequence state cleanly. Both signals
are required — the counter back at `P-001` **and** the mission clock going backwards —
because a counter restart alone could be a corrupted packet number and a clock regression
alone a timestamp glitch; either on its own would let one bad packet reset the ground
station's whole view of the stream.

Fixed identically in `validator.py` and the web console. Ten tests: the reboot itself, that
loss counting still works afterwards, that a real duplicate is still a duplicate, that
neither signal alone triggers it, that the first packet of a session never counts, and that
multiple reboots are each counted.

The count is now visible in all three interfaces — a **Vehicle restarts** row in the web
console and the dashboard's new Stream validation panel, and a `VEHICLE RESTARTED` line in
the headless CLI — and the runbook says what to do about it.

---

## [Unreleased] — 2026-09-04 (cycle 25)

### Fixed — a fault's severity could quietly fall while it was still active

`FaultManager::report()` overwrote the stored severity on every call. The controller
escalates some faults — `imu_init` is reported as an *error*, then as *critical* once it is
clear no compliant packet can ever be produced — so a later routine report at a lower
severity would have silently downgraded it, and `has_critical()` would have stopped seeing
a fault that still applied.

Severity is now monotonic while a fault is active: escalation is honoured, downgrade is
not, and clearing genuinely resets it. The mission's own critical-fault decision reads
specific fault codes rather than `has_critical()`, so flight behaviour was never affected —
but a diagnostic that can lie is worth fixing before something starts relying on it.

`total_occurrences()` also now saturates rather than wrapping: a count that reads as a small
number after wrapping is worse than one that stops at the maximum.

32 assertions cover escalation, refused downgrade, clearing, the latched `ever_critical()`,
occurrence counting across clears, that every fault code has a name, and that an
out-of-range code is refused rather than writing past the fixed array.

---

## [Unreleased] — 2026-09-04 (cycle 24)

### Added — the landing-detection reasoning, written down and tested

A vehicle descending under a parachute at a steady rate has **no net acceleration**: the
accelerometer reads about 1 g, exactly as it does sitting on the ground. The rest test
`||a| − g| < 2.5 m/s²` is therefore satisfied throughout a normal descent, and on its own
would declare a landing seconds after the parachute opened.

The **vertical rate is the only discriminator** — which is why the last two cycles spent so
much effort on it. That reasoning existed nowhere: not in the state machine, not in the
architecture document. It is now in both, along with what each failure direction costs (a
missed landing reports `FLIGHT` on the ground; a false one starts the post-impact window in
mid-air; neither breaks rulebook compliance, because telemetry continues in every state).

`test_landing_is_not_declared_during_a_steady_descent` flies a 28-second descent at 1 g and
−6 m/s and asserts the mission stays in `FLIGHT` throughout, then touches down and asserts
it reaches `LANDED` only after the confirmation window — and separately, that a rate
flickering below the threshold for single samples never accumulates into a false landing.

---

## [Unreleased] — 2026-09-04 (cycle 23)

### Fixed — the vertical-speed hold could have left the mission stuck in FLIGHT

Cycle 4 stopped the vertical-speed estimate from being dragged to zero by repeated
barometer readings. That fix held the estimate **indefinitely** while the pressure was
unchanged — and an unchanged pressure means two opposite things:

- for a sample or two, the loop outran the sensor, and holding is right;
- for longer, the vehicle genuinely is not moving vertically.

The landing detector requires `|vertical speed| < 1 m/s`. A rate held at its last descent
value would therefore **never** let the mission leave `FLIGHT`: the vehicle would sit on the
ground in the flight state, and the post-impact window would never begin. Telemetry would
have continued throughout — the rulebook minimum was never at risk — but the mission state
would have been wrong for the whole recovery.

Real barometer noise would usually have masked this, which is what makes it worth fixing
rather than relying on: a quiet sensor, a frozen one, or a stable day should not change
whether the vehicle notices it has landed.

The hold is now bounded by `altitude_rate_hold_ms` (200 ms, about six sensor periods), after
which the estimate decays to zero. Both behaviours are tested: a brief stall holds, a long
one settles.

---

## [Unreleased] — 2026-09-04 (cycle 22)

### Fixed — battery telemetry could not be told apart from a pin voltage

While `battery_divider_ratio` is unset — which it is, because the divider has not been
built — the firmware reports the raw ADC pin voltage. That is the right behaviour: an
invented ratio would produce a confident wrong number. But nothing said which of the two
the reported value was, so an operator reading **1.6 V** off a 3.7 V cell had no way to know
whether the cell was flat or the scaling was simply absent.

`HealthSnapshot` now carries `battery_voltage_is_scaled`. The low-battery fault already
stayed disabled without a ratio, since a pin reading cannot judge a cell; that is now
covered by a test rather than by inspection.

### Fixed — the ADC channel was hard-coded while the pin was configurable

`battery_voltage()` called `adc_select_input(0)` while the pin came from
`BoardPins::battery_adc`. They agree today — GP26 is ADC0 — but moving the pin to GP27 or
GP28 would have left the code reading a different pin than the one wired, and the reading
would have looked plausible. The channel is now derived from the pin.

---

## [Unreleased] — 2026-09-04 (cycle 21)

### Fixed — the loop tick had a second upper bound nobody had written down

The 2 ms main-loop tick was chosen for scheduling jitter. It has a second, harder limit that
existed only as an accident of the number happening to be small enough: **the GPS is drained
once per tick from a 32-byte hardware FIFO that keeps filling.** At 9600 baud, 8N1, that FIFO
fills in **33.3 ms** — so a tick at or beyond that loses NMEA bytes before anything reads
them. The symptom would not look like a timing bug: truncated sentences, rising checksum
errors, and a GPS that seems unreliable for no visible reason.

Someone raising the tick to 50 ms to save power would have hit it, with nothing in the code
or the documents to warn them.

- `loop_tick_ms` is now a configuration field rather than a literal in `main()`.
- `validate_config()` refuses a tick above **half** the FIFO fill time, and refuses one
  slower than the sensor period.
- `gps_baud` and `gps_uart_fifo_bytes` are configuration too, so the check follows the
  hardware: at 115200 baud the FIFO fills in 2.8 ms and only a 1 ms tick passes.
- The constraint is written up in [sensor-rates.md](documentation/design/sensor-rates.md)
  with the arithmetic, and `check_doc_claims.py` now verifies the documents quote the same
  fill time the formula produces.

Eleven new assertions, and the documented-claim count rises to 61.

---

## [Unreleased] — 2026-09-04 (cycle 20)

### Added — the bring-up record

[`documentation/testing/bring-up-record.md`](documentation/testing/bring-up-record.md)
collects **every number this repository computes but has never observed**, pairs each with
the procedure to measure it, and leaves a blank for the result: nine gates, from a bare
Pico to endurance and recovery.

The project has made a long series of predictions from datasheets and arithmetic — packet
airtime, barometer output rate, acquisition rate and jitter, I2C bus load, RSSI against
distance, log recovery after a power cut. Each is a place where reality can differ, and the
difference is worth finding on a bench rather than in a flight. Every row cites the document
that makes the prediction, so a mismatch leads straight to the reasoning behind it.

The RSSI rows were corrected while writing them: the first draft carried numbers I had
estimated rather than computed. They are now the free-space path-loss values the link
budget actually produces (−28 dBm at 10 m through −68 dBm at 1 km), with an explicit note
that real readings run 10–20 dB weaker and that the useful measurement is the shape of the
curve and the margin to the SX1278's sensitivity.

The document refuses a signed-off gate with a blank row: an unmeasured row that looks
measured is exactly what it exists to prevent.

---

## [Unreleased] — 2026-09-04 (cycle 19)

### Verified — the CMake build path, for the first time

Audit finding F-08 recorded that the CI `cmake-configure` job had never run: the
development machine had no CMake toolchain, so the CMakeLists files were written but never
executed — and this session had since added three targets to them.

CMake and Ninja are available through `pip install cmake ninja`, which needs no system
package manager. With those, the host tree **configures, builds all 31 targets, and passes
all 5 CTest tests**. F-08 closed, and the `CMakeLists.txt` changes made during this pass
are now proven rather than assumed.

The command is documented in the quick start and the contributor guide, including the pip
route for a machine that has neither tool.

---

## [Unreleased] — 2026-09-04 (cycle 18)

### Added — the operator can now see the link degrading before it fails

The bridge has always reported the radio's own RSSI and SNR in its status line, once a
second. **Nothing displayed them.** Every indicator the operator had — rate, loss, missing
packets — only moves once packets are already being lost; RSSI and SNR are the only ones
that degrade while loss is still zero, which makes them the numbers that matter during a
range test and during descent.

They now appear in all three interfaces, with thresholds drawn from the SX1278's own
demodulator limits:

- **Web console** — RSSI, SNR and the bridge's dropped-frame count in the Link health
  panel, amber below −105 dBm or negative SNR, red below −115 dBm.
- **Tk dashboard** — a Bridge radio panel: radio state, RSSI, SNR, frames, drops.
- **Headless CLI** — a `bridge:` line alongside the link summary.

The runbook now says what each reading means and what to do about it.

Four tests cover the parsing, including a negative SNR, the `#radio=lost` line, and that a
status frame is never counted as telemetry.

---

## [Unreleased] — 2026-09-04 (cycle 17)

### Added — documentation drift now fails the build

`tools/check_doc_claims.py` reads the numbers the documentation states and compares them
against the source that defines them: every GPIO pin in the wiring table against
`config.hpp`, the telemetry period and packet budget against the link profile, the quoted
airtime against what the airtime model actually computes, the sensor period and IMU filter
against the sensor configuration, both watchdog timeouts, the measured packet sizes, and
the two rulebook constants that must never drift. **56 claims, all passing**, checked on
every run of `tools/build_host.sh`.

The failure this prevents is quiet and expensive: a constant changes, the prose quoting it
does not, and someone later wires to the pin the document names or trusts a rate the radio
cannot deliver. A wrong number in a document is a defect like any other, and now it fails
the same build.

### Changed — the audit document reflects both passes

Its header still described pass 1 — "four defects found and fixed" — while its findings
table listed F-12 to F-32 from the second pass. It now opens with what each pass covered,
a summary of the twenty-one second-pass findings grouped by what they would have cost, and
recommendations updated for what is now done and what still needs hardware.

---

## [Unreleased] — 2026-09-04 (cycle 16)

### Changed — the warning set now catches the mistakes that matter on ARM

The build used `-Wall -Wextra -Wpedantic`. It now also uses `-Wshadow` (a local hiding a
member), `-Wcast-align` (a pointer cast that faults on ARM but not on x86),
`-Wdouble-promotion` (a float silently widened on a chip with no double-precision FPU),
`-Wnull-dereference`, `-Wnon-virtual-dtor` and `-Wformat=2`. The tree was already clean
under all of them — **zero warnings across 15 translation units and every test** — and CI
now builds a second time with `-Werror` so a new one fails the build instead of scrolling
past.

### Fixed — two unchecked return values on initialisation paths

- The BMP280 driver ignored the result of its soft-reset write. A failed reset leaves the
  device in an unknown configuration, which is worse than an absent one: the calibration
  read might still succeed and the driver would report a healthy sensor it never
  configured.
- The microSD driver ignored the result of `CMD16` (SET_BLOCKLEN) on standard-capacity
  cards. If that fails the card may use a different block length, and every read and write
  after it would be silently wrong.

Both now fail initialisation, which the fault manager already reports and the mission
already survives.

---

## [Unreleased] — 2026-09-04 (cycle 15)

### Changed — the flight image no longer carries iostreams

An earlier pass removed `<regex>` from the shared telemetry library for exactly this
reason. `<sstream>` and `<iomanip>` were still there: `format_packet()` built an
`ostringstream` for **every one of the nine numeric fields**, `format_timestamp()` built
another, `parse_packet()` used a `stringstream` to split on semicolons, and the SD log row
used one more. iostreams pull in the locale machinery and a static initialiser, and they
allocate — on the vehicle's telemetry hot path, once per second, for the whole flight.

All of it is now `snprintf` and direct string building, which rounds identically.

| Measure | Before | After |
|---|---:|---:|
| `telemetry.o` (`g++ -Os`) | 18,941 B | **17,278 B** |
| Undefined iostream/locale symbols in `telemetry.o` | 6 | **0** |
| Flight-core translation units referencing iostreams | 2 | **0** |

The saving that matters is the libstdc++ iostream and locale code the linker no longer has
to pull into the RP2040 image, and the per-packet allocations that no longer happen. Both
are **unmeasured on the target**: there is no ARM toolchain here, so the honest claim is
the symbol dependency, not a flash figure.

### Added — the SD log's column count is now pinned

Rewriting the CSV row by hand is exactly where a column can go missing, so
`test_sd_log_row_matches_its_header` counts the columns in a row with and without a GPS fix
and compares both against the header. It caught a missing comma in the no-fix path during
this change — three empty GPS columns had become two — which would have shifted every
column after it in the flight log. The output is also byte-identical to the previous
implementation, checked by diffing both versions' rows.

Host total: **1360 automated checks.**

---

## [Unreleased] — 2026-09-04 (cycle 14)

### Fixed — a power failure during a header write could erase the whole flight log

`RawBlockLog` rewrites its header after **every** record, so a brownout has many chances to
interrupt exactly that write. With one header block, a torn write left no valid header at
all — and the next boot would restart at the first record block and overwrite the entire
flight it had just recorded. In a flight recorder, on a vehicle whose power design is still
open, that is the worst available failure mode.

The log now keeps **two alternating header copies**, each with a sequence number and a
checksum over its fields. Power can only interrupt the copy being written; the other still
carries the previous complete resume point. On boot the log takes the valid copy, or the
newer of two valid copies. Format version bumped to 2.

Also: records longer than a block are still truncated, but the count is now exposed as
`truncated_records()` rather than being silent — a shortened record in the flight log
should be visible as one.

### Added

- `test_raw_block_log_survives_a_torn_header_write` destroys each header copy in turn and
  checks the log resumes with its records intact, destroys both and checks it starts
  cleanly rather than resuming from corrupted bytes, and checks a region too small for two
  headers plus a record is refused.

Host total: **1351 automated checks.**

---

## [Unreleased] — 2026-09-04 (cycle 13)

### Fixed — the bridge could have rebooted whenever the operator closed the dashboard

The ground-station bridge wrote every frame to USB CDC and flushed it, with no check that a
host was listening. Writing to a USB endpoint with no host attached can block until the
SDK's stdout timeout expires on **every** write, and the bridge runs under a 3 s watchdog:
"the operator closed the laptop lid" would have become a reboot loop, in the one component
whose stated requirement is to keep running when the PC does not. Output is now dropped
while no host is listening, counted, and reported in the bridge's status line as
`dropped=`. The flight computer never writes to stdout at all, so it was never exposed.

### Fixed — a vehicle turning steadily on the pad had its rotation absorbed as gyro bias

Startup calibration gated on gyro *variance*, which a constant rotation passes trivially: a
vehicle spinning steadily on the pad looks perfectly still to a standard-deviation test.
Its rotation was then subtracted as bias for the rest of the flight. The mean is now bounded
too, at 25 deg/s — beyond the MPU-6050 datasheet's ±20 deg/s zero-rate offset, so anything
larger is motion, not bias, and the calibration is refused rather than silently wrong.

### Added — a cross-language end-to-end integration test

`emit_mission` runs the real flight controller through a scripted ascent and descent;
`test_end_to_end.py` pushes its packets through the real ground station — framing, CRC,
parser, validator, logger, CSV export — and checks the two halves against each other rather
than each against its own idea of the format. 13 checks, including a dropped packet, a
corrupted frame, and an unframed link.

Host total: **1326 automated checks.**

---

## [Unreleased] — 2026-09-04 (cycle 12)

### Changed — the project documents now reflect the second development pass

- **[requirements.md](documentation/requirements/requirements.md)** — 23 requirements whose
  acceptance can be judged from software moved from `Not Started` to `Complete`, each with
  a named test in the Evidence column rather than an empty cell. Nothing is marked
  `Verified`: that word is reserved for evidence from hardware, and the vehicle has never
  been powered. The rate requirement (TEL-005) now cites the airtime analysis it was
  actually derived from.
- **[timeline.md](documentation/project/timeline.md)** — the commit table lists all eleven
  commits of this pass instead of one "working tree" row, and says plainly what the pass
  was: a rate the radio could not have delivered, three parsers that disagreed, sensors
  that could not feed their own loop, an attitude filter wrong at the wrap, a packet budget
  below the real packet, a logger that could take reception down with it, and two SPI
  drivers that had never executed.

---

## [Unreleased] — 2026-09-04 (cycle 11)

### Changed — the IMU's range encoding is now testable

`accel_fs_bits()` and `gyro_fs_bits()` lived inside the MPU6050 driver's `PICO_BUILD`
block, invisible to host tests. They now sit in `sensor_math.hpp` beside the sensitivities
they have to agree with, as `accel_range_bits()` / `gyro_range_bits()`.

The failure they guard against is quiet: if the range written to the sensor and the scale
used to convert its output ever disagree, every acceleration is out by a factor of two,
four or eight — and the numbers still look entirely plausible. 42 new assertions check each
range's bits against the register map, each sensitivity against the datasheet, that full
scale lands at the 16-bit limit, and that the flight configuration's plausibility gates sit
outside the configured full scale.

Host total: **1307 automated checks.**

---

## [Unreleased] — 2026-09-04 (cycle 10)

The microSD reader is the project's documented highest-risk integration item, and its
driver had never executed anywhere. It now runs against a simulated card.

### Fixed — the card could keep driving the shared SPI bus

After each transaction the driver deselected the card but did not clock the extra byte the
SD specification requires before the card releases DO. SPI0 is shared with the radio, so a
card still driving MISO corrupts the **radio's** next transaction — a fault that presents as
a dead radio rather than a dead card, on a bus whose sharing is already flagged as a
hardware risk. Every path now releases the bus properly, including every failure path.

### Fixed — a busy card could swallow a write command

`write_block()` issued CMD24 without first waiting for the card to finish programming the
previous block. A card still busy ignores commands. It now waits for ready first.

### Fixed — SD transfers assumed nobody else had touched the bus clock

The driver set the SPI baud rate once during initialisation. The radio shares the bus and
may change it. Each read and write now sets the rate it needs.

### Added

- **`firmware/flight-computer/tests/sd_card_test.cpp`** — 581 assertions against a card
  model built from the SD Physical Layer specification: the CMD0/CMD8/ACMD41/CMD58
  initialisation sequence, the 74-clock requirement, the 400 kHz init limit and the speed-up
  afterwards, **SDHC block addressing versus SDSC byte addressing** (a classic silent
  corruption bug), block round trips, bus release after every transaction, dead cards,
  cards that never finish initialising, missing data tokens, rejected writes, CMD13 status
  errors, and v1 cards where CMD8 is illegal.
- `sd_card.cpp` refactored behind an `SdCardHal` callback struct, mirroring the SX1278
  driver, so it builds and runs on the host. The vehicle entry point is unchanged.
- `sd_card_tests` in `tools/build_host.sh` and CMake/CTest.

Host total: **1265 automated checks.**

---

## [Unreleased] — 2026-09-04 (cycle 9)

The LoRa driver was the largest piece of never-executed code in the repository. It reaches
hardware only through a callback struct, so its entire register sequence can be run against
a fake register bank — which found two defects on the first pass.

### Fixed — reported RSSI was 7 dB optimistic

The driver used the Semtech **high-frequency** offset (−157 dBm) to convert the packet RSSI
register. The RA-02 is a 433 MHz module and therefore sits on the **low-frequency** port,
whose offset is −164 dBm (datasheet 5.5.5). Every reading was 7 dB stronger than reality —
in the one number a range test exists to measure. The offset is now selected from the
configured frequency, and both branches are tested.

### Fixed — the transmit wait hammered the SPI bus

The TxDone wait polled the radio in a tight loop for the whole transmission: hundreds of
milliseconds of continuous traffic on the bus the SD card shares, while the power amplifier
was running. It now yields 1 ms between polls. Also removed a `REG_PA_CONFIG` write that was
immediately overwritten by the next line.

### Added

- **`firmware/common/tests/sx1278_test.cpp`** — 94 assertions over a simulated SX1278:
  silicon-version rejection, LoRa mode entered from sleep, every project setting reaching
  its register, PA_DAC selection at high power, out-of-range settings clamped rather than
  wrapped, low-data-rate optimisation following the symbol time, SF6's special detection
  settings, FIFO loading, TxDone completion, transmit timeouts with and without a
  millisecond clock, RX payload delivery, CRC-error frames dropped, RSSI and SNR
  conversion, sync-word switching, and reconfiguration.
- `sx1278_tests` in both `tools/build_host.sh` and CMake/CTest.
- `flight_tests` now receives the repository root from CTest, so the shared protocol
  fixtures resolve under `ctest` as well as under the host script.

Host total: **684 automated checks.**

---

## [Unreleased] — 2026-09-04 (cycle 8)

A robustness pass over the PC ground station, against the failures that happen on a real
launch day: a full SD card, a removed drive, a link stuck emitting noise.

### Fixed — a corrupted payload could break the raw log's own format

The raw log's contract is one record per line, tab-separated, nothing discarded. But it
deliberately stores corrupted payloads verbatim, and a corrupted payload can contain a tab
or a newline — silently splitting one record into two and desynchronising every column
after it, in the file that exists precisely to be the forensic record.

Control characters are now escaped reversibly on the way in (`escape_raw` / `unescape_raw`),
so a payload containing anything at all still occupies exactly one line and can be
recovered byte for byte. Round-tripped over all 256 code points in tests.

### Fixed — a logging failure could take down reception

`PacketLog.append()` let `OSError` propagate. A full disk, a removed drive or a permission
error would therefore raise on the ground-station thread and end the whole pipeline —
losing the live display and the parser along with the log. Write errors are now counted and
reported (`write_errors`, `last_error`) and reception continues: telemetry is worth more
than its log.

Because a silent logging failure is worse than a loud one, it is surfaced in three places:
the snapshot, a **Logging** row in the Tk dashboard, and a `LOGGING FAULT` line in the
headless CLI.

### Fixed — the unframed serial reader could grow without bound

A link stuck emitting bytes with no newline would have accumulated an ever-growing partial
line for as long as the station ran. Capped at 4096 bytes, counted as a resync.

### Added

- `ground-station/software/tests/test_logger.py` — 18 tests covering escaping, round trips,
  one-record-per-line under corruption, and write-failure handling.
- Two orchestrator tests proving reception survives a failing log.
- Python ground-station total: **63 tests**.

---

## [Unreleased] — 2026-09-04 (cycle 7)

An edge-case sweep over the formatter, the packet budget and the radio path.

### Fixed — the airtime budget was below the typical packet

The 200-byte budget introduced in cycle 2 came from an estimate. Measuring the formatter
directly gives **118 bytes** mandatory-only, **167** with GPS, **206** with GPS and all
four diagnostic tags, and **247** for the absolute worst case. The budget was therefore
*below the packet the vehicle sends in normal flight*, under-estimating channel occupancy
on every transmission.

The budget is now the 255-byte LoRa FIFO limit — the only size a packet cannot exceed, and
still only 40 % duty at 1 Hz on the SF7/125 kHz profile.

### Fixed — an oversized packet would have been truncated by the radio, silently

The driver clamps anything past 255 bytes, so a packet that grew would have been cut
mid-field and read as corruption at the ground station. The controller now sheds optional
content in the rulebook's own priority order instead: diagnostic tags first, then GPS
(recoverable from the SD log), and only if the mandatory block alone still overflows does
it suppress the packet. Every step raises a new `packet_oversize` fault so the ground
station sees it happen.

### Fixed — the timestamp could overflow its own format

`format_timestamp()` widened the hour field past two digits after 99:59:59:999, producing
`100:00:00:000` — a packet this library's own parser rejects, and every ground station with
it. Hours now wrap at 100, which no mission reaches but a bench rig left powered for 4.2
days would.

### Added

- 40 edge-case checks: every timestamp boundary round-trips through the parser, packet
  numbers from 1 to 4294967295 format and parse, values that round to signed zero stay
  valid, and the optional-field degradation ladder is exercised end to end.
- Host total: **464 assertions**.

---

## [Unreleased] — 2026-09-04 (cycle 6)

A correctness pass over the two places where the vehicle turns raw sensor data into
numbers it transmits: attitude fusion and GPS parsing.

### Fixed — the complementary filter was wrong at the ±180° seam

`orientation.cpp` blended the gyro prediction and the accelerometer measurement as a plain
weighted mean. Angles wrap: a prediction of +179° and a measurement of −179° describe
attitudes 2° apart, but their weighted mean is ≈ +175° — and in the worst case the error
approaches 180°. A CanSat under a parachute tumbles through that seam on **every
rotation**, so this was not an edge case.

The filter now blends the *wrapped difference* between prediction and measurement, which is
identical to the old behaviour away from the seam and correct at it.
`test_orientation_blends_across_the_wrap()` fails against the previous formula — verified by
reverting the fix and re-running.

### Fixed — the NMEA parser accepted impossible positions

A sentence can pass its checksum and still carry a corrupted field. The parser accepted:

- latitudes beyond 90° and longitudes beyond 180°;
- a minutes field of 77, which cannot occur;
- a **missing hemisphere character**, silently treating the position as north/east.

All three are now rejected at the source, so an impossible fix never reaches telemetry
rather than being left for the ground station to notice. Rejections are not counted as
checksum errors, since the checksum was fine.

### Added

- Nine GPS validation checks covering southern and western hemispheres, three-digit
  longitudes, the GN/GL multi-constellation talker ids, and RMC's void form.
- Four orientation checks covering the seam, the level case, and a gyro-only spin.
- Host total: **400 assertions**.

---

## [Unreleased] — 2026-09-04 (cycle 5)

### Added

- **[documentation/quick-start.md](documentation/quick-start.md)** — the guide the project
  did not have: zero to a working CanSat in 27 steps. PC software, the bill of materials
  with what each part is for, ordering guidance, tools, part-arrival checks, the power
  problem, wiring, assembly order, Pico SDK setup, team identity, building, flashing both
  Picos, bring-up order, the subsystem test matrix, the end-to-end telemetry test, fault
  injection, pre-flight, launch day, post-flight, troubleshooting, and the ten mistakes
  most likely to cost a day.

  Every step is marked ✅ verified, 🟡 written but never run on hardware, or 🔴 blocked, so
  a reader always knows what is real. Engineering-time estimates are given for beginner,
  intermediate and experienced readers, and are kept explicitly separate from procurement
  time, which the project cannot estimate.

---

## [Unreleased] — 2026-09-04 (cycle 4)

The acquisition loop now runs at 30 Hz, which first required admitting that the barometer
could not have fed it.

### Fixed — the sensors could not supply the rate the loop asked for

- **The barometer was configured for 26.3 Hz.** Its oversampling was hard-coded in the
  driver at the datasheet's "indoor navigation" preset (osrs_t x2, osrs_p x16, 37.5 ms per
  conversion). Any acquisition rate above ~26 Hz would have re-read unchanged conversions.
  Moved to the datasheet's "handheld device, dynamic" preset (x1 / x4, 11.5 ms, 83 Hz),
  which leaves 2.8× margin at 30 Hz.
- **The IMU's anti-alias filter was too wide for the loop rate.** `DLPF_CFG` 3 passes
  44 Hz; sampling at 30 Hz puts Nyquist at 15 Hz, so airframe vibration would have folded
  into the attitude estimate irreversibly. Now `DLPF_CFG` 4 (21 Hz accelerometer, 20 Hz
  gyroscope).
- **Vertical speed could be dragged to zero by a repeated barometer sample.** The rate is
  differentiated from altitude, so an unchanged conversion produced a genuine-looking zero
  climb rate. The controller now updates the rate only when the pressure reading has
  actually changed, so the estimate survives a stalled or slowed sensor.

### Changed

- `sensor_period_ms` 100 → **33** (30 Hz acquisition, orientation and altitude rate).
- Flight loop tick 5 ms → **2 ms**, cutting scheduling jitter on the 33 ms task from 15 %
  to under 6 %.
- Barometer oversampling, IIR filter and IMU DLPF/rate moved from driver constants into
  `Configuration`, so they can be tuned without touching a driver.
- `HealthSnapshot` gains `altitude_agl_m` and `altitude_rate_mps`.

### Added

- **`flight/sensor_timing.hpp`** — `constexpr` BMP280 and MPU6050 datasheet timing model.
  It computes the register encodings *and* the rate limits from one place, so what the
  driver writes and what the validator checks cannot disagree. Pinned by tests to three
  published datasheet figures: the 43.2 ms worst case, the 26.3 Hz preset and the 83 Hz
  preset.
- **Startup sensor-rate guard** — `validate_config()` refuses a `sensor_period_ms` shorter
  than the barometer's worst-case conversion time, naming the number and the document.
- **`documentation/design/sensor-rates.md`** — the rate split, the datasheet arithmetic,
  the aliasing argument, the I2C and CPU budget, and what remains unmeasured.
- Three C++ suites: the timing model, the rate guard, and the repeated-sample behaviour.
  Host total: **357 assertions**.

---

## [Unreleased] — 2026-09-04 (cycle 2)

Radio reality check. The telemetry rate was never derived from the radio's actual
capability, and the two ends of the link kept separate copies of the modem settings. Both
are now single-sourced, computed, and enforced by the build.

### Fixed — the telemetry rate could not have been met

- **The configured 2 Hz was physically impossible.** At the previous provisional default of
  SF9 / 125 kHz, a full telemetry packet occupies over a second of LoRa airtime. The scheduler was set to a 500 ms period, so the vehicle
  would have transmitted at roughly 1 Hz — below the rulebook minimum once any retry or
  recovery was needed — while every document claimed 2 Hz. Changed to **SF7 / 125 kHz at a
  1000 ms period**: ~330 ms for a typical packet, ~40 % worst-case channel occupancy, full
  margin for recovery. The
  arithmetic, the range-margin justification and the 2 Hz upgrade path are in
  [link-budget.md](documentation/design/link-budget.md).
- **The flight computer and the ground-station bridge could silently disagree on the
  modem.** The bridge built `Sx1278Settings` from struct defaults while the vehicle used
  `RadioConfig`; they matched only by coincidence, and changing one would have produced a
  dead link indistinguishable from broken hardware. Both now read one definition,
  `cansat/link_profile.hpp`, and a test compares them field by field.

### Added

- **`cansat/lora_airtime.hpp`** — `constexpr` Semtech SX1276/78 time-on-air model. Pinned
  to two published reference vectors (46.336 ms and 1155.072 ms).
- **`cansat/link_profile.hpp`** — the single radio link profile, with `static_assert`s that
  refuse to compile a profile whose worst-case packet cannot be transmitted on schedule.
- **`tools/link_budget.py`** — design-time airtime and rate-feasibility calculator with a
  spreading-factor sweep, plus **33 tests** in `tools/tests/`, now run by
  `tools/build_host.sh`.
- **Startup airtime guard** — `validate_config()` recomputes the packet airtime for the
  runtime configuration and refuses an impossible telemetry period, naming the airtime, the
  minimum viable period and the design document. It also range-checks every modem parameter.
- **Three new C++ suites** — the airtime reference vectors, the airtime guard, and the
  shared-profile agreement between vehicle and bridge. Host total: **258 assertions**.

### Fixed — three parsers disagreed about what a valid packet is

Probing the C++, Python and JavaScript parsers with 32 packets found two real divergences:

- **`P-000` was accepted by C++ and rejected by the other two.** The C++ formatter refuses
  to emit packet number zero, so its own parser accepting one was incoherent.
- **`P- 7` and `P-99999999999` were accepted by Python and JavaScript.** `int()` and
  `Number()` both skip interior whitespace and have no 32-bit ceiling, so the ground
  station would have accepted packet numbers the vehicle can never send.

All three now apply one rule — digits only, no sign, no whitespace, 1 to 4294967295 — and
the C++ parser no longer routes through `std::stoul`, which silently wraps `-1` to
4294967295. A ground station that disagrees with its transmitter about packet numbers
miscounts packet loss, which is the number an operator watches to judge the link.

### Added — the web console is no longer untested (audit F-07)

- **`ground-station/web/tests/console_core.test.mjs`** — 30 Node tests over the console's
  framing, parser, validator and link health. The code is extracted verbatim from
  `index.html` between new `PORTABLE-CORE` markers, so the tests exercise exactly what
  ships. The harness also asserts that the core touches no DOM or browser API.
- **`test-data/protocol-fixtures.tsv`** — 32 packets, each with a recorded accept/reject
  verdict, read by all three parser implementations. A divergence now fails the build.
- CI installs Node and fails if the web console suite is skipped.

Host totals: **324 C++ assertions, 75 Python tests, 30 Node tests.**

### Removed

- `ground-station/software/src/ui.py` — dead code superseded by `dashboard.py` (audit F-05).
- `ground-station/software/src/radio.py` — compatibility shim with no callers (audit F-10).
- `.claude/` (local tool configuration) is now git-ignored (audit F-11).

---

## [Unreleased] — 2026-09-04

Working tree, not yet committed. The software layer went from "planned" to "implemented
and tested on host", and the documentation was rebuilt around it.

### Added — flight computer

- **Flight loop orchestrator** (`controller.cpp`, ~460 lines) — non-blocking, bounded
  poll loop combining sensor acquisition, calibration, mission state, telemetry, SD
  logging, battery sampling, health and LED status.
- **Mission state machine** — `INIT → SELF_TEST → READY → FLIGHT → LANDED → RECOVERY`,
  with `FAULT` reachable from any operational state. Telemetry continues in every state.
- **Startup calibration** (`startup_calibration.cpp`) — gyro bias, accelerometer offset
  and barometric ground reference captured on the pad, with a stillness gate and a
  best-effort timeout that never blocks the mission.
- **Launch lockout / arming** — launch detection is refused until the arming delay has
  elapsed and calibration has settled, so a startup glitch cannot trigger a false launch.
- **Sensor plausibility gating** — readings outside datasheet-derived bounds are rejected
  *and* the previously held value is dropped, so the vehicle never coasts on data from a
  sensor that is actively wrong.
- **Complementary-filter orientation** (`orientation.cpp`) — roll and pitch fused from the
  gravity vector and integrated body rates; yaw integrated as an explicitly *relative*
  angle, since the vehicle has no magnetometer.
- **Sensor math** (`sensor_math.cpp`) — MPU6050 full-scale conversions, the Bosch BMP280
  fixed-point compensation algorithm, and the barometric altitude formula.
- **Fault manager** (`fault_manager.cpp`) — 16 enumerated codes in a fixed-size array with
  severity, occurrence counts and first/last timestamps. No allocation, constant cost.
- **Telemetry builder** (`telemetry_builder.cpp`) — snapshot to canonical record to
  rulebook packet string, plus the onboard SD CSV row.
- **Periodic-task scheduler** (`scheduler.cpp`) — fixed-period, non-allocating, and
  stall-tolerant (re-anchors instead of firing a catch-up burst).
- **Raw block log** (`raw_block_log.cpp`) — append-only log over 512-byte blocks with a
  rewritten header, so a brownout or impact reset resumes at the correct block. No
  filesystem, no FAT dependency.
- **Streaming NMEA parser** (`gps_parser.cpp`) — GGA and RMC with checksum validation,
  fixed buffers, no allocation, and no exceptions.
- **Health snapshot** (`health.cpp`) — mission state, counters, sensor flags, calibration
  and arming status, battery voltage, gyro bias, GPS checksum errors.
- **Configuration validation** (`config.cpp`) — refuses the `CAN-Team-XX` placeholder, a
  telemetry period above 1000 ms, and a post-impact window below the rulebook's 5 s.
- **Pico HAL** (`src/pico/`) — MPU6050, BMP280, NEO-6M, microSD (raw block access), radio
  glue, board I/O and bus initialisation. Compiled only when the Pico SDK is present.
- **Hardware watchdog** — 2 s on the vehicle; a watchdog reboot is recorded as a fault and
  telemetry restarts automatically.
- **Diagnostic telemetry tags** — `MODE`, `FAULTS`, `CAL` and `ARM` appended after every
  mandatory and GPS field, as permitted optional fields.

### Added — shared library

- **SX1278 / RA-02 LoRa driver** (`sx1278.cpp`, `sx1278.hpp`) — register-level driver
  reached through a `Sx1278Hal` callback struct, so the same code runs on the vehicle, on
  the bridge, or against a fake register bank. Bounded transmit timeout, continuous RX,
  RSSI and SNR readback.
- **Telemetry validity model** — per-field validity flags, so a packet is suppressed when
  any mandatory field cannot be trusted rather than transmitting a wrong value.

### Added — ground station

- **Bridge firmware** (`firmware/ground-station/src/pico/main.cpp`) — continuous RX,
  framed USB output, 1 Hz status lines, radio re-initialisation after repeated failures,
  and a 3 s watchdog.
- **CRC framing** (`framing.cpp` / `framing.hpp`) — `$len,crc,payload` with
  CRC-16/CCITT-FALSE, so transport corruption is diagnosed separately from packet
  validity. Host-buildable and unit-tested.
- **Transport layer** (`transport.py`) — a byte-for-byte Python mirror of the framing plus
  serial, file-replay and loopback transports.
- **Stream validator** (`validator.py`) — team identity, missing, duplicate and
  out-of-order packets, timestamp monotonicity, GPS sanity.
- **Link health** (`health.py`) — sliding-window packet rate, loss percentage, CRC
  errors, staleness and connection state, kept deliberately separate from telemetry
  validation.
- **Orchestrator** (`app.py`) — background thread wiring transport, parser, validator,
  logger and health, with a thread-safe snapshot and a bounded event queue that drops the
  oldest event rather than stalling reception.
- **Tk dashboard** (`dashboard.py`) — non-blocking event drain, live plots when
  matplotlib is available and a full numeric view when it is not.
- **CLI** (`main.py`) — `replay` and `live` subcommands, with or without the dashboard.
- **Web telemetry console** (`ground-station/web/index.html`) — single file, no build, no
  dependencies. Demo replay, file replay and Web Serial, with link health, mission state,
  a 3D flight view, a phase-banded flight profile with apogee and touchdown markers, an
  attitude indicator, GPS and a raw-packet monitor.

### Added — tooling and tests

- `tools/build_host.sh` — one command compiles and runs every host suite plus the Python
  tests.
- `tools/check_pico_syntax.sh` — syntax-checks all 10 `PICO_BUILD` translation units
  against minimal SDK stubs on a machine with no Pico SDK.
- `tools/pico_sdk_stubs/` — the minimal stub headers those checks need.
- `firmware/flight-computer/tests/flight_tests.cpp` — 21 suites, 189 assertions.
- `firmware/flight-computer/tests/mock_hardware.hpp` — mock implementations of all six
  hardware interfaces.
- `firmware/ground-station/tests/framing_test.cpp` — framing round-trip, CRC detection,
  resync, and the standard `0x29B1` known-answer vector.
- Python suites: `test_app.py`, `test_health.py`, `test_transport.py`,
  `test_validator.py` — 37 tests in total with `test_telemetry.py`.
- **GitHub Actions CI** (`.github/workflows/ci.yml`) — host build and tests, Pico syntax
  check, and a CMake configure-and-CTest job on every push and pull request.

### Added — documentation

- [documentation/README.md](documentation/README.md) — documentation index and the rules
  the documentation follows.
- [design/software-architecture.md](documentation/design/software-architecture.md) — layer
  model, module map, flight-loop and pipeline flowcharts, fault model, timing budget,
  design rules.
- [design/wiring.md](documentation/design/wiring.md) — signal wiring for both Picos, pin
  table, bus-sharing rules, power tree, LED and battery-sense notes, RF chain, bring-up
  order.
- [project/timeline.md](documentation/project/timeline.md) — history, phase plan, gate
  status, critical path, blocked work, risk register.
- [testing/test-plan.md](documentation/testing/test-plan.md) — automated coverage,
  per-suite descriptions, hardware and mission test plans.
- [operations/runbook.md](documentation/operations/runbook.md) — configuration, builds,
  ground-station operation, launch-day checklist, troubleshooting, post-flight analysis.
- [audit/2026-09-04-repository-audit.md](documentation/audit/2026-09-04-repository-audit.md)
  — file-by-file verification of code, documentation and claims.
- This changelog, and [CONTRIBUTING.md](CONTRIBUTING.md).
- Rewritten root [README.md](README.md) reflecting what is actually built.

### Changed

- **Removed the `<regex>` dependency from the shared telemetry library.** Team-id
  validation, field-precision checking and timestamp shape checking are now hand-written
  character scans. Behaviour is identical and all tests pass unchanged; compiling the same
  translation unit with the previous regex implementations produces a **337,555-byte**
  object versus **19,183 bytes** now (`g++ -Os`, x86-64) — a 94% reduction in a file that
  is linked into the flight image, where `<regex>` also costs heap and startup time.
  Previously `exact_precision()` constructed two `std::regex` objects on every mandatory
  field, nine times per parsed packet.
- Ground-station modules split by responsibility: `radio.py` became a compatibility shim
  re-exporting `transport.py`, and orchestration moved out of `main.py` into `app.py`.
- `requirements.txt` documents that the core needs no third-party packages; `pyserial` and
  `matplotlib` are optional extras for live serial and plots.
- CMake reorganised: `flight_core` as a hardware-independent host-buildable library,
  with firmware images configured only when `PICO_SDK_PATH` is available.

### Fixed

- **The link-rate meter misread bursts as thousands of Hz.** `LinkHealth` estimated the
  packet rate as an EWMA of instantaneous `1/dt` intervals, so two frames arriving in the
  same millisecond — a duplicate, or a serial buffer flushing a burst — pushed the reading
  to thousands of Hz, which then needed about ten packets to decay. Observed live: the web
  console displayed **249 Hz on a 2 Hz link** after the demo's injected duplicate. Both
  `health.py` and the web console now compute the rate over a **5-second sliding window**,
  which is immune to a single interval and falls to zero when the link drops instead of
  freezing at the last value. Three regression tests added; verified in the browser at a
  steady 2.00 Hz across both injected anomalies.
- **Field-precision validation was rejecting every well-formed packet.** The precision
  pattern escaped its backslash twice, producing `\\.` — a literal backslash followed by
  any character — instead of a literal dot. The replacement scan has no escaping to get
  wrong.

### Known limitations

- No hardware has been brought up. Sensors, radio link, SD media, power and mechanics are
  entirely unverified.
- The Pico HAL and the SX1278 driver are compile-checked only; their register sequences
  have never executed.
- The web console's parser, validator and CRC framing are hand-ported and are not covered
  by an automated suite.
- The CI `cmake-configure` job has not been run locally — no CMake toolchain is installed
  on the development machine.

---

## [2026-09-03] — Initial project

### Added

- Project structure: `avionics/`, `firmware/`, `ground-station/`, `mechanical/`,
  `electrical/`, `simulations/`, `documentation/`, `test-data/` — commit `c6c500b`.
- Hardware project overview — commit `e71706c`.
- Competition requirements and the supplied rulebook, with 30 extracted requirements,
  hardware gap analysis, nine development gates and ten open questions for the organizers
  — commit `a915247`.
- Initial software and engineering documentation: telemetry protocol specification,
  electrical architecture, hardware reference, electrical-compatibility assessment, GPIO
  and resource maps, microSD module analysis, pre-procurement design status — commit
  `27495df`.

### Engineering decisions recorded

- **AMS1117-3.3 rejected** for direct 1S LiPo to 3.3 V regulation: a fully charged cell at
  about 4.2 V does not clear the regulator's high-load dropout, and its 3.3 V output sits
  below the microSD reader's stated 4.5–5.5 V input range. No replacement selected.
- **The microSD reader is the highest-risk integration item** — its supply requirement is
  incompatible with a naive 3.3 V rail.
- **Rulebook contradictions escalated rather than resolved locally** — conflicting
  dimension limits across three pages, and 100 ft versus 150 ft launch altitude.
- **Chip datasheets are not board documentation** — breakout-level supply, logic levels,
  regulators, pull-ups and pinouts stay `TBD` until physically verified.
