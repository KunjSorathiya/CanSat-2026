# Quick Start — Zero to Working CanSat

Everything needed to go from an empty desk to a CanSat transmitting telemetry to a ground
station, in the order you actually do it.

**Read this first:** the software in this repository is complete and tested on a host
machine. The vehicle is not built. Nothing here has been run on real hardware. Steps below
are marked so you always know which is which:

| Mark | Meaning |
|---|---|
| ✅ | Works today, verified in this repository |
| 🟡 | Written and reviewed, never executed on hardware |
| 🔴 | Blocked — a decision or a part is missing, and the step cannot be completed |

---

## Contents

**Part 1 — Nothing to buy**
1. [What this project is](#1-what-this-project-is)
2. [Try it right now, with no hardware](#2-try-it-right-now-with-no-hardware)
3. [PC software you need](#3-pc-software-you-need)
4. [Build and test the software](#4-build-and-test-the-software)

**Part 2 — Getting the parts**

5. [Bill of materials](#5-bill-of-materials)
6. [What each part does](#6-what-each-part-does)
7. [Ordering guidance](#7-ordering-guidance)
8. [Tools you need](#8-tools-you-need)
9. [Checks to do the moment the parts arrive](#9-checks-to-do-the-moment-the-parts-arrive)

**Part 3 — Building it**

10. [The power problem — read before wiring](#10-the-power-problem--read-before-wiring)
11. [Wiring](#11-wiring)
12. [Assembly order](#12-assembly-order)

**Part 4 — Firmware**

13. [Pico SDK setup](#13-pico-sdk-setup)
14. [Configure your team identity](#14-configure-your-team-identity)
15. [Build the firmware](#15-build-the-firmware)
16. [Flash both Picos](#16-flash-both-picos)

**Part 5 — Bring-up**

17. [Bring-up order](#17-bring-up-order)
18. [Subsystem tests](#18-subsystem-tests)
19. [End-to-end telemetry test](#19-end-to-end-telemetry-test)
20. [Fault injection](#20-fault-injection)

**Part 6 — Flying**

21. [Pre-flight](#21-pre-flight)
22. [Launch day](#22-launch-day)
23. [Post-flight](#23-post-flight)

**Reference**

24. [How long this takes](#24-how-long-this-takes)
25. [Troubleshooting](#25-troubleshooting)
26. [Common mistakes](#26-common-mistakes)
27. [Where to go next](#27-where-to-go-next)

---

## 1. What this project is

A CanSat: a soda-can-sized satellite that is carried up by a rocket or drone, released, and
transmits sensor telemetry to the ground while it descends under a parachute.

This one uses two Raspberry Pi Picos — one flying, one on the ground as a radio bridge —
connected by a 433 MHz LoRa link, with a PC application and a browser console displaying
the telemetry.

```text
 VEHICLE                                   GROUND
 ┌──────────────────────────┐              ┌─────────────────────────┐
 │ MPU-9250  ─┐              │              │ RA-02 ── Pico ── USB    │
 │ BMP280   ─┼─ Pico ─ RA-02│ ))))  ((((   │            (bridge)     │
 │ NEO-6M   ─┘   │          │   433 MHz    │              │          │
 │ microSD ──────┘          │              │              ▼          │
 └──────────────────────────┘              │      PC app / browser   │
                                           └─────────────────────────┘
```

Full detail: [software-architecture.md](design/software-architecture.md).

## 2. Try it right now, with no hardware

✅ Open [`ground-station/web/index.html`](../ground-station/web/index.html) in a browser.

Demo mode auto-starts and replays a complete drone-lift mission — `READY` → `FLIGHT` →
`LANDED` → `RECOVERY` — with live altitude and pressure plots, an attitude display, a 3D
flight view, link health, and a raw packet monitor. It deliberately drops one packet and
duplicates another so you can see the link-health counters react.

That is the same parser, validator and framing logic the real ground station uses. Nothing
is installed, nothing is connected.

## 3. PC software you need

| Software | Needed for | Notes |
|---|---|---|
| **Git** | Cloning this repository | Any recent version |
| **A C++17 compiler** (`g++` or `clang++`) | Host tests | On Windows, MSYS2 or WSL |
| **Python 3.10+** | Ground-station application | 3.12 is what CI uses |
| **Node.js 18+** | Web console tests | Optional; the console itself needs only a browser |
| **CMake 3.13+** | Building Pico firmware, and the host build via CTest | Only needed once you have hardware. If your system has neither CMake nor a build tool: `pip install cmake ninja` |
| **Pico SDK** | Building Pico firmware | See [step 13](#13-pico-sdk-setup) |
| **A browser** | Web console | Chrome or Edge for live USB (Web Serial); any browser for demo and file replay |

Python packages: `pip install -r ground-station/software/requirements.txt`.

## 4. Build and test the software

✅ No hardware, no SDK:

```bash
bash tools/build_host.sh
```

This compiles the shared telemetry library, the whole flight core, the ground-station
framing library and every host test, runs them, then runs the Python and Node suites.
Expect **4240 C++ assertions, 152 Python tests and 49 Node tests, all passing, with zero
compiler warnings.**

```bash
bash tools/check_pico_syntax.sh
```

Syntax-checks all ten Pico translation units against minimal SDK stubs — it proves the
firmware compiles, not that it runs.

The same suites also build through CMake, which is what CI uses:

```bash
cmake -S . -B build/host-cmake && cmake --build build/host-cmake --parallel && ctest --test-dir build/host-cmake --output-on-failure
```

31 targets, 5 CTest tests. On a machine with neither CMake nor a build tool,
`pip install cmake ninja` supplies both.

Replay a packet file through the real ground-station pipeline:

```bash
cd ground-station/software && python src/main.py replay ../../test-data/sample-mission.txt --team CAN-Team-01 --export logs/flight.csv
```

If all of that passes, your development environment is ready. Everything from here needs
parts.

## 5. Bill of materials

The confirmed BOM, with supplier SKUs, is in
[product-pages/README.md](hardware/product-pages/README.md). Summary:

| # | Part | Qty | Role |
|---:|---|---:|---|
| 1 | Raspberry Pi Pico | **2** | One flies, one is the ground bridge |
| 2 | SX1278 RA-02 433 MHz LoRa module | **2** | The radio link, one at each end |
| 3 | 433 MHz antenna | 2 | One per radio — **never power a radio without one** |
| 4 | IPEX-to-SMA pigtail | 2 | RA-02 has an IPEX connector, not SMA |
| 5 | MPU-9250 | 1 | Accelerometer + gyroscope |
| 6 | GY-BMP280-3.3 | 1 | Pressure + temperature → altitude |
| 7 | NEO-6M GPS module | 1 | Position (optional telemetry fields) |
| 8 | microSD card reader module | 1 | Onboard logging — **see the warning in [step 10](#10-the-power-problem--read-before-wiring)** |
| 9 | microSD card | 1 | Any small card; the firmware writes raw blocks, no filesystem |
| 10 | 1S LiPo battery, 3.7 V | 1 | Vehicle power |
| 11 | Universal prototype PCB, 10×10 cm | 1–2 | Mounting |
| 12 | Regulator | 1 | 🔴 **Not selected — see [step 10](#10-the-power-problem--read-before-wiring)** |
| 13 | Power switch + power LED | 1 each | 🔴 Required by the rulebook, not yet in the BOM |
| 14 | Parachute / descent system | 1 | 🔴 Not designed |
| 15 | Structure and egg chamber | 1 | 🔴 Not designed |

Also needed but not part of the electronics: an egg (the rulebook payload), wire, headers,
and whatever your structure is made from.

## 6. What each part does

- **Raspberry Pi Pico** — RP2040 microcontroller. The vehicle Pico runs one non-blocking
  loop: read sensors at 30 Hz, estimate attitude, run the mission state machine, build a
  telemetry packet every second, log to SD, drive the status LED. The ground Pico does
  nothing but receive packets and frame them onto USB with a CRC.
- **RA-02 (SX1278)** — LoRa transceiver. Long range at low power, at the cost of data
  rate: one full telemetry packet occupies ~330 ms of airtime, which is why the link runs
  at 1 Hz. See [link-budget.md](design/link-budget.md).
- **MPU-9250** — 3-axis accelerometer, 3-axis gyroscope and a 3-axis AK8963 magnetometer
  over I2C, giving roll, pitch and yaw. The magnetometer is a second die inside the same
  package with its own I2C address and, importantly, its own axis orientation — the driver
  rotates it into the body frame before anything else sees it.

  Yaw is an absolute magnetic angle **only after the magnetometer has been calibrated for
  the assembled airframe**: the battery, the radio and the wiring bias the field by tens of
  microtesla, which is the same order as the field being measured. Uncalibrated, yaw still
  stops drifting but is reported as relative. Every packet says which it is.
- **BMP280** — pressure and temperature over I2C. Altitude is derived from pressure
  relative to a ground reference captured on the pad.
- **NEO-6M** — GPS over UART at 9600 baud, emitting NMEA sentences. The firmware drains it
  without ever blocking, so a missing fix never delays telemetry.
- **microSD reader** — raw 512-byte block logging over SPI. No filesystem, so a brownout
  or impact reset resumes at the correct block instead of corrupting a FAT.
- **Antenna and pigtail** — the RA-02's IPEX connector needs a pigtail to reach an SMA
  antenna. Transmitting without an antenna can destroy the output stage.

## 7. Ordering guidance

- **Order two of everything on the radio path.** Two Picos, two RA-02s, two antennas, two
  pigtails. A ground station is not optional.
- **Order spares of the cheap fragile things**: one extra MPU-9250, one extra BMP280, one
  extra pigtail. IPEX connectors are easy to damage.
- **Check the antenna connector gender before ordering.** The BOM says SMA male; the
  supplier's live listing says RP-SMA female. These do not mate. This is
  [recorded as an open item](design/wiring.md) and must be resolved by looking at the
  physical parts.
- **Do not order the regulator yet.** The power design is not finished — see
  [step 10](#10-the-power-problem--read-before-wiring). Ordering the wrong regulator is
  the most likely way to waste money on this project.
- **Timing:** procurement is the long pole and is outside your control. Order the radio
  path first, since bring-up cannot start without it.

## 8. Tools you need

| Tool | For | Substitute |
|---|---|---|
| Soldering iron + solder | Headers, wiring | None |
| **Multimeter** | Verifying every voltage before connecting anything | **None — do not skip this** |
| Wire strippers, side cutters | Wiring | Scissors, badly |
| Breadboard + jumper wires | Bring-up before soldering | Solder directly, at your peril |
| Micro-USB cables ×2 | Flashing and powering both Picos | Must be data cables, not charge-only |
| Tweezers | IPEX connectors | Fingernails, painfully |
| Kitchen scale | Mass budget | Any scale with 1 g resolution |
| LiPo charger | Battery | None |

## 9. Checks to do the moment the parts arrive

Before soldering anything:

1. **Photograph every board, both sides.** Breakout variants differ, and the exact variant
   determines supply voltage, logic level, and whether there are pull-ups on board.
2. **Read the silkscreen** on each module and write down the actual pin labels. Do not
   assume the pinout from a tutorial.
3. **Measure the module regulators.** Power each module alone from a bench supply or a
   Pico's 3V3 pin and confirm what its `VCC` pin actually accepts.
4. **Check the antenna connector gender** against the pigtail.
5. **Update [hardware.md](hardware/hardware.md).** Every `TBD` in that document is a real
   unknown; replace them with what you measured. This is the step everyone skips and later
   regrets.

## 10. The power problem — read before wiring

🔴 **The vehicle's power system is not designed, and this blocks flight.**

Three specific problems, all documented in
[electrical-architecture.md](design/electrical-architecture.md) and
[sd-module-analysis.md](hardware/sd-module-analysis.md):

1. **No regulator is selected.** The obvious choice, an AMS1117-3.3, was assessed and
   **rejected**: a full 1S LiPo at ≈4.2 V does not clear its dropout under load.
2. **The rulebook requires a manual ON/OFF switch and a visible power LED.** Neither is in
   the BOM.

The microSD reader used to be the third problem here — many of these breakouts state a
4.5–5.5 V input and carry their own regulator and level shifters, which a 1S LiPo cannot
feed. The module that arrived is not one of them: it is a 3.3 V SPI board with no regulator and no level shifter, and runs from
the same 3.3 V rail as everything else. Its **current** draw during a write is still
unmeasured, and shares a regulator with the radio.

**What you can safely do today:** bring everything up on **USB power** with the Pico's
`3V3` output. That is enough for every bench test in [step 18](#18-subsystem-tests) and the
full end-to-end telemetry test.

**What you cannot do:** fly. Battery operation needs the regulator decision, a measured
current budget, and a brownout test under a radio transmit peak.

> **Never connect a LiPo directly to anything until you have measured the voltage at every
> pin with a multimeter.** A 1S LiPo at 4.2 V into a 3.3 V-only GPIO destroys the Pico.

## 11. Wiring

🟡 The pin assignment is fixed in firmware and documented in
[wiring.md](design/wiring.md). Full signal tables, bus-sharing rules and the power tree are
there; this is the summary.

| Pico pin | Signal | Goes to |
|---|---|---|
| GP4 | I2C0 SDA | MPU-9250 SDA **and** BMP280 SDA |
| GP5 | I2C0 SCL | MPU-9250 SCL **and** BMP280 SCL |
| GP6 | SPI chip select | microSD `CS` |
| GP7 | Interrupt in | MPU-9250 `INT` (optional) |
| GP12 | UART0 TX | NEO-6M `RX` |
| GP13 | UART0 RX | NEO-6M `TX` |
| GP14 | GPIO out | Status LED (through a resistor) |
| GP16 | SPI0 MISO | RA-02 `MISO` **and** microSD `MISO` |
| GP17 | GPIO out | RA-02 `NSS` (chip select) |
| GP18 | SPI0 SCK | RA-02 `SCK` **and** microSD `SCK` |
| GP19 | SPI0 MOSI | RA-02 `MOSI` **and** microSD `MOSI` |
| GP20 | GPIO out | RA-02 `RESET` |
| GP21 | GPIO in | RA-02 `DIO0` |
| GP22 | GPIO in | RA-02 `DIO1` (optional) |
| GP26 | ADC0 | Battery sense divider (reserved, not yet designed) |

**The ground-station Pico uses the same SPI and RA-02 pins** — GP16, GP17, GP18, GP19,
GP20, GP21. It has no sensors, no SD card and no GPS. Wiring one teaches you the other.

Three rules that will save you a day of debugging:

- **Two devices share SPI0** (radio and SD). Both must release MISO when deselected. If
  one holds the line, neither works. Test each alone first.
- **Two devices share I2C0** (MPU-9250 at 0x68, BMP280 at 0x76). They must be on different
  addresses; scan the bus and confirm both respond before wiring anything else.
- **TX goes to RX.** The GPS's TX connects to the Pico's RX (GP13). Getting this backwards
  produces silence, not an error.

## 12. Assembly order

🟡 Build in this order, testing at each step. Do not solder the final board until every
subsystem has worked on a breadboard.

1. Solder headers onto the Picos and the modules.
2. Breadboard: Pico + status LED only. Confirm it blinks.
3. Add the I2C sensors. Scan the bus.
4. Add the GPS. Watch raw NMEA.
5. Add the radio, **with its antenna attached**.
6. Add the microSD reader last — it is the highest-risk part.
7. Only then transfer to the prototype PCB, one subsystem at a time, re-testing each.
8. 🔴 Mechanical integration — structure, egg chamber and parachute are not designed.

## 13. Pico SDK setup

🟡 Follow the official [Raspberry Pi Pico SDK setup](https://github.com/raspberrypi/pico-sdk).
You need:

- The SDK cloned somewhere, with `PICO_SDK_PATH` exported to point at it.
- `pico_sdk_import.cmake` copied from the SDK into this repository's root, next to
  `CMakeLists.txt`.
- The ARM cross-compiler toolchain the SDK documentation lists for your OS.

Verify with:

```bash
echo $PICO_SDK_PATH
```

If that is empty, the CMake build will silently produce only the host targets and no
firmware images.

## 14. Configure your team identity

🔴 **The firmware refuses to run until you do this.** The placeholder `CAN-Team-XX` is
rejected by both the packet formatter and the startup validator, on purpose — an
unidentified packet is a non-compliant packet.

Edit [`firmware/flight-computer/src/pico/main.cpp`](../firmware/flight-computer/src/pico/main.cpp):

```cpp
config.team_id = "CAN-Team-25";   // <-- your registered competition identifier
```

Also decide the radio mode. `RadioMode::test` uses sync word `0xF3`; the official launch
requires `0xA5`. Both the vehicle and the bridge must be changed together — the bridge's
sync word is in
[`firmware/ground-station/src/pico/main.cpp`](../firmware/ground-station/src/pico/main.cpp).

**Rehearse the switch before launch day.** A mismatched sync word receives nothing and
looks exactly like a dead radio.

## 15. Build the firmware

✅ Run on 2026-09-05 against SDK 2.3.0 and arm-none-eabi-gcc 15.2.1. Both images built.

```bash
cmake -S . -B build/pico -G Ninja -DCMAKE_BUILD_TYPE=Release -DPICO_BOARD=pico
cmake --build build/pico --parallel
```

**Windows, using the VS Code extension's toolchain.** The extension installs everything under
`%USERPROFILE%\.pico-sdk\` and does not put any of it on `PATH`, so set these in the shell
first — this is the exact invocation that produced the images, in PowerShell:

```bash
$p = "$env:USERPROFILE\.pico-sdk"
$env:PICO_SDK_PATH = "$p\sdk\2.3.0"
$env:PICO_TOOLCHAIN_PATH = "$p\toolchain\15_2_Rel1"
$env:Path = "$p\toolchain\15_2_Rel1\bin;$p\ninja\v1.13.2;$p\cmake\v4.3.4\bin;$env:Path"
```

`-G Ninja` is not optional on Windows: without it CMake picks whatever default generator it
finds, which will not be the one the SDK expects.

**Do not use the extension's `Import Pico Project` on this repository.** It rewrites project
files, and this tree's [CMakeLists.txt](../CMakeLists.txt) is hand-written to build the host
tests and the firmware from one source tree. To get the toolchain without touching the repo,
create a throwaway project with `New Pico Project` in a folder outside it; the SDK download is
shared.

Three images appear only when the SDK is present:

| File | Flash onto |
|---|---|
| `cansat_pico_firmware.uf2` | The vehicle Pico |
| `cansat_ground_bridge_firmware.uf2` | The ground-station Pico |
| `cansat_bringup_firmware.uf2` | The vehicle Pico, **temporarily**, for bring-up only |

**The bring-up image is a diagnostic, not flight software.** The flight firmware speaks only
over LoRa and writes nothing to USB, so a vehicle with no radio attached shows you nothing at
all — which makes Gate 3 impossible to take. This image drives the same drivers and prints
what they find over USB: two I²C bus scans, the barometer's chip ID, the IMU's `WHO_AM_I`,
and 100 stationary samples reduced to the mean and standard deviation that
[bring-up rows 3.2–3.4](testing/bring-up-record.md) ask for, each marked against the limit
from `Configuration`.

Flash the flight image back over it when you are done. The two cannot be confused on the
vehicle — the bring-up image holds the status LED solid and never blinks.

If the build fails on a link-profile `static_assert`, your radio settings cannot meet the
1 Hz telemetry minimum — read the message and [link-budget.md](design/link-budget.md).
That is the build refusing to produce firmware that cannot comply.

## 16. Flash both Picos

🟡

1. Hold **BOOTSEL** on the Pico while plugging in USB.
2. A drive called `RPI-RP2` appears.
3. Copy the `.uf2` onto it. The Pico reboots and runs the firmware.

**Label the two Picos physically.** They look identical and run completely different
firmware. Flashing the bridge image onto the vehicle is a mistake you will make once.

## 17. Bring-up order

🟡 Follow the sequenced [bring-up order](design/wiring.md#bring-up-order). Each step is a
gate: if one fails, stop and fix it rather than carrying the fault forward. The order
exists because a fault in a shared bus is far easier to find with one device on it.

## 18. Subsystem tests

🟡 The full matrix, with pass criteria, is the hardware test plan in
[test-plan.md](testing/test-plan.md). In order:

| # | Test | Pass criterion |
|---:|---|---|
| 1 | Pico alone | GP14 LED blinks, USB serial enumerates |
| 2 | I2C scan | MPU-9250 and BMP280 both acknowledge, on different addresses |
| 3 | IMU | Stationary: total acceleration ≈ 1 g, rotation rates ≈ 0 |
| 4 | Barometer | Pressure within a few hundred Pa of a local reference |
| 5 | Calibration | `CAL-1` appears in telemetry within the sample budget while still |
| 5b | Acquisition rate | Loop actually achieves 30 Hz ([sensor-rates.md](design/sensor-rates.md)) |
| 6 | GPS | Raw NMEA arrives; fix acquired **outdoors**; checksum errors ≈ 0 |
| 7 | Radio identity | RA-02 version register reads back over SPI |
| 8 | Bench link | Packets received end to end at sync word `0xF3` |
| 9 | Range test | Acceptable loss at launch distance, antenna mounted as flown |
| 10 | microSD alone | Block read and write on its own supply |
| 11 | Shared SPI | Radio and SD both work with the other present |
| 12 | Packet rate | Sustained 1 Hz, no gaps in numbering |
| 13–14 | Battery | 🔴 Blocked on the power design |
| 15 | Watchdog | Forced hang reboots and telemetry resumes by itself |
| 16 | Power-on | LED lights immediately, telemetry starts with no manual trigger |
| 17 | Sync word | `0xA5` configuration verified before the official launch |

**GPS needs open sky.** A first fix indoors will not happen; give it several minutes
outdoors with a clear view before concluding anything is broken.

## 19. End-to-end telemetry test

🟡 With both Picos flashed and the vehicle powered:

```bash
cd ground-station/software && python src/main.py live --port COM5 --team CAN-Team-25
```

Replace `COM5` with your bridge Pico's port (`/dev/ttyACM0` on Linux, `/dev/cu.usbmodem*`
on macOS). Or open the web console and connect over Web Serial in Chrome or Edge.

You should see packet numbers advancing `P-001`, `P-002`, `P-003`… at 1 Hz, with no gaps,
and the mission state showing `READY`.

Full operating procedures are in the [runbook](operations/runbook.md).

## 20. Fault injection

🟡 The vehicle is designed to degrade rather than stop. Prove it before you trust it —
deliberately break each subsystem and confirm telemetry continues:

| Break this | Expected behaviour |
|---|---|
| Unplug the GPS | Telemetry continues; GPS fields drop out; a fault is recorded |
| Remove the SD card | Telemetry continues; SD logging disables itself after repeated failures |
| Disconnect the barometer | Packets are suppressed (mandatory data incomplete) but the loop stays alive and resumes when reconnected |
| Power-cycle the ground PC | The bridge Pico keeps running; reconnecting resumes reception |
| Kill the dashboard | The logger and parser keep working |
| Corrupt a serial byte | The frame is reported as a CRC error, never parsed as telemetry |

Anything that stops telemetry entirely, other than losing both the IMU and the barometer,
is a bug worth reporting.

## 21. Pre-flight

🟡 See the [runbook's launch-day procedure](operations/runbook.md#launch-day-procedure) for
the full checklist. The items people forget:

- **Set `reference_pressure_pa`** from a field barometer reading on the day. Altitude is
  meaningless otherwise.
- **Switch the sync word to `0xA5`** on **both** Picos and re-flash both.
- **Charge the battery and check the antenna** is connected at both ends.
- **Power off during other teams' launches** — the rulebook requires it.
- **Hold the vehicle still** during startup calibration so the gyro bias is captured
  correctly.

## 22. Launch day

Follow the [runbook](operations/runbook.md#launch-day-procedure) — it is sequenced by
countdown time, from T-60 to recovery, and tells you what each telemetry field should look
like at each stage.

## 23. Post-flight

1. Recover the vehicle; do not power it off until you have confirmed the ground station has
   the data.
2. Pull the microSD card and read the raw block log.
3. Export the received telemetry to CSV from the ground-station application.
4. Compare the two: the SD log is complete, the radio log has gaps. The difference is your
   measured packet loss.
5. Record the results in [test-plan.md](testing/test-plan.md) — the hardware test table
   exists to be filled in.

## 24. How long this takes

Engineering time only. **Procurement is separate and outside your control** — shipping,
customs and stock vary too much to estimate here, so plan around your own supplier's
quoted lead time and order the radio path first.

| Phase | Beginner | Intermediate | Experienced |
|---|---|---|---|
| Set up PC software, run the host tests | 2–4 h | 1 h | 20 min |
| Read the documentation and understand the system | 6–10 h | 3–4 h | 1–2 h |
| Pico SDK setup and first firmware build | 3–6 h | 1–2 h | 30 min |
| Inspect parts, document breakouts, measure voltages | 3–5 h | 2 h | 1 h |
| **Resolve the power design** 🔴 | 8–16 h | 4–8 h | 2–4 h |
| Breadboard assembly and wiring | 6–10 h | 3–4 h | 1–2 h |
| Sensor bring-up (I2C, IMU, barometer) | 4–8 h | 2–3 h | 1 h |
| GPS bring-up (including waiting for a fix) | 2–4 h | 1–2 h | 1 h |
| Radio bring-up and first bench link | 6–12 h | 3–5 h | 1–2 h |
| microSD bring-up ⚠️ highest risk | 4–12 h | 2–6 h | 1–3 h |
| Shared-bus integration (SPI with both devices) | 3–8 h | 2–4 h | 1 h |
| End-to-end telemetry test | 2–4 h | 1–2 h | 30 min |
| Range test | 3–5 h | 2–3 h | 2 h |
| Fault injection | 2–4 h | 1–2 h | 1 h |
| Soldering the final board | 4–8 h | 2–4 h | 2 h |
| Mechanical: structure, egg chamber, parachute 🔴 | 15–30 h | 8–15 h | 5–10 h |
| Pre-flight rehearsal | 3–5 h | 2 h | 1–2 h |
| **Total engineering time** | **~75–150 h** | **~40–70 h** | **~22–35 h** |

These are working hours, not calendar time. Budget generously: bring-up is where estimates
fail, and the microSD reader and the power design are the two items most likely to consume
a multiple of their estimate.

**Do in parallel while waiting for parts:** everything in Part 1, all documentation
reading, the SDK setup, the mechanical design, and the power decision. That is a
substantial fraction of the total, and none of it needs hardware.

## 25. Troubleshooting

The full table is in the [runbook](operations/runbook.md#troubleshooting). The ones you
will hit first:

| Symptom | Most likely cause |
|---|---|
| Firmware refuses to start, logs a config fault | `team_id` is still `CAN-Team-XX` — see [step 14](#14-configure-your-team-identity) |
| Build fails on a `static_assert` about airtime | Radio settings cannot meet 1 Hz; see [link-budget.md](design/link-budget.md) |
| Build fails on a barometer conversion time | Sensor rate faster than the barometer; see [sensor-rates.md](design/sensor-rates.md) |
| No packets at all | Sync word mismatch between the two Picos, or one is flashed with the wrong image |
| No packets, radio initialises fine | Antenna missing, or the two ends disagree on spreading factor or bandwidth |
| I2C scan finds nothing | SDA/SCL swapped, no pull-ups, or the module is not powered |
| I2C scan finds one of two sensors | Address conflict, or one module is dead |
| GPS silent | TX/RX swapped — the GPS TX goes to Pico **RX** (GP13) |
| GPS never gets a fix | Indoors. Go outside and wait several minutes |
| Radio works, SD does not, or vice versa | Shared SPI: one device is not releasing MISO |
| Packets arrive but are rejected | Team identifier mismatch between vehicle and ground station |
| CRC errors on the serial link | Cable or baud problem, not a telemetry problem — the distinction is the point of the framing |
| Altitude reads wildly wrong | `reference_pressure_pa` not set for the day |
| Yaw drifts steadily | The magnetometer is not correcting it. Check the packet's `YR-` tag: `YR-G` means yaw is running on the gyroscope alone — either the AK8963 is not answering (a module that is really an MPU-6500), the field is outside the 20–70 µT gate, or no calibration has been loaded |
| Yaw is steady but wrong by a constant | Hard iron. Run the figure-of-eight calibration on the fully assembled vehicle, battery and radio included, not on a bare board |

## 26. Common mistakes

1. **Powering the RA-02 without an antenna.** Can destroy the output stage. Attach first.
2. **Connecting a LiPo before measuring.** 4.2 V into a 3.3 V pin kills the Pico.
3. **Flashing the wrong image onto the wrong Pico.** Label them.
4. **Changing the sync word on only one end.** Silent, total link failure.
5. **Assuming a breakout matches its chip's datasheet.** It does not. Boards add
   regulators, level shifters and pull-ups.
6. **Soldering everything before testing anything.** Debug on a breadboard first.
7. **Testing GPS indoors** and concluding the module is broken.
8. **Skipping the shared-bus test.** Radio and SD each work alone and fail together.
9. **Leaving `team_id` as the placeholder.** The firmware catches this; do not work around it.
10. **Trusting a documented `TBD`.** Every `TBD` in the hardware documents is a real
    unknown, not an oversight.

## 27. Where to go next

| You want to… | Read |
|---|---|
| Understand the whole system | [README](../README.md) |
| Understand the software | [software-architecture.md](design/software-architecture.md) |
| Wire it | [wiring.md](design/wiring.md) |
| Know why telemetry is 1 Hz | [link-budget.md](design/link-budget.md) |
| Know why sensors are 30 Hz | [sensor-rates.md](design/sensor-rates.md) |
| Know the packet format | [telemetry-protocol.md](design/telemetry-protocol.md) |
| Run a mission | [runbook.md](operations/runbook.md) |
| Know what is tested | [test-plan.md](testing/test-plan.md) |
| Measure the vehicle on hardware day | [bring-up-record.md](testing/bring-up-record.md) |
| Know what the competition requires | [requirements.md](requirements/requirements.md) |
| Know what is done and what is not | [timeline.md](project/timeline.md) |
| Check the project against its own claims | [repository audit](audit/2026-09-04-repository-audit.md) |
