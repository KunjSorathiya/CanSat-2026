# Operations Runbook

Step-by-step procedures for building the firmware, configuring the vehicle, running the
ground station, operating on launch day, and analysing the flight afterwards.

> [!WARNING]
> This runbook describes the procedure the software is designed for. **No step has been
> executed on real hardware.** Treat every hardware action as first-time bring-up and
> follow the [bring-up order](../design/wiring.md#bring-up-order) before attempting a
> full mission rehearsal.

---

## Contents

- [Before every session](#before-every-session)
- [Configuring the vehicle](#configuring-the-vehicle)
- [Building the firmware](#building-the-firmware)
- [Running the ground station](#running-the-ground-station)
- [Launch-day procedure](#launch-day-procedure)
- [Reading telemetry in flight](#reading-telemetry-in-flight)
- [Post-flight analysis](#post-flight-analysis)
- [Troubleshooting](#troubleshooting)
- [Emergency actions](#emergency-actions)

---

## Before every session

```bash
bash tools/build_host.sh
```

Everything must pass before firmware is flashed or a mission is flown. If any suite fails,
stop and fix it — a red suite means the packet format, the state machine, or the framing is
not behaving as documented.

---

## Configuring the vehicle

Two values must be set before any official test or launch. Both live in
[`firmware/flight-computer/src/pico/main.cpp`](../../firmware/flight-computer/src/pico/main.cpp):

```cpp
config.team_id = "CAN-Team-01";               // registered competition identifier
config.radio_mode = flight::RadioMode::test;  // ::official for the launch
```

| Setting | Test configuration | Launch configuration |
|---|---|---|
| `team_id` | The registered identifier — never `CAN-Team-XX` | Same |
| `radio_mode` | `RadioMode::test` → sync word `0xF3` | `RadioMode::official` → sync word `0xA5` |
| Bridge `SYNC_WORD` | `0xF3` in [ground `main.cpp`](../../firmware/ground-station/src/pico/main.cpp) | Change to `0xA5` |

> [!CAUTION]
> **The sync words must match on both ends, and the wrong one during another team's launch
> can incur penalties.** `0xF3` is for pre-launch testing only; `0xA5` is the official
> launch configuration. Changing the vehicle without changing the bridge produces a silent
> total loss of telemetry — the receiver simply never sees a packet.

The firmware refuses to run with the placeholder identity: `validate_config()` rejects
`CAN-Team-XX`, raises a critical `config_invalid` fault, and the vehicle enters `FAULT`.
This is deliberate — it makes flying with an unset identity impossible.

Other tunables worth reviewing before a flight, all in
[`config.hpp`](../../firmware/flight-computer/include/flight/config.hpp):

| Setting | Default | Review when |
|---|---:|---|
| `telemetry_period_ms` | 500 | Raising the rate; 1000 is the hard ceiling for the 1 Hz minimum |
| `reference_pressure_pa` | 101325 | Always — set it from a field barometer reading on the day |
| `launch_accel_mps2` / `launch_altitude_gain_m` | 30 / 15 | After the first flight data exists |
| `arming_delay_ms` | 3000 | If the pad procedure takes longer to settle |
| `battery_divider_ratio` | 0 (disabled) | Only after the divider is built and measured |

---

## Building the firmware

### Host builds and tests — no SDK needed

```bash
bash tools/build_host.sh
bash tools/check_pico_syntax.sh
```

### Pico firmware image

Requires the standard Raspberry Pi Pico SDK setup: `PICO_SDK_PATH` exported, and
`pico_sdk_import.cmake` placed next to the top-level `CMakeLists.txt`.

```bash
cmake -S . -B build/pico
cmake --build build/pico --parallel
```

Two targets appear only when the SDK is present:

| Target | Image for |
|---|---|
| `cansat_pico_firmware` | The vehicle |
| `cansat_ground_bridge_firmware` | The ground-station bridge |

Flash by holding BOOTSEL while connecting USB, then copying the `.uf2` onto the
`RPI-RP2` drive that appears.

---

## Running the ground station

Run from `ground-station/software/`. The core needs only the Python standard library.

### Live from the bridge Pico

```bash
python src/main.py live --port COM5 --team CAN-Team-01 --framed
```

Use the actual serial port: `COM5` style on Windows, `/dev/ttyACM0` style on Linux. The
`--framed` flag matches the bridge's `$len,crc,payload` framing and should always be used
with real hardware. Requires `pyserial`; plots additionally need `matplotlib`, and the
dashboard runs without it.

### Live without the dashboard — headless, prints link health

```bash
python src/main.py live --port COM5 --team CAN-Team-01 --framed --no-dashboard
```

### Rehearsal from a file, no hardware

```bash
python src/main.py live --replay packets.txt --rate 2 --team CAN-Team-01
```

### Offline replay with a CSV export

```bash
python src/main.py replay packets.txt --team CAN-Team-01 --output logs --export logs/flight.csv
```

### Web console

Open [`ground-station/web/index.html`](../../ground-station/web/index.html) in a browser.
Demo mode starts on its own. **File…** replays a packet file or a `raw_packets.tsv`.
**Web Serial** connects directly to the bridge Pico — Chrome or Edge, over `https` or
`localhost`.

The web console is the fastest way to see link health and mission state; the Python
pipeline is what writes the authoritative logs. Run both when it matters.

### What gets written

| File | Content |
|---|---|
| `logs/raw_packets.tsv` | Every received line with its receipt timestamp — nothing discarded, including malformed packets and CRC failures |
| `logs/telemetry.csv` | One row per packet with validation status, sequence notes and the raw packet |

---

## Launch-day procedure

### T-60 — Preparation

- [ ] Run `bash tools/build_host.sh`; everything passes
- [ ] `team_id` set to the registered identifier on the vehicle
- [ ] `radio_mode = RadioMode::official` on the vehicle
- [ ] Bridge `SYNC_WORD = 0xA5`
- [ ] Both images flashed from the same source revision
- [ ] `reference_pressure_pa` set from a field barometer reading
- [ ] Battery charged; microSD card inserted, blank and seated
- [ ] Antennas attached to **both** radios — never power a radio without its antenna
- [ ] Egg installed and secured; parachute packed for immediate deployment

### T-30 — Ground station up

- [ ] Bridge Pico connected; the serial port enumerates
- [ ] Ground station started with `--framed` and the correct `--team`
- [ ] Bridge status frames arriving once a second (`#state=RX radio=1 …`)
- [ ] Log directory writable and empty of previous runs

### T-10 — Vehicle power-on

- [ ] Vehicle placed on the pad, level and stationary
- [ ] Power switched on; the LED lights **immediately**
- [ ] Telemetry starts automatically with no manual trigger
- [ ] First packet is `P-001`
- [ ] The `MODE` tag advances `INIT` → `SELF_TEST` → `READY`
- [ ] `CAL-1` appears — calibration settled; keep the vehicle still until it does
- [ ] `ARM-1` appears after the arming delay
- [ ] `FAULTS-0`, or every active fault understood and accepted
- [ ] Altitude reads approximately zero at the ground baseline
- [ ] Packet numbering is unbroken and the rate is steady

> [!IMPORTANT]
> Keep the vehicle **still** until `CAL-1`. Calibration needs stationary samples; motion
> forces a best-effort result in which gyro and accelerometer bias are not applied, and
> raises a `calibration` warning.

### T-0 — Launch

- [ ] Other teams' CanSats powered off during your launch, and yours off during theirs
- [ ] Recording confirmed on the ground station
- [ ] Watch for `MODE-FLIGHT` at release

### Descent and landing

- [ ] Altitude rises during the lift and falls during descent
- [ ] `MODE-LANDED` at impact
- [ ] Telemetry continues for at least 5 s after impact — a mandatory requirement
- [ ] `MODE-RECOVERY` follows
- [ ] Keep receiving until the vehicle is physically recovered

### Recovery

- [ ] Vehicle located and powered off
- [ ] microSD card removed and its contents copied before anything else
- [ ] Ground-station logs copied and backed up in two places
- [ ] Egg condition and structural state photographed

---

## Reading telemetry in flight

A packet looks like this:

```text
CAN-Team-01; P-042; Ti-00:01:23:450; A-118.4; Pr-99821.33; T-24.6; Ro-2.1; Pi--1.4; Ya-15.9; AX-0.12; AY--0.31; AZ-9.79; GP-Lat-21.164500; GP-Lon-72.784800; GP-Alt-121.3; MODE-FLIGHT; FAULTS-0; CAL-1; ARM-1;
```

Everything up to `AZ-` is mandatory and fixed by the rulebook. Everything after it is
optional and appended by our firmware.

| Tag | Meaning | Watch for |
|---|---|---|
| `MODE` | Mission state | `FAULT` at any point; `FLIGHT` at release; `LANDED` at impact |
| `FAULTS` | Count of currently active faults | Anything above 0 before launch |
| `CAL` | 1 once calibration settled | Must be 1 before launch |
| `ARM` | 1 once launch detection is enabled | Must be 1 before launch |
| `GP-*` | GPS position, only when a fix exists | Absence is normal indoors and is not a fault |

Link health on the ground station:

| Indicator | Healthy | Investigate |
|---|---|---|
| Rate | Steady at the configured rate, 2 Hz by default | Falling rate means range or power trouble |
| Loss % | Near zero | Rising loss means range, antenna or orientation |
| CRC errors | Zero | Non-zero means transport corruption, not sensor trouble |
| Missing | Zero | Gaps in numbering mean lost packets over the air |
| Duplicates | Zero | Duplicates suggest a receiver or bridge problem |

---

## Post-flight analysis

Four hours are allowed after launch. The three mandatory graphs are altitude, temperature
and pressure, each against time or packet number.

1. Copy `logs/telemetry.csv` and `logs/raw_packets.tsv`, and the microSD log, to a safe
   location — work only on copies.
2. Export a clean CSV if needed:
   ```bash
   python src/main.py replay logs/raw_packets.tsv --team CAN-Team-01 --export analysis/flight.csv
   ```
3. Produce the three mandatory graphs.
4. Optional analysis that scores additional credit: acceleration profile, orientation
   history, descent-rate derivation, packet-loss versus altitude, GPS ground track,
   correlation between barometric and GPS altitude.
5. Compare the transmitted stream against the onboard SD log: differences are radio loss,
   not sensor loss, and the difference itself is a useful result.

---

## Troubleshooting

<details>
<summary><b>No packets at the ground station</b></summary>

1. **Sync words** — the most common cause. Vehicle and bridge must both be `0xF3` or both
   `0xA5`. A mismatch is a silent total loss.
2. Is the bridge sending status frames? No `#state=RX` line means the bridge or the serial
   port is the problem, not the link.
3. `radio=0` in the status line means the bridge cannot talk to its RA-02 — check SPI
   wiring and the reset line.
4. Antennas attached at both ends?
5. Is the vehicle's LED blinking at the `FAULT` rate (very fast, 60 ms)?
</details>

<details>
<summary><b>Packets arrive but are rejected</b></summary>

Check the `error` column in `telemetry.csv`.

| Error | Cause |
|---|---|
| `invalid team identifier` | The vehicle still carries `CAN-Team-XX`, or `--team` does not match |
| `unexpected team identifier` | Another team's packet, or a `--team` typo |
| `invalid <prefix> precision` | Decimal places do not match the rulebook — a formatter change |
| `missing mandatory fields` | Truncated packet, usually a transport problem |
</details>

<details>
<summary><b>Gaps in packet numbering</b></summary>

Gaps mean packets were lost **over the air** — the vehicle never skips a number. A
suppressed packet does not consume one, so numbering stays dense at the transmitter.
Compare against the onboard SD log to separate radio loss from suppression.
</details>

<details>
<summary><b>CRC errors but the packets look fine</b></summary>

CRC errors are transport-level corruption between the bridge and the PC — a cable, a
connector, or serial noise. They are counted separately from packet validation precisely so
this distinction stays visible. Reseat the USB cable and try another port.
</details>

<details>
<summary><b>Calibration never reaches CAL-1</b></summary>

The vehicle is not stationary enough: per-axis gyro standard deviation must be under
2 °/s and the acceleration magnitude within 1.5 m/s² of 1 g, over 80 samples. Wind, a
vibrating surface or handling all prevent it. After 20 s it resolves best-effort — the
barometric reference is still used, but gyro and accelerometer bias are not applied, and a
`calibration` warning is raised. The mission is never blocked by this.
</details>

<details>
<summary><b>Altitude does not read zero on the pad</b></summary>

Altitude is reported relative to the power-on ground baseline captured during calibration
(`altitude_relative_to_baseline`). If it reads non-zero, calibration probably did not
capture a clean barometric reference. Power-cycle on a still surface and watch for `CAL-1`.
</details>

<details>
<summary><b>The vehicle rebooted mid-flight</b></summary>

Look for the `watchdog_reboot` fault. The 2 s hardware watchdog reboots a hung loop, and
telemetry restarts automatically — but the mission clock and packet numbering restart from
zero. The onboard block log increments its boot count and resumes at the correct block
instead of overwriting earlier data.
</details>

---

## Emergency actions

| Situation | Action |
|---|---|
| Vehicle unresponsive before launch | Power off, wait, power on. Telemetry restarts automatically; `P-001` restarts the numbering |
| Radio fails on the pad | Firmware retries with bounded back-off. If the LED shows `FAULT`, power-cycle. Onboard SD logging continues regardless |
| SD failure | Logging disables itself after 10 consecutive write failures; the mission continues. Not flight-critical |
| Another team is launching | Power your CanSat completely off. This is a rulebook restriction |
| Vehicle lands out of sight | Keep the ground station recording — telemetry continues into `RECOVERY`, and the last GPS fix is in the log |

---

Related: [test-plan.md](../testing/test-plan.md) ·
[wiring.md](../design/wiring.md) ·
[software-architecture.md](../design/software-architecture.md) ·
[telemetry-protocol.md](../design/telemetry-protocol.md)
