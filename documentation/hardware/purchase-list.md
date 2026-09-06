# Purchase list

Everything still to buy, as of 2026-09-05, with what each item is for and where the
requirement comes from. Prices are rough Indian retail and are there to show relative cost,
not to be quoted at anyone.

Nothing here is speculative: every line traces to a competition requirement, a bring-up
finding, or a scoring recommendation that has been costed.

---

## Already bought — do not re-buy

| Item | Note |
|---|---|
| Multimeter that zeroes on its resistance range | The first one read 18 Ω across its own shorted probes ([F-3 in the bring-up record](../testing/bring-up-record.md#findings)) |
| 1S LiPo balance charger | Neither battery lead mated with anything supplied |
| microSD card | HP mx310 64 GB, confirmed block-addressed at bring-up row 6.2 |
| JST-RCY female pigtail | To mate the battery's discharge lead |
| Header strips | Soldered to both Picos and every module on 2026-09-04 |
| Power switch, LEDs, and their resistors | Held. An I/O switch and LEDs in two colours, confirmed 2026-09-06 |
| Capacitors — 10 µF 50 V, 100 µF 50 V, 100 µF 25 V, 0.1 µF `104` | **Received 2026-09-06 in the ordered quantities** ([D.7](receiving-inspection.md#d7--the-capacitors)) |
| Resistors: 100 kΩ 5 %, 33 kΩ 1 %, 1 kΩ 5 % | **Received 2026-09-06** and read from their bands ([D.6](receiving-inspection.md#d6--the-resistors)). The 33 kΩ pair is the `GP26` divider, 1 kΩ drives both LEDs, 100 kΩ is spare. **No 330 Ω arrived and none is needed** |
| Hall effect sensor | Held, and **not being used** (decided 2026-09-06). Its supply voltage was never confirmed, and with sensor integration already at its 25-point cap it would have scored nothing. Kept as a spare part, not a planned one |
| LM393 sound detection sensor | Held, confirmed 3.3 V, and confirmed the **four-pin `AO DO GND VCC` variant** on 2026-09-06. Integrated on `GP27` (level) and `GP15` (threshold duty), logged to the SD card, not transmitted |

---

## 1 · Blocking the soldered board

Without these the board cannot be built.

| Item | Qty | Why | ~₹ |
|---|---:|---|---:|
| **Perfboard**, 7×9 cm or larger, ideally double-sided plated-through | 2 | The vehicle board, plus one spare. Buy the spare — the first attempt at a soldered board is rarely the one that flies | 100 |
| **Female headers**, 2.54 mm, 40-pin strips | 4 | **Socket every module rather than soldering it down.** A dead RA-02 or a swapped IMU then costs a minute, not a rebuild. This is also how the Pico itself should mount | 120 |
| **Solid-core hookup wire**, 22 AWG, several colours | 1 set | Board wiring. Solid core, not stranded — it seats in perfboard holes | 150 |
| **Silicone stranded wire**, 22 AWG, red and black | 1 m each | Battery leads only. Stranded survives flexing where solid core work-hardens and snaps | 80 |

## 2 · Capacitors — from [F-10](../testing/bring-up-record.md#findings)

A long supply jumper made **every** microSD write fail while every read passed, and the same
fault on the RA-02's supply made a healthy radio fail eight transmits in a row. These are
what stop that happening on the soldered board. Values and reasoning:
[electrical-architecture.md](../design/electrical-architecture.md#decoupling).

| Item | Qty | Where it goes | ~₹ |
|---|---:|---|---:|
| ~~100 µF electrolytic~~ | — | **Received, in 50 V and 25 V.** A pair goes in parallel across the microSD's own `3V3` and `GND`, giving 200 µF against a [~50 µF requirement](../design/electrical-architecture.md#how-much-bulk-is-actually-needed). Mixing the two voltage ratings in that pair is fine — both are far above 3.3 V, and an electrolytic has no DC-bias derating, so each contributes its full marking | — |

| ~~10 µF electrolytic~~ | — | **Received, 50 V.** Goes across the RA-02's `3.3V` and `GND` — PA key-up is 1.5 mA to 87 mA in microseconds | — |
| ~~0.1 µF 50 V ceramic, marked `104`~~ | — | **Received 2026-09-06, as ordered.** Six are fitted: microSD, RA-02, MPU-6500, BMP280, NEO-6M and the sound board. Not optional and not substitutable — every bulk capacitor, of every type, is too slow for the fast edges these cover | — |

> Electrolytics are polarised — the stripe marks the **negative** leg, to GND. Backwards they
> heat and can vent. Ceramics have no polarity.

**If only some are fitted, fit these two:** the 100 µF pair at the microSD and the 10 µF at
the RA-02. They are the two that map to failures this project actually had, and together they are
about ₹15. The `104`s are ₹2 each and take seconds to fit. Only the spare at `VSYS` is
genuinely optional.

A soldered board has a far better supply path than the breadboard jumper that caused
[F-10](../testing/bring-up-record.md#findings), so it may well work without them. The reason
to fit them anyway is that the failure is **intermittent and load-dependent** — it appeared
and vanished across five bench runs — so a clean bench test does not mean it is absent, and
the flight symptom is an empty log or a radio that stops. A capacitor can be soldered across
two pins at any later date, so this is reversible; **but buy them now**, because discovering
the need the night before a launch is the case worth avoiding.

## 3 · Resistors

| Item | Qty | Why | ~₹ |
|---|---:|---|---:|
| ~~330 Ω~~ | — | **Not needed.** None arrived and 1 kΩ serves both LEDs at ~1.3 mA, which is visible and cheaper in current | — |
| ~~1 kΩ~~, ~~100 kΩ~~ | — | **Received 2026-09-06.** See the table above | — |
| ~~a 1 % divider pair~~ | — | **Received as 33 kΩ ±1 %**, which is the better divider: same 2:1 ratio, a fifth of the tolerance of the 100 kΩ 5 % parts, 64 µA off the battery | — |

## 4 · Scoring — the cheapest points left

| Item | Qty | Why | ~₹ |
|---|---:|---|---:|
| ~~QMC5883L / HMC5883L magnetometer~~ | — | **Declined 2026-09-05.** The additional-sensor credit is taken by the analogue microphone instead, which is integrated and held by tests | — |

The delivered IMU is a six-axis **MPU-6500 with no magnetometer**
([F-1](receiving-inspection.md#findings)), so declining this part leaves the vehicle with no
magnetic sensing of any kind.

**And a hall effect sensor is not a magnetometer — it cannot stand in for one.** A common hall
switch operates around 10 mT and the Earth's field is about 50 µT — roughly 200× too weak. It
gives no yaw reference of any kind.

It can still be a real sensor rather than a token one, and the honest application on this
vehicle is **separation detection**: a magnet on the launch carrier and the sensor on the
CanSat, so the moment the field drops is a hardware-timestamped deployment event. That is
defensible to a judge in a way that "we added a sensor" is not. It needs one free GPIO —
GP0–GP3, GP8–GP11, GP15, GP27 and GP28 are all unused — plus a driver and a telemetry field.

**What the decision leaves exposed is not the 5 points.** Yaw is a *mandatory* field
([TEL-017](../requirements/requirements.md), SEN-010) and this vehicle can only transmit a
relative, gyro-propagated yaw, declared `YR-G`. Whether that is acceptable is
[open question 6](../requirements/requirements.md) and is unanswered. If the organizers
answer "magnetic required", that is a failed mandatory field rather than a missed bonus, and
the magnetometer becomes a purchase again — so **get that answer before the build is closed
out.** The firmware's `YR-M` path already exists and is tested; it needs only a part.

See [scoring-assessment.md](../project/scoring-assessment.md#the-cheapest-points-remaining)
for the other two recommendations, neither of which needs a purchase: the 2 Hz packet rate is
a configuration change, and the custom PCB is a schedule decision.

## 5 · Mechanical — required, and not costed here

The parachute test is worth **5 points** and is being taken; the egg test is being skipped by
choice. Section C is currently **0 of 25**.

| Item | Note |
|---|---|
| **Parachute** — ripstop nylon, shroud line, swivel | Or a ready-made one. The descent-rate requirement drives the canopy area; size it before buying fabric |
| **Structure / body** | Material choice carries a **bonus for sustainable or unconventional materials** — 3D printed, recycled or composite. Worth choosing deliberately rather than by default |
| **Zip ties, heat-shrink, kapton or electrical tape** | Strain relief on every wire that leaves the board, and on the antenna pigtail especially — the u.FL connector is the most fragile thing on the vehicle |

## 6 · Consumables, if not already held

Solder, flux, desoldering braid, a solder sucker. Nothing exotic.

---

## What this does not include

- **A regulator.** None is needed. Every load runs from the Pico's own 3.3 V pin, and both
  the radio and the microSD have been measured holding that rail alone at 100 % duty
  (rows 5.4a and 6.3b). See the [power budget](../design/electrical-architecture.md#power-budget)
  for what that leaves — which is not much.
- **Anything else for that rail.** The all-at-once case already exceeds what the pin is rated
  to supply. Adding a load means re-doing the budget first.
