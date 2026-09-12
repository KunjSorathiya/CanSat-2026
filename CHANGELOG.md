# Changelog

All notable changes to the CanSat 2026 project.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). This project has
no released versions — it tracks a competition build, so entries are grouped by
development cycle.

---

## [Unreleased] — 2026-09-12 (cycle 53) — the vehicle becomes an object

The structure came back from the printer, the electronics went into it, and the whole thing
went on a scale. It is the first mechanical measurement this project has had, and it moved
the largest open risk in the wrong direction.

### Added — the structure exists

`Cansat_D1` printed in **white PETG**, electronics mounted, **egg chamber fitted**. The
assembled vehicle weighs **280 g without a parachute**. Every mechanical claim in this
repository up to now was a model; this one is a scale reading.

White was a finish decision and it is defensible: section D puts 15 of its 30 points on
aesthetics and build quality, white shows a clean print rather than hiding a poor one, and it
photographs against any background — which section F separately requires.

### Changed — the mass budget is now a measurement, and the risk doubled

The solid-volume estimate was **193 g** and was stated as an upper bound. It was one, by
33 %: the printed structure **and its egg chamber together** are **≈ 128.7 g**, derived as
280 − 151.299 g of separately weighed electronics.

| | Before (2026-09-09) | After (2026-09-12) |
|---|---:|---:|
| Structure | 193 g, estimated | **≈ 128.7 g**, measured by difference |
| Committed / as-built | 344.3 g | **280 g** |
| Projected all-up | 414–479 g | **315–345 g** |
| Against the 450 g floor | straddles it | **105–135 g under it** |

**The finding of 2026-09-09 — that coming in light was the likelier risk — was right, and it
outran its own mitigation.** The advice then was to thicken the walls *before* printing. The
part is printed, and a high-infill re-print can recover at most ~64 g of a 105–135 g gap, so
ballast is likely needed as well.

