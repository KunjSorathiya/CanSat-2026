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
- [Launch configuration — switching the sync word](#launch-configuration--switching-the-sync-word)
- [Radio silence — when your vehicle must be off](#radio-silence--when-your-vehicle-must-be-off)
- [Building the firmware](#building-the-firmware)
- [Running the ground station](#running-the-ground-station)
- [Launch-day procedure](#launch-day-procedure)
- [If the link reads 1 Hz](#if-the-link-reads-1-hz)
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
config.team_id = "CAN-Team-25";               // registered competition identifier
config.radio_mode = flight::RadioMode::official;  // 0xA5: the organizers' station hears nothing else
```

| Setting | Test configuration | Launch configuration |
|---|---|---|
| `team_id` | The registered identifier — never `CAN-Team-XX` | Same |
| `radio_mode` | `RadioMode::official` → sync word `0xA5` — **shipped this way since 2026-09-11** | Same |
| Bridge `SYNC_WORD` | `kOfficialSyncWord` (`0xA5`) in [ground `main.cpp`](../../firmware/ground-station/src/pico/main.cpp) | Same |

> [!CAUTION]
> **The sync words must match on both ends, and the wrong one during another team's launch
> incurs penalties.** `0xF3` is the rulebook's pre-launch testing word and `0xA5` the official
> launch configuration — but **both Picos now ship on `0xA5` for testing too**, because the
> organizers' ground station listens on nothing else. So the vehicle must be **off** whenever
> another team is launching, which the rulebook requires anyway. Changing the vehicle without
> changing the bridge produces a near-total loss of telemetry — the receiver sees at most the
> few packets a mismatched sync filter lets through.
>
> **The 2026 revision made stray transmission five times more expensive: -1 point per 2
> packets, where the earlier rulebook said per 10.** This vehicle transmits at **1.43 Hz**, so
> that is **0.71 points per second**. A CanSat accidentally left on during someone else's
> launch throws away the entire 25-point telemetry section in **35 seconds**, and twice that
> in seventy. The faster rate this vehicle now runs at makes that window *shorter*, not
> longer.
>
> The firmware cannot help here — it is *required* to transmit automatically on power-up.
> **The manual switch is the only control, and switch discipline is a scored activity.**

The firmware refuses to run with the placeholder identity: `validate_config()` rejects
`CAN-Team-XX`, raises a critical `config_invalid` fault, and the vehicle enters `FAULT`.
This is deliberate — it makes flying with an unset identity impossible.

**When it refuses, it says which rule refused.** Thirty rules can reject a configuration,
and the vehicle prints the one that did over USB serial at startup:

```text
CONFIG REFUSED: post_impact_transmission_ms must be >= 5000 (rulebook post-impact minimum)
```

If the vehicle sits in `FAULT` with no telemetry, connect a serial monitor before changing
anything: the first line it prints is the answer.

Other tunables worth reviewing before a flight, all in
[`config.hpp`](../../firmware/flight-computer/include/flight/config.hpp):

| Setting | Default | Review when |
|---|---:|---|
| `telemetry_period_ms` | **700** (1.43 Hz) | Rarely. **1000 ms is refused**, not a ceiling to sit on: the rulebook's 1 Hz is a floor, so `validate_config()` and a `static_assert` both reject any period above `kMaxTelemetryPeriodMs` (950 ms). Going *faster* than 700 needs a wider bandwidth or a shorter packet, and `validate_config()` refuses a period the radio cannot deliver — read [link-budget.md](../design/link-budget.md) first |
| `transmit_gps` | `true` | Rarely. The position is on the air in every normal-flight packet and every rich one after `MAX_RATE`, because the organizers score only transmitted sensors. It costs 51 bytes at its widest, inside the 200-byte budget, and `validate_config()` refuses a budget that cannot hold it |
| `sensor_period_ms` | 33 | Changing the acquisition rate — `validate_config()` refuses a period the barometer cannot feed ([sensor-rates.md](../design/sensor-rates.md)) |
| `reference_pressure_pa` | 101325 | Always — set it from a field barometer reading on the day |
| `launch_accel_mps2` / `launch_altitude_gain_m` | 30 / 15 | After the first flight data exists |
| `arming_delay_ms` | 3000 | If the pad procedure takes longer to settle |
| `battery_divider_ratio` | 0 (disabled) | Only after the divider is built and measured. **While it is 0 the reported battery voltage is the raw ADC pin voltage, not the cell voltage** — `battery_voltage_is_scaled` in the health snapshot says which you are looking at, and the low-battery fault stays disabled because a pin reading cannot judge a cell |

---

## Launch configuration — switching the sync word

**This is the procedure [TEL-025](../requirements/requirements.md) asks for.** The rulebook
fixes two sync words: `0xF3` for pre-launch testing, `0xA5` for the official launch. The
vehicle must be on `0xA5` for its own launch — and since `0xA5` is now the word it ships on,
it must be switched off during every other team's.

### Why this needs a procedure rather than a note

The sync word is **not a field in the packet**. It is encoded into the two sync symbols at
the end of the LoRa preamble — the byte splits into two nibbles, each multiplied by 8, so
`0xF3` becomes symbols 120 and 24 and `0xA5` becomes 80 and 40. A receiver correlates
against the symbols *it* is configured for, and on a mismatch the correlator never locks.
Preamble detect never fires, the header is never decoded, the CRC is never evaluated.

**So a mismatch is silence, not an error.** No packet, no CRC failure, no counter moving. It
is indistinguishable from a dead antenna, a dead module, or a vehicle that was never
switched on — and it is discovered at a launch, from a receiver that is working perfectly.

It is a compile-time constant in **two separate images**, which is why one is easy to change
and the other easy to forget.

### The guard that catches you

`bash tools/build_host.sh` now refuses a working tree whose two ends disagree:

```text
FAIL  vehicle and bridge agree: OFFICIAL sync word (0xA5)   [vehicle=official bridge=test]
```

and on a consistent tree it **names the configuration you would fly**:

```text
  ok  vehicle and bridge agree: OFFICIAL sync word (0xA5)
```

Read that line. It is the cheapest confirmation available, it costs nothing, and it is the
only check that runs before the images exist.

### Both Picos are already on the launch word

Since 2026-09-11 the vehicle and the bridge both ship on `0xA5`, for bench work as well as the
launch. The organizers' ground station listens on `0xA5` and nothing else, and on the
2026-09-10 range test — vehicle on `0xF3` — it heard only a few packets while the team's own
station heard every one. So there is no switch to make at T-60, only confirmations:

- [ ] **1 · Prove the tree agrees**, before building anything:

      bash tools/build_host.sh

      The line must read `vehicle and bridge agree: OFFICIAL sync word (0xA5)`.

- [ ] **2 · Build both images from that one tree**, in one command, so they cannot come from
      different revisions:

      cmake -S . -B build/pico && cmake --build build/pico --parallel

- [ ] **3 · Flash both Picos.** Both. This is the step that is actually skipped.
- [ ] **4 · Confirm on the vehicle.** Serial monitor, startup summary:

      team CAN-Team-25 | radio OFFICIAL 0xA5 | telemetry every 700 ms (1.43 Hz)

- [ ] **5 · Confirm on the bridge.** Its status line reports the word rather than assuming it:

      #state=RX radio=1 frames=0 dropped=0 rssi=-44 snr=9.5 sync=0xA5 tx=0

- [ ] **6 · Confirm end to end.** Power the vehicle and watch packets arrive. Nothing before
      this point proves the two ends can actually hear each other; steps 4 and 5 only prove
      each end believes what it was told.
- [ ] **7 · Record the revision flown** — `git rev-parse --short HEAD` — in the flight log.

### Bench testing near other teams

`0xA5` is every team's launch word. **Never power the vehicle while another team is
launching** — the rulebook requires that on any sync word, and on this one the vehicle would be
transmitting into their launch. For a long bench session on a shared field you may move both
ends to the test word — `RadioMode::test` in the vehicle's `main.cpp`, `kTestSyncWord` in the
bridge's — and the build's agreement line will name it `TEST sync word (0xF3)`. The organizers'
station will not hear the vehicle while it is there, and both must come back to `0xA5` before
the launch.

> [!CAUTION]
> **`0xA5` does not separate you from other teams — every team uses it for their launch.**
> It separates launch traffic from test traffic. During your launch the sky is meant to hold
> exactly one `0xA5` transmitter, and it is yours. What protects you from another team is the
> team identifier in every packet and everyone else being powered off ([TEL-026](../requirements/requirements.md)).
>
> Sync words are **weak isolation in any case.** A transmitter on the wrong word still
> occupies the channel and can still occasionally trip a correlator. Being on `0xF3` during
> someone else's launch is not harmless — it is the penalty above, at **0.71 points per
> second**.

---

## Radio silence — when your vehicle must be off

**This is the procedure [TEL-026](../requirements/requirements.md) asks for.** Every other
team's CanSat must be off during your launch, and yours must be off during theirs.

### The firmware cannot help, and that is by design

There is no silent mode and there will not be one. [PWR-004](../requirements/requirements.md)
requires telemetry to begin **automatically at power-on**, with no manual trigger — and the
firmware satisfies it: the controller transmits from `READY` onward with no arming step. A
powered vehicle is a transmitting vehicle.

So the only control is physical, and **switch discipline is a scored activity rather than a
courtesy.** At 1.43 Hz the 2026 penalty of −1 point per 2 packets is **0.71 points per
second**: 35 seconds throws away the entire 25-point telemetry section, 70 seconds costs
fifty points. There is no recovering that with a good flight.

**And a vehicle left on does not stay at 1.43 Hz.** Five minutes after power-on its command
window closes and it goes to max rate by itself — **3.11 Hz, over 1.5 points per second**, so
the same 25 points go in about sixteen seconds.

### What "off" means today

> [!NOTE]
> **The manual ON/OFF switch and the power LED are fitted** (reported by the team,
> 2026-09-14), satisfying [PWR-001](../requirements/requirements.md) and PWR-002. "Off" is
> now the switch open **and the power LED dark** — a glance from outside the structure
> rather than opening the vehicle to look at a battery lead.
>
> **Still true: the Schottky diode is not fitted.** Without it, USB back-powers the LiPo, so
> **never connect a USB cable while the battery is connected**, switch closed or not. If the
> vehicle has to be reflashed on the field, disconnect the battery first.

### Before another team's launch

- [ ] **One named person owns the power state**, for the whole event. Not "the team" — a
      person. Shared ownership of a switch is how a vehicle gets left on.
- [ ] **Open the switch.** Disconnect the battery as well if the vehicle will sit for long.
- [ ] **Confirm the power LED is dark** — by eye, not by memory.
- [ ] **Also power down the ground bridge** if you are not using it. It only transmits on an
      uplink command, but a Pico you are not watching is a Pico you are not sure about.

### Confirm it, rather than assume it

**Your own ground station is the detector, and this costs nothing.** With your vehicle
supposedly off, leave the bridge running and watch its status line:

```text
#state=RX radio=1 frames=12 dropped=0 rssi=-44 snr=9.5 sync=0xA5 tx=0
```

- [ ] **Watch `frames=` for 30 seconds.** If it does not move, nothing is transmitting on
      your bridge's sync word.
- [ ] **If it moves, read the team identifier in the packet.** If it is yours, your vehicle
      is on — go and find it. If it is not, another team is on the air — theirs to
      manage, and worth noting.

This is the same trick the launch-configuration procedure uses: an existing counter turning
an assumption into an observation. With both ends on `0xA5` it watches the word that matters; a
vehicle moved to the test word for bench work is visible only to a bridge moved with it.

### During your own launch

- [ ] Your vehicle on, everyone else's off — the mirror of the above, and the organizers'
      to enforce.
- [ ] If you see packets from another team arriving on your station during your launch,
      **record it and say so afterwards.** Your logs are timestamped and carry the foreign
      team id; the validator counts them separately as `wrong_team`. That is evidence, and
      it costs nothing to keep.

### After your launch

- [ ] **Power the vehicle off as soon as it is recovered** and its card is out. Recovery is
      the phase where a vehicle is most likely to be left transmitting: it is in someone's
      hands, the flight is over, and attention has moved on.
- [ ] Revert the sync word to the test configuration — see
      [Launch configuration](#launch-configuration--switching-the-sync-word).

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

Three targets appear only when the SDK is present:

| Target | Image for |
|---|---|
| `cansat_pico_firmware` | The vehicle |
| `cansat_ground_bridge_firmware` | The ground-station bridge |
| `cansat_bringup_firmware` | The vehicle, temporarily, for bring-up only — a USB diagnostic, never flight software |

Flash by holding BOOTSEL while connecting USB, then copying the `.uf2` onto the
`RPI-RP2` drive that appears.

---

## Running the ground station

Run from `ground-station/software/`. The core needs only the Python standard library.

### Live from the bridge Pico

```bash
python src/main.py live --port COM5 --team CAN-Team-25 --framed
```

Use the actual serial port: `COM5` style on Windows, `/dev/ttyACM0` style on Linux. The
`--framed` flag matches the bridge's `$len,crc,payload` framing and should always be used
with real hardware. Requires `pyserial`; plots additionally need `matplotlib`, and the
dashboard runs without it.

### Live without the dashboard — headless, prints link health

```bash
python src/main.py live --port COM5 --team CAN-Team-25 --framed --no-dashboard
# Safe to pipe: the status lines are flushed as they are printed
python src/main.py live --port COM5 --team CAN-Team-25 --framed --no-dashboard | tee logs/console.log
```

### Rehearsal from a file, no hardware

```bash
python src/main.py live --replay ../../test-data/sample-mission.txt --rate 2 --team CAN-Team-01
```

### Offline replay with a CSV export

```bash
python src/main.py replay ../../test-data/sample-mission.txt --team CAN-Team-01 --output logs --export logs/flight.csv
```

### Web console

Open [`ground-station/web/index.html`](../../ground-station/web/index.html) in a browser.
Demo mode starts on its own. **File…** replays a packet file or a `raw_packets.tsv`.
**Web Serial** connects directly to the bridge Pico — Chrome or Edge, over `https` or
`localhost`.

The web console is the fastest way to see link health and mission state; the Python
pipeline is what writes the authoritative logs. Run both when it matters.

### Erasing the onboard log

The log **appends across power cycles** — the dual-header resume is what makes a brownout
mid-flight survivable — so a card accumulates every run until something empties it. At 1.43 Hz
a 64 MB region holds about **30 hours** of records, and when it does fill the vehicle raises
`sd_write`, then `sd_unavailable`, and keeps transmitting: logging stops, telemetry does not.

Two ways to empty it, and the first is the one to use before a launch:

1. **Re-prepare the card** with `python tools/prepare_sd_card.py`. This is the only method
   that also rewrites a stale column header ([F-19](../testing/bring-up-record.md#findings)),
   so a card carried over from an older firmware stops describing its rows wrongly.
2. **The console's Erase SD log button**, over the uplink, for iterating on the bench
   without unplugging the vehicle and pulling the card. It reaches the same end state as
   the script — empty log, fresh column header, and the old blocks overwritten with spaces
   — in two stages. **The log reads empty immediately**, before a single block is
   scrubbed; the physical overwrite then runs in the background and **takes several
   minutes**, because the card manages 297–367 blocks/s and the region is 64 MB. Telemetry
   is unaffected throughout: the scrub walks *down* from the end of the region while
   records append *up* from the start, so the two never touch the same block, and power
   lost mid-scrub leaves a consistent empty log rather than a half-built one.

   Two things it cannot do, both consequences of the file already existing: it does not
   delete and recreate `FLIGHT.CSV`, so it cannot **defragment** the file or change its
   size. If the firmware ever reports the file as fragmented, that needs the script and a
   quick-format. It also keeps the **boot count**, deliberately — that is the vehicle's
   life story rather than the file's, and an erased card should not become
   indistinguishable from one that has never flown.

**The button will do nothing on a flight build, by design.** `allow_ground_commands`
defaults to false; set it in the vehicle configuration and reflash to use it, and unset it
before you fly. Even enabled, the vehicle obeys only in `READY` with `ARM-0`.

**Authorising it.** The button asks for the vehicle's `command_password` and shows you the
packet number it will use. The password is **not transmitted**: the console sends a 64-bit
digest of the password and that packet number, so the wire never carries the secret and the
vehicle refuses any packet number it has already accepted, has not yet reached, or that is
older than `command_replay_window` (64 packets, about 45 seconds at 1.43 Hz). **A recorded
command is therefore worth exactly one erase — the one you meant.**

The password is `change-me` — the team's choice (2026-09-11), and the repository default, so
anyone who has read the repository knows it. It lives in the gitignored `local_secrets.hpp`
(below); change it there if that matters more than convenience.

> [!WARNING]
> **This is not cryptography and must not be relied on as though it were.** FNV-1a is a
> hash, not a MAC; the digest is 64 bits; the link is unencrypted. It defeats accidents,
> corrupted frames, another team's traffic on the shared sync word, and replay of a
> command someone watched work. It does not defeat somebody who knows the password. What
> protects the log is that the vehicle listens only in `READY` with `ARM-0`, on the ground,
> and only if it was built to listen at all.

A wrong password produces a valid-looking frame that the vehicle silently ignores — there is
no "wrong password" reply, because answering one would tell an attacker they had guessed
wrong. The bridge answers `#tx=queued` at once and `#tx=ok` when it sends: it holds a command
until just after it next hears the vehicle, so the command lands while the vehicle is
listening and never on top of a telemetry packet. Then check the vehicle itself.

`#tx=ok` from the bridge means the command reached the air. **It does not mean the log was
erased** — the vehicle may have been armed, out of range, or built with the uplink off.
Confirm on the vehicle.

> [!WARNING]
> The command key is four clear-text characters on a link every team shares at sync word
> `0xA5`. It stops accidents, not people. The protection that matters is the state window
> and the compile-time default.

### Commanding the maximum packet rate

The rulebook scores packet rate with no ceiling — "higher packet rates will be rewarded
with more points, provided transmissions remain consistent" — and the organizers count only
*transmitted* telemetry for the extra-sensor points. Normal flight already sends GPS and
sound in every packet at 1.43 Hz. One console button, **Max rate**, trades some of that
per-packet content for rate, once:

| Slot | Shape | Length |
|---|---|---:|
| 1 | rich — mandatory fields, `GP-` position, `SN-` sound | **374 ms** |
| 2 | lean — mandatory fields only | **296 ms** |
| 3 | lean | **296 ms** |
| | one full cycle | **966 ms** |

That is **3.11 Hz** of packets with GPS and sound on the air at **1.04 Hz** — one rich packet
every 966 ms. The twelve mandatory fields are in every packet, and the SD log records GPS
and sound for every packet, lean ones included.

> [!NOTE]
> **The sealed flight build (2026-09-11) goes to max rate by itself** — when its command
> window closes, and at once after a watchdog reset. The window stays at 1.43 Hz because at
> max rate no gap between packets is long enough to hear a command in. The button only
> closes the window early.

> [!WARNING]
> **This cannot be undone.** An accepted `MAX_RATE` closes the uplink in the same instant it
> changes the schedule: the vehicle stops listening for the rest of the power cycle, and a
> second press does nothing. Only a power cycle restores the flashed schedule — 1.43 Hz with
> the uplink open.

**Why 374 and 296 rather than something rounder.** Each slot is its packet's measured airtime
plus 50 ms, rounded up, and the rich slot is sized for 200 bytes — the longest packet the
organizers' station accepts, and so the longest ever sent. The 50 ms covers two things that
happen in the same gap: the vehicle's own work between transmits — one SD block write at its
worst case ([F-11](../testing/bring-up-record.md#findings) measured 30 ms in two of five
sessions on two different boards), plus the sensor loop and the watchdog feed — and the
organizers' receiver, which spends about 35 ms printing each packet to its serial port before
it listens again. A slot inside that guard does not fail loudly — it loses packets on the
station that scores them. Since the transmit stopped blocking, the card write overlaps the
packet itself, so the organizers' receiver is what the 50 ms is really for.

**How to send it.** The vehicle listens only during its **command window** — the first five
minutes after a clean power-on, on a build with the uplink (see below). The console's Mission
panel counts the window down from the vehicle's own clock and greys the button out once it
has closed. The button asks for the vehicle password and shows the packet number it binds the
command to; the password is never transmitted. Then:

1. `#tx=queued`, then `#tx=ok` within about a second, from the bridge means the frame reached
   the air. **It does not mean the vehicle
   acted** — confirm on the station's rate, which moves within one cycle.
2. Every third packet carries `GP-` and `SN-`, and the two between carry neither. That
   pattern in the raw monitor is the second confirmation.
3. If the rate does not move: the vehicle was armed, out of range, already latched, or built
   without the uplink. Read its startup summary over USB — the state line says
   `rate COMMANDED MAX - uplink closed` once it is latched, and the `uplink` line says how many
   commands it has accepted and refused.

### The command window, and why the drone waits for it

On a build with the uplink, power-on opens a **five-minute command window**. During it the
vehicle transmits at 1.43 Hz, listens for commands, and **does not arm**. The window closes
when `MAX_RATE` is accepted or when five minutes have passed, whichever comes first. The
vehicle then discards the calibration it took at power-on, recalibrates where it now sits, and
arms — about six seconds later if it is still.

1. Power the vehicle on the pad and leave it still.
2. The window closes by itself at five minutes, and the flight build then goes to max rate on
   its own — nobody has to press anything. Press **Max rate** only to close it sooner.
3. **Do not lift off until the window has closed and the vehicle has armed.** The console's
   **Armed** row reads `ARMED` once it has — from the `ST-` status field the vehicle sends
   about once a second — and **Calibration** reads `complete`. The *Command window* row's
   "should be armed (est.)" is only an estimate from the clock; trust the **Armed** row. A launch inside the window
   is not detected: launch, apogee and landing detection all need the vehicle armed.
4. A watchdog reset skips the window. The vehicle closes its uplink at once and arms, so a reset
   in flight never leaves it deaf to its own descent.

**The uplink needs a password file, and the build refuses a bad one.** Copy
`firmware/flight-computer/include/flight/local_secrets.example.hpp` to `local_secrets.hpp` beside
it and set the password — eight characters or more, and not the template's `SET-ME`, which the
build refuses to compile. This vehicle's file says `change-me`. The file is gitignored and never reaches the repository. **A build
without it has no uplink and no window, and arms three seconds after power-on as it always has.**

### What gets written

| File | Content |
|---|---|
| `logs/raw_packets.tsv` | Every received line with its receipt timestamp — nothing discarded, including malformed packets and CRC failures. Control characters in a corrupted payload are escaped (`\t`, `\r`, `\n`, `\xNN`) so one record is always one line; `logger.unescape_raw()` recovers the original bytes |
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
- [ ] **No USB cable connected while the battery is in** — the Schottky is not fitted
- [ ] Antennas attached to **both** radios — never power a radio without its antenna
- [ ] Ballast secured and the vehicle inside the 450–550 g band — weigh it if a scale is available
- [ ] Egg chamber closed. **No egg flies** — team decision, [PAY-001](../requirements/requirements.md)
- [ ] Parachute stowed for immediate deployment, **not tightly packed** (REC-003, REC-004)

### T-30 — Ground station up

- [ ] Bridge Pico connected; the serial port enumerates
- [ ] Ground station started with `--framed` and the correct `--team`
- [ ] Bridge status frames arriving once a second (`#state=RX radio=1 …`)
- [ ] **Sync word on screen matches the one this flight is using** — `LAUNCH · 0xA5`,
      the shipped setting on both Picos (`TEST · 0xF3` only if both were moved for bench work). The console reads it
      from the bridge, so a mismatch here means one of the two Picos was not
      reflashed. `UNKNOWN` means neither rulebook word is programmed.
- [ ] Log directory writable and empty of previous runs

### T-10 — Vehicle power-on

- [ ] Vehicle placed on the pad, level and stationary
- [ ] Power switched on; the LED lights **immediately**
- [ ] Telemetry starts automatically with no manual trigger
- [ ] First packet is `P-001`
- [ ] The status field reads `ST-R0…` — READY, not yet armed. The five diagnostic tags
      (`MODE`, `FAULTS`, `CAL`, `ARM`, `YR`) are **not on the air** since 2026-09-11; `ST-`
      carries the same facts in nine bytes, on rich packets whenever it fits
- [ ] Calibration settles: the third character becomes `1`, e.g. `ST-R010`. Keep the
      vehicle still until it does
- [ ] Faults: the fourth character is `0`, or every active fault is understood and accepted
- [ ] Altitude reads approximately zero at the ground baseline
- [ ] Packet numbering is unbroken and the rate is steady at 1.43 Hz
- [ ] **Five-minute command window.** Send `MAX_RATE` from the console to close it early, or
      wait it out; either way the vehicle then goes to max rate by itself and arms
- [ ] **Armed: `ST-R11…`**, and the rate at the station rises to ~3.1 Hz

> [!WARNING]
> **Do not let the drone lift until the vehicle reads armed.** Launch detection is disabled
> for the whole command window, so a lift that starts inside it is never detected and the
> vehicle reports `READY` through the entire flight.

> [!IMPORTANT]
> Keep the vehicle **still** until calibration settles. Calibration needs stationary samples; motion
> forces a best-effort result in which gyro and accelerometer bias are not applied, and
> raises a `calibration` warning.

### T-0 — Launch

- [ ] Other teams' CanSats powered off during your launch, and yours off during theirs
- [ ] Recording confirmed on the ground station
- [ ] Watch the status field reach `ST-F…` — on a drone lift this happens during the climb, past 15 m, not at release

### Descent and landing

- [ ] Altitude rises during the lift and falls during descent
- [ ] `ST-L…` about three seconds after impact
- [ ] Telemetry continues for at least 5 s after impact — a mandatory requirement
- [ ] `ST-V…` (RECOVERY) five seconds later
- [ ] Keep receiving until the vehicle is physically recovered

### Recovery

- [ ] Vehicle located and powered off
- [ ] microSD card removed and its contents copied before anything else
- [ ] Ground-station logs copied and backed up in two places
- [ ] Structure, egg chamber and canopy photographed as found, before anything is moved

---

## If the link reads 1 Hz

**It is not the vehicle's configuration.** `validate_config()` refuses any telemetry period
above 950 ms and the profile will not compile above it, so no build can ship at or below
1 Hz. The ground station says so out loud when the received rate falls short — the
headless station prints `RATE BELOW THE RULEBOOK MINIMUM` and the dashboard carries a
`>= 1 Hz rulebook` row.

Work through it in this order:

1. **Read the vehicle's startup summary.** It prints `telemetry every 700 ms (1.43 Hz)`. If
   it says anything else, the image is older than the period change — reflash it. This is
   the usual answer.
2. **Check the bridge's own status cadence.** `#state=RX` is emitted once a second by design
   and is unrelated to the telemetry rate. Reading the status line as the packet rate is an
   easy mistake to make.
3. **Compare `frames` on the bridge with `packets_ok` at the station.** Frames arriving but
   packets not accepted is a parser or team-id problem, not a rate one.
4. **Check the loss counter.** A vehicle transmitting at 1.43 Hz over a link losing a third
   of its packets is received at about 1 Hz, and the fix is the link, not the period.

## Reading telemetry in flight

A packet looks like this:

```text
CAN-Team-25; P-042; Ti-00:01:23:450; A-18.4; Pr-100821.33; T-24.6; Ro-2.1; Pi--1.4; Ya-15.9; AX-0.12; AY--0.31; AZ-9.79; GP-Lat-21.16450; GP-Lon-72.78480; GP-Alt-21; SN-412.5; ST-F110;
```

Everything up to `AZ-` is mandatory and fixed by the rulebook. Everything after it is
optional and appended by our firmware.

| Tag | Meaning | Watch for |
|---|---|---|
| `SN-` | Acoustic level, mV peak-to-peak — relative, not dB | Absent only if the microphone is stale |
| `ST-` | Status in four characters: state, armed, calibrated, active faults. `ST-F110` is FLIGHT, armed, calibrated, no faults. State letters: `I` INIT, `T` SELF_TEST, `R` READY, `F` FLIGHT, `L` LANDED, `V` RECOVERY, `X` FAULT | `X` at any point; armed must be `1` before the drone lifts. Opportunistic: dropped from any packet it would push past 200 bytes, so its absence from one packet means nothing |
| `GP-*` | GPS position, only while the receiver keeps confirming a fix | Absence is normal indoors and is not a fault. Fields that were present and then disappear mid-flight mean the receiver stopped refreshing its fix — the vehicle withdraws the position rather than repeat a stale one, and raises `gps_unavailable`. Use the last logged fix for recovery, and note its timestamp |

Link health on the ground station:

| Indicator | Healthy | Investigate |
|---|---|---|
| Rate | **1.43 Hz** (700 ms) through the five-minute command window, then about **3.11 Hz** once the vehicle goes to max rate | Falling rate means range or power trouble. Anything below 1.00 Hz is a rulebook failure, not just a warning, and the station says so: the headless form prints `RATE BELOW THE RULEBOOK MINIMUM` and the dashboard carries a `>= 1 Hz rulebook` row |
| Loss % | Near zero | Rising loss means range, antenna or orientation |
| CRC errors | Zero | Non-zero means transport corruption, not sensor trouble |
| Missing | Zero | Gaps in numbering mean lost packets over the air |
| Duplicates | Zero | Duplicates suggest a receiver or bridge problem |
| Vehicle restarts | Zero | Non-zero means the vehicle rebooted mid-mission — its watchdog fired, or it browned out. Telemetry resumes automatically and packet numbering starts again from `P-001`; the ground station recognises this and keeps counting cleanly. Check `FAULTS` for `watchdog_reboot`, and investigate power |
| RSSI | Falls as range grows; roughly −60 dBm close in | Below about −105 dBm the link is running out of headroom, **before** loss appears; below −115 dBm expect packets to start dropping |
| SNR | Positive on a healthy link | Negative SNR means the signal is near the demodulator's floor. It is the earliest warning available |
| Bridge drops | Zero | Non-zero means the PC application was not reading; the bridge kept running and discarded output rather than blocking |
| Resyncs | Zero | The frame decoder threw away a partial header and started again. A few mean noise on the serial line; a rising count means the link is corrupting bytes, not packets |
| Oversized length | Zero | A length field larger than any frame this link can carry. Not noise: either a badly corrupted header, or a sender configured for frames this receiver will never accept |

> [!TIP]
> **RSSI and SNR come from the bridge radio, not from the packets.** They are the only
> indicators that degrade *before* packets start disappearing, which makes them the numbers
> to watch during a range test and during descent. They appear in the web console's Link
> health panel, the Tk dashboard's Bridge radio panel, and the headless CLI's `bridge:` line.
>
> **Resyncs and oversized lengths come from the frame decoder on the PC**, not from the
> radio, and they answer a different question: whether the bytes arriving over USB are
> intact, independently of whether the packets inside them are. They appear in the Tk
> dashboard's Serial framing panel. An unframed run shows `--` for all of them, because no
> decoder ran — which is a different statement from zero.

---

## Post-flight analysis

Four hours are allowed after launch. The three mandatory graphs are altitude, temperature
and pressure, each against time or packet number.

1. Copy `logs/telemetry.csv` and `logs/raw_packets.tsv`, and the microSD log, to a safe
   location — work only on copies.
2. Export a clean CSV if needed. `replay` reads the raw log directly — it recognises the
   receipt timestamp and the escaping, so the file the flight produced is the file you
   replay. **Send the output somewhere other than the directory being read**, or the
   station appends to the log it is reading and the replay never ends:
   ```bash
   python src/main.py replay logs/raw_packets.tsv --team CAN-Team-25 --output analysis --export analysis/flight.csv
   ```
   Leaving `--output` at its default of `logs` is refused rather than allowed to run, with
   a message saying so — but the refusal is a stop, not a rescue, so pass `--output`.

   The summary line it prints (`received=… accepted=… rejected=…`) is worth reading before
   the graphs: `received=0` means the file is not what the command thinks it is.
3. **Run the analysis** — written and tested before the launch, in
   [`analysis/`](../../analysis/README.md). Extract the SD log, then one command writes the
   three mandatory graphs, the optional analysis and a `summary.md`:
   ```bash
   python tools/read_flight_log.py FLIGHT.CSV --out-dir analysis-input
   python analysis/flight_analysis.py analysis-input/flight-1.csv --compare logs/telemetry.csv --mass 0.50 --out analysis-output
   ```
   Or open `analysis/flight_analysis.ipynb`, change the paths in its **Configuration** cell,
   and *Run All*. **Read `summary.md`'s data table before any graph.**
4. The optional analysis is in the same output: descent rate and the drag coefficient it
   implies, acceleration, orientation, spin and pendulum, GPS drift, acoustic level against
   speed, and pressure and temperature against altitude. **Quote the descent rate from
   temperature-corrected height, not from the vehicle's altitude** — the firmware's ISA formula
   reads ~5 % low on a hot day ([F-21](../testing/bring-up-record.md#findings)), and the summary
   gives both.
5. Compare the transmitted stream against the onboard SD log: differences are radio loss,
   not sensor loss, and the difference itself is a useful result.

   The two files are written by different programs on different sides of the link, so their
   columns do not line up by name. **Join on `packet_number`**, which means the same thing
   in both, and read across:

   | Quantity | Onboard SD log | Ground `telemetry.csv` |
   |---|---|---|
   | Packet number — **the join key** | `packet_number` | `packet_number` |
   | Mission clock | `mission_ms` (integer milliseconds) | `timestamp` (`HH:MM:SS:MS`, the same clock, formatted) |
   | Altitude, pressure, temperature | `altitude_m`, `pressure_pa`, `temperature_c` | `altitude`, `pressure`, `temperature` |
   | Attitude | `roll_deg`, `pitch_deg`, `yaw_deg` | `roll`, `pitch`, `yaw` |
   | Acceleration | `ax_mps2`, `ay_mps2`, `az_mps2` | `ax`, `ay`, `az` |
   | GPS | `gps_valid`, `gps_lat`, `gps_lon`, `gps_alt`, `gps_satellites`, `gps_hdop` | `gps_lat`, `gps_lon`, `gps_alt`, `gps_satellites`, `gps_hdop` (all blank when there was no fix — 0 satellites and HDOP 0.0 are both readings a receiver produces, so zeros would describe a perfect fix that never happened). `gps_satellites` and `gps_hdop` are the two numbers the fix gate judged on, recorded beside the position it let through ([F-18](../testing/bring-up-record.md#findings)) |
   | Mission state and faults | `state`, `fault_total` | **not carried — on the card only by default.** The diagnostic tags left the air so GPS and sound could have the room. A bench build with `append_diagnostic_fields` on still carries them inside `raw_packet` as `MODE` and `FAULTS` |
   | Acoustic level | `sound_mv_pp`, `sound_clipped`, `sound_gate_pct` | `sound_mv` — the `SN-` field as received, the same peak-to-peak level in millivolts. Blank for a lean packet after `MAX_RATE`, which carries no sensors, and for a microphone that was not producing valid windows. Clipping and the gate are on the card only. Blank on the card means no microphone was fitted or it was not reporting; `0.0` means it measured silence, and the two are not the same thing |
   | The packet itself | `packet` | `raw_packet` |

   The onboard names carry their units because they are written by the flight computer,
   where a number without a unit is how a wrong number gets believed. The ground columns
   are named for the fields of the rulebook packet they came from.

   Columns that exist on one side only are the point of the exercise, not a defect. Each
   one answers a question the other side could not have been asked:

   | Column | Only in | What it records |
   |---|---|---|
   | `receipt_time` | ground | When the **ground station** received the packet, which is not when the vehicle sent it |
   | `valid` | ground | Whether the packet parsed |
   | `error` | ground | Why it did not, when it did not |
   | `seq_missing` | ground | How many packets never arrived before this one |
   | `seq_note` | ground | What the validator said about it — duplicate, out of order, reboot |
   | `yaw_reference` | ground | Whether the `yaw` column is magnetic or gyro-integrated, read from the packet's `YR-` tag |
   | `heading` | ground | That yaw as a compass bearing. Present only when the reference is magnetic, so **empty for every packet this vehicle sends** — the delivered IMU is a six-axis MPU-6500 with no magnetometer ([F-1](../hardware/receiving-inspection.md#findings)) |
   | `state` | onboard | The mission state the **vehicle** was in when it transmitted |
   | `fault_total` | onboard | Every fault it had raised by then, not just the active ones |

   `team_id` is on the ground side because the vehicle already knows whose log it is
   writing. `team_id` is on the ground side because the
   vehicle already knows whose log it is writing.

---

## Troubleshooting

<details>
<summary><b>No packets at the ground station</b></summary>

1. **Sync words** — the most common cause. Vehicle and bridge must both be `0xF3` or both
   `0xA5`. A mismatch is a silent total loss. Half of that is now on screen: the bridge
   reports its own word in the status line, and the console's **Sync word** field shows it.
   That tells you the ground half without a reflash; the vehicle half still has to be
   inferred from which image was loaded, because a vehicle nobody is receiving cannot say.
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
<summary><b>Calibration never settles (the third `ST-` character stays 0)</b></summary>

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
capture a clean barometric reference. Power-cycle on a still surface and watch the third `ST-` character become `1`.
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
| Vehicle lands out of sight | **Read this before you fly.** With `transmit_gps` false — the default — the position is **not on the air at all**, so the ground station cannot tell you where the vehicle is. Telemetry still continues into `RECOVERY`, which tells you it is alive and roughly how far away by RSSI, and the fix is in the **vehicle's** SD log, which you can only read once you have found it. **If losing sight of it is plausible, set `transmit_gps = true`, raise `worst_case_packet_bytes` to 255 and the period to 850 ms, and reflash** — the vehicle then transmits `GP-Lat`/`GP-Lon`/`GP-Alt` again at 1.18 Hz instead of 1.43. When it is transmitting, read the fix's **timestamp** as well as its coordinates: the position is withdrawn once the receiver stops confirming it, so the last one received is the last the vehicle stood behind |

---

Related: [test-plan.md](../testing/test-plan.md) ·
[wiring.md](../design/wiring.md) ·
[software-architecture.md](../design/software-architecture.md) ·
[telemetry-protocol.md](../design/telemetry-protocol.md)
