# Scoring Assessment — CanSat 2026, 200 Points

Where this project stands against
[the 2026 rulebook](../requirements/updated%20CanSat%20Final%20Guidelines%202026.pdf), what
it would score today, and what the cheapest remaining points are.

> **This is an estimate, and half of it is a judgement call about work nobody has done yet.**
> Sections D and F are scored on appearance, craftsmanship and presentation; section C's
> descent scoring is explicitly *comparative across teams*, so no absolute figure is
> knowable. Every number below is stated with what it assumes.

**Assessed 2026-09-05, against the bring-up state recorded in
[bring-up-record.md](../testing/bring-up-record.md).**

---

## Contents

- [Summary](#summary)
- [Section by section](#section-by-section)
- [The cheapest points remaining](#the-cheapest-points-remaining)
- [What would lose points](#what-would-lose-points)

---

## Summary

| Section | Max | **Secured today** | **Achievable** | Gap |
|---|---:|---:|---:|---|
| A · Payload safety | 25 | **0** | **5** | Egg test declined — 20 points forgone by choice |
| B · Telemetry & communication | 25 | **0** | 23 | Link never flown; rate is at the floor |
| C · Parachute, descent, stability | 25 | **0** | 23 | No descent system, no switch, no LED |
| D · Structural & material innovation | 30 | **0** | 24 | Nothing mechanical exists |
| E · Technical design & analysis | 70 | **~29** | 58 | Perfboard not PCB; one sensor short |
| F · Final report | 25 | **~18** | 24 | Needs photos and flight graphs |
| **Total** | **200** | **~47** | **~157** | |

> **The team has elected not to fly an egg payload**, on personal grounds. The 20 points for
> egg integrity are treated as forgone from here on, and every figure in this document
> reflects that. The 5 points for parachute deployment are unaffected and remain in scope.

**"Secured today"** counts only what is built and verified — the firmware, the ground
station, the documentation, and the five bring-up gates that have passed on hardware.

**"Achievable"** assumes the mechanical build is done competently, the flight succeeds, and
the recommendations below are acted on. It is not a ceiling; it is what this design earns
without changing its approach.

**The whole remaining gap is mechanical and procedural, not electronic or software.** Every
point in C and D is waiting on a structure, a parachute, a switch and an LED.

**The four cheapest recommendations below total about 23 points — slightly more than the egg
forgoes.** That is the useful way to read this table: the 20 points are recoverable
elsewhere, at a cost of roughly one evening, one small sensor, a configuration change and a
PCB order.

---

## Section by section

### A · Payload Safety — 25 points

| Item | Points | Status |
|---|---:|---|
| Egg recovered unbroken | 20 | **Forgone by team decision.** Scored as 0 |
| Parachute deployment | 5 | **In scope, not yet built.** No parachute exists |

**Omitting the egg is not a disqualification.** The rulebook's disqualification list is
closed and specific: exceeding size or mass by more than 10 %, unsafe deployment, no attempt
at a communication system, arriving late, or violating the code of conduct. An absent egg
appears on none of them, so this costs 20 points and nothing else.

**Two consequences worth planning around, neither of them the points.**

**First, build the egg chamber anyway.** Section 8 requires "a cushioned and secure chamber
specifically designed to hold the egg payload", and the general rules list an egg payload
among the mandatory systems. A chamber that exists and demonstrably works is compliance with
a stated requirement; an absent chamber invites a judge to read "mandatory system missing"
more broadly than the 20 points. The chamber also carries its own **+7 cm** dimensional
allowance, and section D scores effective use of the volume the rules permit — so building it
costs nothing and protects marks elsewhere. Whether anything rides inside it on the day is a
separate question from whether it was engineered.

**Second, it changes the tie-break.** Ties are broken on payload safety first, telemetry
accuracy second, descent stability third. At 5 of 25 in the first category, a tie is likely
to go the other way — which makes the second and third categories worth more than their raw
point value. Both are areas this project is strong in.

**Worth one email to the organizers:** whether declining the egg carries any consequence
beyond the 20 points, given the general rules call it mandatory. The disqualification list
says no. Getting that in writing costs nothing and removes the only real uncertainty here.

The 2026 revision adds a specific constraint worth designing to now: the parachute **must
not be tightly packed** — it has to be external or semi-exposed so it deploys instantly on
release. A chute stuffed inside the body risks both the 5 points and a disqualification for
unsafe deployment.

### B · Telemetry & Communication — 25 points

| Item | Points | Assessment |
|---|---:|---|
| Real-time data transmission | 20 | **20 achievable.** The firmware satisfies every stated condition |
| Transmission capability & reliability | 5 | **~2–3 as configured.** We sit at the 1 Hz floor |
| Data format compliance | gate | **Compliant** |

What is already right, and verified:

- **Transmits automatically on power-up** with no manual trigger — a 2026 requirement.
- **Continuous from power-on through landing**, and `post_impact_transmission_ms` is held at
  ≥ 5000 ms by `validate_config()`, which refuses to build otherwise.
- **Format is exact**, held to `test-data/protocol-fixtures.tsv` by three independent parsers
  in C++, Python and JavaScript.
- **Team identity enforced** — the firmware refuses to run as `CAN-Team-XX`, and the
  rulebook awards zero telemetry points for a wrong team number regardless of transmission
  success.
- **Radio is compatible with an official ground station** — SX1278 RA-02, one of the two the
  Physics Club provides. Confirmed on the bench: version register `0x12`, airtime within
  1.8 % of the model.

**Where the points are being left:** the rulebook rewards packet rates above 1 Hz. This
vehicle transmits at exactly 1 Hz, the minimum. See
[the recommendations](#the-cheapest-points-remaining).

### C · Parachute, Descent, Stability & Integrity — 25 points

| Item | Points | Assessment |
|---|---:|---|
| Descent rate & stability | 6 | **Not addressed**, and scored *comparatively against other teams* |
| Sensor data continuity | 9 | **7–9 achievable.** The firmware's strongest area |
| Structural integrity & post-landing transmission | 5 | Post-landing half is done; structure does not exist |
| Bare minimum: manual switch + power LED | 5 | **Not built. Five of the cheapest points on the board** |

Sensor continuity is where the software earns its keep: faults are classified rather than
fatal, a failed sensor degrades the packet instead of stopping it, and the packet number is
not consumed when mandatory data is invalid. A dropped sensor mid-descent costs a field, not
the stream.

**Descent time is scored comparatively** — longer stable descents score higher, subject to
≤ 5 m/s. That makes chute sizing a competitive decision, not just a safety one.

### D · Structural & Material Innovation — 30 points

| Item | Points | Status |
|---|---:|---|
| Compactness & efficiency | 5 | Nothing built |
| Structural & material design (CAD, justification) | 5 | No CAD |
| Fabrication & innovation | 5 | Nothing built |
| Aesthetics & build quality | 15 | Nothing built |

**This is the largest untouched section, and 15 of its 30 points are aesthetics and build
quality** — neatness, cable management, labelling, finish. That is unusually high weighting
for presentation, and it is won or lost during assembly, not design.

The rulebook explicitly permits any outer material (PVC, plastic, 3D print) and offers a
**bonus for sustainable or unconventional materials** — 3D printed, recycled, composites.

### E · Technical Design & Analysis — 70 points

| Item | Points | Secured | Assessment |
|---|---:|---:|---|
| PCB design | 15 | ~0 | Perfboard, not a custom PCB |
| Code originality | 10 | **~9** | The strongest single area in the project |
| Sensor integration | 25 | **20** | 15 mandatory + 5 for GPS |
| Data analysis | 20 | ~0 | Tooling ready; needs flight data |

**Code originality — 9 or 10 of 10.** Self-written, no third-party libraries anywhere in the
flight path, heavily commented, and held by 4333 assertions across 166 Python and 57 Node
tests. The drivers for the MPU-9250, BMP280, NEO-6M, SX1278 and the SD card are all written
here against their datasheets and register maps. This section rewards exactly what this
repository is.

**Sensor integration — 20 of 25 today.** The mandatory set is complete and worth 15 in the
2026 revision (it was worth nothing in the previous one — **this is a scoring change in our
favour**): gyroscope and accelerometer, pressure and altitude, and LoRa telemetry. GPS adds
the first +5.

**The second +5 is missing, and the reason is [F-1](../hardware/receiving-inspection.md#findings).**
The rulebook lists *magnetometer* first among its additional-sensor examples and defines
`MGX`/`MGY`/`MGZ` prefixes for it. The module sold as an MPU-9250 turned out to be an
MPU-6500 with no magnetometer — so the vehicle has no magnetometer to score, **and** no
absolute yaw.

**PCB design — near zero on a perfboard.** The section asks for original PCB design with
layout diagrams, minimal external wiring and well-routed traces, with a bonus for custom
boards *instead of* generic dev boards. A hand-wired single-sided prototype board with
isolated pads is the opposite of that on every criterion.

**Data analysis — the tooling is ready and unused.** The ground station already exports CSV,
the web console already plots altitude and pressure live, and the mandatory graphs are
altitude, temperature and pressure against time. Extra credit is offered for acceleration
profiles, orientation changes, descent rate and correlations — all of which are already in
the telemetry.

### F · Final Report — 25 points

| Item | Points | Assessment |
|---|---:|---|
| Structure & documentation | 20 | **~18 of it already written** |
| Imaging & media | 5 | **0.** Needs the physical build |

The report asks for design approach, system architecture, mission procedure, results and
lessons learned, with schematics, wiring diagrams and CAD attached, and proper citations.
Most of that exists: `software-architecture.md`, `wiring.md`, `electrical-architecture.md`,
`link-budget.md`, `sensor-rates.md`, `test-plan.md`, `bring-up-record.md`,
`receiving-inspection.md` and this repository's changelog are a fuller engineering record
than most teams will submit.

**Lessons learned writes itself from the findings register** — a supplier's MPU-9250 that
answered as an MPU-6500, a microSD listing that described a board with a regulator when the
delivered one had none, a multimeter that invented a short circuit, and an airtime model
that survived contact with a real radio to within 1.8 %.

Media is mandatory and unstarted: top, side and bottom views of the CanSat, PCB views, a
team photo with the CanSat, and a group photo with mentors.

---

## The cheapest points remaining

Ordered by points per unit of effort.

### 1. Manual switch and power LED — **5 points, ~₹50, one evening**

Already an open item in [wiring.md](../design/wiring.md). The rulebook is explicit that
missing either loses points, and the LED must light **immediately on power-on** — so wire it
across the 3.3 V rail through a resistor, not from a GPIO the firmware drives. A
firmware-driven LED does not light until the firmware boots.

### 2. Add a magnetometer — **5 points, ~₹100–150**

The single best-value part left, because it buys three things at once:

- **+5** in sensor integration, taking the section to its 25-point cap.
- **Restores absolute yaw**, which the MPU-6500 cannot provide. The firmware's entire
  magnetometer path — the AK8963 driver, the axis rotation, the `YR-M` / `YR-G` declaration —
  already exists and is tested; it needs a source.
- **Removes an open question with the organizers** about whether a relative gyro-integrated
  yaw is acceptable for the mandatory `Ya-` field.

A **QMC5883L** or **HMC5883L** breakout is 3.3 V, I²C, and sits on the bus already proven at
GP4/GP5. It needs a new driver — perhaps 150 lines against the existing `Imu` interface
pattern — plus `MGX`/`MGY`/`MGZ` fields, which the rulebook already defines prefixes for.

### 3. Raise the packet rate to 2 Hz — **~2–3 points, no new hardware**

The rulebook rewards rates above 1 Hz "provided transmissions remain consistent", and
penalises loss. We measured airtime on the delivered radio: **333.7 ms** for a 206-byte
packet, **33.4 %** channel occupancy at 1 Hz.

Naively doubling to 2 Hz gives **66.8 %** occupancy, which is aggressive with other teams on
the band. **The clean route is already named in [link-budget.md](../design/link-budget.md):
move to 250 kHz bandwidth**, which roughly halves airtime to ~167 ms and puts 2 Hz back at
~33 % occupancy — the same duty we run today.

The cost is about 3 dB of receiver sensitivity, against a link that only has to cross **30
metres**. The margin is enormous at that range.

> **Do this only after a range test at the venue**, and only if loss stays at zero. The
> rulebook subtracts for packet loss, so a 2 Hz stream with drops scores worse than a clean
> 1 Hz one. `validate_config()` enforces the airtime arithmetic, so a profile that cannot
> sustain the rate will refuse to build rather than fly badly.

### 4. A custom PCB — **up to ~10 points, the largest single technical gain**

15 points sit in a section this project currently forfeits. A custom board also feeds
section D's compactness and build-quality marks, and section F's PCB imaging requirement.

This is the most expensive recommendation in time and the only one with a lead time. Whether
it fits the schedule is a judgement call, not an engineering one — but the pin map has been
frozen and hardware-verified since Gate 5, so a schematic could be drawn from
[wiring.md](../design/wiring.md) today without waiting for anything.

### 5. Data analysis preparation — **up to 20 points, entirely preparation**

Four hours post-launch is not long to build graphs from scratch. **Write the analysis
notebook before launch day**, against `test-data/sample-mission.txt`, so that on the day it
is a matter of pointing it at the real CSV.

Mandatory: altitude, temperature and pressure against time. Extra credit is explicitly
offered for acceleration profiles, orientation changes, descent rate and correlations — all
already present in the packet.

---

## What would lose points

**Stray transmission during another team's launch. The 2026 revision made this five times
harsher: −1 point per 2 packets, where the old rulebook said per 10.** At 1 Hz that is half a
point per second. Ninety seconds of a CanSat accidentally left on costs more than the entire
telemetry section is worth. The firmware is *required* to transmit on power-up, so the manual
switch is the only control — see the runbook.

**Wrong sync word.** `0xF3` for testing, `0xA5` for the official launch, and both ends must
change together. A mismatch is a silent, total loss of telemetry that looks exactly like a
dead radio.

**Wrong team number or a malformed packet** zeroes the telemetry section regardless of how
well the link performs. The firmware guards both, and three parsers hold the format to one
fixture.

**Exceeding 21 cm (+7) × 12 cm or 500 g by more than 10 % is a disqualification**, not a
deduction. So is an unsafe deployment, a missing communication attempt, or arriving late.

---

## Related documents

- [Requirements](../requirements/requirements.md) — the traceable rulebook-to-verification map
- [Bring-Up Record](../testing/bring-up-record.md) — what has actually been measured
- [Link Budget](../design/link-budget.md) — the airtime arithmetic behind the rate recommendation
- [Receiving Inspection](../hardware/receiving-inspection.md) — the findings register that feeds "lessons learned"