**Whether 450 g binds is now the single most consequential open question in the project**,
because it decides whether the structure is re-printed. GEN-005 reads as a band; GEN-006's
disqualification is worded one-sidedly, about *exceeding*. It is
[question 7](README.md#open-questions-for-the-organizers) and it wants an email today.

### Changed — what the new mass does to the descent, which is nothing bad

The 80.0 cm canopy was sized at 550 g on a hot day and **stays compliant across everything
this vehicle can now weigh**, so it does not have to be re-sized before the mass decision:

| Flight mass | Rate | Descent time | Packets at 1.43 Hz |
|---|---:|---:|---:|
| 315 g, as-built | **3.66 m/s** | **8.59 s** | **12** |
| 500 g, ballasted | 4.61 m/s | 6.94 s | 9 |
| 550 g, 35 °C — the sizing case | 5.00 m/s | 6.45 s | 9 |

Flying light descends a third slower and yields a third more descent telemetry, and section C
scores descent time *comparatively*. So if 450 g does not bind, staying light is the better
flight — which is an unusual thing for a mass problem to be.

The as-built mass also makes study 3's 100 N impact load **more** conservative, not less:
1.15 N·s of momentum at 315 g and 3.66 m/s, against the 2.50 N·s it assumed.

### Changed — Gate 7 is part-passed

It read "Nothing built". The structure half is now built, assembled and weighed; recovery —
canopy, deployment, drop test — is untouched. Section D moves from ~0 to **~12 of 30**, and
the project's secured estimate from ~47 to **~61 of 200**, without a purchase.

### Added — the final project report

[`documentation/project/final-report.md`](documentation/project/final-report.md), with
`.docx` and `.pdf` generated beside it. Mission, requirements, architecture, hardware,
firmware, ground station, protocol, simulations, structure, testing, timeline, findings and
lessons learned, against section F's 25 points.

### Fixed — documentation that the repository had already overtaken

- The README described the microphone as fitted but reaching "neither the card nor the
  radio". It reaches both, as `sound_mv_pp` and `SN-`, and has since cycle 48.
- The README listed the battery divider as unfitted; it is fitted at ratio 2.0.
- The concept of operations described GPS as logged and not transmitted, from before the
  organizers' ruling moved it onto the air.
- The Python test badge read 229; the suite is 232.
- `check_doc_claims.py` now holds the as-built masses, the estimate's overshoot, the all-up
  projection, the distance to both edges of the band, and the three descent cases — 299
  claims, up from 290. **The checker caught two of the errors in this list itself.**

---

## [Unreleased] — 2026-09-11 (cycle 52) — the sealed flight build

The USB port is closed after this image, so nothing a button press or a lucky boot decides may
set the rate, and every setting is the one the vehicle flies with.

### Changed — max rate without a button

`auto_max_rate`: the vehicle goes to the max-rate pattern the moment its five-minute command
window closes, and at once after a watchdog reset. A missed press, or a reset in flight, used to
mean 1.43 Hz for the rest of the power cycle. The window itself stays at 1.43 Hz, because at max
rate no gap between packets is long enough to hear a ~110 ms command in; `MAX_RATE` now only
closes it early. The uplink and its `change-me` password stay, at the team's decision.

### Changed — the battery divider

`battery_divider_ratio` 2.0 (33 kΩ over 33 kΩ into GP26) and a 3.5 V low-battery warning, on the
team's confirmation that the divider is fitted. Bring-up row 2.6, the measurement, is still open.

---

## [Unreleased] — 2026-09-11 (cycle 51)

The first bench run of cycle 50 held `MAX_RATE` at 3.10 Hz with 1 of 327 packets missing — and a
console that could not say whether the vehicle had armed.

### Added — the vehicle's status is back on the air

`ST-<state><armed><calibrated><faults>` — `ST-R110` is READY, armed, calibrated, no active
faults — on every rich packet it fits. Nine bytes, and opportunistic: dropped silently from any
packet it would push past 200, so it moves no budget, slot or sensor field. Both parsers expand
it into the `MODE`, `ARM`, `CAL` and `FAULTS` tags, and the console holds the last one through
lean packets for up to 3 s of vehicle clock. `test-data/status-tag-cases.tsv` defines it for the
encoder and both decoders.

### Fixed — the bridge talked over the vehicle

The bridge relayed a command the instant the PC sent it, and a half-duplex radio cannot listen
while it transmits. A command landing on a telemetry packet cost that packet — both `MAX_RATE`
bench runs lost exactly one, 1 in 544 and 1 in 327, each with one command sent — and one landing
while the vehicle was transmitting was never heard at all. The bridge now holds a command
(`#tx=queued`) until 20 ms after it next hears the vehicle, when the vehicle is listening and
its next packet is at least ~375 ms away, and sends it anyway after 1.5 s of silence
(`ground/uplink_timer.hpp`).

### Not changed — transmit power

17 dBm is the most the RA-02 gives on PA_BOOST for continuous use. The SX1278's 20 dBm mode is
limited by its datasheet to 1 % transmit duty; this vehicle transmits 46 % of the time, and 84 %
after `MAX_RATE`. At −35 dBm on the bench the signal was not what lost the packet.

---

## [Unreleased] — 2026-09-11 (cycle 50)

The range-test log, and the organizers' ground station. Their receiver code was shared with the
team: it discards any packet over 200 bytes and listens only on `0xA5`. On the 2026-09-10 range
test it heard a few packets at a slower rate while the team's own station heard every one.

### Changed — every packet fits the organizers' receiver

- **Sync word `0xA5` on both Picos**, for testing as well as the launch. The vehicle was on the
  test word, `0xF3`, and their station listens on nothing else. The bring-up image follows the
  bridge.
- **A 200-byte budget**, `kGroundStationMaxPacketBytes`, their `MAX_PACKET_SIZE`. GPS goes on the
  air at 5 decimals and whole metres — 1.1 m against the receiver's ~2.5 m, and a vertical error
  of several — so mandatory + GPS are 198 at their widest; the SD row keeps 6 and 1. `SN-` is
  shed from any packet that would pass 200, then `GP-`. `validate_config()` refuses a flight
  budget above 200; a tagged bench build may still use the FIFO.
- **A 50 ms guard between max-rate packets**, up from 40: their receiver prints each packet to a
  115200-baud port, about 35 ms deaf, before it listens again. Slots 374 and 296 ms, a 966 ms
  cycle: 3.11 Hz with the sensors at 1.04 Hz, 84 % duty. Normal flight stays 1.43 Hz, now 46 %.

### Fixed — from the range-test log

- **GPS fixes with no satellites went on the air.** `RMC` set a fix on its own, with no
  satellite count, HDOP or altitude for the gates to judge, and the log holds 219 of them — 0
  satellites, HDOP 0.0, `GP-Alt-0.0`. `RMC` now renews the ground track only, and a refused
  `GGA` no longer renews the fix clock, so a position the gates keep refusing ages out.
- **`fault_total` counted polls, not faults.** It rose 21 a second on a vehicle whose only fault
  was no GPS fix: `gps_unavailable` was counted on every 33 ms poll. Conditions re-checked every
  cycle now count once per episode, through `FaultManager::hold()`.
- **A hung transmission reset the board instead of raising `radio_tx`.** The transmit timeout
  was 2000 ms, the watchdog's own. It is 1000 ms.
- **The sensor loop stood still while a packet was on the air.** `Sx1278::transmit()` waited
  for TxDone, so the 33 ms sensor task ran 15 times per 700 ms packet in the log instead of 21,
  and about 11 times a second after `MAX_RATE` — roughly a 9 Hz loop in flight, with gaps of up
  to ~320 ms. The driver now starts a packet and returns (`start_transmit()` and
  `poll_transmit()`), the controller counts it sent or failed when it ends, and a packet still
  on the air holds the next one back. The blocking `transmit()` stays for the bridge and the
  bring-up image.

### Checked — the organizers' receiver's modem settings

Frequency, spreading factor, bandwidth, coding rate, CRC, preamble, header mode, IQ and
low-data-rate optimisation all match their arduino-LoRa receiver. The carrier registers are
identical: both write `0x6C4000` for 433 MHz. The sync word was the only mismatch.

### Changed — the password

`change-me`, at the team's decision. The build still refuses the template's `SET-ME`.

### Found — not yet fixed

- **Two boots began with one fault more than a clean power-on**, which is what a watchdog reset
  leaves behind; the sessions before them ended at 3 min 43 s and at 23 s. The log cannot say
  why.

---

## [Unreleased] — 2026-09-11 (cycle 49)

The first bench run of the new schedule, what it found, and the pre-arm command window it
led to.

### Measured — `MAX_RATE` holds at 3.11 Hz

With arming held off by hand, `MAX_RATE` was accepted (`commands accepted 1`), the vehicle's
summary read `COMMANDED MAX` in every block for the rest of the run, and the station measured
**3.11 Hz** against a predicted 3.13, with 1 packet in 544 lost at bench range. No reset. The
latch holds. Bring-up rows 8.17 and 8.18 carry the numbers and their verdicts.

### Found — the old fallback was a command that never latched

The earlier build that "fell back to 1.43 Hz" never latched. The command window was READY with
`ARM-0`, and a still vehicle calibrates and arms about three seconds after power-on — so the
window closed before anyone could use it, and a press after that was simply not heard.

### Added — a five-minute pre-arm command window

A clean power-on on a build with the uplink now opens a five-minute window: the vehicle
transmits and listens and **does not arm**. `MAX_RATE` or the timeout closes it; the vehicle
then discards its power-on calibration, recalibrates where it sits on the pad, and arms. A
watchdog reset skips the window, because a reset in flight must not leave the vehicle unarmed
for five minutes. **A launch inside the window is not detected**, and the runbook says so.

The console counts the window down from the vehicle clock — the `ARM` tag is off the air, so it
is an estimate and is labelled one — and greys out **Max rate** once the window has closed. A
claim check holds the console's window length to the firmware's.

### Changed — the password leaves the repository

The uplink is now part of a flight build, so the password cannot be `change-me`. It lives in a
gitignored `local_secrets.hpp`; the build refuses a placeholder or anything under eight
characters, and **without the file the build has no uplink at all** — no window, arming three
seconds after power-on as before. The bench-only flags that used to sit uncommitted in
`main.cpp` are gone.

### Fixed — the summary quoted a slot, not a rate

After `MAX_RATE` the summary printed whichever slot it landed in — "385 ms (2.60 Hz)", then
"286 ms (3.50 Hz)". It now prints the pattern and its real rate.

---

## [Unreleased] — 2026-09-11 (cycle 48)

The organizers ruled that only transmitted telemetry earns extra-sensor points, and the
packet changed shape around that ruling. Nothing in this cycle has transmitted a packet on
hardware yet.

### Changed — GPS and sound on the air, tags off it

Until now neither GPS nor the microphone was transmitted in normal flight; both went to the
SD log, which under the ruling scored them nothing. Every normal-flight packet now carries
`GP-Lat`, `GP-Lon`, `GP-Alt` and a new `SN-` sound level after the twelve mandatory fields,
at the same 700 ms and 1.43 Hz. The room came from the five diagnostic tags — `MODE`,
`FAULTS`, `CAL`, `ARM`, `YR` — which left the air by default: tags, GPS and sound together
are 267 bytes at their widest, past the FIFO, and the tags are the only part the rulebook
does not reward. They stay in the SD log. **The console loses its live view of mission state
as a result**, and says so rather than guessing.

### Changed — one max-rate command, and it is a pattern of slots

`MAX_RATE_GPS` and `MAX_RATE_LEAN` became `MAX_RATE`. After it the vehicle sends one rich
packet — mandatory, position, sound — then two lean ones, in slots of 385, 286 and 286 ms:
957 ms a cycle, **3.13 Hz**, with the sensors still on the air at **1.04 Hz**. Three packets is
not a preference; a fourth slot would take the sensors under once a second. `PeriodicTask`
gained `reschedule()`, because the interval after a packet depends on which shape it was.

### Fixed — the widths were a commit message, not a measurement

The first revision of the design carried 145 bytes for the mandatory block and 56 for GPS.
A test that constructs the widest packets — packet number 4294967295, a 99-hour clock,
extreme negatives — said **147 and 55**, and 147 crosses a LoRa symbol boundary that 145 does
not. The lean slot moved from 281 to 286 ms and the rate from 3.17 to 3.13 Hz. The
`static_assert`s had already refused 363 and 280 ms in the design before that, for being
inside their own guard by a fraction of a millisecond.

### Changed — a byte floor that protects the sensors

`validate_config()` now refuses a budget smaller than the mandatory block plus whatever of
GPS and sound is enabled. The failure it refuses is silent: a budget too small for the
sensors has the controller shed them from every packet, and the sensor points go to zero with
no fault. The tags are deliberately not counted — they are shed first and cannot displace
the sensors. The controller's own shedding step used to drop the fix from that packet's SD row
as well; it now takes the sensors off the air only.

### Added — the ground side reads `SN-`

The Python station and the web console learned the field in one commit, because a test holds
their field names equal. The console shows the sound level under the GPS panel and holds the
last value through the lean packets, rather than flickering to a dash between rich ones.

### Added — the vehicle says what its uplink did

The startup summary prints whether the uplink is listening or closed and how many commands it
has accepted and refused. It was added to chase an open bench fault and kept because it is the
one line an operator needs when a command appears to do nothing.

### Open

- **The bench fallback.** A build of the previous design raised the rate and then returned to
  exactly 1.43 Hz with packet numbers still climbing, so the vehicle did not reset — and nothing
  in the firmware restores the period without one. The summary's new uplink line is what will
  say whether the vehicle ever latched. Bring-up rows 8.16 to 8.18 are waiting for it.
- **The bench card reports `SD card FAILED`.** With the tags off the air the SD log is the only
  in-flight record of mission state, so this matters more than it did.
- **Official ground-station compatibility.** The updated guidelines require it, and this
  repository has never seen the Physics Club stations' modem parameters.

---

## [Unreleased] — 2026-09-10 (cycle 47)

Two ground commands that raise the packet rate and close the uplink behind themselves, and
a token flaw found while adding the second one.

### Fixed — a token authorised any command, not one command

`command_token()` hashed the password and the packet number; the `CMD-` text was parsed
beside it rather than covered by it. **So a captured `ERASE_LOG` frame became a valid
`MAX_RATE_LEAN` frame by editing four words, key untouched.** Invisible while there was one
command, unacceptable with three, two of which cannot be undone — so the fix landed before
them rather than with them.

The material is now `password | COMMAND | packet_number`, `parse_command()` resolves the
name before checking the token, and an unrecognised name is `none` rather than the nearest
match. `test-data/command-tokens.tsv` gained a command column and was regenerated: 14 rows,
at least one per kind, both non-ASCII rows kept.

### Added — `MAX_RATE_GPS` and `MAX_RATE_LEAN`

| | Period | Rate | On the air |
|---|---:|---:|---|
| Normal flight | 700 ms | 1.43 Hz | Mandatory fields plus five diagnostic tags |
| `MAX_RATE_GPS` | **364 ms** | **2.75 Hz** | Mandatory fields **plus position** |
| `MAX_RATE_LEAN` | **281 ms** | **3.56 Hz** | Mandatory fields only |

**The gain is not a duty-cap decision.** 700 ms was already the fastest the 199-byte packet
could go under the 50 % policy. The rate comes from the packet instead: shedding the five
project-local tags — which the rulebook's mandated twelve fields do not contain — takes the
worst case to 145 bytes and 240 ms of measured airtime.

**And position turned out to be free.** LoRa quantises the payload into symbol blocks, so
201 bytes and 199 both come to 298 symbols at SF7/125 kHz and cost the same 317.70 ms. The
three `GP-` fields are paid for entirely by the tags that leave, which is the whole reason
the GPS variant exists.

**The period is airtime + 40 ms, not airtime / duty.** The 40 ms is one worst-case SD block
write ([F-11](documentation/testing/bring-up-record.md#findings): 30 ms, on two boards, in
two of five sessions) plus the sensor loop and the watchdog feed. The rulebook scores
consistency on the same five points as rate, so a period that fits a duty target but not the
card buys rate by making packets late.

**The `static_assert`s earned their place immediately.** The design document said 363 and
280 ms; 323.41 + 40 is 363.41 and 240.02 + 40 is 280.02, each inside its own guard by a
fraction of a millisecond. The build refused them. 364 and 281, and the document corrected
to match rather than the other way round.

### Added — the latch, and why it is four things

One accepted command sets the period, the packet shape, the runtime oversize cap and the
uplink itself. `service_ground_commands()` checks the latch first, so afterwards there is no
poll, no parse and no RX for the rest of the power cycle — **including for the other rate
command, which is therefore not refused but unheard.** Whichever lands first wins.

The packet is set by the command rather than inherited from the build. That is the
difference between transmitting 201 bytes on a 364 ms period and 255 on 281 — an airtime it
cannot fit, discovered on the air, with no way left to tell it to stop.

The latch lives in RAM. A power cycle or a watchdog reset restores the flashed
configuration, which is deliberate: a card carrying "max rate, no uplink" from a bench
session would apply it silently to the next flight.

### Fixed — the builder had to be told separately

`TelemetryBuilder` holds a **copy** of the configuration, so the controller changing its own
copy reached every accessor a test would read and nothing that renders a packet. Found by
writing the test first; it would have passed the controller-level assertions and transmitted
a packet with no `GP-` fields in it.

### Changed — the console, and the vehicle's own summary

Three buttons share one send path and one set of guards; what differs is what each says
before it asks for the password. Two of the three cannot be undone by any later command, and
the prompt says so in those words.

The startup summary prints the **live** period rather than the configured one, and the state
line reads `rate COMMANDED MAX - uplink closed` once latched — the configured value stops
being true the moment a command is obeyed.

### Verification

flight_tests 113 → 123 suites and 4107 → 4204 assertions; sx1278 129 → 139; Node 62 → 65;
documented claims 278 → 286, including both rates and both periods derived from
`link_profile.hpp` and the 201-costs-what-199-costs identity the GPS variant rests on.

**None of it has transmitted a packet.** Bring-up rows 8.16, 8.17 and 8.18 are what would
change that — one per variant, plus loss at the commanded rate, since the rulebook scores
rate and packet loss on the same five points and a rate that costs packets is not a gain.

---

## [Unreleased] — 2026-09-09 (cycle 46)

A repository-wide pass with nothing new built. Everything below is something the repository
already knew and one of its documents had not been told.

### Fixed — the timeline was a day behind its own project

[timeline.md](documentation/project/timeline.md) carried **Status date: 2026-09-08** while
**eight changelog cycles, 38 through 45, are dated 2026-09-09** — the cycles in which the
mechanical design arrived, the envelope question was answered, the electronics went on a
scale and the print orientation was chosen. The document that exists to say where the
project stands was the last to hear about the biggest day it has had.

Its history table and mermaid timeline both stopped at 2026-09-08, so a 2026-09-09 row was
added to each, and *"nearly two hundred commits over six days"* is now **207 over seven**.

### Fixed — two gates were still reporting that no link existed

The same file's gate table said this, three sections below its own entry recording that the
first radio link closed on 2026-09-07:

| Gate | Said | Says |
|---|---|---|
| 5 · Telemetry verified | *"**No link has been established** — one radio transmitting is not two radios talking"* | **A link has been established** — 66 packets, `P-001` to `P-066`, no gaps, no duplicates, sync word `0xF3` |
| 6 · Ground station verified | *"The receive pipeline has still never seen a packet that arrived over the air"* | **It has now seen 66**, parsed, validated and counted, with `dropped=0` throughout |

**Neither is a small error, because a gate table is read to decide what to work on next.**
Both rows stay 🟠 Partial — what is actually missing is now named instead: the `0xA5` sync
word, a loss figure over the 500-packet window rows 8.1 and 8.2 ask for rather than 66, the
in-flight transmissions, and compatibility with the official dual ground stations.

### Fixed — two stale counts and a decision recorded as still open

- The timeline's tooling row said **33 Python tests**. Cycle 41 took that suite to **49**,
  and [test-plan.md](documentation/testing/test-plan.md) has said 49 ever since. The
  timeline was the copy nobody updated.
- The timeline's phase 7 still asked to *"decide and record the print orientation"* — decided
  in cycle 45. It is struck through now, and the item that decision **exposed** replaces it:
  **all three studies load horizontally, and a vehicle under a parachute lands base-first**,
  along the build axis, which is the print's weak direction.
- [mechanical/README.md](mechanical/README.md) contradicted itself on the same point,
  asking for the orientation in its material section and recording the decision in its
  open-items table sixteen lines later.

**The pattern is worth naming.** Every one of these is a document left behind by work
recorded correctly elsewhere, and `check_doc_claims.py` caught none of them — it holds
numbers against the source that defines them, and *"no link has been established"* is prose.
**278 / 278 claims passed the whole time these five statements were false.**

### Changed — the CI actions were three majors behind

| Action | Was | Is |
|---|---|---|
| `actions/checkout` | v4 | **v7** |
| `actions/setup-python` | v5 | **v7** |
| `actions/setup-node` | v4 | **v7** |
| `actions/upload-artifact` | v4 | **v7** |
| Node | 22 | **24** |

The v5–v7 majors are Node 24 runtime moves and an ESM repackaging. The one real behaviour
change — checkout v7 blocking fork-PR checkout under `pull_request_target` and
`workflow_run` — **does not reach this workflow**, which triggers on `push`, `pull_request`
and `workflow_dispatch`. setup-python v7 drops a `pip-install` input this repository never
used.

**Node moves to 24 because that is what the development machine runs**, and the web console
suite is the only thing that needs it. **Python stays on 3.12 deliberately**, for the reason
now written beside the pin: everything here has only ever been run on 3.12, and a CI
interpreter ahead of the one the work is done on reports failures nobody can reproduce.

### Verified

`tools/build_host.sh` clean after every change above: **4879 C++ assertions, 229 Python
tests, 62 Node tests, 278 / 278 documented claims** — 5170 checks, zero failures.

---

## [Unreleased] — 2026-09-09 (cycle 45)

Three answers, and the first one resolves the contradiction cycle 43 could not.

### Fixed — the "expected to break" line is template text, and the 15 proves it

**Minimum safety factor is 15 in all three studies**, read out of Fusion directly.

**That is a display cap, not a result**, and noticing so is what settles the question. The
three studies' peak stresses differ by more than a factor of two — 2.885, 1.330 and
2.345 MPa — so their true minimum safety factors cannot all be exactly 15:

| Study | Max von Mises | Yield ÷ stress |
|---|---:|---:|
| Horizontal | 2.885 MPa | **18.9** |
| Tearing | 1.330 MPa | **40.9** |
| Impact | 2.345 MPa | **23.2** |

Fusion's safety factor legend caps at 15 by default. All three sitting on it means **≥ 15
everywhere on the part, in every load case** — which is a stronger statement than a single
minimum, and it agrees with the yield-derived figures above.

**So the sentence *"the design is expected to bend permanently or break"*, which appears in
all three reports, is template text and can be disregarded.** It was printed above a block
containing *both* guided-result branches, and Fusion's Result Summary table exported empty,
which is why it took a number from outside the report to settle it.

### Added — print orientation, and the derating it makes possible to compute

**Decided 2026-09-09: printed as modelled, sitting on its base.** Layers stack vertically, so
the weak directions are tension normal to the layers and interlayer shear.

That turns the anisotropy caveat from a warning into arithmetic:

| Study | Yield ÷ stress | ×0.40 | ×0.55 | ×0.70 |
|---|---:|---:|---:|---:|
| Horizontal | 18.9 | **7.5** | 10.4 | 13.2 |
| Tearing | 40.9 | 16.4 | 22.5 | 28.6 |
| Impact | 23.2 | **9.3** | 12.8 | 16.2 |

**Even at the pessimistic 40 %, and even taking the capped 15 rather than the yield-derived
figure, the effective safety factor is 6.** The margin was worth having.

**One load case is still missing, and it is the one that matters most.** All three studies
load horizontally. **A vehicle hanging under a parachute lands base-first** — along the
build axis, which is exactly the print's weak direction. That case has not been run.

### Changed — the switch has a home

**The rectangular cutout in the structure is the switch**, confirmed 2026-09-09, with the two
indicator LEDs in the round holes beside it. Three of the four penetrations the vehicle needs
are accounted for; **USB access for the Pico is the one left to confirm**, against 2.5 mm of
clearance per side.

`PWR-001` records it. The part has been held since 2026-09-06 and is still not fitted, but it
is no longer a part without a place to go.

### Changed

Claims 273 → **278**. The new ones hold the capped safety factor as a **floor** rather than
a result, keep the yield-derived margins beside it so nobody reads the cap as the answer, and
check the derated worst case — which is the figure that actually answers whether a *printed*
part survives.

---

## [Unreleased] — 2026-09-09 (cycle 44)

### Changed — the structure has a mass, and the risk turns out to be the other way round

The PETG structure is estimated at **193 g**, from its solid volume at 1.27 g/cm³. With the
electronics measured at 151.299 g that puts **344.299 g committed**.

**The estimate cross-checks two ways**, which matters because it is the only figure in the
budget not on a scale:

- 193 g at 1.27 g/cm³ implies **152.0 cm³** of solid material — **10.1 %** of the
  118.5 × 115 × 110 mm bounding box, the right order for an open frame with two faces cut
  away.
- Fusion, using the PET material the studies assigned, would report **234.2 g**; the ×0.82
  density correction gives **192.0 g**.

It is also an **upper bound**: solid volume × density is the part with no infill saving at
all, and thin walls print mostly as perimeters, so the real print lands at or under it.

### The finding: this vehicle is more likely to come in light than heavy

**Two days ago this page said mass was "very unlikely to be the binding constraint" and that
the structure had "upwards of 380 g" to spend. Both were wrong, and in opposite directions.**

| | |
|---|---:|
| Committed | 344.299 g |
| To reach the **450 g floor** | **+105.7 g needed** |
| To stay under the **550 g cap** | +205.7 g available |

Parachute and harness at 30–55 g, an egg chamber at 30–60 g and fasteners at 10–20 g
project an all-up mass of **414–479 g** — and **the lower half of that is under 450 g.**

**Whether that matters is genuinely unclear, and it is now open question 12.** `GEN-005`
states *"500 g (±10%)"*, which reads as a band. But the disqualification condition in
`GEN-006` is explicitly one-sided — *exceeding* the limit by more than 10 % — and an
underweight vehicle appears on no disqualification list. Band, or ceiling with a nominal
attached.

**If it is a floor, the fix is cheap and worth doing regardless: thicken the walls.** It adds
mass exactly where the [anisotropy caveat](mechanical/simulation/README.md) says a printed
part is weakest, so it buys strength margin and mass in one change — a slicer setting and a
re-print rather than a redesign. Ballast is the cruder alternative and earns nothing.

**It has to be known before the print, not after**, which is why it goes in the same email as
the envelope question rather than waiting.

### Changed

`GEN-005` and `MEC-002` record the open lower edge. `check_doc_claims.py` holds the five
masses, the distance to **both** edges of the band, and the volume the structure estimate
implies — a density slip would move that by an order of magnitude and nothing else would
catch it. Claims 270 → **273**.

---

## [Unreleased] — 2026-09-09 (cycle 43)

Three simulation reports, two renders, a material, and the first time the electronics have
been on a scale. One of those overturns an estimate this page made two days ago.

### Added — the structural studies, and what they do and do not establish

Three static-stress studies, run in Fusion on 2026-09-09, now in
[`mechanical/simulation/`](mechanical/simulation/README.md):

| Study | Load | Mesh | Max von Mises | SF vs 54.40 MPa yield |
|---|---|---|---:|---:|
| Horizontal force | 30 N on +Z | 5838 nodes | **2.885 MPa** | ~19 |
| Tearing force | 30 N on −X | 5838 nodes | **1.330 MPa** | ~41 |
| Impact force | 100 N on −X | 7148 nodes | **2.345 MPa** | ~23 |

**The structure is nowhere near failing in any of the three.** That is the headline and it
is a good one. Four caveats keep it honest, and none is small:

**1 · The reports say the design breaks, and it does not.** All three contain *"the design
is expected to bend permanently or break"* — contradicted by their own stress plots by a
factor of 19 to 41, and printed above a block containing *both* the "Below Safety Factor
Target" and "Above Safety Factor Limit" advice lists, which is what a template emits when it
renders every branch. **It has not been confirmed either way**, because Fusion's *Result
Summary* table **exported completely empty** in all three reports. Every number above was
read off a colour-bar legend in a result plot. The minimum safety factor — the one number
these studies exist to produce — has never actually been read, and going back for it is now
an open item.

**2 · The simulation material is PET; the part is being printed in PETG.** Density 1.541
against ~1.27 g/cm³ — **21 % high**, which lands directly on the mass budget — plus a
stiffer modulus and a slightly higher yield.

**3 · A printed part is not isotropic, and these studies assume it is.** FDM inter-layer
strength is typically 40–70 % of in-plane. Which loads are the weak ones depends on print
orientation, which is not in the model and has not been decided. At safety factors of 19–41
there is room to absorb that; at 2 there would not have been.

**4 · The 100 N impact load assumes a 25 ms arrest.** A 0.5 kg vehicle at 5 m/s carries
2.5 N·s, so the force is entirely a function of how long stopping takes: 25 ms is 100 N,
10 ms is 250 N, 5 ms is 500 N. 100 N models a compliant arrival on grass. Concrete is
several times worse. The assumption is not wrong — it just has to be written beside the
result.

### Added — PETG, and the reasoning section D asks for

The body is out for 3D printing in **PETG**, decided 2026-09-09. Tougher than PLA and not
brittle, printable without an enclosure unlike ABS, and it deforms rather than shatters —
which is the failure mode that leaves a recoverable vehicle. Section D awards a bonus for
material selection and expects the argument, so it is written down.

**Print orientation is now the open mechanical decision.** It is the single free variable
that changes a printed part's strength, it costs nothing at slicing time, and it cannot be
changed afterwards.

### Fixed — the mass estimate was wrong in the dangerous direction

**Measured 2026-09-09:** assembled vehicle PCB **110.573 g**, battery **40.726 g**,
electronics all-up **151.299 g**.

The estimate they replace counted ~67 g of vendor figures with three `TBD` rows in it and
concluded the avionics were *"unlikely to exceed ~120 g"*. **The measured figure is 26 %
past that ceiling**, and it makes the electronics **30 % of the entire mass budget** rather
than the ~14 % the estimate implied. The two lines that were `TBD` — wiring, headers,
passives and solder, and the antenna — are most of the difference.

**348.701 g remains** for structure, egg chamber and parachute. That is comfortable for a
printed frame but it is not unlimited, and PETG is not weightless: 200 cm³ of solid PETG is
254 g. The slicer's own mass estimate is the best number available before anything is
printed and costs one minute; Fusion's figure needs multiplying by 0.82 first, because the
assigned material is the denser PET.

**The lesson is one this repository keeps relearning.** A vendor figure is not a
measurement, and a sum of vendor figures with `TBD` rows in it is not a budget — it is an
estimate wearing a table's clothes. `check_doc_claims.py` now holds the four masses and the
percentage to each other, so the arithmetic cannot drift even if the measurements do.

### Added — renders, and a home for them

[`mechanical/photos/`](mechanical/photos/README.md) holds two CAD renders of `Cansat_D1`:
an open box frame with two solid side panels, two faces opened out with arched cutouts, a
central spine, harness slots top and bottom, and a square cutout with two small round holes
beside it on one upper face.

**That last feature is worth confirming against the electrical design.** The vehicle needs a
USB cutout, a manual switch, a power LED and a status LED — four penetrations — and there
is [2.5 mm of clearance per side](mechanical/README.md#the-envelope-question--asked-and-answered)
before the envelope is exceeded. If those two small holes are the LEDs, the switch has
nowhere yet.

Photographs of the printed article belong in the same directory. Section D awards **15 of
its 30 points for aesthetics and build quality**, which is judged from what the vehicle
looks like.

### Changed

`MEC-002`, `MEC-003` and `MEC-005` move to `In Progress`. Claims 262 → **270**.

---

## [Unreleased] — 2026-09-09 (cycle 42)

### Changed — the organizers answered, and the design fits

**Open question 11 is closed: a 12 cm sided box is acceptable.** Confirmed with the
organizers on 2026-09-09, hours after the question was raised.

So the section limit is a **120 mm square**, not a 120 mm bore, and `Cansat_D1` fits:

| | |
|---|---:|
| Envelope section, confirmed | 120 × 120 mm |
| Design section | 115 × 110 mm |
| **Clearance per side** | **2.5 and 5.0 mm** |
| Height | 118.5 mm against 210 mm |

**The question was worth asking.** Read as a diameter, the 159.1 mm corner-to-corner
diagonal would have been **33 % over**, and exceeding a dimensional limit by more than 10 %
is a disqualification rather than a deduction — the fallback would have been a structural
redesign plus a board rebuild. It was not a scored margin; it was the whole flight.

**What is left of it is the clearance, and that is now the live constraint.** 2.5 mm per
side on the wide axis is not much. Any feature protruding past the modelled body — a
switch boss, an LED bezel, a connector, a parachute attachment, the antenna — has to live
inside it, or the envelope grows past 120 mm and the question reopens as a real failure
rather than an interpretive one. The claim checker now holds the documented clearance to
what the STEP says, where it previously held the diameter overage.

> **Keep the written confirmation with the submission.** The answer is currently recorded
> only in this repository, and it is a disqualification-class dimension rather than a
> scored one.

The [envelope drawing](mechanical/drawings/envelope-and-board-fit.svg) is redrawn against
the confirmed limit: a 120 mm square section with the design inside it and the per-side
clearance dimensioned. The bore that was ruled out stays on the drawing, faint, so somebody
reading it next year can see the question was answered rather than never considered.

`MEC-001` moves to *designed and compliant; nothing built*. The mechanical badge moves from
`not started` to `designed · not built`.

### Fixed — a commit message mangled by its own backticks

Cycle 41's message was passed to `git commit -m` inside a double-quoted shell string
containing a backticked literal. Bash substituted it as a command, so the sentence about the
STEP real `8.` lost the very literal it was about. Amended and force-pushed with
`--force-with-lease`; the tree was verified byte-identical first, so only the message
changed. Commit messages go through a file from here on, as the changelog entries already do.

---

## [Unreleased] — 2026-09-09 (cycle 41)

The mechanical design arrives, and it overturns a conclusion this repository reached three
days ago about a board that could not fit.

### Added — `Cansat_D1`, and a reader for it

`mechanical/CAD/` now holds the Fusion archive and a STEP export of the same solid. The
`.f3d` is committed for the team rather than for the tooling: its container uses a
compression method standard tools do not implement, and its contents are Autodesk binary
blobs. What is readable from it is the directory listing, which is how we know it carries
**three simulation studies**.

The STEP is readable, and [`tools/cad_dimensions.py`](tools/cad_dimensions.py) reads it:

```text
mechanical/CAD/Cansat_D1.step
  solids            Body1
  faces             56
  bounding box      115.0 x 118.5 x 110.0 mm
  cross-section     159.1 mm diagonal (the two smallest extents)
  circular features D5.1, D12.0, D16.0, D45.0 mm
```

**No document types those numbers.** `check_doc_claims.py` extracts them and fails the build
if `mechanical/README.md` disagrees, and the envelope drawing is generated from the same
file. Re-export the STEP after a model change and the build names whichever figures moved.

### Fixed — the board-fit conclusion was reasoned against an assumption

Cycle 36 concluded that the 100 × 100 mm vehicle board **could not be mounted flat** and
should go in edge-on as a spine, because a 100 mm square needs a 141.4 mm bore and the
envelope is 120 mm across. That reasoning assumed a **circular** section, three days before
there was a design to check it against.

**The design is prismatic — 115 × 110 mm — and the board fits flat with 15 and 10 mm to
spare.** The edge-on recommendation is withdrawn. It remains the right answer for a circular
section, where a 120 mm bore caps a flat deck at 84.9 mm square, and the drawing still says
so.

This is the second time in this project that a confident conclusion turned out to rest on an
unstated assumption about a part nobody had yet seen — the first was the microSD reader's
supply voltage. Both were caught by the artifact arriving, not by re-reading the reasoning.

### Added — open question 11, which is a disqualification-class one

**"12 cm across" is not defined as a width or a diameter, and a prismatic body answers the
two differently:**

| Reading | Verdict |
|---|---|
| Width — no face wider than 120 mm | 115 and 110 both pass |
| Diameter — fits a 120 mm bore | needs **159.1 mm**, **+33 %** |

Exceeding a dimensional limit by more than 10 % is a **disqualification**, not a deduction.
Under the width reading the design passes comfortably; under the diameter reading it is not
marginal, it is out. The vehicle is drone-released and never passes through a tube, which
argues for the width reading — but the cost of being wrong is the whole flight, and the
fallback is a structural redesign *plus* a board rebuild.

The [envelope drawing](mechanical/drawings/envelope-and-board-fit.svg) now draws both
readings over the actual section so the question can be asked with a picture attached.

### Fixed — a STEP real of the form `8.` was being silently dropped

Found while writing the dimension reader, and it is exactly the failure its own docstring
promises not to have. ISO 10303-21 writes the integer 8 as `8.` — dot, no digits after it —
and a regex requiring a digit after the decimal point matches `8`, then fails on the `)` it
expected. The radius is skipped.

It first showed as the design's `D16` feature being absent from the report. **The same
pattern feeds the bounding-box expansion**, so a part whose extreme lay on such a circle
would have been measured too small — and a bounding box that is quietly too small says a
part fits an envelope it does not fit.

Sixteen tests now cover the reader, including every STEP real spelling, a circle bulging
fifty times past every vertex, a tilted circle expanding by its projection rather than its
full radius on all three axes, and the two files it must **refuse** rather than
under-measure: one containing B-splines, whose extremes need not lie on any control point,
and one in metres.

### Changed — the power LED hangs off `+3V3`, and the LEDs have colours

The netlist and `electrical-architecture.md` had disagreed for two days about which node the
power LED sits on — the netlist said the switched battery node, the architecture said the
3.3 V bus. That contradiction was introduced in cycle 36 when the netlist was written.
**It is `+3V3`**, and the trade is now recorded rather than settled silently:

- **Brightness must not depend on charge.** PWR-002 requires the indicator to be *visible*;
  on the battery node it would dim from 4.2 V to 3.4 V over a flight.
- **The current is deterministic**, so the resistor is sized to a value rather than a range.
- **It tracks the question radio silence asks.** On `+3V3`, lit means powered means
  potentially transmitting (TEL-026). On the battery node it would mean only "connected".

PWR-003's "immediately" is satisfied either way, because the LED reaches a rail through a
resistor with no firmware in the path — which is precisely why it cannot be a GPIO.

**The colours are recorded: one red and one green 5 mm.** Red takes the mandatory power
indicator and green the status LED, because **5 mm green is ambiguous** — older dice are
≈2.0 V, modern bright ones ≈3.0–3.4 V — and at 3.3 V through 1 kΩ that is the difference
between 1.30 mA and 0.10 mA. Red is not ambiguous, so the risk goes on the diagnostic read
at arm's length rather than on the rulebook indicator.

**And 1 kΩ is bench-bright, not daylight-bright.** It gives the power LED 1.30 mA, which is
obvious on a desk and marginal in sunlight where a judge will see it. 220–470 Ω gives
2.8–5.9 mA and costs 0.4 % of the pack over an hour. Neither value is held; the purchase
list previously concluded no low-value resistor was needed, on the strength of bench
visibility alone.

### Changed

`.gitattributes` marks `*.f3d`/`*.f3z` binary and `*.step`/`*.stp` text. Without the first,
git's heuristic could apply line-ending normalisation inside a compressed archive and
corrupt it silently.

`MEC-001` moves to *designed, limit's meaning open*; `MEC-003` and `MEC-006` to
`In Progress` — MEC-006 cannot be `Complete` until there is a built article for the model to
represent. Python tooling tests 33 → **49**. Claims 255 → **261**.

---

## [Unreleased] — 2026-09-09 (cycle 40)

### Added — the radio-silence procedure, and what writing it exposed

**TEL-026 closed** — other CanSats powered off during another team's launch. It is the
companion to cycle 39's sync-word procedure and the other half of switch discipline, which
the 2026 revision made a scored activity rather than a courtesy: at 1.43 Hz, −1 point per
2 packets is **0.71 points per second**, so 35 seconds of a vehicle left on throws away the
entire 25-point telemetry section.

**The firmware cannot help here, and is not meant to.** `PWR-004` requires telemetry to
begin automatically at power-on, and the controller satisfies it — it transmits from
`READY` with no arming step. There is no silent mode and there will not be one. **A powered
vehicle is a transmitting vehicle**, so the only control is physical.

What makes the procedure verifiable rather than assumed is that **the team's own bridge is
the detector**, at no cost and with no new code. With the vehicle supposedly off, `frames=`
in the bridge status line must not move over 30 seconds; if it does, the team identifier in
the packet says whose it is — yours, or another team testing on `0xF3`, which is what
`0xF3` is for. Same trick as the sync-word procedure: an existing counter turning an
assumption into an observation.

The procedure also covers the phase where a vehicle is most likely to be left transmitting
— **recovery**, when it is in someone's hands, the flight is over and attention has moved
on — and it asks for one *named person* to own the power state for the whole event.
Shared ownership of a switch is how a vehicle gets left on.

### Fixed — three requirement rows that were wrong, not stale

Writing TEL-026 meant looking at what executes it, and **`PWR-001`, `PWR-002` and `PWR-003`
— the manual switch and the power LED — all read `Blocked`.** That status is defined
in this document as *"requires missing hardware, an external decision, or another
prerequisite"*.

**Nothing is missing.** Both parts have been held since 2026-09-06 and are listed as held in
the purchase list. They are simply not fitted, which is `Not Started`.

The distinction is not pedantry. Calling unfitted parts *blocked* hides work that could be
done in an evening behind a word that means somebody else's problem — and these two are
the only control behind TEL-026, so their absence costs considerably more than the five
points they carry on their own. All three rows now say what is actually true: the part is
held, its place in the power path is drawn in the netlist, and nothing stands in the way.

`PWR-003` gained the reasoning that makes it satisfiable by construction: the LED hangs off
the **switched rail** through a series resistor, so it lights the instant the switch closes.
A GPIO-driven LED could not meet "immediately" — it would wait for boot — and the
separate GP14 status LED is firmware-driven and is not this indicator.

### Changed — a procedure cannot outlive the hardware it assumes

`check_doc_claims.py` now cross-checks the runbook against the requirements table: while
`PWR-001` is not `Complete`, the radio-silence procedure **must** carry its warning that the
switch is not fitted and that "off" therefore means the battery lead is physically out with
nothing indicating it from outside the structure.

**The check flips when the switch is fitted.** Marking `PWR-001` complete without deleting
the warning fails the build, because at that point the warning is the stale thing. A
procedure that tells an operator to open a switch that does not exist reads as authoritative
and is unfollowable, which is worse than an obvious gap.

Requirements `Complete` 36 -> **37**. Claims 254 -> **255**.

---

## [Unreleased] — 2026-09-09 (cycle 39)

### Added — the two ends of the link can no longer disagree about the sync word

**TEL-025 closed.** It was the last requirement whose implementation column read *"Launch
configuration procedure - TBD"* for something purely procedural, and it stayed there because
a procedure on its own is a promise.

The sync word is a **compile-time constant in two separate images**. Reflashing one Pico and
not the other has always been possible, and the failure it produces is the worst kind this
system can generate: **silence, not an error.** A LoRa sync word is not a field in the
packet — it is encoded into the two sync symbols at the end of the preamble, each nibble
multiplied by 8, so `0xF3` is symbols 120 and 24 and `0xA5` is 80 and 40. A receiver
correlates against the symbols it is configured for, and on a mismatch the correlator never
locks: preamble detect never fires, the header is never decoded, the CRC is never evaluated.
No packet, no CRC failure, no counter moving. It is indistinguishable from a dead antenna, a
dead module, or a vehicle that was never switched on — discovered at a launch, from a
receiver that is working perfectly.

`check_doc_claims.py` now parses `config.radio_mode` out of the vehicle image and
`SYNC_WORD` out of the bridge image and holds them to each other:

```text
FAIL  vehicle and bridge agree: OFFICIAL sync word (0xA5)   [vehicle=official bridge=test]
```

and on a consistent tree it **names the configuration the working tree would fly**, so a
build log answers "which word am I on" without opening either file:

```text
  ok  vehicle and bridge agree: TEST sync word (0xF3)
```

The most expensive mistake available here is now a red build.

### Added — the launch-configuration procedure

[runbook.md](documentation/operations/runbook.md#launch-configuration--switching-the-sync-word):
why the sync word needs a procedure rather than a note, the nine-step switch to `0xA5` at
T-60, and the revert afterwards.

Three things it insists on that a checklist item would not have:

- **Prove the tree agrees before building anything.** The guard above runs without the SDK
  and costs nothing; every other confirmation needs images and hardware.
- **Both images from one tree, in one command**, so they cannot come from different
  revisions.
- **Step 8 is end to end.** Reading `OFFICIAL 0xA5` on the vehicle and `sync=0xA5` on the
  bridge proves only that each end believes what it was told. Nothing before packets
  actually arriving proves they can hear each other.

And the revert, because `0xA5` is the launch word and a vehicle left on it transmits into
every other team's launch — at 0.71 points per second.

**One thing worth stating plainly, because it is easy to get backwards:** `0xA5` does not
separate you from other teams. Every team uses it for their own launch. It separates launch
traffic from test traffic, and during your launch the sky is meant to hold exactly one
`0xA5` transmitter. What protects you from another team is the team identifier in every
packet and everyone else being powered off (TEL-026, still open).

### Changed

Requirements `Complete` 35 -> **36**. Claims 251 -> **254**.

`TEL-025` is **not** `Verified` and cannot be until `0xA5` has carried a real link. Only
`0xF3` ever has — bring-up row 5.13 is still half taken.

---

## [Unreleased] — 2026-09-09 (cycle 38)

### Fixed — the runbook was still describing the old telemetry rate

Found while answering a question about the sync words, which is the usual way: nobody reads
a runbook end to end, they read the one section they need.

Four figures in [runbook.md](documentation/operations/runbook.md) were computed from a rate
the vehicle no longer transmits at, and one of them had gone from stale to **actively
wrong**. The tunables table gave `telemetry_period_ms` a default of **1000** and described
1000 ms as *"both the rulebook ceiling and roughly what the SF7/125 kHz modem sustains"* —
after cycle 37 made 1000 ms a value `validate_config()` **refuses**. A wrong ceiling in the
document an operator reads before a launch is worse than a wrong figure anywhere else in
this repository.

| Was | Is |
|---|---|
| `telemetry_period_ms` default 1000, "the rulebook ceiling" | **700** (1.43 Hz), and 1000 ms is refused — the rulebook's 1 Hz is a floor |
| Link health: "1.18 Hz (850 ms) by default" | **1.43 Hz (700 ms)**, with 1.18 Hz named as the `transmit_gps` case it actually is |
| Stray transmission: "1.18 Hz … ninety seconds" | **1.43 Hz, 0.71 points/s, 35 seconds** to lose the whole 25-point telemetry section |
| Log capacity: "36 hours at 1.18 Hz" | **30 hours** at 1.43 Hz |

`ground-station/web/README.md` carried the same 1.18 Hz figure for the flight radio.

**The stray-transmission arithmetic was wrong in both directions.** The old text said ninety
seconds costs more than the 25-point section; at 1.18 Hz it actually costs fifty-three
points, so the warning understated itself by more than double. At the current rate the true
figure is **35 seconds for 25 points**, and the faster rate makes that window *shorter*, not
longer — which is the opposite of the intuition, and worth saying out loud on a page about
switch discipline.

A `transmit_gps` row was added to the same table. It was the one setting that changes the
rate, the packet size and the recovery story at once, and it was not listed.

### Changed — the arithmetic is checked, not just the figure

Quoting the right rate and computing the right consequences from it are different
properties, and only the first was guarded. `check_doc_claims.py` now derives the penalty
rate and the wipe-out time from the shipped telemetry period and holds the runbook to both,
so a period change fails the build until the prose that reasons about it is updated. It also
refuses the specific sentence that called 1000 ms a ceiling, and extends the quoted-rate
sweep to the runbook, `avionics/telemetry/README.md` and the web console's README.

Claims 244 -> **251**.

---

## [Unreleased] — 2026-09-08 (cycle 37)

Two changes to flight behaviour, and both are about a rule the vehicle was following in
spirit and not in mechanism: it landed when it had not descended, and it was fast enough
only because nobody had set it slower.

### Fixed — F-20: the descent gate

**A landing may no longer be declared until a real descent has been observed.**

`StateMachine` latches `descent_observed_` once the vertical rate has been below
`-landing_descent_rate_mps` (**2 m/s**) for `landing_descent_confirm_ms` (**1 s**) during
this `FLIGHT`, and refuses `FLIGHT → LANDED` until it is set. Three properties make it hold
rather than merely usually work:

- **The at-rest timer does not start without it.** Gating only the final transition would
  have let a hover accumulate three seconds of "at rest" and then fire the instant the gate
  happened to open.
- **The latch belongs to one `FLIGHT`.** It clears in `enter()`, so a descent seen in an
  earlier state cannot authorise a landing in this one.
- **`validate_config()` refuses thresholds that could overlap.** The descent rate must
  exceed the at-rest rate — otherwise a single sample could mean both "descending" and
  "stopped", which is the exact confusion the gate exists to remove — and the confirm
  window may not be zero, or one noisy barometer sample re-admits the hover.

**Why these numbers.** They sit in the wide gap between the two things they separate. The
mission descends at up to 5 m/s and reaches terminal rate in about half a second, so the
gate opens roughly a second into a 6.45 s descent with five seconds to spare; a failed
parachute falls far faster and opens it sooner. A hovering drone, a gentle lift and
barometric noise are all far below 2 m/s. They are **PROVISIONAL** like every other
detection threshold and want tuning against real drop data.

**The same reproduction, re-run against the fixed state machine:**

```text
t= 10362 ms  alt=  16.1 m  rate= +3.0 m/s   READY -> FLIGHT      <-- during the ascent, correct
t= 44022 ms  alt=   0.0 m  rate= +0.0 m/s   FLIGHT -> LANDED     <-- 3 s after touchdown
t= 49038 ms  alt=   0.0 m  rate= +0.0 m/s   LANDED -> RECOVERY   <-- window spent on the ground
```

It used to reach `LANDED` at 18.018 s and `RECOVERY` at 23.034 s — twelve seconds before
release.

**One consequence, stated rather than discovered later.** If the barometer fails during
descent the vertical rate never goes negative, the gate never opens, and the vehicle stays
in `FLIGHT` after touchdown. **Telemetry continues**, which is what REC-008 actually
requires; what is lost is the vehicle's own declaration that it landed. That is a strictly
better failure than declaring one in mid-air, and it is the same class of degradation as
every other barometer loss on this vehicle.

**An altitude gate would also have worked** — refusing a landing unless `altitude_agl_m` is
near the ground baseline — and was rejected because it leans on barometric altitude still
being trustworthy after several minutes of drift. The descent gate does not.

Five tests. `test_a_hovering_drone_is_not_a_landing` flies the whole drone profile at the
real thresholds; the other four cover the slow-lift case, the latch clearing on a state
change, a spike that never opens the gate, and the two configuration rules.

### Fixed — a test that flew 170 m up and back down without descending

Found by the gate rather than by anyone looking.
`test_the_packet_cadence_is_the_same_in_every_state` set the mock barometer's
`altitude_m` through a full ascent and descent and **never moved `pressure_pa`**. The
controller updates its vertical-rate estimate only when the pressure changes — a repeated
reading means no fresh conversion, not a stationary vehicle — so the vertical rate was
**exactly zero for the entire profile**. The test still reached `LANDED`, because at-rest
was satisfied the moment the acceleration came back to 1 g, so nothing noticed.

The profile now derives pressure from altitude. The test's own subject is packet cadence
and is unaffected; what changed is that the mission it flies is now a mission.

### Changed — the vehicle can no longer be built at 1 Hz

The rulebook requires **at least** one packet per second. Everything in the tree enforced
exactly that: `static_assert(kTelemetryPeriodMs <= 1000)`, and `validate_config()` accepting
`1..1000`. So a period of 1000 ms — 1.00 Hz — built and ran, and would have been compliant
on paper and wrong in two ways at once.

**It sits on the requirement.** What a ground station measures is the transmit period plus
whatever jitter the loop, the radio and the receiver add. At exactly 1000 ms every one of
those puts an interval past a second and the vehicle momentarily below a requirement that is
*checked*, not estimated. **And it scores nothing**: rate above 1 Hz is a scored line in the
2026 revision, and 1.00 Hz is the floor of it.

So the profile now carries a hard ceiling rather than a default:

| Constant | Value | Meaning |
|---|---:|---|
| `kRulebookMinRatePeriodMs` | 1000 ms | The rulebook's 1 Hz, as a period |
| `kTelemetryJitterMarginMs` | 50 ms | Against measured mission-clock jitter of under **4 ms** — bring-up row 5.4 |
| **`kMaxTelemetryPeriodMs`** | **950 ms** | The slowest period any build may carry, **1.053 Hz** |
| `kTelemetryPeriodMs` | 700 ms | What this vehicle ships, **1.43 Hz** |

Enforced four times over, because a flight build silently at 1 Hz is precisely the failure
this is for:

1. **Compile time.** `static_assert(kTelemetryPeriodMs <= kMaxTelemetryPeriodMs)`.
2. **Startup.** `validate_config()` refuses anything above the ceiling, naming the rate it
   was given. This is the layer that catches a period written by hand at a call site rather
   than taken from the profile.
3. **The claim checker.** `check_doc_claims.py` holds the three constants to each other, and
   holds the documented rate to what the profile computes.
4. **The receiving end.** `LinkHealth.rate_meets_rulebook` reports whether what *arrived*
   cleared 1 Hz, which is a different question once the link starts losing packets. It
   answers three ways — compliant, not compliant, or **not yet enough link to judge** —
   because "not measured" and "measured, and too slow" call for very different reactions and
   a boolean cannot say the first. The headless station prints
   `RATE BELOW THE RULEBOOK MINIMUM` with the likely cause; the dashboard carries a
   `>= 1 Hz rulebook` row.

**And one stale comment that would have cost somebody an afternoon.** `make_config()` in the
vehicle's `main.cpp` said *"1 Hz: the fastest the default SF7/125 kHz modem sustains with
margin"* directly above the line assigning **700 ms**. It is now correct, and the startup
summary prints the rate in Hz beside the period — `telemetry every 700 ms (1.43 Hz)` —
because the question this summary is most likely to be consulted about is "the link is
running at 1 Hz and I do not know why", and the usual answer is an image flashed before the
period changed. The runbook now has a section that says so and lists the four things to
check in order.

### Changed — counts

flight_tests 106 suites / 4013 assertions → **113 / 4107**. Python ground station
134 → **140**. Documented claims 235 → **244**, the new ones covering the rate ceiling, the
three constants agreeing with each other, and the quoted rate in four documents.

---

## [Unreleased] — 2026-09-08 (cycle 36)

A repository-wide currency pass, and the parts of the project that had never been analysed
at all. Five directories that had held nothing but a `.gitkeep` since day one now hold work,
and the analysis needed to fill two of them produced a finding in the flight software.

### Added — F-20: the vehicle declares a landing while hanging under the drone

**Found by writing down what the mission actually does, and reproduced against the
unmodified `StateMachine`.**

Landing detection asks two questions: is acceleration within 2.5 m/s² of 1 g, and is
vertical speed below 1 m/s? **A vehicle hovering under a drone answers yes to both.** Held
for `landing_confirm_ms` — three seconds — that is a landing.

Driven with a 3 m/s climb to 30 m and a 20-second hover before release:

```text
t= 10362 ms  alt=  16.1 m  rate= +3.0 m/s   READY -> FLIGHT
t= 18018 ms  alt=  30.0 m  rate= +0.0 m/s   FLIGHT -> LANDED     <-- still under the drone
t= 23034 ms  alt=  30.0 m  rate= +0.0 m/s   LANDED -> RECOVERY   <-- 12 s before release
```

`MODE-` then reads `RECOVERY` through the entire real descent, so anything that segments the
flight by mission state is wrong. The five-second post-impact window is spent in the air —
REC-008 is still physically satisfied, because `RECOVERY` transmits, but the vehicle's own
declaration that it landed is gone. And `RECOVERY` is terminal, so it never returns.

**`min_flight_ms` does not catch it.** That guard suppresses landing detection for the first
three seconds of `FLIGHT`, which is aimed at a boost-then-coast profile where the vehicle is
genuinely moving. On a drone lift, `FLIGHT` is entered at 15 m *during the ascent*, so those
three seconds are used up long before the hover. The exposure is also wider than hovering:
**any three-second interval with a vertical rate under 1 m/s** does it, including a gentle
lift at less than 1 m/s.

**Not fixed, deliberately.** Changing launch or landing detection is a mission-logic decision
for the team, not something to slip into a documentation pass. The candidate fix is recorded
where it can be argued with: a vehicle cannot land without descending first, so latch a flag
once a sustained descent has actually been observed and refuse `FLIGHT → LANDED` until it is
set. That is physical rather than threshold-tuned, and a hover cannot satisfy it.

Recorded as [F-20](documentation/testing/bring-up-record.md#findings) and written up in
[concept-of-operations.md](documentation/mission/concept-of-operations.md#the-hover-problem-f-20).

### Added — the descent model, and a parachute that has a size

`REC-005` said *"Parachute size and system - TBD"* against a **mandatory** 5 m/s descent
requirement. The rulebook fixes the release altitude, the mass and the descent cap; nothing
in the repository had turned those three numbers into a parachute.

[`simulations/descent.py`](simulations/descent.py) does, from `S = 2 m g / (ρ Cd v²)`, with
the descent time from the closed-form solution of `m dv/dt = mg − ½ρCdSv²` rather than
height ÷ rate — the vehicle starts at rest and accelerates into the terminal rate. Air
density comes from the gas law taking **pressure and temperature the vehicle itself
measures**, so a descent computed after a flight can use the air it was actually falling
through.

**The answers.** 73.7 cm at 500 g and ISA; **80.0 cm sized at 550 g on a 35 °C day**, which
is the case that has to still pass.

**Three findings came out of it, and two were not what anyone was looking for:**

- **Size at the top of the mass tolerance.** Area is linear in mass, so ±10 % of mass is
  ±10 % of area but only ~5 % of diameter. A canopy sized at 500 g and flown at 550 g
  **breaks the 5 m/s cap**. Sized at 550 g it is compliant across the whole band for 6 cm of
  extra cloth. `test_sizing_at_the_top_of_the_tolerance_covers_the_whole_band` asserts it.
- **The descent is 6.45 seconds — nine packets at 1.43 Hz.** That is the entire
  over-the-air descent dataset. It is an argument for the SD log being the primary record
  rather than a backup, and it means one lost packet is 11 % of the descent.
- **Drag coefficient is the dominant uncertainty and paper cannot close it.** The spread
  between canopy types is larger than every other term combined. A drop test with a known
  mass and a stopwatch closes it.

40 tests, pinned to three independent things: the ISA sea-level density, the free-fall limit
`t → √(2y/g)` *including its leading correction term*, and the terminal limit
`t → y/v + v ln2 / g`. Asserting "close to free fall" at one height only says the tolerance
was chosen generously; asserting the shape of the departure pins the physics.

### Added — the board does not fit, and the drawing says so

The rulebook envelope is 12 cm across. The vehicle board is 100 × 100 mm. **A 100 mm square
has a 141.4 mm diagonal, and a 120 mm circle inscribes only an 84.9 mm square** — so the
board does not fit flat, and nothing in the repository had noticed, because the two numbers
were decided a week apart in different documents.

Edge-on as a spine it fits with 20 mm to spare, costs nothing, and puts the antenna along the
vehicle axis where it wants to be. The alternative is cutting the board to 84.9 mm and
stacking two decks, which is a rebuild — the current floorplan uses the full 100 mm in both
axes.

[`tools/gen_envelope_drawing.py`](tools/gen_envelope_drawing.py) draws it to scale in
elevation and section, with the arithmetic written out beside it. Every coordinate is derived
from the dimensions at the top of the file, so changing a requirement moves the drawing rather
than making it wrong, and `check_doc_claims.py` fails the build if the committed SVG stops
matching its generator.

### Added — the netlist, generated from the firmware

The wiring existed in three places: `BoardPins`, a drawing, and whatever is soldered. The
first two can be held together mechanically, and now are.

[`electrical/schematics/vehicle-netlist.tsv`](electrical/schematics/vehicle-netlist.tsv) —
25 nets, 87 connections, 23 reference designators — is generated by
[`tools/gen_netlist.py`](tools/gen_netlist.py), and every GPIO row carries the name of the
`flight::BoardPins` constant it comes from. **The generator refuses to write a netlist whose
pin numbers disagree with the header**, and it also fails if the firmware assigns a GPIO that
no net mentions. Both conditions are checked again in `check_doc_claims.py`, so the committed
file cannot drift from the generator either.

There is no schematic file and there should not be: the vehicle is a point-to-point perfboard
build, and a schematic of one is correct on the day it is drawn. What is actually useful is a
table you can work a multimeter through, and that is what this is — including the ordering
that matters, which is probing each net against its *neighbours*, not just within itself. A
bridged pin reads as a perfectly good connection when you only check continuity.

### Added — five empty directories, filled

Each had held a one-line `.gitkeep` since the first commit.

| Directory | Now holds |
|---|---|
| `documentation/mission/` | [concept-of-operations.md](documentation/mission/concept-of-operations.md) — the mission from power-on to recovery, phase by phase, with the data budget and the failure behaviour of each phase. **Writing it found F-20** |
| `simulations/` | The descent model, its 40 tests, and a README carrying the answers and the rules anything added here has to follow |
| `mechanical/` | Envelope, the board-fit constraint, the canopy spec, a first mass budget with every figure's source named, and generated drawings |
| `electrical/` | The netlist, the authority hierarchy for electrical facts, and what a fabricated PCB would need |
| `avionics/` | Per-subsystem summaries — power, sensors, telemetry — against what was actually measured, quoting the documents that own each number rather than competing with them |

**The mass budget is a plan, not a measurement, and says so on its face.** Nothing has been
weighed; every component figure is a vendor number marked as unverified. What the table is
*for* is its shape: the avionics are of the order of 70 g against a 500 g budget, so the
structure has ~380 g to spend and mass is very unlikely to be the binding constraint. The
120 mm section is.

### Fixed — a repository that had got ahead of its own README

The board was built, the link had closed, and the front page still said **"The hardware has
not been touched."** A sweep against the source found eleven more of the same kind:

| Claim | Was | Is |
|---|---|---|
| Total automated checks | 4395 | **5054** |
| Python test badge | 37 | **207** |
| Hardware badge | "not yet verified" | board built, link closed |
| Firmware drivers | "the microSD driver and the flight image as a whole have not [run]" | every driver has, and running the flight image is what found F-16 and F-19 |
| Mechanical | "blocked on a rulebook contradiction" | unblocked since 2026-09-05, and now sized |
| Pin table | `MPU-9250`, and no microphone pins | `MPU-6500`, `GP15` and `GP27` added |
| "The three hardware blockers" | all three open | all three closed, with how each closed |
| BOM statuses | written before anything was built | per-item state, measured where measured |
| Telemetry rate (README, `requirements.md`, `telemetry-protocol.md`) | "1 Hz default" | **1.43 Hz** |
| `timeline.md` | status date 2026-09-04, phase 5 "not started — blocking", 36 suites / 537 assertions, 76 Python tests | 2026-09-08, phase 5 substantially complete, 106 suites / 4013 assertions, 207 Python tests |
| `software-architecture.md` | LoRa driver 101, microSD 581 | 129 and 613 |

**Two of the corrections were not staleness but errors.**

`requirements.md` had **two different requirements both numbered `GS-002`** — the original
dual-ground-station compatibility row, and one added when the 2026 revision named the official
radios. In a 127-row traceability table that is invisible and fatal. The newer one is now
`GS-006`; the original keeps its number, because that is the one this changelog records as
new. A uniqueness check now runs on every id.

And **the check that counts the requirements table was counting 116 of its 127 rows.** The
pattern matched three-letter ids, so every `GS-` and `SW-` row was silently skipped — and the
paragraph above the table, which said "35 of the 116", passed. A count that quietly excludes
eleven rows is worse than no count, because it reads as verified.

Ten requirement rows were re-stated against work that had overtaken them: `PWR-006` and
`PWR-007` asked for a regulator that turned out not to be needed; `MEC-001`, `MEC-002` and
`MEC-006` waited on a clarification the 2026 revision has given; `REC-001`, `REC-005` and
`REC-006` said `Blocked` for a parachute that now has a size; `TEL-005` said 1 Hz; and
`REC-008` gained F-20 as a caveat.

Bring-up rows **5.4** and **5.5** were marked superseded rather than corrected: both were
measured at the 1000 ms period, and the prediction column now reads 1.43 Hz and 46 % duty.
The measurements stand as evidence that the scheduler holds its configured rate; they are not
evidence about the rate now configured, and the difference is worth a re-take rather than an
edit.

### Changed — more of the documentation is now checked rather than trusted

`tools/check_doc_claims.py` grew from 220 claims to **235**, and the new ones are all of the
kind that would otherwise rot quietly:

- the total automated-check figure the README opens with;
- the simulations suite's own count, and the Python total that now includes it;
- the netlist against `BoardPins`, **and** the committed netlist against its generator;
- the committed envelope drawing against its generator;
- the canopy diameter, descent time and packet count where three documents quote them;
- every requirement id being unique, and the row count no longer skipping two prefixes.

`build_host.sh` runs the simulations suite. Its three `unittest` discovery runs are read
**positionally** by the claim checker, so the file now says so where the runs are defined and
`CONTRIBUTING.md` repeats it — a new discovery run inserted in the middle would silently
re-label two suites.

---

## [Unreleased] — 2026-09-07 (cycle 35)

### Fixed — the console demanded the demo's team of every real vehicle

**Found the first time the radio link ever closed.** Packets from `CAN-Team-25` arrived,
parsed, and were rejected one after another as `unexpected team identifier` — sitting on
screen, correct and completely readable, in red.

`const TEAM = "CAN-Team-01"` did two unrelated jobs. It was the demo generator's own
identity, which is right, and it was also the team the validator and parser demanded of
**every** source including live serial, which meant the console could only ever accept
telemetry from a vehicle named after its own demo.

The Python side it is documented as a port of does not do this: `expected_team` defaults to
`None` and [`validator.py`](ground-station/software/src/validator.py) checks it only when one
is supplied — `--team` is opt-in. The divergence was in the default rather than the logic,
which is why the shared protocol fixtures never saw it.

`DEMO_TEAM` now names the generator and `expectedTeam` defaults to null, so the console
accepts what arrives and displays whose it is. Pinning to one vehicle stays available for a
launch where other teams are transmitting, and the `CAN-Team-XX` placeholder rejection was
never conditional and is untouched.

**Two tests, because the behaviour and the wiring fail separately.** One asserts a foreign
team is accepted with no expected team set and still rejected when pinned; the other asserts
the console never hands `DEMO_TEAM` back to the validator — the behavioural test takes
whatever it is passed and cannot see that. Node 57 -> 59.

### Fixed — Web Serial reported every failure as a cancellation

One `catch` wrapped both `requestPort()` and `open()`, so a port another program was holding
— a serial monitor left open, and Windows COM ports are exclusive — was reported as
"Serial connection cancelled". An operator is then looking at the radio instead of at their
own desktop. The two failures are now reported apart, carrying the exception's `name` and
`message` verbatim.

The unavailable-API message named the browser: "Web Serial needs Chrome/Edge over HTTPS",
shown to somebody already running Chrome. Chrome exposes the API only to a secure,
non-opaque origin, so a console opened as a file has no API to call however capable the
browser is. It now says the page must be served.

### Fixed — the quick start told you to open the port unframed, and nothing failed

The bridge wraps every payload it emits as `$len,crc,payload` and always has. `main.py` builds
the framed transport only when `--framed` is given, so a station started without it reads the
port raw and rejects every line.

**The runbook required the flag; [quick-start.md](documentation/quick-start.md)'s end-to-end
telemetry test did not.** Both commands parse, so the existing guard — every documented
`python src/main.py` invocation is fed to the real argument parser — passed on both. The
difference only appears against hardware: the port opens, the reader runs, the packet count
stays at zero, and the operator is looking at a working radio and a dead-looking link.

Nobody had a bridge with a radio on it to type either command at, which is why two documents
were free to disagree. **Found while writing up ground-station assembly, before the link was
first closed rather than after.**

The quick start now carries `--framed` and says why it is not optional. A ninth test in
`test_documented_commands.py` requires every documented `live --port` command to carry it,
because parsing was never the property that mattered here: `--replay` stays the deliberate
exception, since the sample mission is a plain packet file. Python ground station 133 -> 134
tests, 166 -> 167 Python total.

### Fixed — [F-15] the log now carries the columns its header names, microphone included

`SdLogger::append()` took `(const TelemetryRecord&, const std::string& packet)` and wrote **only the packet**, discarding its first argument. That one signature caused all three symptoms: the microphone's data reached nothing, the rows were the semicolon-separated radio packet, and the header above them was a second hand-rolled copy of a comma-separated column list that described something else.

**The interface now takes one rendered line.** The controller passes `builder_.sd_line(*built, state_machine_.state(), faults_.total_occurrences())`, so rendering lives with `TelemetryBuilder` — where `sd_header()` and `sd_line()` are a matched pair the tests already hold to the same column count. `PicoSdLogger::initialize()` writes `TelemetryBuilder::sd_header()` rather than its own copy of the columns. The logger is now a line writer that decides nothing about what a row looks like, which is what let it drift in the first place.

**Bring-up rows 3.11 and 3.12 are takeable again**, and the additional sensor this vehicle chose over a magnetometer produces data that survives a flight.

### Fixed — the test that should have caught it asserted the opposite of its own name

`test_a_working_microphone_reaches_the_log()`, comment *“A working one reaches the log, and the log alone”*, contained:

```cpp
for (const std::string& packet : logger.packets) {
    CHECK(packet.find("250.0") == std::string::npos);   // absent from the LOG
}
```

It iterated the **logger** and required the level to be **absent** — asserting the bug, under a name that promised the opposite, and passing. **That is why F-15 survived every host run.** It now checks that the radio does not carry the level, that the log does, and that the log's first line is the builder's header. Three assertions more: flight_tests 3658 -> 3661, total 4430 -> 4433.

### Added — `tools/read_flight_log.py`, and what it found in the first real log

A recovered `FLIGHT.CSV` reads as good data followed by garbage, and nothing on its face says where one becomes the other — the file is pre-allocated and never truncated, so everything past the last record is whatever was in those blocks before.

**The file already knew.** `RawBlockLog` keeps two alternating header copies and rewrites them after every record, so `next free lba` is always current. The reader takes the valid header copy — checksum-verified, because a torn write has a *higher* sequence number and is exactly the one you must not trust — and stops where the data stops. It also splits by flight on `packet_number` restarting, so several runs in one file come out as `flight-1.csv`, `flight-2.csv`, without the firmware needing to know anything about it.

On the first real log: **67,108,864 bytes in, 17,829 out.** Header sequence 112, boot count 1, next free block 113, 111 records, one flight. It reads a file rather than a raw volume, so it needs no elevation and cannot touch the card.

**And extracting a real log made [F-15] worse than recorded.** The same root cause has a second consequence: `PicoSdLogger::initialize()` writes a hand-rolled **comma**-separated header — `team_id,packet,mission_time,…` — and then `append()` writes the **radio packet**, which is **semicolon**-separated tagged fields: `CAN-Team-25; P-001; Ti-00:00:00:000; A-38.8; …`. The header does not describe the rows, and the file does not open as a spreadsheet — which is the stated reason for writing a header at all. `sd_header()` and `sd_line()` are a matched pair that would produce both correctly, and neither is called.

### Changed — the startup summary names its faults instead of counting them

First boot after the [F-16] fix: **`SD card OK`**, every subsystem reporting, and `faults active 3` on a vehicle where nothing said FAILED. A count is not actionable. The summary now lists the names — `mag_unavailable watchdog_reboot calibration` tells an operator what to do; `3` tells them to go reading source.

Row 6.1a records the pass: the card initialises under the flight firmware, first boot after the fix. That row exists separately from 6.1 because the bring-up image could never have shown the failure — it initialises the radio first.

### Fixed — [F-16] the radio sat *selected* through the card's entire initialisation

The new startup summary earned its place in one boot: `SD card FAILED - NOTHING IS BEING LOGGED`. The card had passed twelve consecutive times under the bring-up image and failed every time under the flight firmware, which share the same drivers deliberately — so the difference had to be the order they run in, and it was.

**`BoardPins::lora_cs` (GP17) was configured only inside `PicoRadio::initialize()`**, and `Controller::initialize()` calls `logger_.initialize()` **before** `radio_.initialize()`. Through the whole of the SD initialisation GP17 was therefore an unconfigured pad — and an RP2040 pad resets with its pull-down enabled (`PADS_BANK0` reset value `0x56`, `PDE` set), which holds the line **LOW**, which on the RA-02 means **selected**. The radio drove MISO for the entire sequence and every response the card sent came back corrupted.

The bring-up image passed because it initialises the radio first — `NSS` was already high before the card was touched. Twelve passes and a string of failures, same hardware, same drivers, different order.

**The fix puts both chip selects with the bus rather than with the drivers.** `ensure_spi0()` now drives `lora_cs` and `sd_cs` high as part of bringing SPI0 up, so the order the drivers initialise in stops mattering — which is the property a shared bus needs and never had. The drivers keep their own setup; it is idempotent.

**This is separate from [F-12]**, which was intermittent and measured on the bring-up image. F-12 stays open: eleven clean runs bound it and do not close it.

Worth noting what found this. The bug has been in the flight firmware since the SD logger existed, through every host test and every documentation check, because **no host test can see a pad's reset state.** What exposed it was a vehicle that could finally say which subsystem had failed.

### Added — the flight firmware says what is fitted and what answered

The vehicle had no way to tell an operator anything. The launch build carries no debug output by design, so a flashed board that was working and a flashed board that was dead looked identical: an enumerated serial port with nothing on it. `cansat_pico_firmware` now prints a startup summary — IMU, barometer, GPS, radio, SD card and sound, each with OK/FAILED and a note, plus `state`, active fault count, armed and calibrated.

**Three design choices worth stating, because the launch build's silence is a property worth keeping.**

It prints **from inside the flight loop rather than before it**, so it delays nothing and the first telemetry packet still leaves on schedule. It waits ~1.2 s so two health refreshes have run and the numbers describe sensors that were actually read, not init return codes. And it **repeats every 3 s only while unarmed, then stops for good** — because the USB port re-enumerates for a second or two after a flash, so a single print at boot lands before anything is listening, which is exactly how a working vehicle looks dead. In flight the loop is silent again.

**It also makes a failed SD card visible for the first time.** `sd_unavailable` is warning-severity and only `critical_fault` reaches `MissionState::fault`, so a vehicle whose card never initialised would sit in `ready`, blink the normal 0.56 Hz, transmit happily and log nothing, with no local indication at all. The summary now says `SD card FAILED - NOTHING IS BEING LOGGED`.

It partly closes open item 39: the summary reports whether the microphone is fitted, silent or working. It does **not** put the microphone's values anywhere, which remains [F-15].

### Added — [F-15]: the microphone's data reaches neither the card nor the radio

Chasing an empty `FLIGHT.CSV` turned up something larger than the empty file.

**The sound data is read, validated, fault-managed, counted into `health_.sound_ok` — and then discarded.** The chain breaks in two places:

1. `TelemetryBuilder::build()` copies the sound fields into `Built`, and `sd_line()` renders them. **But `sd_line()` and `sd_header()` are called only from `flight_tests.cpp`** — nothing in the flight path calls either.
2. `PicoSdLogger::append()` is declared `(const cansat::TelemetryRecord&, const std::string& packet)` and **writes only `packet`**, discarding its first argument. It never sees `Built` at all.

The packet carries no sound either: its optional fields are GPS lat/lon/alt plus `MODE`, `FAULTS`, `CAL`, `ARM` and `YR`, and `telemetry.cpp` mentions sound nowhere.

**So the additional sensor this vehicle chose over a magnetometer produces nothing that survives the flight.** Bring-up rows 3.11 and 3.12 instruct the operator to read `sound_mv_pp` and `sound_gate_pct` “in the SD log”, and those values are not in the SD log and never were. The rows are untakeable as written, and the scoring credit rests on data nobody can recover.

The tests did not catch it because they exercise `sd_line()` directly — the renderer is correct and well covered. **What is missing is the call.** No firmware change is made here: the fix touches the `SdLogger::append()` interface, which is a design decision and not one to take while the board is being wired.

### Fixed — `CAN-Team-25` is the registered identifier, confirmed against the registration

Closed 2026-09-07 / KS. The number in `main.cpp` is correct.

**It was still worth asking, and the reason is the shape of the guard.** `is_valid_team_id()` rejects exactly one thing — the rulebook's `CAN-Team-XX` example — so any well-formed identifier passes. A wrong team number would have flown, on every packet, with the firmware, the tests and the documentation all silent. The guard tells you the field was *set*; it can never tell you it was set *correctly*. Only a person with the registration can.

The comment in `main.cpp` used to read *“SET THIS to the registered competition identifier”*, which invites the next reader to treat `25` as a stand-in and change it. It now records the confirmation and says plainly that it must not be “corrected” back. TEL-006 carries the same note, for the same reason.

### Added — the last sensor fitted is the only one the diagnostic cannot check

The sound module is wired. `bringup_main.cpp` reads the IMU, barometer, GPS, radio and card and reports each live — and **never reads `GP27` or `GP15`**. Grepped: not one reference. So every other sensor on this vehicle can be confirmed in a single run, and this one cannot.

Rows 3.11 and 3.12 have to go through the **flight firmware** instead, which logs `sound_mv_pp`, `sound_clipped` and `sound_gate_pct` into `FLIGHT.CSV` — flash, run, pull the card, read the columns. That works, but it means the last thing fitted is the hardest thing to verify, which is the wrong way round. Recorded as open item 39.

### Fixed — `team_id` is not the placeholder, and that is worse

Open item 32 said `team_id` was still `CAN-Team-XX`. It is not: `main.cpp:40` sets **`CAN-Team-25`**, and that value **passes** `is_valid_team_id()`.

The guard was written to catch an unset identifier — the rulebook's example placeholder — and it has already been satisfied by a number nobody has checked against a registration. **So the safety net will never fire again on this vehicle.** If `25` is not the registered identifier, every packet it ever sends carries the wrong one and nothing in the firmware, the tests or the documentation will say so.

### Changed — a sweep for everything the built board made false

With the vehicle working, several documents were still describing a project that had never applied power. Corrected across the repository:

**`wiring.md`'s headline warning said “no wire in this document has been built or measured”.** Every signal on that page is now solder, and every device it names has answered. It is a note rather than a warning, and it now names the three things that genuinely are unbuilt — the sound module, the divider, and the switch and Schottky — plus the one live hazard: **do not leave the battery connected while USB is plugged in until the Schottky exists.**

**`pico-gpio-map.md` was YELLOW**, pending “physical breakout verification and electrical compatibility”. Both are done and it is **GREEN**, with the two wired-but-inert pins named so nobody debugs them. The historical assessment is kept below it, because it records what the map rested on before hardware existed.

**`software-architecture.md` claimed no hardware bring-up had been performed.** Five gates say otherwise.

**The electrical architecture listed the switch and the power LED as “hardware not selected”** when both have been held since 2026-09-06, and still carried the AMS1117 as a TBD when it is neither used nor needed. The Schottky is added as the one part still to buy. Four items in *Required Hardware Before Prototype* are struck through with what resolved them.

**The development gates moved**: 2 and 3 from not-passed to partial, and 4 and 5 re-described against what the soldered board has actually shown.

**Two requirement rows said “hardware not built”** — true of the structure, misleading about the avionics, which are complete. They now distinguish the two.

No firmware was changed. Three code items are decided but deliberately not applied while the board is being wired: `battery_divider_ratio` stays `0.0f` until step 14 measures it, `team_id` stays the placeholder until a registration exists, and the `INT_ENABLE` and `GPIO23` changes are proposals in open items 37 and 38 rather than edits.

### Added — Gate 3 and Gate 4 pass, and the board is complete but for the sound module

Every device on the vehicle answered in one run. **`0x68` and `0x76` on the bus together** — which was the open half of row 3.1 and is now closed — with `0x0C` correctly absent from both scans. Chip ID `0x58`, `WHO_AM_I` `0x70`, barometer at **83.0 Hz**, and the GPS emitting all six sentences cleanly at 162 B/s with zero checksum errors while the radio and card share the board. Gates 5, 6 and 7 repeated in the same run.

**The honest number in there is 3.8.** Read cost was previously 0.282 ms — measured with the barometer alone, because the IMU was not wired. With both sensors it is **0.763 ms mean, 0.833 ms worst**, roughly treble. Still **2.5 % of the 33 ms period**, and now it describes the actual vehicle rather than half of it.

### Added — [F-13]: 0.41 dps of gyro-Z bias movement is the yaw budget

Z bias read **−0.4720 dps** on 2026-09-05 and **−0.0628 dps** on 2026-09-07. X and Y repeated to within 0.09 and 0.05.

Startup calibration removes the bias *present at boot*, so a bias stable across a flight costs nothing. What this shows is that Z bias **moves ~0.4 dps between sessions**, and the obvious driver is temperature — the die read **35.6 °C** on the bench, while flight is colder at altitude and warmer from self-heating. **0.4 dps held for a 180 s flight is 72° of yaw error**, on a vehicle that reports `YR-G` because [it has no magnetometer](documentation/testing/bring-up-record.md#findings) to catch it.

**The number worth having is not the bench bias but the drift after calibration across a temperature change**, and nothing has measured it. A cheap first look: calibrate, warm or cool the board, watch integrated yaw walk.

### Added — [F-14]: the temperature in telemetry is board temperature, not air

The barometer reported **32.96 °C** while the IMU die read **35.6 °C**, both well above a normal room. That is correct for the BMP280's own purpose — it needs die temperature to compensate pressure, and the altitude is unaffected. But the same figure is carried in telemetry where it reads as an air temperature and **is not one**: it is board temperature, biased warm by self-heating and by sitting beside a transmitting radio. Either label the field for what it is or stop treating it as ambient.

### Changed — three `104`s are deliberately omitted, and the blanket claim is retired

The IMU, the barometer and the GPS are fitted **without** a bypass capacitor. Decided at the bench on 2026-09-07 and recorded as **D-8**, because an omission nobody wrote down looks like an oversight to the next fault investigation — and this repository has already caught a wrong RA-02 pin description and a wrong card capacity that way.

These documents carried a blanket claim that the `104`s are *“not optional and not substitutable”*. **That is true where it was written from — the microSD's ~100 mA write spike and the RA-02's 1.5 to 87 mA key-up — and both remain fitted. It does not generalise.**

| Module | Draw | Why the omission holds |
|---|---:|---|
| MPU-6500 | ~4 mA | Carries **its own 10 µF tantalum** and 0402 passives, seen at C.3.5 |
| BMP280 | ~1 mA | Single-digit mA on a 400 kHz bus with ~250 ns rise times through 5 kΩ pull-ups |
| NEO-6M | ~45 mA tracking | **Has its own onboard regulator**, and its load is essentially **DC** — which a 100 nF does not address |

**The GPS is the largest of the three by current and still the strongest case for omission**, which is worth saying because current is the intuitive reason to fit one and it is the wrong reason here. Count goes from six `104`s to **three** — microSD, RA-02 and the sound board — across the procedure, the decoupling table, the receiving record, the purchase list and the wiring schedule. If any of the three ever misbehaves, this is the first thing to eliminate.

### Added — the GPS shares a board with a 50 mW transmitter, and nothing had said so

A **+17 dBm** 433 MHz PA sits on the same 100 mm board as a GPS receiver working near **−130 dBm** behind an active patch antenna. **No document in this repository mentioned it until now** — searched, and there was not one line.

433 MHz has no low-order harmonic on L1: 1575.42 MHz against a 3rd harmonic of 1299 and a 4th of 1732, so the obvious mechanism is off the table. What remains is **front-end overload** — the patch antenna's LNA driven toward compression by a strong nearby carrier, costing sensitivity with nothing in band. Symptoms would be fewer satellites, slower acquisition, or a fix that drops on key-up.

**It is cheap to measure and now has a row.** 4.5 already asked whether the fix survives transmission; new row **4.5a** asks the harder question — satellite count and C/N0 with the radio idle, then at flight duty from the same position without moving the vehicle. Row 4.2 already logged the format to use. Mitigations are physical and free: maximum antenna separation, patch skyward with the LoRa antenna perpendicular, pigtail routed away. **Decide the placement before the structure is designed around it** — that is the point where it stops being free. Recorded as an electrical risk as well.

### Fixed — the README described a vehicle two weeks out of date

The status line still called F-5 and F-6 open when [F-10](documentation/testing/bring-up-record.md#findings) closed both, and quoted breadboard rail figures. It now says what is true: the board is soldered, Gates 5, 6 and 7 pass on it, the rail held 3.28–3.29 V through 45 back-to-back transmits, and the open intermittent is F-12. The assembly procedure's parts table also still said the card was 32 GB.

### Changed — eleven consecutive passes bound [F-12], and do not close it

Ten power-cycled runs after the first pass, no failures. The record on the soldered board is now **3 failures in the first 4 runs, 0 in the next 11.**

**What ten clean runs are worth, stated honestly.** By the rule of three, zero failures in ten trials bounds the per-run rate at about **30 % with 95 % confidence** — a weak bound. A fault that bites once in ten runs survives this test 35 % of the time, so this rules out a frequent fault and nothing more.

**The clustering is the stronger evidence.** Three failures in the first four runs and none in the next eleven is very unlikely if the rate were constant, which favours *something changed* over *random intermittent*. That is genuinely good news. The open part is that **nobody can name what changed** — the connections were checked and the board was handled to place meter probes, and either could have moved a marginal contact into a good position.

**And a static bench run is the wrong test for a mechanical fault on a vehicle that is launched.** The supply was measured innocent during a failing run — 3.28 V at the module's own pins — so this is not [F-10](documentation/testing/bring-up-record.md#findings) repeating; what is left moves. Provocation and vibration are the tests that would settle it, and neither has been run.

Next on this row: flex the board and press the card mid-run to try to provoke it, reflow the module's six joints, and re-take 6.1 after any mechanical work.

### Added — Gates 6 and 7 pass on the soldered board, and the card failed eleven minutes earlier

Everything ran. The card initialised in **31 ms**, wrote 100/100 at a 6.128 ms mean, sustained **3598 writes in 10 s at 100 % duty**, and Gate 7 passed every row it has: `0x12` unchanged after the SD driver raised SPI to 4 MHz, 200 interleaved rounds with zero card-read failures and zero radio misreads, and 30 transmit-then-write rounds clean. **Gate 2's headline number came with it: the 3V3 rail held 3.28–3.29 V through 45 back-to-back transmits**, against 3.26–3.27 V on the breadboard, which closes row 5.4a.

**And the run before it failed completely.** Same board, same card, nothing changed, eleven minutes apart: ACMD41 timed out at 2007 ms with `R1 = 0xFF`. That pair is the finding, not the pass.

**[F-12] is open and must stay open.** `0xFF` is not a slow card — an initialising card answers `0x01` until it answers `0x00`, so the card had stopped responding altogether. **The supply is measured innocent:** the module's own `3V3` read **3.28 V** during the failing run, and the rail carried ~120 mA of radio through 45 transmits without moving. What is left is *mechanical* — the friction-fit card holder, or a joint that moves when the board is handled, and the board was being handled to place meter probes.

**This repository has been here twice already.** [F-5](documentation/testing/bring-up-record.md#findings) was the radio failing 5/5 then passing 45/45 minutes later. [F-6](documentation/testing/bring-up-record.md#findings) was every write failing on one power cycle and 100/100 on the next. Both looked fixed, both were a marginal supply connection, and both took five bench runs to name. One good run is not evidence of a fix — it is the same thing those two looked like.

### Changed — [F-11]'s ~30 ms write stall is no longer rare

The fourth session gave **28.031 ms worst against a 6.128 ms mean**. So the worst case reads 4.814, 29.756, 4.915 and 28.031 ms across four sessions on two different boards: **~30 ms has been seen twice, not once.** A stall that reproduces is a design input rather than an anomaly, and `RawBlockLog` writes twice per telemetry append — so a bad second can block the loop for ~56 ms, close to two whole sensor periods against a 33 ms `sensor_period_ms`.

### Changed — two rows that look finished and are not

**6.3b** has now run four times and **has never once produced the current figure it exists for.** Throughput is settled at 297–367 writes/s; the series-current measurement the Gate 2 load budget waits on has been skipped every session. Marked ⚠️.

**7.3** has passed three times, on both boards, and has never been run for the five minutes it asks for. Also ⚠️. Passing a shorter version of a test repeatedly is not the same as passing it.


### Added — Gate 5 passes on the soldered board, first attempt, and the airtimes did not move

Fifty-five transmits, no failures. `0x12` on the version register, **5/5 at 206 bytes, 5/5 at 255 bytes, 45 of 45 through a 15-second back-to-back burst** with an unbroken line of dots. Rows 5.1, 5.2, 5.3 and 5.4a re-taken and dated 2026-09-07.

**The breadboard could never have told us this.** [F-5](documentation/testing/bring-up-record.md#findings) is the same 206-byte transmit failing 5/5 and then succeeding 45/45 minutes later on an unchanged code path, and [F-10](documentation/testing/bring-up-record.md#findings) proved the cause was the RA-02's 3V3 jumper. Soldering that supply removed the fault instead of hiding it — the first run on the new board, cold, passed every row it was offered.

**And the airtimes did not move: 333.7 ms and 406.9 ms, identical to 0.1 ms**, across a complete change of wiring. The model is independent of the board, which is what makes it safe to build a telemetry rate on.

**Row 5.4a is marked ⚠️ rather than ✅, because the rail voltage was not read.** The row exists to be measured with a meter through the burst and nobody measured it; the 3.26–3.27 V it still quotes is the breadboard figure and is labelled as such. A transmit count is not the measurement that row asks for.

### Fixed — the card is 64 GB, and one row said 32

`receiving-inspection.md` recorded the microSD as 32 GB and reasoned from it that the card is “the top of the SDHC range”. The purchase list and bring-up rows 6.1 and 6.2 have said **HP mx310 64 GB** throughout, and the card in the slot is 64 GB — so it is **SDXC**. Nothing the driver does changes: 6.2 tests block addressing through `high_capacity()`, which is the thing that actually matters. It does explain the exFAT reformat already recorded in Gate 6.

### Changed — row 6.1 has regressed, and says so

Gate 6.1 read ✅ for the breadboard. **The soldered board fails it**, so the row now carries both results and a ❌ date rather than looking settled to the next reader.

**[F-12] ACMD41 times out at 2008 ms with `R1 = 0xFF`** — on the same card that initialised in **32 ms** two days earlier. The byte is the diagnosis and it is not slowness: a card that is initialising answers `0x01` until it answers `0x00`, so `0xFF` means MISO was undriven and the card had stopped answering. That is [F-8](documentation/testing/bring-up-record.md#findings)'s signature one command earlier — ACMD41 is the first moment the card powers its own controller and draws real current.

**Two candidates are already eliminated by evidence in the same run.** The HCS/SDXC trap is not it: `sd_card.cpp` sets `0x40000000` when CMD8 validates, and a 64 GB card denied HCS answers `0x01` forever rather than `0xFF`. The 3.3 V rail is not it: the radio pulled ~120 mA sustained through 45 consecutive transmits at 100 % in that same run. **The fault is therefore downstream of the 3.3 V distribution node** — the module's own supply pair, its joints, or the card contacts — and the next measurement is the module's own `3V3` against `GND` during the 2 s ACMD41 window.

---

## [Unreleased] — 2026-09-06 (cycle 34)

### Added — `AGND` is its own plane, and the ADC's real error is offset, not impedance

Asked whether the Pico's grounds are all internally connected to `AGND`. **On the board, no.** The datasheet: *"there is a separate analog ground plane running under these signals and terminating at this pin."* Pins 3, 8, 13, 18, 23, 28 and 38 are one digital-ground net; **pin 33 is a plane of its own**, meeting them inside the RP2040 rather than on the PCB. It has to be wired, at one point to the GND ring beside pin 38 — and the divider's lower leg and the microphone's return belong on it rather than on the nearest digital ground, which is the whole reason the plane is there.

Reading that page turned up two things that matter more than the capacitor withdrawn earlier in this cycle ever did.

**The ADC carries a documented ~30 mV offset.** §4.3: the ADC draws about 150 µA through the 200 Ω filter feeding `ADC_AVDD`, *"an inherent offset of about 150 µA × 200 Ω = ~30 mV"*, varying with sampling and temperature. Through a 2:1 divider that is **~60 mV referred to the pack** — comfortably larger than the source-impedance effect the withdrawn `104` was invented to fix. It needs no part: a systematic offset is exactly what the step 14 comparison against a metered pack absorbs into the calibration.

**SMPS ripple reaches the ADC supply, and the fix is free.** Driving `GPIO23` high forces the RT6150 into PWM mode and *"can greatly reduce the inherent ripple"*. `pico_hal.cpp` calls `adc_init()` and two `adc_gpio_init()`s and never touches it. Firmware only, no wiring, and it can be toggled around the reading to keep the light-load efficiency.

**`GP28` stays unwired, deliberately.** The datasheet's zero-reference trick — tie a second ADC channel to ground and subtract — would track the offset as it drifts. It is declined for now: inert without firmware that does not exist, redundant against a one-time calibration on a flight lasting minutes, and pins 33 and 34 are adjacent, so it is a one-joint retrofit whenever it is wanted.

The pattern is worth stating plainly, since it is the fourth time this cycle: **the answers were in a PDF this repository has held since day one.** Three corrections and two genuine improvements came out of finally opening it.


### Fixed — `GND` is on one side of the RA-02's supply pin, not both

Five documents said the RA-02's `3.3V` pin sits **"with `GND` either side of it"**. The transcribed silkscreen, printed directly above that sentence in two of them, says:

```text
J2   GND   GND   3.3V   RST   DIO0   DIO1   DIO2   DIO3
```

`GND` immediately before it, **`RST` immediately after**. And the warning attached to the claim gives the game away: it says a one-pin offset "puts 3.3 V onto `RST`" — which is only possible because `RST` is the neighbour.

It matters because of what the sentence is for. It is the pin-counting warning on the one module where a single-position slip drives the supply into a reset line, and a warning that miscounts its own pins is worse than none. Corrected in `wiring.md`, `hardware.md`, `receiving-inspection.md`, `bring-up-record.md` and the assembly procedure, and each now names both neighbours and both failure directions rather than one.

**The decoupling pair is unaffected and is now easier to place than the wrong version implied:** the 10 µF and the `104` go between `3.3V` (J2 position 3) and the `GND` at J2 position 2, which is the adjacent hole. A capacitor fitted one position the other way lands between `3.3V` and `RST`, which is not decoupling at all.


### Removed — the `GP26` capacitor is deferred, because the reasoning did not survive being asked twice

Asked whether the 100 nF at the divider tap was really needed. On inspection: **no**, and it is now out of the build.

**The load-bearing claim was that 16.5 kΩ is a high source impedance for the ADC's sample-and-hold.** It is not, on any plausible number. A SAR's sampling capacitor runs to a few picofarads; at 5 pF the time constant is about 82 ns and settling to half an LSB at 12 bits wants roughly nine of them — under a microsecond, inside even a maximum-rate sample window. This vehicle reads the battery **once per second**. The RP2040's real figure is still unread, and no plausible value makes this a problem.

**The second claim argued against itself.** The signal-routing section rejects wider pin spacing on the grounds that coupling is not a serious threat on this board. It cannot then be the justification for a capacitor.

**And fitting it was never free.** The part bridges pins 31 and 33 with pin 32 between them, and a bridge onto `GP27` produces a microphone channel that works and lies rather than one that fails visibly. That is a real hazard accepted to guard against a speculative one — the wrong side of the trade.

**D-7 replaces it with a trigger instead of a part.** Step 14 already meters both divider legs and step 15 puts the pack on; compare what `GP26` reports against the pack at its terminals. Agrees within a few tens of millivolts → it was never needed. Reads low, drifts, or jumps → fit it then, on evidence. One `104` stays in the drawer for that; the build consumes **six** again.

This is the third correction in one cycle to a claim this repository made without reading its source — the ADC impedance, the RP2040 datasheet's absence, and USB back-powering. The pattern is the same each time and the rule already exists in the receiving-inspection procedure: a principle is not a specification, and a plausible number is not a measured one.


### Fixed — USB back-powers the battery, and this page said it did not

Asked whether `VBUS` is used anywhere. It is not, and answering that caught a genuine error in this repository's own guidance.

The assembly procedure said USB and battery could both be connected because the Pico's `D1` Schottky *"means the 5 V USB rail simply wins and the pack idles."* **That is wrong.** `D1` prevents `VSYS` from back-feeding `VBUS`. It does nothing to stop `VBUS` pushing current *into* a battery wired to `VSYS`. With USB plugged in, `VSYS` sits near 4.7 V and a 3.9 V cell hangs directly off it: uncontrolled charging, no CC/CV, no termination, no current limit beyond what the port allows.

**The Pico datasheet is in `datasheets/` and says so plainly.** §4.5: a second source is added *"via another Schottky diode … with the diodes preventing either supply from back-powering the other"*, and the caution on the same page is about unprotected lithium cells catching fire. [D.4](documentation/hardware/receiving-inspection.md#d4--battery) already records that this pack shows no protection board. The document that would have prevented this was in the repository the whole time.

**D-6 adds the diode**: a 1 A Schottky between the switch and pin 39, band toward the Pico, fitted *after* the divider tap so the battery reading still sees the pack rather than the pack minus 0.3 V. `VSYS` takes 1.8–5.5 V, so a 3.0 V cell still arrives at 2.7 V. The datasheet's P-FET alternative is the better circuit and is noted, but the diode is sufficient and is a part that can be bought for a rupee.

**Until it is fitted the rule is procedural and fragile:** battery switch OFF whenever a USB cable is in. Gates 1 to 14 all run on USB, so that is most of the build.

And the answer to the original question, recorded where it belongs: **`VBUS`, pin 40, is unused and stays unwired.** It is live 5 V whenever a cable is in, nothing on this vehicle tolerates 5 V, and it sits immediately beside `VSYS`. The datasheet permits shorting 40 to 39 *when USB is the only supply*; on this vehicle that bridge wires 5 V straight to the LiPo with no diode at all. It is now on the do-not list and in the step 3 bridge check.


### Added — signal routing, and a capacitor that is not a decoupling capacitor

A build instinct worth answering properly: hold the signal wires three rows apart to avoid interference. It is right about the risk and wrong about the remedy.

**Wire-to-wire coupling falls logarithmically with separation.** Mutual capacitance between parallel round wires goes as `1 / ln(d/r)`, so on 22 AWG the step from one row of separation to three moves `ln(d/r)` from about 2.1 to about 3.2 — a third less coupling for three times the routing area. The longer runs that area forces then put the coupling back.

**And only two lines here are victims at all.** `GP26` carries a divider whose source impedance is 33 k in parallel with 33 k — **16.5 k** — and `GP27` carries an `AO` output the board may or may not buffer. Everything else is either the aggressor (SPI0, at 4 MHz the only fast thing on the vehicle) or immune to it: I2C rises in ~250 ns through its 5 k pull-ups, UART0 is 9600 baud, and `CS`, `RST`, `DIO0` and the LEDs are static. The radio's 87 mA key-up is a genuine disturbance and spacing does nothing about it, because it travels the supply — that is what the star feeds and the local capacitors are for.

What works instead, in order: a ground return run alongside the sensitive line back to `AGND`; crossing at right angles rather than running parallel, since coupling scales with parallel length; shortening the run; lowering the victim's impedance; and only then separation.

**Which adds one part.** A `104` from the `GP26` tap to the `AGND` tie, doing two jobs that are not decoupling: it gives the RP2040's sample-and-hold a local charge reservoir so a 16.5 k source does not have to settle the sampling capacitor alone, and it shorts any coupled glitch before the conversion sees it. 1.65 ms against a battery read once a second. **The equivalent must not be fitted on `AO`** — that line carries the audio envelope the driver reduces to a peak-to-peak span, and filtering it removes the measurement. `104` count six -> seven, against about twenty in hand.

**The real weakness is not spacing, it is that the board has no ground plane.** Copper on one face, isolated pads, no rails ([C.9.3, C.9.4](documentation/hardware/receiving-inspection.md#c9--prototype-pcb-quantity-2)) — so every return current finds its way home through a hand-built ring, and return paths outrank separation for that reason. On a board with a plane the instinct would have been sound.

### Changed — the RA-02 fit question is closed by insertion, not by a caliper

The one measurement that could have stopped the build was the RA-02's row-to-row spacing: not a whole multiple of 2.54 mm and the module cannot be pressed flat. **It seats in the grid.** That is the answer, and it is better evidence than a reading — a module whose two rows drop into holes has whole-pitch spacing by construction.


### Changed — the board has a floorplan, and the microSD is soldered after all

The modules were laid out on the perfboard and photographed. The Pico sits with its **USB facing the left edge**, and that single choice decides the rest of the board: with USB left, the Pico's top pin row carries power at its left end, both ADC channels in the middle and all of SPI at its right end, while the bottom row carries I2C, the GPS and the status LED. **There is no 3.3 V pin on the bottom row at all** — pin 36 is the only supply output the part has.

The first arrangement put the RA-02 and the microSD in the bottom band, on the reasonable-sounding ground that it kept them near the Pico's 3.3 V pin. It does the reverse, and it also puts both devices as far as they can get from the SPI pins they need. Counted out, the two layouts differ by **nineteen wires crossing the Pico against two**. The bands are swapped: SPI devices and the power zone up top, sensors and the GPS below.

**The microSD reader is now soldered down rather than jumpered**, which reverses part of the 2026-09-05 mounting decision. That decision jumpered anything held singly, and it was right about the IMU, the barometer and the GPS. It is wrong about the card reader for one reason: **the microSD's supply jumper is the only wire on this project that has actually failed.** [F-10](documentation/testing/bring-up-record.md#findings) was five bench runs of every write failing while every read passed. Soldering the module deletes that wire instead of asking a capacitor to stand in for it, and the reader is four resistors and two capacitors with no active part on it — the thing swapped in service is the card, not the board.

**Which creates a requirement, and it is a mechanical one:** the card slot must reach an opening in the airframe. The flight log is recovered off that card, and a reader soldered inside a sealed body with its slot facing inward loses it.

The jumper board-end question is closed with it: **male header strips, 24 pins across four footprints.**

### Fixed — the claim checker did not check cross-document anchors, and let one through

Renaming a heading in `wiring.md` broke a link to it from `assembly-procedure.md`, and `check_doc_claims.py` reported 217/217 anyway. Its own comment says a renamed heading breaks navigation silently and that this is what the check exists to prevent — but it only validated anchors *within* a document, and for a link to another file it checked that the file existed and stopped there.

It now resolves the fragment against the target document's own headings whenever the target is Markdown. The broken link is fixed. Claim count 216 -> 218 across the two cycles: one for the new document, one for the new check.


### Added — the assembly procedure, and the five contradictions it had to settle

[assembly-procedure.md](documentation/hardware/assembly-procedure.md) is the order this vehicle gets soldered in: thirty-four pre-solder checks, a floorplan, physical Pico pin numbers rather than GPIO numbers, and sixteen steps each ending in a gate from the bring-up record. It exists because the wiring diagram says what connects to what and never said what to do first — and on this vehicle the order is the design. Both faults this project has actually had were supply wires, and both were findable only because one subsystem was powered at a time.

**Writing it down forced five decisions the documentation had left in conflict.**

**The microSD bulk capacitor is 2 × 100 µF, not 470 µF.** Two documents still ask for a 470 µF part that never arrived. The requirement was never 470 µF — it is the ~50 µF the [droop arithmetic](documentation/design/electrical-architecture.md#how-much-bulk-is-actually-needed) computes, and the 470 µF was described in the same paragraph as generous rather than calculated. The delivered pair gives 200 µF, four times the requirement.

**No sockets.** The purchase list still asks for female headers to socket every module including the Pico. That line predates the [mounting decision of 2026-09-05](documentation/design/wiring.md#module-mounting), which decided the opposite and gave its reasons. The later decision wins and the purchase is cancelled.

**The power LED comes off the 3.3 V bus.** The wiring document worried that only a branch on the switched battery node could light immediately at power-on. It does not need one: the Pico's `3V3(OUT)` rises from the RT6150 the moment `VSYS` is energised, with no firmware involved, so the LED lights when the switch closes and dies when it opens. The switch itself goes in the battery positive lead, ahead of everything.

**Jumper board-ends are male header strips — except the microSD's supply pair, which is soldered wire.** The open item offered two options and stated an intent; the intent is taken, with the one exception [F-10](documentation/testing/bring-up-record.md#findings) demands.

**The microSD and the RA-02 are starred off the Pico's supply pins; everything else taps a ring.** Those two are the modules that pulse — a write spike and a PA key-up — and they are the two that failed on a jumper. The other five modules and both LEDs draw single-digit milliamps between them.

**And one measurement the board is now built to take.** A current figure has never been recorded for any device on this vehicle, only rail voltage, because a series connection would not hold on a breadboard. The procedure puts a **two-pin test link in the 3.3 V feed** so a meter can sit in it, and solders the link shut once the readings are taken — a removable shunt is a mechanical failure point on a vehicle that lands hard.

**Three checks failed and all three closed the same day.** Nothing in this repository recorded solid-core hookup wire, silicone battery wire, or how much male header strip survived 2026-09-04 — all three are held, confirmed 2026-09-06 / KS, and the rows now say so. That is the whole point of a check that can fail: the parts were always in the drawer, and the only thing missing was the sentence saying so. **Section 1 of the purchase list is now empty**, and the female headers in it are cancelled rather than bought — they were asking to socket modules the [mounting decision](documentation/design/wiring.md#module-mounting) had already decided to solder down.


### Added — the capacitors, read off their sleeves

The capacitors were in the batch photograph and nowhere else: no crop, no read-out, no quantities. Cropped and transcribed now as D.7. **`10µF 50V`, `100µF 50V` and `100µF 25V`**, read off the sleeves. Mixing the two 100 µF voltage ratings in the microSD pair is fine: both are far above a 3.3 V rail, and an electrolytic has no DC-bias derating, so each contributes its full marking.

**A correction, in the same cycle that caused it.** An earlier draft of this entry read a shortfall of `104` ceramics off the photograph. There is none: one of each part was laid out to be identified, and the delivered quantities are as ordered. This repository's own receiving procedure already says a photograph of one board is never evidence that two arrived, and a tray of passives is the same rule. Six `104`s are still what the build consumes — microSD, RA-02, MPU-6500, BMP280, NEO-6M and the sound board — and they remain the one part here with no substitute.

**The disc print is worn illegible**, so their value is on record from the purchase and not from the part. An orange disc looks identical at 100 pF and at 0.1 µF, and the documentation says which of those two kinds of fact it is holding.

The resistor values were metered on 2026-09-06 and all three agree with the bands, so D.6 now records a measurement rather than a reading.

### Added — the second-batch photographs, and the divider they settle

Five photographs filed: the LM393 module cropped out of the batch shot, the batch shot itself, and one close-up per resistor group. The close-ups exist because the batch shot could not resolve the colour bands, and a delivery photograph that settles quantities but not values is only half of Part C.

**The resistors read as 100 kΩ ±5 % carbon film, 33 kΩ ±1 % metal film and 1 kΩ ±5 %.** Two are four-band and were photographed with the gold tolerance band on the left, so they read right to left; the five-band 33 kΩ is the one that matters to get the right way round, because backwards it reads as a plausible 1.2 kΩ rather than as an invalid code.

**The `GP26` battery divider is now designed rather than reserved:** two 33 kΩ 1 % parts in series across the pack, tapped at the midpoint. Ratio 2.0, 2.10 V at the pin on a full 4.20 V cell against a 3.3 V limit, 64 µA continuous. The 100 kΩ 5 % parts would give the same ratio for a quarter of the current and were not chosen: the pack voltage is the one telemetry quantity nothing else can cross-check, so a fifth of the tolerance is worth 43 µA.

**No 330 Ω arrived and none is needed.** 1 kΩ drives both LEDs at about 1.3 mA instead of 4, which is visible and cheaper in current — it hands back most of what the microphone took last cycle, even with the power LED now counted as its own continuous load. The realistic case is 246 mA of peripherals, 281 with the RP2040, against the 300 mA the pin is rated for.

The module photograph also resolves what the last one could not: `LM393` `49M` `BWQ64` on the comparator, `PWR-LED` and `DO-LED` silkscreen, `+`/`−` marks on the capsule pads, and SMD resistors marked `102` and `201`.

**Values read from bands are what a part claims, not what it measures.** D.6 says so and asks for one from each group to be metered before anything is fitted.

---

## [Unreleased] — 2026-09-05 (cycle 33)

### Added — an acoustic sensor, logged and deliberately not transmitted

An analogue microphone on `GP27` / ADC1, as the project's second additional sensor. It
closes sensor integration at its 25-point cap — which is a cap, so a third sensor scores
nothing more there, and planning for ten points from two sensors would have been planning
for five.

**The rulebook was checked before the packet was touched, and the packet was not touched.**
`TEL-022` says optional sensor data *may* be appended after mandatory data. May, not must.
Every question this sensor exists to answer is a post-flight one — when the canopy
inflated, when it landed, how the level tracked descent rate — and all of them are asked
of the log, against the altitude and acceleration columns beside them. So the level goes to
the SD card as `sound_mv_pp` and `sound_clipped`, and never into a packet. The nine bytes it
would have cost are airtime the mandatory fields need more. `telemetry-protocol.md` records
the decision, its bandwidth assessment and what it gives up: if the vehicle is never
recovered, the acoustic record is gone.

**A window, not a sample.** The module's output is AC-coupled and rests near half its
supply, so a single reading per tick would say where in the waveform the tick landed, not
how loud anything was. Each tick takes a burst of 256 conversions — about 0.5 ms, 1.6 % of
one 33 ms period — and reduces it to a peak-to-peak envelope.

**It reports millivolts and not decibels, and that is not laziness.** dB SPL needs a
calibrated reference source and a record of where the module's gain trimpot was left. This
project has neither, so the value is a relative level comparable across one flight at one
gain setting and with nothing else. That limit is in the header comment, the requirements
row, the log column name and a new bring-up row that asks for the trimpot position to be
written down.

**Nothing mandatory can depend on it.** The controller holds it as a pointer that may be
null: a vehicle built without a microphone behaves exactly as before, a failed
`initialize()` does not fail the self-test, and a failed read raises a warning that cannot
move the mission state. Three of the eight new tests exist only to keep that true.

An unfitted microphone writes empty columns rather than `0.0`, because a working one reports
zero for silence and a column that cannot tell those apart is worse than no column.

### Changed — the load budget, because 5 mA of 23 is not a rounding error

The microphone is a continuous load: it draws whether or not anything is listening. Roughly
5 mA, almost all of it the LM393 and its indicator LEDs rather than the capsule. The
all-at-once case moves from 302 to 307 mA against a 300 mA pin, and the realistic case from
18 mA of margin to 13.


### Closed — both bench intermittents were the same fault, on two wires

The radio that failed eight transmits and then sent seventy-five (F-5), and the microSD
whose writes failed and then did not (F-6), were one fault on two modules: **the 3V3 jumper
feeding each of them.** Shortening both fixed both, and produced the first run in which
every row of Gates 5, 6 and 7 passed together.

Neither module has a regulator and the microSD reader has two capacitors, so the supply
jumper is the whole delivery path. Idle draw crosses a marginal one; a transmitting PA and
a programming flash die do not. That is why reads always passed and writes never did, and
why the radio's timing was identical to 0.1 ms on the packets that did get out.

On the soldered board this is a short track to each module's supply pin and a **470 µF bulk
capacitor** across the microSD's own 3V3 and GND — a requirement now, not a precaution.

Still open: worst-case block-write latency reached 29.8 ms in one of three sessions against
a 33 ms sensor period, and one telemetry append is two writes. Loop jitter has never been
measured with the logger running.


### Fixed — a diagnostic that read "nobody answered" as five simultaneous card faults

A bench run reported `R1 = 0xFF, R2 = 0xFF` on a failed write, and the decoder walked the
bits and announced a write-protect violation, a locked card, a controller fault, an ECC
failure and an out-of-range address — all at once, about a card that was working.

`0xFF` is not a status byte. Every valid R1 has bit 7 clear, and `0xFF` is MISO idling high
with nothing driving it: the card had gone silent, not refused. Five unrelated
catastrophes appearing together is the tell. The init path has refused to decode `0xFF`
since it was written; the write path did not inherit that guard.

- A `0xFF` in either byte is now reported as no response, and no bit is decoded behind it.
- Fields the write never reached are no longer printed. The same run showed
  `data token = 0xE5` for a write refused at `CMD24`, which never sends a data token — a
  leftover from an earlier attempt presented as this one's measurement. The capture is
  cleared per attempt, and a host test holds it against a fake card that goes silent.
- On a total write failure row 6.3 now re-initialises the card and says whether it comes
  back. A damaged card stays damaged; one that recovers was dropping out, and `CMD24` is
  the first moment in the run that it draws programming current.


### Added — three registers, so a failed transmit says which fault it was

The same 206-byte transmit failed 5 of 5 in bring-up row 5.2 and then succeeded 45 of 45 in
row 5.4a, minutes later on the same power cycle: same code, same length, same antenna. At
the failures `IRQ_FLAGS` read `0x00`, so `TxDone` was never set and the packets did not
complete rather than merely going unreported.

`IRQ_FLAGS` answers one question — did the radio finish and DIO0 fail to say so — and it
cannot answer this one. It cannot tell a chip that is trying and failing from a chip that
is no longer the chip we configured, and those want opposite investigations.

- `Sx1278` now captures **`RegOpMode` and `RegVersion` alongside `RegIrqFlags`**, all three
  read at the moment of the failure and before anything is cleared or reset.
- The diagnostic decodes them in the order that matters. A version that is not `0x12` means
  SPI was broken at that instant, so nothing about the RF side is implicated — and on this
  vehicle the microSD shares that bus and corrupts the radio rather than itself. `OP_MODE`
  bit 7 clear means the modem left LoRa mode, which only a reset does: the module lost power
  and took its configuration with it. `0x83` means it accepted the transmit and never
  finished it, which is the PLL, the PA or the rail behind them.
- Three host tests pin the three cases, including a `FakeRadio` that browns out mid-transmit.

### Fixed — a test reporting failures that belonged to the test before it

The radio's failure counters are cumulative, and the diagnostic printed them raw. Row 5.3
therefore announced "8 timeout(s)" when three of its five had failed and the other five
belonged to row 5.2 — a test looking twice as bad as it was, in the one place a number is
read to decide whether something is worth chasing. Each test now snapshots the counters and
reports its own.


### Fixed — the post-flight command ate the flight log it was meant to read

The runbook's step 2 told an operator to replay `logs/raw_packets.tsv`, and left `--output`
at its default of `logs`. The station therefore opened the file it was reading for append.
The reader kept finding the lines the writer had just written, so the replay never ended,
the flight's only forensic record filled with re-logged copies of itself, and the disk
filled behind it. Run as written it reached **69 MB in under two minutes**, with the
parsed CSV at 99 MB, before it was killed.

This is the one documented command that is run over a real flight's only record. It is the
one that must not damage it.

- **The station refuses.** A replay whose input resolves to a file the logger would write
  under the chosen `--output` is rejected before anything is opened, with a message naming
  the conflict and the flag that resolves it. `main.py` prints that refusal rather than a
  traceback and exits `2` — it is read by somebody standing over a recovered vehicle.
- **The runbook passes `--output analysis`**, and says why. A test holds the document to
  it, so the refusal never becomes the only thing standing between an operator and the
  log.
- **Three tests**: the refusal leaves the log byte-for-byte unchanged; the corrected
  command still returns `30 / 30 / 0` and does not touch the input; and the runbook's own
  line carries a separate `--output`.

Refusing was the whole fix. Quietly writing somewhere else would lose the operator's chosen
output location, and quietly snapshotting the input would still append rubbish to the
flight log.

### Fixed — questions the bench closed that four documents went on asking

The barometer variant was settled on 2026-09-05 by the only method that settles it: chip ID
`0xD0` returned `0x58`, and a BME280 answers `0x60`. The finding was written up and closed.
Four documents never heard.

- `hardware.md` listed the variant as **Unresolved**, status `TBD - blocking`, and its
  explanatory paragraph still instructed the reader to resolve it by die photograph or
  chip-ID read.
- `pre-procurement-design-status.md` listed it as **Blocking**.
- `wiring.md` carried it as an unticked box under *Open items before any wiring is built*.
- `photos/README.md` still asked for a macro photograph of the die.

The same bus scan settled both I2C strap directions — the IMU replies at `0x68` and the
barometer at `0x76`, so `AD0` and `SDO` are both low — and two documents still asked for
them, one specifying a meter for a question the scan had already answered without one.
Battery polarity and the 1S charger were closed too, and still listed as open; the charger
item was genuinely half-open, and now says which half — the label's absent charge current
and cutoff voltage.

Somebody working that checklist would have redone work already done, or treated a cleared
blocker as one.

**The gate is keyed on the measurement, not on the date.** `check_doc_claims.py` now holds
a short table of settled values against the phrases that must not survive them: if the
bring-up record carries `0x58`, no document may still ask which variant this is. Each entry
disarms itself if the measurement is ever withdrawn — no evidence in the record, no
obligation on the documents.

### Fixed — three measurements that never reached the table they were taken for

The bench settled the IMU's `WHO_AM_I`, its I2C address and the barometer's on 2026-09-05,
and the bring-up record says so plainly. The hardware reference tables — the rows an
electrical design is drawn from — still read `Value on the delivered board - TBD`,
`AD0 wiring and available address - TBD` and `SDO wiring - TBD`, nine lines below the
paragraph recording `0x70`.

- **`WHO_AM_I` on the delivered board is `0x70`**, an MPU-6500, recorded against `F-1`.
- **The IMU answers at `0x68`**, so `AD0` is strapped low; `0x0C` never answers, before or
  after `BYPASS_EN`.
- **The barometer answers at `0x76`**, so `SDO` is strapped low. The `SDO strap direction`
  row no longer waits on a continuity check the bus scan already answered.

`check_doc_claims.py` now holds each of these rows to the bring-up record: if the record
carries a measured value, the matching reference row must carry it too and must not say
`TBD`. Scoped to the row rather than the file, for the reason written at the top of that
script — the document that records the value three rows away is not the document that
records it here.

The same tables asserted a magnetometer configuration in the *Breakout-board status*
column, the one column that is specifically about the delivered board: 16-bit continuous
mode 2 at 100 Hz, per-axis ASA read and applied. There is no AK8963 on this board. Those
rows now say which part they describe.

### Fixed — the electrical architecture had never heard the register read

`electrical-architecture.md` still listed `MPU-9250` in the bill of materials with
`exact board documentation TBD`, and its Gate 2 blocker asked the team to identify board
variants that a register read had already identified. Someone planning the wiring from that
document would have provisioned for an AK8963 at `0x0C` that does not exist. The bill of
materials, the block diagram and the gate text now name the delivered part, both measured
addresses, and the finding that recorded them.

### Added — the predicate that gates every transmission is now held to its own struct

`TelemetryValidity::mandatory_valid()` is what stands between a record and the antenna:
`format_packet()` refuses any record it rejects. It is a nine-term AND over a struct of
nine flags, and nothing exercised it or held the two lists together. A tenth flag added to
the header and forgotten in the function would have let a reading the vehicle never took
travel as though it had — silently, and only for the field that was added.

- **Behavioural half**, in `flight_tests`: a default-constructed validity is invalid,
  all nine set is valid, and dropping any one of the nine both fails the predicate and
  stops the record becoming a packet.
- **Structural half**, in `check_doc_claims.py`: the struct's flag count, the number of
  terms in the AND, and the flags the test names must all agree.

Adding a flag to the header without adding it to the function now fails the checker;
removing a term from the function fails the test.

### Fixed — the console named a mission state the vehicle never claimed

`MODE` is a diagnostic tag, not part of the mandatory telemetry block, so a well-formed
packet can arrive without one. The parser already kept that absence as `null`, the way it
does for every other optional tag. The state chip then wrote `latest.mode || "READY"`, so a
packet with no `MODE` tag put **READY** on the screen and lit the READY step of the phase
ladder to agree with it — for a vehicle that might have been in `FLIGHT` or `FAULT`.

- **An unreported state is now reported as unreported.** The decision moved into the
  portable core as `missionStateView()`, which returns the chip text, the screen-reader
  announcement and the phase to light, all `null` when the vehicle did not say.
- **The default cannot be written a second time.** A structural test requires the renderer
  to go through `missionStateView` and forbids `.mode ||` anywhere below the portable-core
  marker.
- **Every state the firmware names now has a colour.** A new test reads the state strings
  out of `health.cpp` and requires an entry in the console's `STATE_COLOR` for each. It
  found one missing on its first run: `UNKNOWN`, the value returned for a `MissionState`
  outside the enum, which would have been painted in the muted `INIT` styling. It has its
  own warning colour now.

The same audit dropped `r.mode || ""` from the mission sample builder. An empty string is
harmless there — it matches no state name — but every consumer compares with `===`, so
`null` carries through unchanged and the habit does not survive anywhere.

### Fixed — the review document had gone stale about its own result

The continuous-review audit trail recorded `159 / 159` documented claims long after the
answer was 199. `check_doc_claims.py` already counts its own checks and holds the test plan
and the README to that number; it now holds the review's audit trail to it too, so the
document that exists to keep other documents honest cannot drift about itself.

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

> [!NOTE]
> **Everything from here down to [Gate 6 on the diagnostic](#added--gate-6-on-the-diagnostic-with-the-destructive-half-behind-a-prompt)
> is one continuous review pass**, run in a single session on 2026-09-05 and summarised as a
> whole in [documentation/audit/2026-09-05-continuous-review.md](documentation/audit/2026-09-05-continuous-review.md).
> Seventeen findings, `F-43` to `F-59`. Three are defects in flight or ground software that
> would have produced wrong data; the rest are documents that had stopped describing the
> software, or guarantees nothing was holding. Read the audit for the shape of it; these
> entries are the detail.

### Changed — the checks that asked the wrong question

The nine-axis rule failed because it asked *does this file mention the delivered part*
rather than *does this paragraph contradict itself*. Two other checks written in this pass
had the same shape, so they were re-examined rather than left to fail the same way later.

**The wiring gate.** It checked that `| GP4 |` appeared in `wiring.md` and that `i2c_sda`
appeared in `wiring.md` — separately. Swapping two signal names between rows leaves every
pin and every name still present in the file, and the check passed it. Confirmed by doing
exactly that:

```text
FAIL  wiring.md lists GP4 for `i2c_sda`   [| GP4 | I2C SDA | I2C0 | Bidirectional | ...]
FAIL  wiring.md lists GP5 for `i2c_scl`   [| GP5 | I2C SCL | I2C0 | Output ...]
```

Both now have to be on the same row. That is the one way a pin table can be wrong and
still look right, and it was the way the check could not see.

**The log column mapping.** It checked that each column name appeared anywhere in the
runbook — and `packet_number` appears in its prose too. Scoped to table rows, it
immediately reported seven ground-side columns that were explained in a paragraph rather
than mapped in a table. The check was right to insist: somebody joining two files wants a
complete column list, not a paragraph to read around. Those seven are a table now, each
saying which side it belongs to and what it records.

The reasoning is written at the top of `check_doc_claims.py`, because the next person to
add a check will reach for the file-wide form first — it is shorter, and it is wrong
whenever the claim is really about two things belonging together.

### Fixed — the hardware documents still said the part was nine-axis, in the sections nobody reread

The rule added earlier in this pass required any document mentioning the magnetometer to
name the delivered `MPU-6500` or link the finding. Both hardware documents passed it — and
both still asserted, somewhere else on the page, that this vehicle has nine axes:

- `hardware.md`: *"The die marking makes this a genuine nine-axis part, so the AK8963 and
  the absolute-yaw path apply."* The sentence right after it said `WHO_AM_I` at bring-up
  would be the final word — and it was, and it said `0x70`.
- `hardware/README.md`: *"The IMU is a nine-axis MPU-9250."*

**The gate was too weak, and the weakness is instructive.** Acknowledging a fact once in a
file says nothing about the paragraph three screens away that contradicts it. The rule is
now per paragraph: nine axes may be discussed anywhere, and the same paragraph must say
this is not one.

That immediately caught two more, both describing code rather than the vehicle and neither
distinguishable as such by a reader: the link budget's note that the nine-axis upgrade cost
six bytes per packet, and the flight core's file listing describing `orientation.hpp`. Both
now say what runs here. The link-budget one is worth the sentence it gained — those six
bytes are still spent on a tag that always reads `YR-G`, and that is still worth paying,
because the tag says the yaw is relative and a receiver would otherwise assume.

Four places, one fact, found only because the first version of the rule was not strict
enough to catch its own hardest cases.

### Fixed — the bring-up diagnostic asked an operator to find a chip that is not there

`cansat_bringup_firmware` scans the I2C bus twice, before and after the IMU is
initialised, because the magnetometer sits behind a pass-through bridge and only answers
once `BYPASS_EN` is set. The second scan was headed:

```text
-- I2C0 bus scan (after IMU init - AK8963 at 0x0C SHOULD now appear) --
```

It will not appear. The delivered IMU is a six-axis MPU-6500, and this program **says so
itself, two lines earlier**: *"SIX axes - there is no magnetometer in this package at
all."* Then it asks the operator to look for one. Somebody with a board in front of them
and a bus scan showing two devices where the screen wanted three goes hunting a wiring
fault that does not exist.

The heading is now chosen by what the part actually reported: `SHOULD now appear` on a
nine-axis package, and *"six-axis part, so 0x0C will NOT appear, which is correct"*
otherwise.

The bus scan's address labels had the same problem in miniature. `0x68` was labelled
`MPU-9250` — but the scan runs **before** `WHO_AM_I` is read, so it cannot know which part
it is looking at, and on this vehicle it was naming one the identity report contradicts a
moment later. The labels now say what answered rather than what the board was sold as.

Bring-up row 3.1 predicted *"3 after (0x0C appears)"*. It now states both cases and which
one this vehicle is.

### Fixed — the vehicle refused to fly and would not say which setting was wrong

`validate_config()` has **thirty** ways to refuse a configuration, and each one writes an
exact reason into a `why` string: which setting, what the rule is, and often the rulebook
clause behind it. The controller received that string and replaced it with four words:

```cpp
if (!validate_config(config_, why)) {
    last_error_ = "configuration invalid";
```

The vehicle then latches a critical fault and enters `FAULT`, where it will never produce a
compliant packet. An operator gets a dead vehicle and thirty candidates.

This is the sharpest instance of the shape this pass keeps finding, because the diagnosis
was not merely uncollected — it was **computed, handed over, and discarded**, on the one
failure that stops a mission before it starts.

The reason is kept now, exposed as `Controller::config_error()`, and **printed over USB at
startup**:

```text
CONFIG REFUSED: post_impact_transmission_ms must be >= 5000 (rulebook post-impact minimum)
```

That is the console an operator already has open during bring-up, and it beats an accessor
nobody calls. The runbook says to connect a serial monitor before changing anything when the
vehicle sits in `FAULT`, because the first line it prints is the answer.

`test_a_refused_configuration_says_which_setting_was_wrong` holds all three halves: two
different rules produce two different messages rather than one generic phrase, and an
accepted configuration leaves the reason empty rather than stale.

### Fixed — two faults the vehicle can raise and no document explained

Every packet carries `FAULTS-n`. The architecture document has a table of what each code
means, and comparing that table against the `FaultCode` enum found two codes in the
firmware and in no document at all:

- **`mag_unavailable`** — no magnetometer answered at initialisation, or one that was
  answering stopped. Not an edge case here: it is **standing on this vehicle**, whose IMU
  has no magnetometer, so an operator counting faults on the pad is looking at it right now
  and had nowhere to look it up.
- **`yaw_reference_disagreement`** — the magnetic heading and the GPS course over ground
  disagree while the vehicle is moving fast enough for the comparison to mean anything.
  Unreachable on a six-axis part, which never claims a magnetic heading to disagree with.

Both are in the table now, with the conditions that raise them and what they do to the
mission, and the enum is held to the table: all **19** codes must appear.

Checked in the same pass and deliberately left alone: 60 of the 78 fields in `config.hpp`
are named in neither the runbook nor the architecture document. That is not a gap. The
runbook's table says *"other tunables worth reviewing before a flight"* and lists six —
the ones an operator should think about. Documenting the DLPF register values and the
Mahony gains beside them would bury the six that matter under seventy that do not.

### Added — the pin table somebody actually wires from is checked too

The GPIO assignment is gated against `config.hpp` in `wiring.md`. The quick start carries a
**second copy** of that table — the summary, on the page a builder has open with the board
in front of them — and nothing held it to anything. A pin changed in firmware would have
left the authoritative table right and the practical one wrong.

All fifteen pins are now checked in both places. Changing one entry in the quick start's
table fails the build and names the pin.

Doing that needed the checker's document reads hoisted into one block at the top: checks
get added in whatever order the reasoning arrives in, and one that could not see a document
because it sat above the line that opened it is a poor reason to reorder a file.

The [continuous review](documentation/audit/2026-09-05-continuous-review.md) is updated with
the second wave, `F-60` to `F-69`, and with what that wave says about the first: three of
its findings are the same shape as two from the first wave — a value computed and shown to
nobody — and **one of those was introduced by this pass**. Fixing four instances by hand did
not prevent a fifth. A script comparing the whole snapshot against the whole display did,
and found eight more while it was there.

### Fixed — the quick start's install step installs nothing, and never said what to install

Step 3 ends *"Python packages: `pip install -r ground-station/software/requirements.txt`"*.
That file is **entirely commented out** — the core is standard library only, and the file
exists to say so. Running the command installs nothing, which is the correct outcome and an
alarming one if you were not told to expect it.

Meanwhile the two things that *do* need a package were named nowhere in this document.
A reader following it reaches step 19, the end-to-end telemetry test, and stops with
`pyserial is required for SerialTransport` — a good error message for a situation the
document should not have walked them into.

Step 3 now says plainly that step 4 needs nothing installed and why the file looks empty,
and names `pyserial` and `matplotlib` with what each one buys. Step 19 installs `pyserial`
in its own code block and says what happens without it.

Two more numbers in the same document:

- *"all **ten** Pico translation units"* — it is eleven, and has been since the bring-up
  diagnostic was added. The existing check missed it because this page spells the number
  out in words; it now checks both spellings.
- *"**31 targets**, 5 CTest tests"* — the CMake files declare thirteen targets, and a host
  configure without the Pico SDK builds fewer still. The figure could not be reproduced
  here, so rather than replace one unverifiable number with another it is **gone**: the
  sentence now states the 5 CTest tests, which is read off the `add_test` calls, and
  explains that the target count depends on whether the SDK is present.

Fewer numbers, each of them checked, beats a confident one nobody can reproduce.

Also gated: that `requirements.txt` still has nothing uncommented in it. The claim "no
packages needed" is true only while that stays true, and a dependency added there would
turn a correct sentence into an `ImportError` for the next reader.

### Fixed — post-flight step 5 told an operator to compare two files that share one column name

*"Compare the transmitted stream against the onboard SD log: differences are radio loss,
not sensor loss."* Good instruction. The two files are written by different programs on
different sides of the link, and of their 18 and 23 columns, **`packet_number` is the only
name they share**. Altitude is `altitude_m` on one side and `altitude` on the other; the
mission clock is integer milliseconds in one and `HH:MM:SS:MS` in the other. Nothing said
how to line them up.

The runbook now carries the mapping, joined on `packet_number`, and says which columns exist
on one side only **and why that is the point of the exercise rather than a defect**:
`receipt_time`, `valid`, `error`, `seq_missing` and `seq_note` are what the ground station
observed about the link; `state` and `fault_total` are what the vehicle knew about itself
when it transmitted.

The names are left as they are. The onboard columns carry their units because the flight
computer writes them, and a number without a unit is how a wrong number gets believed.

Both column sets are now read off the source and held to that table, and the check earned
itself on its first run by naming two columns the table had missed — `yaw_reference` and
`heading`, both ground-side derivations from the `YR-` tag. Writing them up then tripped the
magnetometer rule from earlier in this pass, which required the new sentence to name the
delivered part. Two gates catching a third change is the arrangement working.

The C++ record was compared for names in the same pass and deliberately left alone. It is
the transmitter's struct, it names its units, and unlike the Python and JavaScript pair it
is nobody's hand-port — the reasoning is recorded beside the test rather than acted on.

### Fixed — the two parsers disagreed about what a field is called

The same comparison, pointed at the web console. Its display turned out to reference every
field its parser produces — but comparing the two *parsers* found something the shared
fixtures cannot see.

`test-data/protocol-fixtures.tsv` holds all three parsers to one definition of a valid
packet. It says nothing about what the parsed record is **called** afterwards, and a name is
exactly what a hand-port gets wrong. Python exposed `fault_count`; JavaScript exposed
`faults`. Both worked. The asymmetry is in how they fail: reading the wrong name in Python
raises `AttributeError` on the spot, and in JavaScript yields `undefined`, which this
console renders as an em dash — a fault count of *none reported* where the vehicle said
three.

The console now uses `fault_count`, and a test runs its parser under Node and compares the
record's keys with Python's. Three names exist on the Python side by design — `has_gps` and
`yaw_is_magnetic`, which the console derives where it needs them, and `optional`, which the
CSV writer uses — and they are listed rather than glossed over, with a second test that
fails if a listed name stops describing reality.

Renaming it back fails the check with `Lists differ: ['fault_count'] != []`.

### Fixed — the live dashboard was missing nine of the values the ground station knows

Having found the shape three times by hand, the fourth was found by comparing the whole
snapshot against the display: run a packet through the pipeline, list every value the
snapshot carries, and check the dashboard names each one.

Nine did not appear, and two of them matter on a pad:

| Missing | Why it matters |
|---|---|
| **`calibrated`**, **`armed`** | The two flags an operator stands there waiting for. The vehicle sends them in every packet as `CAL` and `ARM`, and the web console has always shown them. The display the runbook opens for a live flight did not |
| `received`, `accepted`, `rejected` | The totals every other validation row is a fraction of. The replay command prints them; the live view showed the faults without the denominator |
| `timestamp_regressions` | A clock going backwards, with no reboot to explain it |
| `gps_rejected` | An implausible fix is kept and flagged rather than dropped, so the count is the only place it is visible at all |
| `status_frames` | A bridge that is alive but hearing nothing still sends these. Without the count, that case looks exactly like a dead serial link |
| `team_id` | Redundant when `--team` is set and the only identity on screen when it is not |

All nine are shown now, and **the comparison is a test**. Every value in the snapshot must
be named by the dashboard, with a short explicit list of the few rendered another way —
`logging.last_error` is folded into the logging line beside its error count. A second test
fails if anything on that list stops being rendered at all, so an exception cannot outlive
its reason.

That is the difference between fixing four instances of a defect and closing the way it
gets in.

### Fixed — the frame decoder's counters reached nobody, including the one added earlier today

`FrameDecoder` counts frames decoded, CRC errors, resyncs and overflows. The decoder lives
inside the transport, and the application above it only ever sees whole `Frame` objects —
so nothing read any of them. CRC errors reach an operator by another route entirely, as a
frame kind the link-health module counts. Resyncs and overflows reached nobody at all.

That includes the `overflows` counter **added earlier in this same pass**, when the three
decoders were unified. The argument then was that an oversized length deserves its own
diagnosis rather than being folded into a generic resync. That argument is only worth
anything if someone can see it, and this is the third time this pass has found the shape it
is an instance of. Finding one's own instance of it is the least comfortable and most
useful kind.

`Transport.framing_stats()` exposes them, the snapshot carries them, and the Tk dashboard
has a **Serial framing** panel. The runbook gains two monitoring rows, because the two
counters answer different questions:

| Indicator | What it means |
|---|---|
| **Resyncs** | A partial header thrown away and the decoder started again. A few are noise on the serial line; a rising count means the link is corrupting bytes, not packets |
| **Oversized length** | A length field larger than any frame this link can carry. Not noise: a badly corrupted header, or a sender configured for frames this receiver will never accept |

An unframed transport reports **nothing** rather than zeroes. No decoder ran, and a zero
would read as "nothing went wrong" — a different statement from "this was never
measured". The dashboard shows `--`.

### Fixed — four more requirements that had been done for weeks

The same sweep, applied to the blocks outside the sensor rows.

| | Said | Says |
|---|---|---|
| `PWR-004` — telemetry begins automatically at power-on | `Not Started`, *"Flight firmware startup - TBD"* | **Complete** — the controller transmits from `READY` with no arming step, trigger or operator action, and a test initialises and polls with none of them |
| `GEN-008` — the vehicle includes a communication system | `Not Started`, *"Paired RA-02 telemetry system - TBD"* | **Complete** — one shared link profile across both ends, a driver suite, and a radio that has answered `0x12` and transmitted on a bench |
| `MIS-003` — ground-floor telemetry reads about zero | `Not Started` | **Implemented; awaiting a lift test** |
| `MIS-004` — telemetry reflects the altitude change while lifting | `Not Started` | **Implemented; awaiting a lift test** |

The last two get the middle answer rather than `Complete`, because both are about what
telemetry *shows during a lift*. The software produces the behaviour — altitude is relative
to a pad reference the calibrator takes on the ground, and the end-to-end test watches it
change through a scripted ascent — but nobody has yet watched it happen on a rope. A
requirement about an observation is not met by the code that would make the observation
true.

Every remaining `Not Started`, `Blocked` and `TBD` row was checked in the same pass and is
correct: they need the power design, the mechanical build, a launch, an organiser's answer,
or a decision only the team can make. **34 of the 112** requirement rows now stand `Complete`.

### Fixed — the requirements checklist held two statuses for the same fact

`TEL-015` says roll must be transmitted, and was marked `Complete` with a named test.
`SEN-008` says roll must be generated and transmitted, and was marked `Not Started` with an
implementation column reading *"MPU-9250 orientation processing - TBD"*. One page, one
fact, two statuses.

`SEN-001` through `SEN-009` were the last block still describing the project as it was
before the flight software existed: altitude, pressure, temperature, gyroscope, all three
acceleration axes, roll and pitch, every one of them *"integration - TBD"* or
*"pin/interface - TBD"*. The pins have been fixed and gated for weeks, the drivers are
written and tested, and four of those sensors have since read on the bench.

All nine now carry the implementation that exists and the tests that cover it, and the
count in the opening paragraph moves from 23 to **32**. `SEN-011` gets the honest middle
answer rather than either extreme: the GPS is implemented, parsed, transmitted and
confirmed at 9600 baud on hardware, and **has never had a fix** — implemented, not
demonstrated.

Nothing moved to `Verified`. That word is reserved for hardware evidence, and no
requirement has been demonstrated end to end on a powered vehicle.

Two checks keep the page honest about itself: the count in that opening sentence is now
read from the table beneath it, and **no row may claim `Complete` without naming its
evidence** — which is the rule the document states about itself two paragraphs earlier, and
was until now enforced by nobody. Emptying one evidence cell fails the build and names the
requirement.

### Fixed — three mandatory requirements cited a test that does not exist

`requirements.md` names a test for each requirement. That citation is the only thing
connecting a compliance claim to something that runs, and a requirement pointing at nothing
reads exactly like a requirement that is covered.

`TEL-018`, `TEL-019` and `TEL-020` — the three mandatory acceleration fields — cited
`test_mpu_scaling`. That test became `test_imu_scaling` when the MPU-6050 was replaced by a
nine-axis part, and the citations stayed behind. Three of the rulebook's mandatory
telemetry fields have had dangling evidence ever since.

Nothing else was dangling: 249 test names are defined across the suites, and every other one
named in a document exists. Both facts are now checked, along with a second class found in
the same sweep — **every repository path named in prose**, not only the ones written as
links. Paths appear in prose far more often, and a moved file leaves them behind silently.

The changelog and the audits are excluded from the path check on purpose: `ui.py` and
`radio.py` were deleted in cycle 2, and the entries recording that deletion have to be able
to name them. A record of a removal is not a stale reference.

### Added — every link and heading anchor in the documentation is checked

Thirty-four documents, a table of contents in most of them, and cross-references between
them throughout. Nothing checked that any of it still pointed anywhere. A renamed heading
or a moved file breaks navigation in the quietest way there is: the document still reads
correctly, and the link simply goes nowhere.

Two aggregate checks now cover all of it — every relative link resolves to a file that
exists, and every same-document anchor resolves to a heading in that document. Aggregate
rather than one per link, so the claim count stays a measure of what is checked rather than
of how much prose there is.

Both were clean on the first run, which is the answer this pass wanted. Breaking one
deliberately — dropping a hyphen from the quick start's `#10-the-power-problem--read-before-wiring`,
the double hyphen an em dash produces — fails the check and names the file and line.

This one started as a mistake. A grouping note added to the changelog above needed a link
to a heading with an em dash in it; the throwaway script written to confirm the anchor said
it was wrong, because it collapsed runs of whitespace where GitHub does not. The repository's
own conventions settled it — the anchor was right — and the episode is the argument for the
check: if a hand-written verification of one link can be wrong, the several hundred nobody
has verified deserve better than trust.

### Changed — a cut LoRa payload is counted, like a cut log record

`poll_receive()` trims a payload longer than the caller's buffer and returns the trimmed
length, with nothing to say it did. That is the third instance of one shape this cycle --
after the log record cut to fit a block and the number too wide for its buffer -- and it is
the one with the most misleading consequence: **the bridge frames and CRCs a truncated
payload exactly like a whole one**, so it reaches the ground station as a valid frame
carrying a malformed packet. The diagnosis points at the vehicle, and the fault is in the
receive path.

Not reachable with the buffers this project uses -- the bridge passes 256 bytes and a LoRa
payload cannot exceed 255. `truncated_receives()` exists so it cannot become silent if one
of those buffers ever shrinks, and a test holds both halves: a payload that overruns is cut
and counted, one that fits is neither.

Two other things in the radio driver were read and deliberately left alone. The RSSI offset
selection is correct and already carries the reasoning that fixed it. And `poll_receive()`
clears the IRQ flags before reading the FIFO, where Semtech's own examples read first --
a race whose window is microseconds against a 1 Hz packet rate, in the one path on this
radio that **hardware has never exercised**. Reordering a register sequence on a part that
passed its bench gate with the current order, to close a race that cannot be observed at
this rate, is a change to make with the radio in front of you. It is written down here
instead.

### Fixed — a latitude marked `W` was read as a southern one

`parse_coordinate()` handles both NMEA coordinate fields, and accepted any of `N`, `S`, `E`
or `W` on either of them. The caller always knows which axis it is asking about, and the
function was not told.

So a latitude field carrying `E` parsed as a northern latitude, and one carrying `W` parsed
as a **southern** one — the fix placed on the wrong side of the equator, from a sentence
that was already saying something had gone wrong with it. The same in reverse for a
longitude marked `N` or `S`.

The checksum catches most corruption, and this is what is left when it does not: a single
character wrong in a field the checksum was computed over before the fault, or a receiver
emitting a malformed sentence. The parser's own comment already said a hemisphere it cannot
account for must be rejected rather than assumed; it just did not know enough to tell.

Each axis is now given the two characters it accepts. `test_a_hemisphere_from_the_wrong_axis_is_rejected`
covers all four wrong-axis combinations, in `GGA` and in `RMC`, and checks that a rejected
sentence drops the fix rather than leaving the previous one standing. Against the old
parser it fails on every one of them.

Read in the same pass and found correct, with no change needed: the BMP280 compensation
against the Bosch 64-bit reference path, the IMU and AK8963 scaling constants, the
barometric altitude formula, the startup calibrator's stationary test and its separation of
"not shaking" from "not turning", the magnetometer bounding-box calibration and its
divide-by-zero guard, and the mission state machine's launch, landing and arming logic.

### Fixed — the estimator could claim a magnetic heading it never computed

Reading `orientation.cpp` line by line found one path where the vehicle's own rule -- never
claim an absolute heading it has not earned -- did not hold.

The magnetometer is admitted on **field strength**: anything between the earth-field bounds
is usable. Recovering a *heading* from it needs something more, a horizontal component, and
a field can have the first without the second -- pointing straight down at a magnetic pole,
or through a local vertical disturbance, which on a launch field means a motor, a vehicle or
a steel structure.

`seed()` handled that correctly and silently: no heading recoverable, so yaw stays at zero.
Its caller then set magnetometer confidence to the threshold **because the field passed the
magnitude gate**, and with a calibrated magnetometer the next packet went out declaring
`YR-M` — an absolute magnetic heading of 0 deg that nothing had measured, on a vehicle that
was in fact pointing anywhere at all.

`seed()` now reports whether it used the field, and confidence is granted only then. A
degenerate field leaves the estimator on gyro-integrated yaw, declared `YR-G`, which is what
it actually has.

`test_a_field_with_no_heading_in_it_is_not_seeded_as_one` covers all three cases -- the
vertical field claiming nothing, a good field earning the claim over the required run of
corrections, and an uncalibrated magnetometer never earning it however good the field. It
was confirmed to fail against the previous code before the fix was kept.

Worth noting what this does *not* change today: the delivered IMU has no magnetometer at
all, so this vehicle reports `YR-G` regardless. The defect was in the code that runs on a
nine-axis part -- the part the project would buy if the organisers rule that an absolute
yaw is required.

The rest of the estimator was read against its own comments and found correct: the
quaternion integration, the body-to-level rotation, the magnetometer reference construction
that keeps a disturbance out of roll and pitch, the bias integrator's sign and clamp, and
the compass-bearing conversion. No other change was needed.

### Added — the documented commands are a test suite now, and CI stops racing itself

Two of this cycle's defects were found by typing documented commands in exactly the form
the documents give them, and neither would have been caught by testing the code those
commands reach. `test_documented_commands.py` makes that a suite rather than a habit:

- the sample mission the documents name **exists**;
- the documented replay command **accepts every packet** in it;
- a raw log this station wrote **replays through the documented command** as the mission it
  recorded -- the runbook's post-flight step, end to end, from writing the log to reading it
  back;
- a file that cannot be read **fails loudly**, because the failure that hid the raw-log
  defect was a silent zero;
- every `python src/main.py` invocation in the README, the quick start, the runbook and the
  ground-station README **parses against the real argument parser**, so a document cannot
  offer a flag the program does not have.

Reverting the raw-log handling fails the third of those with `AssertionError: 0 != 30` --
the silent zero, caught. That was run before the suite was kept.

**CI also stops racing itself.** A second push to a branch made the first run's answer stale
before it arrived, and both ran to completion anyway. Superseded runs are cancelled now,
except on `main`, where every commit's result is part of the record and worth having even
after the next one lands.

Two more claims are gated: the number of CI jobs the test plan describes, and that **no job
carries `continue-on-error`**. The workflow's own header says every gate blocks; that is now
checked rather than asserted.

### Fixed — three frame decoders disagreed about what a broken frame is

The parser, the validator and the raw-log escaping each now read one fixture file. The
framing did not, and checking the last unshared pair found the divergence it was hiding.

All three decoders agreed on what a *valid* frame is. On an **oversized length field** —
what a corrupted length byte looks like — the C++ reference counted an `overflow` and
returned a distinct status, while Python and JavaScript counted a generic `resync` and had
no overflow counter at all. The same corruption, three ways, depending on which end an
operator was reading.

Resolved toward the richer diagnosis rather than the common one: a length no frame on this
link can have means a corrupted header or a sender configured for frames this receiver will
never accept, and that is worth naming. Both other decoders gained the counter.

**`test-data/framing-cases.tsv` now pins all of it** — 14 byte streams with the exact
events and counters each decoder must produce, and every recovery path the format has: a
torn header, a truncated length, a `$` arriving inside a CRC field, an oversized length, a
frame arriving immediately after a CRC error. Each suite also replays every stream **split
at every single byte**, because a serial port splits wherever it likes.

The C++ suite gained a repository-root argument to read it, the way `flight_tests` already
had, and both `build_host.sh` and CTest pass it. Run without it the suite says so and fails
rather than quietly checking nothing — which was confirmed by running it from another
directory.

Reverting the Python counter to its old behaviour fails two of the new tests. That was run
before the fixture was kept.

With this, **every pair of implementations named in the cross-implementation table is held
to a shared fixture**: packet format, validation semantics, raw-log escaping, and now
framing. What is left in that table is single-definition code, where drift is not possible.

### Fixed — the web console replayed raw logs without unescaping them

Driving the console's DOM half in a browser — the half no suite covers — led to its file
replay, and to the same defect the Python replay had, one layer along.

The console already stripped a raw-log line's receipt timestamp. It never undid the
escaping. `logger.py` turns tab, CR, LF and backslash into two-character escapes and every
other control character into `\xNN`, which is what keeps one record on one line; replayed
without reversing that, a payload containing a tab comes back carrying a literal backslash
and a `t`. **The records this matters for are the corrupted ones — which is what a forensic
replay is for.**

`unescapeRaw()` joins the console's portable core, mirroring `unescape_raw()`, and the
replay path uses it. `check_doc_claims.py` checks the console actually calls it, not merely
that it owns a function that could.

**And the two implementations are now held to one file.** `test-data/raw-log-escapes.tsv`
carries 16 cases — every rule, alone and together, plus the two the scheme actually turns
on:

| Escaped | Means | Not |
|---|---|---|
| `\\there` | a backslash, then `there` | a tab, then `here` |
| `\\x41suffix` | a backslash, then `x41suffix` | `Asuffix` |

An unescaper a single character out of step reads the left column as the right one — and
invents a payload the vehicle never sent, while trying to recover one. The plain text is
stored as hex, because it is allowed to contain tabs and newlines: the same reason the log
escapes it in the first place.

Malformed escapes are covered too, and pass through unchanged rather than being guessed at.
A truncated `\x4` at the end of a line is corruption in a forensic record; keeping it
verbatim keeps the evidence.

Also checked in the browser, with no defect found: the console survives every packet shape
the parser accepts — mandatory-only, full GPS, both yaw references, all four diagnostic
tags, an unknown optional field — with no exception and no wrong field. The `—` shown for
the sync word before the bridge reports one behaves as designed.

### Fixed — a dashboard row that could only ever say `n/a`

Sweeping for the same shape as the cut-record counter — values computed and surfaced
nowhere — turned up its mirror image: a display with nowhere to get its value from.

The Tk dashboard carried a **Battery (V)** row, filled from `bridge_status["battery"]`.
Nothing has ever written that key. The bridge's status line reports `radio`, `frames`,
`dropped`, `rssi`, `snr` and now `sync` — no battery, and the bridge is a *different Pico
with no battery sense at all*. The vehicle does measure its pack, and raises a fault when
it is low, but the voltage stays in its health snapshot: the transmitted optional fields
are `MODE`, `FAULTS`, `CAL`, `ARM` and `YR`, and nothing else.

So the row could only ever read `n/a` — which an operator reads as *the link is not
reporting this*, rather than *this is not sent*. The row is gone, and the reason is written
where the decision would be made rather than left as a comment in a UI file.

`telemetry-protocol.md` now records the open question honestly: `BAT-x.xx` would cost about
nine bytes against a 255-byte budget whose measured worst case is 206, and the controller
already drops optional fields before overrunning it — so it is affordable, and it is a
change to the packet contract that this document requires a documented consumer and a
bandwidth assessment for. **The decision is the team's to make, and it is now written down
as one rather than silently made in either direction.**

The sweep that found it — every `const` accessor in the firmware headers, checked against
every call site outside the test suites — found nothing else reaching an operator falsely.
The remaining test-only accessors are library surface the flight image has no need of.

### Fixed — the flight log counted its own cut records and told nobody

`RawBlockLog` writes one record per 512-byte block and cuts anything longer. It counts
those cuts in `truncated_records()`, under a comment that says *"Never silent: the flight
log is evidence, and a shortened record should be visible as such."*

Nothing read it. The counter was incremented, asserted in one test, and exposed to no
health snapshot, no diagnostic and no operator — which is the same as not counting it. The
onboard log is what a flight has left when the radio does not work, and a cut row in it
would have looked exactly like a complete one.

`PicoSdLogger` now exposes it and `cansat_bringup_firmware` prints it at Gate 6.6, beside
the boot count, with the zero case spelled out rather than left as a bare number.

**And the reason it should stay zero is now pinned.** `test_the_widest_sd_row_still_fits_one_block`
builds the widest row the builder can produce — every column at its legitimate maximum,
negative so each carries a sign, the longest state name, both counters at `4294967295` —
and holds *its metadata plus the packet budget's cap* to one block:

| | bytes |
|---|---:|
| Row metadata around the packet | 147 |
| `kWorstCasePacketBytes` | 255 |
| **Widest possible record** | **402** |
| One block holds | 511 |
| **Margin** | **109** |

So truncation is not reachable today. The value of the test is the day someone raises the
packet budget for airtime reasons: that now fails the build here, instead of quietly
shortening every long row in the flight log. Bring-up row 6.6a records the expected zero.

### Added — the numbers an operator reads off the vehicle are now gated too

The status LED is the only thing the vehicle can say with no radio and no serial cable, and
both the bring-up record and the runbook tell an operator which blink rate means what. All
five cadences live in one `switch` in `controller.cpp`, and nothing connected the two. The
same was true of the calibration gates the runbook quotes on its "Calibration never reaches
`CAL-1`" page — the numbers someone reads while standing over a vehicle on the pad,
wondering whether the part is faulty or the table is vibrating.

Ten more checks: every LED cadence and its full cycle, and the stillness, acceleration,
sample-count and timeout gates. **145 claims now**, up from 66 at the start of this cycle.

The bring-up record also gained the two LED states it never listed — `LANDED`/`RECOVERY` at
250 ms and `FAULT` at 60 ms. `FAULT` is the one an operator most needs to recognise, and it
was the one state with a distinct cadence and no row to write it down in. That makes 79
rows, 18 of them measured.

### Fixed — every documented command now runs, starting with the file none of them had

Four documents tell a reader to run `python src/main.py replay packets.txt`. **There is no
`packets.txt` in this repository, and never was.** The first ground-station command in the
quick start ended in `FileNotFoundError` for anyone who followed it exactly.

`test-data/sample-mission.txt` is now shipped, and it is not hand-written: it is the output
of `build/host/emit_mission`, which runs the **real flight controller** through a scripted
ascent and descent. Thirty packets carrying what the vehicle actually transmits — `MODE`
advancing `READY` to `FLIGHT`, the arming and calibration flags changing, GPS and diagnostic
tags present, and `YR-G` on every packet because the delivered IMU has no magnetometer.
Every document now points at it, and `check_doc_claims.py` holds each of its packets to the
rulebook field order and each of those documents to naming a file that exists.

`test-data/README.md` explains all three fixture files and how to regenerate the sample.

### Fixed — the headless monitor said nothing when piped

`live --no-dashboard` prints a link summary every two seconds, and the runbook offers it as
the form to use without a display. Python block-buffers stdout when it is not a terminal, so
piped anywhere — `tee`, a log file, a second window — **it printed nothing at all**: twelve
seconds of a real run produced no output, where a terminal would have shown five status
lines. A monitoring command whose entire purpose is periodic status, silent in exactly the
configuration an operator would pipe on launch day.

Every line in that loop is flushed now, and the runbook shows the `| tee` form. The bridge
line also gained `sync=`, so the sync word the bridge reports is visible without a display
as well as with one.

**All eleven documented commands that do not need hardware were run as written**, from the
directory the document says to run them in: both build scripts, the Node suite, the replay
and the paced live replay. The two failures above were found that way, and the remaining
four are the CMake and Pico-SDK builds, which need a toolchain this machine does not have.

### Fixed — the runbook's post-flight replay produced nothing, silently

Step 2 of post-flight analysis replays the raw log to export a clean CSV. The raw log is
this ground station's forensic record: every line received, corrupted ones included, each
written as `<receipt timestamp>` TAB `<escaped payload>`. `FileReplayTransport` fed each
line to the parser whole — timestamp, tab and all — so every line of a real flight log was
rejected before it reached the validator.

Run against a log written by the real logger, the command reported:

```
received=0 accepted=0 rejected=0
```

Not an error, not a warning. Zero of everything, four hours after a launch, with the graphs
still to produce.

`FileReplayTransport` now recognises a raw-log line and undoes both transformations — the
timestamp and the escaping, the latter through `logger.unescape_raw()` rather than a second
copy of the same four rules. Detection is per line and unambiguous, because no telemetry
packet begins with a date; `raw_log=False` forces the plain reading for a file that somehow
does. The same log now replays as **`received=5 accepted=5 rejected=0`**.

Five tests cover it, and the runbook says what `received=0` would mean if it ever came back.

### Added — the validator's two implementations now read one file

The three parsers have read `test-data/protocol-fixtures.tsv` since cycle 2, so they cannot
disagree about what a valid packet is. The **validator** had no equivalent. Its Python and
JavaScript versions are hand-ports held together by two sets of similarly-named tests, which
is agreement by convention rather than by construction — and the project's own
cross-implementation table said so, listing the guard as "shared test packets; the web
console is a direct port".

`test-data/validator-scenarios.tsv` is the construction: **11 scenarios, 29 packets**, each
row carrying the verdict the validator must reach. Gaps counted as the packets that never
arrived; a duplicate; a late packet that fills nothing; a vehicle reboot, and the corrupted
`P-001` that is not one; a foreign team; a clock regression; an implausible fix that is
flagged while the packet is kept.

Both suites replay the file through their own validator. The fixture earned itself twice
during the writing:

- It rejected a **wrong expectation in the fixture**: a recognised restart clears the
  timestamp reference it would be compared against, so a reboot is *not* also reported as a
  clock regression. The file now says why.
- Deleting the clock-regression requirement from the JavaScript restart rule — the single
  edit that would make the console call every corrupted `P-001` a reboot — **fails two Node
  tests**. That was run before the fixture was kept.

`check_doc_claims.py` holds both suites to actually reading the file, so it cannot become a
fixture sitting in the tree looking like a guarantee.

### Fixed — a number too wide to format became a number, not an error

`number()` formats every mandatory telemetry field into a 64-byte buffer. Values are
finite-checked first, but finite is not the same as representable: `"%.2f"` of `1e300` is
over three hundred characters, and the formatter returned the first 63 of them. That is a
long digit string with no decimal point — a corrupted reading wearing the shape of a
reading.

Both the old and the new behaviour end in a rejected packet, because every parser in this
project is precision-strict. The difference is what the packet carries on the way there:
`Pr-;`, a field that says nothing, rather than `Pr-1000000000000…`, a field that says
something false. Anything downstream that logs the raw payload — and this project logs every
raw payload, deliberately — keeps the honest one.

No sensor can reach these values; the plausibility checks would suppress them long before.
This is the class of defect that only appears when something else has already gone wrong,
which is exactly when a log has to be trustworthy.

`test_a_value_too_wide_to_format_invalidates_the_packet` covers it, and was confirmed to
fail against the previous formatter before the fix was kept.

### Changed — the bridge status buffer, sized for the field after next

The status line's worst case is 121 characters: both counters at `4294967295`, and an SNR
that is whatever float the radio last returned, which formats to 42 characters at its most
negative. That fits the old 128-byte buffer with seven bytes to spare — enough today, and
not enough for the next field somebody adds. `snprintf` truncates rather than overflows, but
a truncated status line is a field that silently vanishes exactly when the link is
misbehaving. The buffer is 160 now, with the arithmetic written down beside it.

### Fixed — the design documents caught up with F-1, and a rule now keeps them there

The hardware documents recorded the six-axis IMU the day it was identified. The design
documents and the requirement checklist did not, and went on describing a vehicle that
fuses nine axes and can reference yaw to magnetic north.

| Document | Said | Says |
|---|---|---|
| `requirements.md` | `SEN-004a` *Not Started*; `SEN-010` "nine-axis fusion; absolute magnetic yaw"; `TEL-017` "magnetometer-referenced yaw from the AK8963" | `SEN-004a` **Blocked — part absent**; both others state that this airframe transmits `YR-G` and why |
| `telemetry-protocol.md` | "an absolute magnetic yaw is available" | `YR-M` is in the protocol and implemented; **this vehicle emits `YR-G` and nothing else** |
| `sensor-rates.md` | A magnetometer section with no caveat | Opens by saying it describes a part the delivered IMU does not have |
| `wiring.md` | "**I2C0 carries three devices, not two**" | "designed for three devices and carries two" — `0x0C` never appears, in either scan, with or without the bypass |
| `pico-gpio-map.md` | Three devices on one bus | Budgeted for three, two present |
| `software-architecture.md` | Nine-axis attitude, no caveat | Same, plus what the vehicle actually runs: gyro-propagated yaw, magnetometer branches dormant |
| `timeline.md` | Yaw risk mitigated by "the MPU-9250's magnetometer" | Mitigation is procurement or an organizer ruling — the mitigation it named does not exist |

Nothing describing the nine-axis design was deleted. The code implements it, the tests
exercise it, and a real MPU-9250 would run it; what changed is that no document now leaves a
reader believing this airframe can produce an absolute heading.

**The rule is now enforced.** `check_doc_claims.py` requires every document that mentions
the magnetometer to also name the delivered `MPU-6500` or link the finding. Sixteen
documents are held to it. The repository audit is excluded on purpose: it is a dated record
of a past run, and rewriting it would be falsifying history rather than fixing a document.

### Fixed — the README described a vehicle with a magnetometer, and a project that had measured nothing

Two of its status claims had been overtaken by the bench.

**The IMU.** The README named an `MPU-9250` with an `AK8963`, said yaw "can be referenced to
magnetic north", and listed the part as *Confirmed; unverified*. The part on the bench is an
**MPU-6500 — six axes, no magnetometer** (`WHO_AM_I` `0x70`, and `0x0C` never answers with
or without the bypass). That is [F-1](documentation/hardware/receiving-inspection.md#findings),
recorded in the hardware documents since it was found and absent from every design document
and from the README. The nine-axis path is implemented, tested, and would produce `YR-M` on
a real MPU-9250; **this vehicle has no absolute yaw reference and its yaw drifts.** Open
question 6 is rewritten around that: if the organisers require an absolute magnetic yaw,
this is a part the vehicle does not have — a procurement item, not a software change.

**The bring-up status.** *"No bring-up, no wiring, no power system, no measurement"* and
*"never executed on real silicon"* were true when written and had not been true for a day.
**18 of the 77 rows in the bring-up record are measured**: the bare Pico, the IMU's bias and
noise, the barometer's real output rate, the acquisition rate and its jitter, NMEA arriving
at 9600 baud, the radio answering `0x12` and its airtime within 1.8 % of the model. Power is
the row that is still genuinely untouched, and it now says so on its own rather than under a
blanket denial that covered work already done.

The timeline's gate table moves with it: gates 4, 5 and 6 go from ⬜ to 🟠, each saying what
was measured and what was not. Gate 5 keeps the sentence that matters — one radio
transmitting is not two radios talking.

**Both are now gated.** `check_doc_claims.py` counts the measured rows in the bring-up
record and holds the README's figure to it, and holds the README and the receiving
inspection to the driver's own list of accepted `WHO_AM_I` values. A status that overstates
progress and one that understates it fail the same build.

The bring-up record's own Findings table, empty while its rows pointed at findings, now
carries four: the six-axis IMU, the barometer rate procedure that measured the wrong thing
and reported 44 Hz for it, the multimeter whose resistance range invented ~51 Ω between pins
with no path between them, and the GPS checksum row taken without a fix and marked
provisional.

Also stale in the README's own testing table: `flight_tests` at 36 suites and 537
assertions, Python at 93, Node at 36, the claim checker at 56, and the Pico syntax check at
10 translation units — every one of which the table now states correctly and the build now
checks, along with the table's contradiction of itself, which listed the web console as both
tested and *not covered*.

### Fixed — the console stated a sync word nobody had told it

The web console's **Sync word** field read `TEST · 0xF3`, written as a literal into its own
markup and into `startDemo()`. The bridge never reported which sync word it had configured,
so the console was not displaying a measurement — it was displaying an assumption, and the
only way to make it wrong is the exact change the audit recommends rehearsing: reflashing
both ends onto the rulebook's launch word `0xA5`. It would then have shown `LINK · 0xF3`
over a link running on `0xA5`, on the one day the field matters.

- **The bridge reports it.** `sync=0x..` joins the once-a-second status line, and the
  `#bridge=online` line carries it too, so the answer is on screen from the bridge's first
  line rather than a second later. The value is read back from the same `link_profile.hpp`
  constant the radio was programmed with.
- **The console reads it.** `parseBridgeStatus()` and `syncWordLabel()` move into the
  portable core, so the field is parsed by tested code rather than by four regexes wired
  straight into a DOM update. The label reads `TEST`, `LAUNCH`, or `UNKNOWN · 0x..` for
  anything that is neither — and `—` until the bridge has said anything at all.
- **The Tk dashboard shows it too**, as a new `Sync word` row under Bridge radio.
- **A field the bridge omits stays absent.** An older bridge image reports no sync word;
  both displays leave the field empty rather than filling it with a default that would be
  indistinguishable from a measurement.

Eight tests cover it — three under Node over the portable core, two in `test_app.py` — and
`check_doc_claims.py` now holds the console's own copies of `SYNC_TEST` and `SYNC_LAUNCH`
to `link_profile.hpp`, since a self-contained HTML file cannot include a C++ header and a
copy is a thing that drifts.

The runbook gains a T-30 line for it, and its "no packets" entry now says that half the
sync-word diagnosis is on screen: the bridge's word is reported, the vehicle's still has to
be inferred from the image that was loaded, because a vehicle nobody is receiving cannot
say what it is transmitting on.

### Fixed — the per-suite test counts in the test plan, and the plan's silence about three files

`test_app.py` was documented as holding 3 tests while holding 11; `test_validator.py` as 9
while holding 20; `test_telemetry.py` as 9 while holding 14. `test_logger.py` (18 tests),
`test_protocol_fixtures.py` (5) and the Pico syntax check's eleventh translation unit
(`bringup_main`) were not described at all. All are now written up, and all are checked:
every Python suite's documented figure is held to the file it describes, and every
`src/pico/*.cpp` must appear in `tools/check_pico_syntax.sh` — a driver added there and
left out of that list is checked by nothing at all.

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

### Decided — no egg payload will be flown

The team has elected not to carry an egg, on personal grounds. **The 20 points for egg
integrity are forgone deliberately**, and every figure in the scoring assessment now reflects
that: achievable total moves from ~177 to **~157**. The 5 points for parachute deployment are
unaffected.

**This is not a disqualification.** The rulebook's disqualification list is closed and
specific - size or mass over by more than 10 %, unsafe deployment, no communication attempt,
late arrival, code of conduct - and an absent egg is on none of them. Recorded as `PAY-001`
so nobody later reads the zero as a failure rather than a choice.

**The chamber should still be built**, and that is `PAY-002`. Section 8 states it as its own
requirement, it carries a **+7 cm** dimensional allowance the body does not have to share,
and section D scores effective use of the volume the rules permit. A chamber that exists and
demonstrably works is compliance; an absent one invites a judge to read "mandatory system
missing" more broadly than the 20 points.

**It also shifts the tie-break.** Ties resolve on payload safety first, telemetry accuracy
second, descent stability third. At 5 of 25 in the first, the second and third are worth more
than their face value - and both are areas this project is strong in.

The four cheapest recommendations already in the assessment total about 23 points, slightly
more than the egg forgoes. The 20 are recoverable elsewhere.

### Changed — the 2026 rulebook revision, and three contradictions it closes

`documentation/requirements/updated CanSat Final Guidelines 2026.pdf` supersedes the earlier
extract. Diffed against it rather than read fresh, so what changed is recorded rather than
re-derived.

**Three open questions to the organizers are answered**, and each was a genuine
self-contradiction in the old document rather than an omission:

- **Dimensions: 21 cm (+7 cm maximum, egg chamber) x 12 cm.** The old rulebook said 12 cm
  width on page 1, 21 x 9 cm on page 4 and 21 (+8) x 12.5 cm on page 10. The revision states
  one figure in both places it appears.
- **Launch altitude: 100 ft, released from a drone.** The old one said 100 ft in the mission
  profile and 150 ft from a drone in the launch guidelines, and mentioned an 8-story rooftop.
  The rooftop is gone.
- **Mass: 500 g (+/-10%)**, with >10% on either size or mass a disqualification.

`GEN-004`, `GEN-005`, `GEN-006` and `MIS-001` move from `TBD` to locked, and the descent
system is no longer blocked on the organizers.

**Two changes make the rules harsher or different in ways worth acting on:**

- **Stray-transmission penalty is five times harsher: -1 point per 2 packets**, where the
  old rulebook said per 10. At this vehicle's 1 Hz that is half a point per second, so ninety
  seconds of a CanSat left on during another team's launch costs more than the whole
  25-point telemetry section. The firmware is *required* to transmit on power-up, so the
  manual switch is the only control - recorded in the runbook and the protocol document.
- **Ground-station compatibility is now an explicit rule**, satisfied by two named radios:
  433 MHz LoRa SX1278 RA-02, or nRF24L01. This vehicle carries the RA-02, confirmed on the
  bench at Gate 5. New requirement `GS-002`.

**And one change is straightforwardly in our favour:** the mandatory sensor set is now worth
**15 points**, where the previous rulebook awarded it nothing.

### Added — a scoring assessment against the 200-point rulebook

`documentation/project/scoring-assessment.md` works through all six sections.

**~47 points secured today, ~177 achievable** with the current design competently built. The
entire gap is mechanical and procedural: every point in payload safety, descent and
structural innovation waits on a chamber, a parachute, a switch and an LED, none of which
exist. Nothing in the gap is electronic or software.

The four cheapest points remaining, in order:

1. **Manual switch and power LED - 5 points, one evening.** The LED must light on power-on,
   so it belongs across the rail through a resistor, not on a GPIO the firmware drives.
2. **A magnetometer - 5 points, ~150 rupees.** It caps the sensor section at 25, restores the
   absolute yaw the MPU-6500 cannot provide, and closes an open question with the organizers
   about whether a relative yaw is acceptable. The firmware's magnetometer path already
   exists and is tested; it needs a source.
3. **2 Hz telemetry - 2 to 3 points, no new hardware.** Measured airtime is 333.7 ms at
   33.4% occupancy, so 2 Hz naively costs 66.8%. Moving to 250 kHz bandwidth halves airtime
   and puts 2 Hz back at today's duty, against a link that only has to cross 30 m. Only
   after a range test: the rulebook subtracts for loss, and a clean 1 Hz beats a lossy 2 Hz.
4. **A custom PCB - up to 10 points.** 15 points sit in a section a perfboard forfeits
   entirely, and it feeds the build-quality and PCB-imaging marks too. The pin map has been
   frozen and hardware-verified since Gate 5, so a schematic could be drawn today.

Worth stating plainly because it is not intuitive: **15 of section D's 30 points are
aesthetics and build quality** - neatness, cable management, labelling, finish. That is won
during assembly, not design.

### Added — a sustained write burst, so the Gate 2 current is actually measurable

Gate 6.3 timed 100 block writes and finished in under half a second. A handheld multimeter
samples two or three times a second, so pointed at that it averaged a window that was mostly
idle and reported a number far below the truth. **The measurement Gate 2 has been waiting on
was not takeable with the instrument anyone actually owns.**

Row **6.3b** now holds the card writing continuously for **10 seconds**, with a three-second
warning first so there is time to look at the meter.

What it reports alongside is the **duty cycle**, and that is what makes the reading mean
anything: near 100 % the meter is reading the write current itself rather than an average of
writes and gaps, and below 80 % the true figure is higher by 1/duty. The diagnostic says
which case you are in rather than leaving it to be assumed.

This measures **sustained** write current, not the instantaneous spike, and that is
deliberate — sustained is the figure a regulator is sized against, and the spike is what the
bulk capacitor is for.

Two things the bring-up record now says out loud. **Take the idle reading first**, because
the write cost is the difference and not the absolute. And **the card is the whole
variable**: the module has no active component at all, so what is being measured is flash
programming inside the card, and write current varies enormously between cards. Measure the
one that will fly.

For anyone who would rather not break the circuit, the go/no-go question has a simpler
answer that needs no series connection: watch the 3V3 rail on DC volts during the burst. If
it holds, the regulator is coping.

### Fixed — the negative-coordinate fix had no test holding it

`test-data/optional-tag-cases.tsv` was written, and read by nothing. The parser fix it
existed to pin was in place in both languages, but nothing checked it, so the bug it
describes could have returned in silence — which is exactly how it arrived.

The bug is worth restating because it is the quiet kind. Optional tags are `<key>-<value>`
and the keys themselves contain dashes, so both ground parsers split on the last one. That
is the separator right up until the value is negative, and then the last dash is the minus
sign: `GP-Lat--18.5` split to the key `GP-Lat-` and the value `18.5`. Sign eaten, key
unrecognisable, `gps_lat` back to null — **a southern-hemisphere fix vanishing from the
console, the CSV and the map with no error and no rejection counter.** Both implementations
were wrong the same way because they were hand-ports of each other and no fixture carried a
negative coordinate.

- `test_telemetry.py` gains three tests reading the fixture, plus an end-to-end one proving
  a southern fix reaches `gps_lat` and `gps_lon` rather than merely splitting correctly.
- `console_core.test.mjs` gains the same two against the console's own parser, reading the
  same file. **One definition, two languages** — the fixture's stated purpose, finally
  wired up.
- `check_doc_claims.py` now requires both suites to read it, and requires the fixture to
  keep its negative cases. A fixture nothing reads is a comment.
- The comment in `index.html` pointed at `protocol-fixtures.tsv`, which does not hold these
  cases. Corrected.

Counts move with it: **166 Python tests, 57 Node tests, 214 documented claims.**

### Changed — the flight log now lives inside a file a PC can open

The card wrote raw blocks at a fixed LBA and did not mount afterwards. Flight data nobody
can read is not evidence, so the log now lands **inside a pre-allocated `FLIGHT.CSV` on a
FAT32 card**, and the card still mounts and opens in a spreadsheet.

**The power-cut safety is kept in full, which is the whole point of doing it this way.** A
filesystem in firmware would have thrown it away: FAT metadata updates are not atomic, and a
brownout or hard landing during one can cost the entire recording. Instead the filesystem
work happens once, on a PC, before the flight — `tools/prepare_sd_card.py` creates the file
— and none of it happens in the air. `FatVolume` finds where that file's blocks physically
live and `RawBlockLog` writes into them exactly as before. The directory entry, the FAT chain
and the file size never change in flight, so there is no metadata to corrupt, and the dual
alternating headers still resume correctly after a torn write.

- **Contiguity is checked, not assumed.** Writing linearly into a fragmented file would
  scribble over whatever occupies the gap while still reporting success. A file whose FAT
  chain is not strictly sequential is refused.
- **There is no fallback to a guessed address.** A missing, fragmented or undersized file
  fails initialisation and says which. Falling back to a fixed LBA would destroy the very
  filesystem this arrangement exists to preserve — and a vehicle with no log still flies and
  still transmits, while a vehicle that silently ate the card gets nothing back either way.
- **A fresh log writes a CSV column header as its first record**, so the file opens as a
  spreadsheet rather than a wall of unlabelled fields.
- **The log's two header blocks are now space-padded and newline-terminated.** They are the
  first two lines of the CSV; padding with spaces rather than NULs keeps them one printable
  line each instead of a wall of NULs that some editors truncate the file at. The checksum
  covers bytes 0–23, so the padding is free.

21 new assertions cover the lookup, one per way it can go wrong: no volume, no file, a
fragmented file, one too small, a card that stops answering, and deleted or long-name
directory entries that must be skipped. The fragmentation case is the dangerous one and is
tested directly.

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
