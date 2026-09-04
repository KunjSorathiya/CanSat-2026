# Changelog

All notable changes to the CanSat 2026 project.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). This project has
no released versions — it tracks a competition build, so entries are grouped by
development cycle.

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
