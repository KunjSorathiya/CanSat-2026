# Changelog

All notable changes to the CanSat 2026 project.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). This project has
no released versions — it tracks a competition build, so entries are grouped by
development cycle.

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
  SF9 / 125 kHz, one real telemetry packet (188 bytes measured, 200 budgeted) occupies
  **1004 ms** of LoRa airtime. The scheduler was set to a 500 ms period, so the vehicle
  would have transmitted at roughly 1 Hz — below the rulebook minimum once any retry or
  recovery was needed — while every document claimed 2 Hz. Changed to **SF7 / 125 kHz at a
  1000 ms period**: 318 ms airtime, ~32 % channel occupancy, full margin for recovery. The
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
