# Repository Audit — 2026-09-04

A file-by-file verification of the CanSat 2026 repository: does the code build and pass,
does the documentation describe what the code actually does, do the links resolve, and does
the web console behave as documented.

**Auditor:** automated verification run with manual review
**Scope:** all project files (excluding `.git/`, `build/`, `__pycache__/`, and local tool
configuration under `.claude/`)

> [!NOTE]
> **This document covers two passes on the same day.**
>
> **Pass 1** audited the software as first written: 124 files, four defects found and fixed
> (F-01 to F-04), six open items recorded. Its summary tables and evidence blocks below are
> preserved as the record of that run.
>
> **Pass 2** was a deeper engineering review of the same software, and it found
> substantially more — twenty-five further defects, F-12 to F-36, including several that
> would have produced a failed or mis-recorded flight. The headline: the telemetry rate the
> project had chosen was one the radio physically could not deliver. Findings F-12 onward,
> the [second-pass summary](#second-pass-summary) and the file-by-file rows marked with a
> cycle number are from that pass.

**Verdict:** ✅ **Pass. 29 defects found and fixed across both passes; the remaining open
items all require hardware.**

---

## Second-pass summary

Twenty-five findings, all fixed, all with regression tests. Grouped by what they would have
cost:

| Would have caused | Findings |
|---|---|
| **A link that never worked** — the two ends configured different modems, or the rate was unachievable | F-12, F-13 |
| **Wrong numbers in flight** — attitude wrong at the wrap, impossible GPS accepted, RSSI 7 dB optimistic, a steady pad rotation absorbed as bias | F-16, F-17, F-24, F-30 |
| **Lost or corrupted flight data** — a torn header erasing the log, a truncated packet reading as corruption, a raw log splitting its own records, a logging failure ending reception | F-19, F-21, F-22, F-31 |
| **Hardware that would not have worked together** — the microSD holding the shared SPI bus, presenting as a dead radio | F-26, F-27, F-28 |
| **A bridge that rebooted when the operator closed the dashboard** | F-29 |
| **Silent disagreement between implementations** — three parsers, three rules | F-14 |
| **Under-budgeted airtime, unmeasured packet size, a timestamp that overflowed its own format** | F-18, F-20 |
| **Untested code and unnecessary weight** — the web console, the LoRa driver, iostreams in the flight image | F-07, F-25, F-32 |

**Automated checks after pass 2: 1360** — 1221 C++ assertions across the flight core, the
LoRa driver and the microSD driver, 109 Python tests including a cross-language end-to-end
trace, and 30 Node tests over the web console. Zero warnings under an extended warning set;
CI fails on any new one.

---

## Contents

- [Second-pass summary](#second-pass-summary)
- [Summary](#summary)
- [Method](#method)
- [Evidence](#evidence)
- [Findings](#findings)
- [File-by-file audit](#file-by-file-audit)
- [Claim verification](#claim-verification)
- [Web console verification](#web-console-verification)
- [Timeline verification](#timeline-verification)
- [What remains unverified](#what-remains-unverified)
- [Recommendations](#recommendations)

---

## Summary

*Pass 1, preserved as the record of that run. Counts and verdicts have moved since; the
current figures are in the [second-pass summary](#second-pass-summary) above.*

| Area | Files | Verdict |
|---|---:|---|
| Build system | 5 | ✅ Host build verified; Pico build unverifiable here (no SDK) |
| Shared library | 4 | ✅ Builds, tested, `<regex>` dependency removed |
| Flight core | 26 | ✅ Builds, 189 assertions pass |
| Flight tests | 3 | ✅ All pass |
| Pico HAL | 14 | 🟡 Syntax-checked only — never executed |
| Ground bridge firmware | 6 | 🟡 Framing tested; bridge `main` syntax-checked only |
| Ground software (Python) | 17 | ✅ 37 tests pass, all modules compile |
| Web console | 3 | ✅ Verified live in a browser; 🟡 no automated tests |
| Tooling | 11 | ✅ Both scripts run clean |
| Documentation | 25 | ✅ All links resolve; 68 numeric claims match source |
| Directory placeholders | 10 | ✅ Added this audit — documented directories now exist in git |
| **Total** | **124** | |

**Automated checks passing at the end of pass 1: 226** — 189 C++ assertions plus 37 Python
tests, with 10 Pico translation units syntax-clean. After pass 2: **1360**.

---

## Method

Nine verification passes, each producing evidence rather than an opinion:

1. **Build** — `tools/build_host.sh` compiled and ran every host suite.
2. **Firmware syntax** — `tools/check_pico_syntax.sh` compiled all `PICO_BUILD` branches.
3. **Python integrity** — `py_compile` over every module and test.
4. **Numeric claims** — a script asserted 68 documented values (pin numbers, periods,
   thresholds, radio parameters, watchdog timings, bus speeds) against the source that
   defines them.
5. **Link integrity** — every relative Markdown link across all 26 Markdown documents
   resolved against the filesystem.
6. **Anchor integrity** — every in-page and cross-file heading anchor resolved against the
   target document's headings.
7. **Web console** — loaded in a real browser, run through a complete demo mission in both
   themes, with console errors and live values read back from the DOM.
8. **Repository hygiene** — dead code, unreferenced modules, empty directories, `TODO` and
   `FIXME` markers.
9. **Timeline** — every date and commit reference checked against `git log`.

---

## Evidence

### Build and test

```text
== compiling flight_smoke_test ==
== compiling flight_tests ==
== compiling ground_station_tests ==
== running C++ tests ==
flight smoke test passed
189/189 checks passed
flight_tests passed
ground framing tests passed
== running Python ground-station tests ==
Ran 37 tests in 0.038s
OK
ALL HOST BUILDS AND TESTS PASSED
```

Compiler: `g++` with `-std=c++17 -O2 -Wall -Wextra -Wpedantic`. **Zero warnings.**

### Pico syntax check

All 10 translation units returned `OK`: flight `main`, `pico_hal`, `pico_radio`,
`mpu6050`, `bmp280`, `neo6m`, `sd_card`, `sd_logger`, shared `sx1278`, ground bridge
`main`.

### Static checks

| Check | Result |
|---|---|
| Python modules compile | 15 / 15 ✅ |
| `TODO` / `FIXME` / `XXX` markers in source | 0 ✅ |
| Relative Markdown links | 165 checked, 0 broken ✅ (1 fixed, see F-02) |
| In-page anchors | 79 checked, 0 missing ✅ |
| Cross-file anchors | 9 checked, 0 broken ✅ |
| Documented numeric claims | 68 checked, 0 mismatches ✅ |
| Fault codes declared vs named | 16 declared, 16 handled by `fault_name()` ✅ (17 since cycle 7, still 1:1) |

---

## Findings

### Fixed during this audit

<a id="f-01"></a>
#### F-01 · Link-rate meter misread bursts as thousands of Hz — **high**

**Found:** While verifying the web console live, the link rate displayed **249.01 Hz on a
2 Hz link**.

**Cause:** `LinkHealth` estimated the packet rate as an EWMA of instantaneous `1/dt`
intervals. The demo deliberately injects one duplicate packet to exercise link health; that
duplicate arrives in the same millisecond as its original, giving `dt ≈ 0` and an
instantaneous estimate in the thousands of Hz. With a 0.7 / 0.3 EWMA the spike then needs
about ten packets to decay. The identical formula existed in `health.py`, so real hardware
would do the same whenever a serial buffer flushed more than one frame at once.

**Why it matters:** The rate meter is what an operator watches to spot a failing link. A
reading that jumps to 249 Hz and drifts back over five seconds is worse than no reading.

**Fix:** Both implementations now compute the rate over a **5-second sliding window**
(`(n − 1) / span`), which no single interval can distort and which falls to zero when the
link drops instead of freezing at its last value.

**Verification:** three regression tests added (`test_rate_matches_a_paced_stream`,
`test_burst_does_not_inflate_the_rate`, `test_rate_falls_to_zero_when_the_link_drops`);
re-run live in the browser through a full demo mission — rate held **2.00 Hz** across both
the injected drop and the injected duplicate, with loss correctly showing 1.2 % and
duplicates 1.

<a id="f-02"></a>
#### F-02 · Broken documentation link — **low**

`documentation/hardware/pico-gpio-map.md:37` linked to `raspberry_pi_pico_datasheet.pdf`
in its own directory; the file lives in `datasheets/`. Corrected. All 165 relative links
now resolve.

<a id="f-03"></a>
#### F-03 · `<regex>` in the shared telemetry library — **medium (embedded footprint)**

`firmware/common/src/telemetry.cpp` is linked into the flight image and used
`std::regex` for team-id validation, timestamp shape checking and field-precision
checking. `exact_precision()` **constructed a `std::regex` on every call** — nine times per
parsed packet.

Replaced with hand-written character scans. Behaviour is identical and every test passes
unchanged.

| Build of the same translation unit | Object size |
|---|---:|
| With the `<regex>` validators | 337,555 B |
| With hand-written validators | **19,183 B** |

A **94.3 % reduction** (`g++ -Os`, x86-64). On the RP2040 the saving is flash, heap and
startup cost in code that runs on the telemetry hot path.

<a id="f-04"></a>
#### F-04 · Documented directories did not exist in git — **low**

Thirteen directories referenced by the README's repository layout — `avionics/power`,
`avionics/sensors`, `avionics/telemetry`, `electrical/schematics`, `electrical/PCB`,
`mechanical/CAD`, `mechanical/drawings`, `simulations`, `test-data`,
`documentation/mission` and their parents — were empty. Git does not track empty
directories, so **a fresh clone would not contain them** and the documented layout would be
wrong for every new contributor.

Added a `.gitkeep` to each, carrying a one-line statement of what belongs there.

### Open — recorded, not fixed

| ID | Finding | Severity | Why it is open |
|---|---|---|---|
| **F-05** | ~~`ground-station/software/src/ui.py` is dead code~~ | Low | ✅ **Closed 2026-09-04 (cycle 2)** — file removed |
| **F-06** | `ground-station/web/index.legacy.html` (1081 lines) is superseded | Low | Kept intentionally for reference; now labelled as such in the web README |
| **F-07** | ~~The web console's parser, validator, link health and CRC framing are hand-ported with **no automated tests**~~ | Medium | ✅ **Closed 2026-09-04 (cycle 2)** — 30 Node tests extract the core from `index.html`; all three parsers now read one fixture file |
| **F-08** | ~~The CI `cmake-configure` job has never been executed~~ | Low | ✅ **Closed 2026-09-04 (cycle 19)** — CMake and Ninja installed via `pip`, the full host tree configured, all 31 targets built, and all 5 CTest tests passed |
| **F-09** | The Pico HAL and SX1278 driver have never executed | High | **Partly closed 2026-09-04 (cycle 9)** — the SX1278 driver now executes against a fake register bank (94 assertions). The Pico HAL still requires hardware, and no code has run on a real RA-02; this remains the project's central open risk, tracked as gates 3–5 |
| **F-10** | ~~`radio.py` is a compatibility shim with no remaining callers~~ | Low | ✅ **Closed 2026-09-04 (cycle 2)** — file removed |
| **F-11** | ~~`.claude/` (local tool configuration) is untracked and **not** in `.gitignore`~~ | Low | ✅ **Closed 2026-09-04 (cycle 2)** — added to `.gitignore` |
| **F-12** | The telemetry rate was set without reference to LoRa airtime: SF9/125 kHz gives 1004 ms per packet against a 500 ms schedule | **High** | ✅ **Closed 2026-09-04 (cycle 2)** — see [link-budget.md](../design/link-budget.md); profile moved to SF7 at 1 Hz, with compile-time, startup and test guards |
| **F-13** | The flight computer and the ground-station bridge held independent copies of the modem settings and agreed only by coincidence | **High** | ✅ **Closed 2026-09-04 (cycle 2)** — both read `cansat/link_profile.hpp`; a test compares them field by field |
| **F-14** | The C++ parser accepted `P-000`, and the Python and JavaScript parsers accepted `P- 7` and packet numbers beyond 32 bits — three parsers, three rules | **Medium** | ✅ **Closed 2026-09-04 (cycle 2)** — one rule in all three, pinned by `test-data/protocol-fixtures.tsv` |
| **F-15** | The barometer was hard-coded to the 26.3 Hz "indoor navigation" preset, so no acquisition rate above ~26 Hz could return fresh data; the IMU's 44 Hz anti-alias filter was also too wide for the loop rate | **Medium** | ✅ **Closed 2026-09-04 (cycle 4)** — sensor settings moved into `Configuration`, barometer at the 83 Hz preset, 30 Hz acquisition, startup guard and a duplicate-sample check on vertical speed. See [sensor-rates.md](../design/sensor-rates.md) |
| **F-16** | The complementary filter averaged raw angles, so blending across the ±180° seam produced errors approaching 180° — on a vehicle that tumbles through that seam every rotation | **High** | ✅ **Closed 2026-09-04 (cycle 6)** — blends the wrapped difference; a regression test fails against the old formula |
| **F-17** | The NMEA parser accepted any checksum-valid coordinate, including latitudes past 90°, minutes past 59, and a missing hemisphere character (silently treated as north/east) | Medium | ✅ **Closed 2026-09-04 (cycle 6)** — coordinates are range-checked at the source, so an impossible fix never reaches telemetry |
| **F-18** | The airtime budget (200 bytes) was below the *typical* in-flight packet (206 bytes measured), so channel occupancy was under-estimated on every transmission | **Medium** | ✅ **Closed 2026-09-04 (cycle 7)** — budget is now the 255-byte FIFO limit; sizes measured rather than estimated |
| **F-19** | A packet exceeding 255 bytes would have been silently truncated by the radio driver and read as corruption at the ground station | **Medium** | ✅ **Closed 2026-09-04 (cycle 7)** — optional fields are shed in rulebook priority order, and the packet is suppressed with a `packet_oversize` fault rather than truncated |
| **F-20** | `format_timestamp()` emitted a three-digit hour past 99:59:59:999, a packet its own parser rejects | Low | ✅ **Closed 2026-09-04 (cycle 7)** — hours wrap at 100, with every boundary round-tripped in tests |
| **F-21** | The raw log stored corrupted payloads verbatim, so a payload containing a tab or newline split one record into several and desynchronised the forensic log | **Medium** | ✅ **Closed 2026-09-04 (cycle 8)** — control characters escaped reversibly; round-tripped over all 256 code points |
| **F-22** | An `OSError` from either log write propagated onto the ground-station thread, so a full disk would have ended reception, not just recording | **High** | ✅ **Closed 2026-09-04 (cycle 8)** — errors counted and surfaced in the snapshot, the dashboard and the CLI; reception continues |
| **F-23** | The unframed serial reader accumulated an unbounded partial line if no newline ever arrived | Low | ✅ **Closed 2026-09-04 (cycle 8)** — capped at 4096 bytes and counted as a resync |
| **F-24** | The radio reported RSSI using the Semtech high-frequency offset (−157 dBm), but the RA-02 is a 433 MHz module on the low-frequency port, whose offset is −164 dBm — every reading was 7 dB optimistic | **Medium** | ✅ **Closed 2026-09-04 (cycle 9)** — offset selected from the configured frequency; RSSI is the number a range test depends on |
| **F-25** | The transmit wait polled SPI in a tight loop for the whole transmission — hundreds of milliseconds of needless traffic on the bus the SD card shares | Low | ✅ **Closed 2026-09-04 (cycle 9)** — yields 1 ms between polls |
| **F-26** | The microSD driver deselected the card without the extra clock the SD specification requires, so the card could keep driving MISO — corrupting the **radio's** next transaction on the shared SPI0 bus | **High** | ✅ **Closed 2026-09-04 (cycle 10)** — every path, including failures, releases the bus properly |
| **F-27** | `write_block()` issued CMD24 without waiting for a card still programming the previous block, which ignores commands while busy | **Medium** | ✅ **Closed 2026-09-04 (cycle 10)** — waits for ready first |
| **F-28** | SD transfers assumed the SPI clock was still whatever initialisation left, on a bus the radio shares and can reconfigure | Low | ✅ **Closed 2026-09-04 (cycle 10)** — each transfer sets its own rate |
| **F-29** | The bridge wrote to USB CDC with no check that a host was listening. A blocked write under its 3 s watchdog would turn a closed dashboard into a reboot loop — in the component whose requirement is to survive exactly that | **High** | ✅ **Closed 2026-09-04 (cycle 13)** — output dropped and counted while no host is attached, reported as `dropped=` in the status line |
| **F-30** | Startup calibration gated on gyro variance alone, so a vehicle turning at a constant rate on the pad passed as "still" and had its rotation subtracted as bias for the whole flight | **Medium** | ✅ **Closed 2026-09-04 (cycle 13)** — the mean is bounded at 25 deg/s, beyond the datasheet's zero-rate offset |
| **F-31** | `RawBlockLog` kept one header block, rewritten after every record. A power failure during that write left no valid header, and the next boot would restart at the first record block — overwriting the entire flight just recorded | **High** | ✅ **Closed 2026-09-04 (cycle 14)** — two alternating header copies with sequence numbers and checksums; each is destroyed in turn by a test |
| **F-32** | `<sstream>` and `<iomanip>` remained in the flight image after `<regex>` was removed for the same reason: an `ostringstream` per numeric field, once per second, for the whole flight | Medium | ✅ **Closed 2026-09-04 (cycle 15)** — replaced with `snprintf`; flight-core translation units referencing iostreams went 2 → 0 |
| **F-33** | The loop tick had a second upper bound nobody had written down: the GPS is drained once per tick from a 32-byte UART FIFO that fills in 33 ms at 9600 baud | Medium | ✅ **Closed 2026-09-04 (cycle 21)** — `loop_tick_ms` is configuration, and `validate_config()` enforces both bounds |
| **F-34** | Reported battery voltage could not be told apart from a raw ADC pin voltage, and the ADC channel was hard-coded while the pin was configurable | Low | ✅ **Closed 2026-09-04 (cycle 22)** — `battery_voltage_is_scaled` in the health snapshot; channel derived from the pin |
| **F-35** | The vertical-speed hold introduced in cycle 4 was unbounded, so a quiet or frozen barometer would have held a descent rate for ever — and the landing detector, which requires under 1 m/s, would never have fired | **Medium** | ✅ **Closed 2026-09-04 (cycle 23)** — bounded by `altitude_rate_hold_ms`; both the brief stall and the long one are tested |
| **F-36** | After a watchdog reboot — a path the firmware explicitly supports — the ground station would have marked every remaining packet of the flight as a duplicate and out of order, making the loss statistics meaningless | **High** | ✅ **Closed 2026-09-04 (cycle 26)** — restart detection requiring both a counter reset and a clock regression, in `validator.py` and the web console; surfaced in all three interfaces |

---

## File-by-file audit

Legend: ✅ verified · 🟡 partially verified · ⬜ content-only review (no executable claim)

### Build system

| File | Lines | Verified | Verdict |
|---|---:|---|---|
| `CMakeLists.txt` | 20 | Structure reviewed; guards the SDK import correctly | 🟡 Configure not run — no CMake here |
| `firmware/common/CMakeLists.txt` | 7 | Target definition matches sources | 🟡 |
| `firmware/flight-computer/CMakeLists.txt` | 75 | Source list matches `src/`; firmware target gated on `PICO_SDK_PATH` | 🟡 |
| `firmware/ground-station/CMakeLists.txt` | 39 | Same pattern; framing library host-buildable | 🟡 |
| `.github/workflows/ci.yml` | 47 | YAML reviewed; two of three jobs replicate locally verified commands | 🟡 |

### Shared library — `firmware/common/`

| File | Lines | Verified | Verdict |
|---|---:|---|---|
| `include/cansat/telemetry.hpp` | 65 | Interface matches implementation and both parsers | ✅ |
| `src/telemetry.cpp` | 235 | Format and parse covered by `test_telemetry_format_exact`, `test_packet_numbering_and_padding`, `test_parser_rejects_precision_and_order`; `<regex>` removed (F-03) | ✅ |
| `include/cansat/sx1278.hpp` | 83 | Settings match `RadioConfig`; sync-word defaults correct | ✅ |
| `src/sx1278.cpp` | 312 | Register sequence executed against a fake register bank, 94 assertions | 🟡 Never run on real silicon (F-09) |

### Flight core — `firmware/flight-computer/`

| File | Lines | Verified | Verdict |
|---|---:|---|---|
| `include/flight/config.hpp` | 155 | **47 documented values asserted against this file** | ✅ |
| `src/config.cpp` | 36 | `test_config_validation` | ✅ |
| `include/flight/interfaces.hpp` | 96 | Implemented by both mocks and the Pico HAL | ✅ |
| `include/flight/controller.hpp` | 113 | Matches implementation | ✅ |
| `src/controller.cpp` | 460 | Six controller suites; LED timings and EWMA asserted | ✅ |
| `include/flight/state_machine.hpp` | 45 | | ✅ |
| `src/state_machine.cpp` | 103 | `test_state_machine_full_mission`, `test_state_machine_fault_paths` | ✅ |
| `include/flight/scheduler.hpp` | 30 | | ✅ |
| `src/scheduler.cpp` | 31 | `test_scheduler` including stall re-anchoring | ✅ |
| `include/flight/orientation.hpp` | 42 | | ✅ |
| `src/orientation.cpp` | 89 | `test_orientation_levels_and_yaw` | ✅ |
| `include/flight/sensor_math.hpp` | 65 | | ✅ |
| `src/sensor_math.cpp` | 99 | `test_mpu_scaling`, `test_bmp280_compensation_datasheet_vector`, `test_pressure_altitude` | ✅ |
| `include/flight/startup_calibration.hpp` | 64 | | ✅ |
| `src/startup_calibration.cpp` | 113 | `test_startup_calibrator_stationary_and_moving` | ✅ |
| `include/flight/telemetry_builder.hpp` | 57 | | ✅ |
| `src/telemetry_builder.cpp` | 105 | `test_telemetry_builder` | ✅ |
| `include/flight/fault_manager.hpp` | 66 | 16 codes; all handled by `fault_name()` | ✅ |
| `src/fault_manager.cpp` | 100 | `test_fault_manager` | ✅ |
| `include/flight/raw_block_log.hpp` | 53 | | ✅ |
| `src/raw_block_log.cpp` | 94 | `test_raw_block_log` including reset resume | ✅ |
| `include/flight/gps_parser.hpp` | 38 | | ✅ |
| `src/gps_parser.cpp` | 185 | `test_gps_parser`, plus a live sentence in the smoke test | ✅ |
| `include/flight/health.hpp` | 36 | | ✅ |
| `src/health.cpp` | 18 | All seven states named | ✅ |
| `README.md` | 80 | Claims cross-checked against `config.hpp` and `controller.cpp` | ✅ |

### Pico HAL — `firmware/flight-computer/src/pico/` and headers

| File | Lines | Verified | Verdict |
|---|---:|---|---|
| `pico/main.cpp` | 88 | Watchdog 2000 ms and 5 ms tick asserted (tick is 2 ms since cycle 4, see F-15) | 🟡 Syntax only |
| `pico/pico_hal.cpp` | 146 | Bus speeds asserted: I2C 400 kHz, SPI 400 kHz, UART 9600 | 🟡 Syntax only |
| `pico/mpu6050.cpp` | 123 | Uses verified scaling from `sensor_math` | 🟡 Syntax only |
| `pico/bmp280.cpp` | 122 | Uses verified compensation from `sensor_math` | 🟡 Syntax only |
| `pico/neo6m.cpp` | 50 | Feeds the tested `NmeaParser` | 🟡 Syntax only |
| `pico/sd_card.cpp` | 253 | Raw block access, no filesystem; command sequence executed against a simulated card (581 assertions) since cycle 10 | 🟡 Never run on real media |
| `pico/sd_logger.cpp` | 64 | Wraps the tested `RawBlockLog` | 🟡 Syntax only |
| `pico/pico_radio.cpp` | 97 | Wraps the SX1278 driver | 🟡 Syntax only |
| `include/flight/pico/*.hpp` (6 files) | 251 | Interfaces consistent; `pico_types.hpp` used by four headers | ✅ Reviewed |

### Ground station firmware — `firmware/ground-station/`

| File | Lines | Verified | Verdict |
|---|---:|---|---|
| `include/ground/framing.hpp` | 59 | Contract matches the Python and JS mirrors | ✅ |
| `src/framing.cpp` | 136 | Round-trip, CRC error, resync, `0x29B1` known-answer vector | ✅ |
| `include/ground/radio_bridge.hpp` | 16 | Interface only | ⬜ |
| `src/pico/main.cpp` | 134 | Watchdog 3000 ms, 1 s status period, 20-failure threshold asserted | 🟡 Syntax only |
| `tests/framing_test.cpp` | 107 | Runs and passes | ✅ |
| `README.md` | 16 | Matches the implementation | ✅ |

### Tests

| File | Lines | Verified | Verdict |
|---|---:|---|---|
| `flight-computer/tests/flight_tests.cpp` | 687 | 21 suites, 189 assertions, all pass | ✅ |
| `flight-computer/tests/flight_smoke_test.cpp` | 37 | Passes | ✅ |
| `flight-computer/tests/mock_hardware.hpp` | 144 | Implements all six interfaces | ✅ |
| `ground-station/software/tests/*.py` (5) | 418 | 37 tests, all pass | ✅ |

### Ground station software — `ground-station/software/`

| File | Lines | Verified | Verdict |
|---|---:|---|---|
| `src/telemetry.py` | 192 | 9 tests | ✅ |
| `src/validator.py` | 104 | 9 tests | ✅ |
| `src/transport.py` | 267 | 9 tests, including the shared CRC vector | ✅ |
| `src/health.py` | 108 | 7 tests; rate estimator rewritten (F-01) | ✅ |
| `src/logger.py` | 73 | Covered by telemetry and app tests | ✅ |
| `src/app.py` | 171 | 3 end-to-end tests | ✅ |
| `src/main.py` | 105 | CLI surface reviewed; `replay` exercised | 🟡 `live` needs hardware |
| `src/dashboard.py` | 210 | Compiles; matplotlib fallback path reviewed | 🟡 Needs a display |
| `src/radio.py` | 27 | Compat shim, no callers (F-10) | 🟡 |
| `src/ui.py` | 24 | **Dead code** (F-05) | ⚠️ |
| `requirements.txt` | 10 | Accurate: core needs no third-party packages | ✅ |
| `README.md` | 33 | Module table matches the source | ✅ |

### Web — `ground-station/web/`

| File | Lines | Verified | Verdict |
|---|---:|---|---|
| `index.html` | 2491 | Run live: full demo mission, both themes, zero console errors | ✅ Manual |
| `index.legacy.html` | 1081 | Superseded, retained for reference (F-06) | ⬜ |
| `README.md` | 49 | Rewritten this audit to match the panels actually present | ✅ |

### Tooling — `tools/`

| File | Lines | Verified | Verdict |
|---|---:|---|---|
| `build_host.sh` | 74 | Runs clean; source list matches CMake | ✅ |
| `check_pico_syntax.sh` | 43 | All 10 units `OK` | ✅ |
| `pico_sdk_stubs/*` (9 files) | 133 | Sufficient for the syntax check; correctly marked not-an-SDK | ✅ |

### Documentation — 25 files (18 Markdown under `documentation/`, 3 reference PDFs, 3 root documents, plus 5 component READMEs audited in their own sections)

| File | Lines | Verdict |
|---|---:|---|
| `README.md` | 523 | ✅ Rewritten; every status claim cross-checked against code and tests |
| `CHANGELOG.md` | 202 | ✅ Written this cycle; commit references match `git log` |
| `CONTRIBUTING.md` | 164 | ✅ Written this cycle |
| `documentation/README.md` | 111 | ✅ Index; all links resolve |
| `design/software-architecture.md` | 504 | ✅ Written this cycle; flowcharts trace the real call order in `controller.cpp` |
| `design/wiring.md` | 357 | ✅ Written this cycle; every pin asserted against `BoardPins` |
| `design/telemetry-protocol.md` | 364 | ✅ Pre-existing; consistent with the implementation |
| `design/electrical-architecture.md` | 336 | ⬜ Pre-existing; engineering analysis, no executable claims |
| `hardware/*` (8 files) | 1789 | ⬜ Pre-existing; one broken link fixed (F-02) |
| `requirements/requirements.md` | 280 | ⬜ Pre-existing; statuses consistent with the README |
| `project/timeline.md` | 254 | ✅ Written this cycle; dates verified against `git log` |
| `testing/test-plan.md` | 264 | ✅ Written this cycle; every suite name verified against the test sources |
| `operations/runbook.md` | 363 | ✅ Written this cycle; every command and config field verified against source |
| `firmware/*/README.md`, `ground-station/*/README.md`, `tools/*/README.md` (5) | 186 | ✅ Cross-checked against their code |

---

## Claim verification

68 values stated in the documentation were asserted against the source that defines them.
**All 68 matched.** Sample:

| Claim | Source of truth | Result |
|---|---|---|
| 15 GPIO assignments | `BoardPins` in `config.hpp` | ✅ |
| Telemetry 1000 ms, sensor 33 ms, SD flush 2000 ms, health 1000 ms, battery 1000 ms | `Configuration` (telemetry was 500 ms and sensor 100 ms; changed in cycles 2 and 4, see F-12 and F-15) | ✅ |
| Calibration: 80 samples, 20 s timeout, 2 °/s, 1.5 m/s² | `Configuration` | ✅ |
| Launch: 30 m/s², 15 m, 300 ms hold, 3 s arming delay | `Configuration` | ✅ |
| Landing: 3 s minimum flight, 2.5 m/s², 1.0 m/s, 3 s hold | `Configuration` | ✅ |
| Post-impact window 5000 ms | `Configuration` | ✅ |
| Plausibility: 30–115 kPa, −50…95 °C, 170 m/s², 2200 °/s | `Configuration` | ✅ |
| Radio: 433 MHz, SF7, 125 kHz, CR 4/5, 17 dBm, preamble 8, CRC on | `link_profile.hpp` (was SF9 in `RadioConfig`; changed in cycle 2, see F-12) | ✅ |
| Sync words `0xF3` / `0xA5` | `RadioConfig` | ✅ |
| SD 10-failure cutoff, radio 5-failure threshold, 1000 ms back-off | `Configuration` | ✅ |
| LED periods 900 / 400 / 100 / 250 / 60 ms | `Controller::update_led` | ✅ |
| Watchdogs 2000 ms flight, 3000 ms bridge; 2 ms flight tick | `main.cpp` × 2 | ✅ |
| Bus speeds I2C 400 kHz, SPI 400 kHz, UART 9600 | `pico_hal.cpp` | ✅ |
| Vertical-rate EWMA 0.7 / 0.3 | `controller.cpp` | ✅ |
| Frame payload limit 512 B | `framing.hpp` | ✅ |
| 16 fault codes | `fault_manager.hpp` / `.cpp` | ✅ (17 since cycle 7, see F-19) |

---

## Web console verification

Loaded `ground-station/web/index.html` in a browser and ran a complete demo mission.

| Check | Result |
|---|---|
| Page loads with no console errors | ✅ |
| Demo auto-starts and streams at 2 Hz | ✅ |
| Panels present: Mission, Link health, Flight view 3D, Flight profile, Attitude, GPS, Raw packet monitor | ✅ |
| Mission progresses `SELF_TEST → READY → FLIGHT → LANDED → RECOVERY` | ✅ |
| Calibration and arming tags surface (`CAL` complete, `ARM` armed) | ✅ |
| Sync word displayed as `TEST · 0xF3` | ✅ |
| Injected dropped packet counted (missing 1, loss 1.2 %) | ✅ |
| Injected duplicate counted (duplicates 1) | ✅ |
| Packet rate steady at **2.00 Hz** through both anomalies | ✅ after F-01 |
| Light and dark themes both render legibly | ✅ |
| Mission restarts cleanly at profile end with a toast, not a silent stall | ✅ |

Before F-01 was fixed, the same run displayed 249.01 Hz. This is the one defect that a
documentation-only review would have missed entirely.

---

## Timeline verification

Every date in [timeline.md](../project/timeline.md) was checked against `git log`.

| Claim | Verified |
|---|---|
| `c6c500b` initial structure, 2026-09-03 | ✅ |
| `e71706c` hardware overview, 2026-09-03 | ✅ |
| `a915247` requirements and rulebook, 2026-09-03 | ✅ |
| `27495df` initial software and documentation, 2026-09-03 | ✅ |
| Working tree: software implementation plus this documentation cycle, 2026-09-04 | ✅ 24 modified files, 66 new files |
| Phases 0–4 complete, 5–9 not started | ✅ Consistent with the absence of any hardware evidence |
| Gate 1 partial, gates 2–9 not passed | ✅ Consistent with `requirements.md` |
| No competition dates asserted | ✅ Correct — none appear in the supplied rulebook |

The forward plan deliberately uses phases and dependencies rather than calendar dates,
because no deadline exists in the source material. That is the right call and is stated
explicitly in the document.

---

## What remains unverified

This is the honest boundary of the audit. Nothing below has been demonstrated.

| Area | Status | Blocking gate |
|---|---|---|
| Every sensor reading from real hardware | Never executed | Gate 4 |
| The radio link, at any range | Never established | Gate 5 |
| SD card behaviour on real media | Never executed | Gate 4 |
| Power system: regulator, switch, LED, divider, brownout, endurance | Not designed | Gates 2–3 |
| Timing behaviour under real load | Only synthetic clocks tested | Gate 5 |
| Mechanical: structure, egg chamber, parachute, descent rate | Not started | Gate 7 |
| Ground-station compatibility with the official dual stations | Never tested | Gate 6 |
| Pico firmware image builds and boots | Never built — no SDK here | Gate 4 |

**The software is verified. The vehicle is not.** No part of this repository establishes
flight readiness, and no document in it claims otherwise.

---

## Recommendations

Ordered by value. Items 3 and 4 from pass 1 are done; what remains needs hardware or an
answer from the organisers.

1. **Start hardware bring-up.** Every remaining gate depends on it, and the
   [bring-up order](../design/wiring.md#bring-up-order) is written and sequenced. This is
   the single highest-value action available, and it is now more valuable than before: two
   of the drivers have been executed against simulated devices, so bring-up is checking
   physical behaviour rather than finding basic logic errors.
2. **Escalate the ten organizer questions**, especially the dimension contradiction — it
   blocks the entire mechanical phase — and the 433 MHz channel and duty-cycle question
   raised by [link-budget.md](../design/link-budget.md).
3. ~~Add a test harness for the web console (F-07)~~ — done in pass 2: 30 Node tests, and
   all three parsers now read one fixture file.
4. ~~Decide on `ui.py` (F-05) and `.claude/` (F-11)~~ — done in pass 2: both removed,
   `.claude/` ignored.
5. **Push once, to prove the CI workflow** (F-08), including the CMake job that cannot run
   on this machine. CI has since grown a Node job, a `-Werror` job and new CTest targets,
   so this matters more than it did.
6. **Set `team_id` and the launch sync word early** and rehearse the switch, so it is not a
   launch-day change. The firmware already refuses to run with the placeholder identity,
   and both ends now read the sync word from one shared definition — so the rehearsal must
   include reflashing **both** Picos.
7. **Measure, on the first hardware available, the four numbers this repository computes
   but has never observed**: packet airtime, the achieved acquisition rate and its jitter,
   the barometer's real output rate, and RSSI against distance. Each has a documented
   predicted value to compare against.

---

## Audit trail

| Step | Command | Result |
|---|---|---|
| Host build and tests | `bash tools/build_host.sh` | All pass, 0 warnings |
| Firmware syntax | `bash tools/check_pico_syntax.sh` | 10 / 10 `OK` |
| Python integrity | `python -m py_compile` over 15 files | All pass |
| Python suite | `python -m unittest discover` | 37 / 37 |
| Numeric claims | scripted assertion of 68 values | 68 / 68 |
| Link integrity | scripted resolution of 165 relative links | 165 / 165 after F-02 |
| Anchors | scripted resolution of 88 heading anchors | 88 / 88 |
| Object-size comparison | `g++ -Os -c` on both variants | 337,555 B → 19,183 B |
| Web console | browser session, DOM readback, both themes | Pass after F-01 |
| Repository hygiene | dead-code, empty-directory and marker scans | 4 findings, 1 fixed |

**Audit complete.** Re-run this audit after hardware bring-up, when the 🟡 rows can start
becoming ✅.
