# Telemetry cadence and the max-rate command — design

**Status: revised and approved 2026-09-11; implementation in progress. Never run on
hardware.** This replaces the 2026-09-10 design of two max-rate commands. The bench rows
under Gate 8 of the [bring-up record](../testing/bring-up-record.md) are what would make any
of it real.

> [!WARNING]
> **Open at the time of writing.** On the bench, a build of the previous design raised the
> rate and then fell back to exactly 1.43 Hz, with packet numbers still climbing — so the
> vehicle did not reset. Nothing in the firmware can restore the configured period without a
> reset, which makes "the vehicle never latched and the rise was a link artefact" the leading
> explanation. An instrumented build that prints the uplink's accepted/refused counters was
> built on 2026-09-11 and is waiting for a bench capture. **This design uses the same latch.**

---

## Contents

- [Why the packet changes](#why-the-packet-changes)
- [Two packet shapes](#two-packet-shapes)
- [Normal flight](#normal-flight)
- [The max-rate command](#the-max-rate-command)
- [Where the numbers come from](#where-the-numbers-come-from)
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

The requirement that follows is simple to state: **GPS and sound on the air at least once
a second, in every mode, and the mandatory data as fast as the link allows.**

## Two packet shapes

| Shape | Carries | Worst case |
|---|---|---:|
| **Rich** | The twelve mandatory fields, then `GP-Lat`, `GP-Lon`, `GP-Alt`, then `SN-` | **212 B** |
| **Lean** | The twelve mandatory fields only | **145 B** |

`SN-` is the sound level in millivolts peak-to-peak, one decimal — a short prefix, as the
rulebook asks of optional fields. It tops out at `SN-3300.0`, the ADC reference.

A field is only transmitted when its data is valid: a rich packet from a vehicle with no GPS
fix carries `SN-` alone, and one with a stale microphone carries `GP-` alone. The SD row
always records everything, whichever shape went on the air.

**The five diagnostic tags — `MODE`, `FAULTS`, `CAL`, `ARM`, `YR` — leave the air in every
mode.** They are project-local, the rulebook's mandated packet does not contain them, and
there is no room for them: tags plus GPS alone already fill the 255-byte FIFO, and sound
takes it to 266. They continue to reach the SD log.

## Normal flight

**Every packet is rich.** At the 700 ms cadence a sensor reading at least once a second means
every packet has to carry one — alternating rich and lean would put the sensors at 0.71 Hz.

| | |
|---|---:|
| Period | 700 ms (unchanged) |
| Packet rate | **1.43 Hz** (unchanged) |
| GPS and sound rate | **1.43 Hz** |
| Duty at the 212-byte worst case | **49.2 %** measured, 48.3 % modelled — under the 50 % cap |

## The max-rate command

One command, `MAX_RATE`, replacing the two of the previous design. The same envelope, the same
token scheme, the same window (READY, `ARM-0`, a build with `allow_ground_commands`), and the
same replay rules:

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
| 2 | lean | **281 ms** |
| 3 | lean | **281 ms** |
| **Cycle** | | **947 ms** |

| | |
|---|---:|
| Packet rate | **3.17 Hz** |
| GPS and sound rate | **1.06 Hz** — one rich packet every 947 ms |
| Duty | **87 %** |

A fourth lean slot would take the cycle to 1228 ms and the sensors below 1 Hz, so three is
not a choice but the largest pattern that keeps the requirement.

## Where the numbers come from

Every slot is **measured airtime + 40 ms, rounded up**:

| Bytes | Airtime, model | +1.8 % measured | + 40 ms | Slot |
|---:|---:|---:|---:|---:|
| 145 | 235.78 ms | 240.02 ms | 280.02 | **281 ms** |
| 212 | 338.18 ms | 344.26 ms | 384.26 | **385 ms** |

The 1.8 % is this hardware's measured excess over the model (bring-up rows 5.2 and 5.3). The
40 ms is the vehicle's own work between two transmits: one SD block write at its measured
worst case — 30 ms, on two boards, in two of five sessions
([F-11](../testing/bring-up-record.md#findings)), a healthy card's housekeeping rather than
a fault — plus the sensor loop and the watchdog feed. The rulebook scores consistency on the
same five points as rate, so a slot inside that guard buys rate by making packets late.

**The byte figures are defended by construction, not by arithmetic.** A test builds the
widest packet each shape can produce — longest team id, widest packet number, extreme values,
every optional field at its widest — and checks it against its constant:

| Part | Bytes |
|---|---:|
| Mandatory fields | 145 |
| `GP-Lat` + `GP-Lon` + `GP-Alt`, with separators | 56 |
| `SN-3300.0; ` | 11 |
| **Rich** | **212** |

If the measurement disagrees, the constant moves and the slot moves with it. `static_assert`s
in `link_profile.hpp` refuse a build where any slot is inside its own airtime plus the guard,
where the max-rate cycle exceeds 1000 ms, or where either shape exceeds the FIFO.

## What latches

One accepted `MAX_RATE` sets, permanently for the power cycle:

1. **The schedule** — the telemetry task moves from a fixed 700 ms period to the three-slot
   pattern. `PeriodicTask` gains a way to set the *next* due time from the slot of the packet
   just sent, because a rich slot and a lean slot are different lengths.
2. **The uplink** — `service_ground_commands()` returns immediately from then on. The vehicle
   never enters RX again, so the command cannot be repeated or undone.
3. **The report** — `HealthSnapshot::rate_maxed`, printed on the startup summary's state line.

The packet *shapes* do not change at the latch — both modes already transmit rich packets and
carry no tags. The command changes only how often, and whether two lean packets follow each
rich one.

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
- **The console**: one **Max rate** button replacing two, with a prompt stating 3.17 Hz, GPS and
  sound once a second, and that it cannot be undone.

## Validation

`validate_config()`'s rule that "`transmit_gps` needs a 255-byte budget" assumed the tags were
always on the air. It is replaced by a computed floor: the budget must be at least

```text
145 + (transmit_gps ? 56 : 0) + (transmit_sound ? 11 : 0) + (append_diagnostic_fields ? 54 : 0)
```

and at most 255. So re-enabling the tags alongside GPS and sound — 266 bytes — is refused
outright rather than truncated on the air.

## Consequences accepted

| | Effect |
|---|---|
| **Live mission state on the console** | **Lost in every mode.** Without `MODE` and `ARM` on the air the console shows mission state as "unreported". The vehicle's startup summary over USB and the SD log still carry it |
| **The SD log** | Becomes the only record of mission state, faults and calibration. The bench vehicle currently reports **`SD card FAILED`**, which matters more under this design than before it |
| **Log capacity** | ~30 hours at 1.43 Hz; ~13.5 hours after `MAX_RATE` |
| **Average current** | Normal flight moves ~41 → ~43 mA for the radio. After `MAX_RATE`, ~76 mA. Peaks unchanged — they are set by coincident TX, SD write and GPS acquisition, not by rate |
| **Channel occupancy** | 49 % in normal flight; 87 % after `MAX_RATE`. Acceptable in a reserved launch slot; antisocial during shared bench testing on `0xF3` |
| **Format compliance** | The twelve mandatory fields are untouched and always first; `GP-` and `SN-` are optional fields with short prefixes, which is what the rulebook's *Optional Sensor Fields* section describes |

## Testing

**C++:** the widest rich and lean packets match 212 and 145 by construction; both slots clear
their airtime plus the guard; the max-rate cycle is under 1000 ms; normal flight transmits
`GP-` and `SN-` in every packet and no tags; after `MAX_RATE` the pattern is rich, lean, lean
with the right spacing and the SD row still carries GPS and sound for lean packets; the
uplink closes; the command is refused when armed, out of READY, replayed, or minted for
another command; a flight build never polls the radio; `validate_config()` refuses tags + GPS
+ sound.

**Python and Node:** `SN-` parses to the same key and value in both, from the shared fixture;
`sound_mv` reaches both CSVs; the console mints a `MAX_RATE` token the fixture agrees with.

**Documented claims:** 212, 145, both slots, the 947 ms cycle, 3.17 Hz and 1.06 Hz enter
`check_doc_claims.py` derived from the shipped constants.

**Bench, on hardware:** before anything else, the capture that settles the open fallback.
Then: normal flight shows `GP-` and `SN-` in every packet at 1.43 Hz; `MAX_RATE` moves the
station to ~3.17 Hz with a rich packet every ~947 ms; no gaps in numbering over two minutes;
a power cycle restores 1.43 Hz.

## Decisions recorded

- **Sensors on the air in every mode.** The organizers' ruling makes any mode that does not
  transmit them a mode that does not score them.
- **Every normal-flight packet is rich.** Alternating would put the sensors at 0.71 Hz.
- **Tags leave the air.** There is no byte budget that holds tags, GPS and sound together.
- **One command, not two.** The lean variant's only advantage was dropping GPS, which the ruling
  now makes costly.
- **A slot per packet shape, not one period.** A rich packet needs 385 ms and a lean one 281; one
  period sized for the rich packet would waste 104 ms on every lean one.
- **Ground-only window, RAM latch, token bound to the command.** Unchanged from the previous
  design, for the same reasons.

## Out of scope

Persisting the latch. Commanding in flight. Changing bandwidth or spreading factor. Raising the
normal-flight rate. Putting the diagnostic tags back on the air in any mode.
