# Telemetry cadence and the max-rate command — design

**Status: revised and approved 2026-09-11 and implemented the same day. `MAX_RATE` has been
measured on the bench at 3.11 Hz; the command window has not yet been measured.** This replaces the 2026-09-10 design of two max-rate commands. The packet
widths were corrected by measurement the same day — see [where the numbers come
from](#where-the-numbers-come-from). The bench rows under Gate 8 of the [bring-up
record](../testing/bring-up-record.md) are what would make any of it real.

> [!NOTE]
> **Resolved on the bench, 2026-09-11.** The fallback to 1.43 Hz an earlier build showed was a
> command that never latched: the vehicle calibrated and armed about three seconds after
> power-on, and the old command window — READY with `ARM-0` — closed with it. With arming held
> off, `MAX_RATE` was accepted (`commands accepted 1`), the summary read `COMMANDED MAX` for the
> rest of the run, and the station measured **3.11 Hz with 1 packet in 544 lost**. That is what
> led to the [five-minute command window](#the-command-window).

---

## Contents

- [Why the packet changes](#why-the-packet-changes)
- [Two packet shapes](#two-packet-shapes)
- [Normal flight](#normal-flight)
- [The max-rate command](#the-max-rate-command)
- [Where the numbers come from](#where-the-numbers-come-from)
- [The command window](#the-command-window)
- [What latches](#what-latches)
- [What the ground side learns](#what-the-ground-side-learns)
- [Validation](#validation)
- [Consequences accepted](#consequences-accepted)
- [Testing](#testing)
- [Decisions recorded](#decisions-recorded)
- [Out of scope](#out-of-scope)

---

## Why the packet changes

The rulebook rewards extra sensors in one place and conditions it in another:

> **Sensor Integration:** "Additional Working Sensors (5 Points each, up to 25 Points)."
>
> **Data Transmission Guidelines, note 5:** "Extra sensors will be rewarded under the
> Sensors & Innovation scoring section, but only if data is correctly formatted and
> consistently transmitted."

The two read differently, and the organizers settled it on 2026-09-11: **only transmitted
telemetry is considered for extra-sensor points.** Until this design, neither the GPS nor
the sound sensor was transmitted in normal flight — both went to the SD log only — so under
that ruling both scored nothing unless an operator pressed a button.

The requirement that follows: **GPS and sound on the air at least once a second, in every
mode, and the mandatory data as fast as the link allows.**

## Two packet shapes

| Shape | Carries | Worst case |
|---|---|---:|
| **Rich** | The twelve mandatory fields, then `GP-Lat`, `GP-Lon`, `GP-Alt`, then `SN-` | **213 B** |
| **Lean** | The twelve mandatory fields only | **147 B** |

`SN-` is the sound level in millivolts peak-to-peak, one decimal — a short prefix, as the
rulebook asks of optional fields. It tops out at `SN-3300.0`, the ADC reference.

A field is only transmitted when its data is valid: a rich packet from a vehicle with no GPS
fix carries `SN-` alone, and one with a stale microphone carries `GP-` alone. The SD row
always records everything, whichever shape went on the air.

**The five diagnostic tags — `MODE`, `FAULTS`, `CAL`, `ARM`, `YR` — leave the air by
default, in every mode.** They are project-local, the rulebook's mandated packet does not
contain them, and there is no room for them: at their widest, tags plus GPS already overfill
the 255-byte FIFO at 256 bytes, and sound takes the total to 267. They continue to reach the
SD log, and a bench build can turn them back on — the controller sheds them first whenever
they do not fit.

## Normal flight

**Every packet is rich.** At the 700 ms cadence a sensor reading at least once a second means
every packet has to carry one — alternating rich and lean would put the sensors at 0.71 Hz.

| | |
|---|---:|
| Period | 700 ms (unchanged) |
| Packet rate | **1.43 Hz** (unchanged) |
| GPS and sound rate | **1.43 Hz** |
| Duty at the 213-byte worst case | **49.2 %** measured, 48.3 % modelled — under the 50 % cap |

## The max-rate command

One command, `MAX_RATE`, replacing the two of the previous design. The same envelope, the same
token scheme and the same replay rules, accepted only during the pre-arm [command
window](#the-command-window):

```text
CAN-Team-25; CMD-MAX_RATE; PN-1234; KEY-<16 hex>;
```

**The token covers the command**, which the previous design established: FNV-1a over
`password | COMMAND | packet_number`, so a token minted for one command is refused for every
other. The fixture is regenerated for the new command name.

After it, the vehicle transmits a repeating pattern of three:

| Slot | Shape | Length |
|---|---|---:|
| 1 | rich | **385 ms** |
| 2 | lean | **286 ms** |
| 3 | lean | **286 ms** |
| **Cycle** | | **957 ms** |

| | |
|---|---:|
| Packet rate | **3.13 Hz** |
| GPS and sound rate | **1.04 Hz** — one rich packet every 957 ms |
| Duty | **87 %** |

A fourth lean slot would take the cycle to 1243 ms and the sensors below 1 Hz, so three is
not a choice but the largest pattern that keeps the requirement.

## Where the numbers come from

Every slot is **measured airtime + 40 ms, rounded up**:

| Bytes | Airtime, model | +1.8 % measured | + 40 ms | Slot |
|---:|---:|---:|---:|---:|
| 147 | 240.90 ms | 245.23 ms | 285.23 | **286 ms** |
| 213 | 338.18 ms | 344.26 ms | 384.26 | **385 ms** |

The 1.8 % is this hardware's measured excess over the model (bring-up rows 5.2 and 5.3). The
40 ms is the vehicle's own work between two transmits: one SD block write at its measured
worst case — 30 ms, on two boards, in two of five sessions
([F-11](../testing/bring-up-record.md#findings)), a healthy card's housekeeping rather than
a fault — plus the sensor loop and the watchdog feed. The rulebook scores consistency on the
same five points as rate, so a slot inside that guard buys rate by making packets late.

**The byte figures are defended by construction, not by arithmetic.** A test builds the
widest packet each shape can produce — the rulebook's fixed-width team id, packet number
4294967295, a 99:59:59:999 mission clock, the extreme negative values the overflow test uses,
and every optional field at its widest — and checks it against its constant:

| Part | Bytes |
|---|---:|
| Mandatory fields | 147 |
| `GP-Lat` + `GP-Lon` + `GP-Alt`, with separators | 55 |
| `SN-3300.0`, with separator | 11 |
| **Rich** | **213** |

**That test corrected this document.** Its first revision carried 145, 56 and 212 from a
commit message, and the construction said 147, 55 and 213. The rich packet's change is
harmless — 212 and 213 bytes share a LoRa symbol block. The lean packet's is not: 147 bytes
crosses a symbol boundary that 145 does not, 43 blocks against 42, which is 5 ms of airtime
on every lean packet. It moved the lean slot from 281 to 286 ms, the cycle from 947 to 957,
and the rate from 3.17 to 3.13 Hz.

`static_assert`s in `link_profile.hpp` refuse a build where any slot is inside its own
airtime plus the guard, where the max-rate cycle exceeds 1000 ms, where a fourth lean slot
would still fit, or where either shape exceeds the FIFO.

## The command window

The old window was READY with `ARM-0`, and on a desk the vehicle calibrates and arms about three
seconds after power-on. That closed the window before an operator could use it — which is what
the bench fallback turned out to be. The window is now its own phase:

1. **A clean power-on opens it**, on a build with the uplink. It lasts `command_window_ms`, five
   minutes. The vehicle transmits, listens, calibrates once for a working height reference, and
   **does not arm**.
2. **An accepted `MAX_RATE` or the timeout closes it**, whichever is first. An erase does not.
3. **Closing it discards the power-on calibration.** The vehicle recalibrates where it now sits
   — on the pad, after the operator has finished with it — and arms when that settles and the
   3 s arming delay, counted from the close, has run.
4. **A watchdog reset skips it.** A reset may come mid-flight, and five minutes unarmed and
   listening would be five minutes without launch or landing detection.
5. **A build without `local_secrets.hpp` has no uplink and no window**, and arms three seconds
   after power-on exactly as before. The password lives in that gitignored file; the build
   refuses `SET-ME`, `change-me` and anything under eight characters.

**The operator cannot see arming directly** — the `ARM` tag is off the air. The console infers
the window from the mission clock in every packet and labels it an estimate. **A launch inside
the window is not detected**, so the drone waits for the vehicle to arm.

## What latches

One accepted `MAX_RATE` sets, permanently for the power cycle:

1. **The schedule** — the telemetry task moves from a fixed 700 ms period to the three-slot
   pattern. `PeriodicTask` gains a way to set the *next* due time from the slot of the packet
   just sent, because a rich slot and a lean slot are different lengths.
2. **The uplink** — `service_ground_commands()` returns immediately from then on. The vehicle
   never enters RX again, so the command cannot be repeated or undone.
3. **The report** — `HealthSnapshot::rate_maxed`, printed on the startup summary's state line.

The packet *shapes* do not change at the latch — both modes transmit rich packets and carry
no tags. The command changes only how often, and whether two lean packets follow each rich
one.

**The latch lives in RAM.** A power cycle or watchdog reset returns the flashed schedule. A
card carrying "max rate, no uplink" from a bench session must never apply itself silently to
a flight.

## What the ground side learns

Both parsers already store unknown optional fields generically, so nothing breaks — but
nothing displays `SN-` either, so:

- **Python** (`telemetry.py`): a `sound_mv` accessor, and a `sound_mv` column in the CSV.
- **JavaScript** (`index.html`): `rec.sound_mv`, a sound readout on the console, and the column
  in the console's CSV export.
- **Both**: rows in `test-data/optional-tag-cases.tsv` for `SN-`, so the two cannot disagree.
- **The console**: one **Max rate** button replacing two, with a prompt stating about
  3.13 packets a second, GPS and sound once a second, and that it cannot be undone.

## Validation

`validate_config()`'s rule that "`transmit_gps` needs a 255-byte budget" assumed the tags were
always on the air. It is replaced by a computed floor: the budget must be at least

```text
147 + (transmit_gps ? 55 : 0) + (transmit_sound ? 11 : 0)
```

**The failure this guards against is silent.** A budget too small for the sensors has the
controller shed GPS and sound from every packet to fit it, the duty check passes on a packet
that is never sent, and the extra-sensor points — scored only on what is transmitted — go to
zero with no fault anywhere.

**The diagnostic tags are deliberately not counted.** The first revision of this design
counted them, which would have refused any tagged configuration outright — including the
end-to-end fixture that proves `MODE` crosses the whole ground pipeline. There is no
protective reason to: the controller sheds the tags first whenever a packet would not fit,
so they can never push the sensors off the air.

## Consequences accepted

| | Effect |
|---|---|
| **Live mission state on the console** | **Lost by default.** Without `MODE` and `ARM` on the air the console shows mission state as "unreported". The vehicle's startup summary over USB and the SD log still carry it |
| **The SD log** | Becomes the only in-flight record of mission state, faults and calibration. The bench vehicle currently reports **`SD card FAILED`**, which matters more under this design than before it |
| **Log capacity** | ~30 hours at 1.43 Hz; ~13.7 hours after `MAX_RATE` |
| **Average current** | Normal flight moves ~41 → ~43 mA for the radio. After `MAX_RATE`, ~76 mA. Peaks unchanged — they are set by coincident TX, SD write and GPS acquisition, not by rate |
| **Channel occupancy** | 49 % in normal flight; 87 % after `MAX_RATE`. Acceptable in a reserved launch slot; antisocial during shared bench testing on `0xF3` |
| **Format compliance** | The twelve mandatory fields are untouched and always first; `GP-` and `SN-` are optional fields with short prefixes, which is what the rulebook's *Optional Sensor Fields* section describes |

## Testing

**C++:** the widest rich and lean packets match 213 and 147 by construction; both slots clear
their airtime plus the guard; the max-rate cycle is under 1000 ms and a fourth slot is not;
normal flight transmits `GP-` and `SN-` in every packet and no tags; after `MAX_RATE` the
pattern is rich, lean, lean with the right spacing and the SD row still carries GPS and sound
for lean packets; the uplink closes; the command is refused when armed, out of READY,
replayed, or minted for another command; a flight build never polls the radio;
`validate_config()` refuses a budget below the sensors and accepts a tagged bench build.

**Python and Node:** `SN-` parses to the same key and value in both, from the shared fixture;
`sound_mv` reaches both CSVs; the console mints a `MAX_RATE` token the fixture agrees with.

**Documented claims:** 213, 147, both slots, the 957 ms cycle, 3.13 Hz and 1.04 Hz enter
`check_doc_claims.py` derived from the shipped constants.

**Bench, on hardware:** before anything else, the capture that settles the open fallback.
Then: normal flight shows `GP-` and `SN-` in every packet at 1.43 Hz; `MAX_RATE` moves the
station to ~3.13 Hz with a rich packet every ~957 ms; no gaps in numbering over two minutes;
a power cycle restores 1.43 Hz.

## Decisions recorded

- **Sensors on the air in every mode.** The organizers' ruling makes any mode that does not
  transmit them a mode that does not score them.
- **Every normal-flight packet is rich.** Alternating would put the sensors at 0.71 Hz.
- **Tags leave the air by default.** There is no byte budget that holds tags, GPS and sound
  together.
- **Widths by construction.** A commit message's arithmetic was 2 bytes short on the mandatory
  block, and 2 bytes was a whole LoRa symbol block.
- **The floor counts only what must reach the air.** Tags are shed first and cannot displace
  the sensors, so counting them protects nothing and forbids tagged bench builds.
- **One command, not two.** The lean variant's only advantage was dropping GPS, which the
  ruling now makes costly.
- **A slot per packet shape, not one period.** A rich packet needs 385 ms and a lean one 286;
  one period sized for the rich packet would waste 99 ms on every lean one.
- **A pre-arm command window, not one gated on `ARM-0`.** Arming three seconds after power-on
  closed the old window before anyone could use it. Five minutes of listening, then
  recalibration and arming, is a window an operator can actually use — and a launch inside it
  is not detected, which the runbook says in bold.
- **RAM latch, token bound to the command.** Unchanged from the previous design, for the same
  reasons.

## Out of scope

Persisting the latch. Commanding in flight. Changing bandwidth or spreading factor. Raising the
normal-flight rate. Putting the diagnostic tags back on the air in normal flight.
