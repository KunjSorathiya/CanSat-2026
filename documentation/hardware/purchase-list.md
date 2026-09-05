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
| Power switch, LEDs, and their resistors | Held as of 2026-09-05. The 100 kΩ divider pair and the ¼ W spares below are still needed if not among them |
| Hall effect sensor | Held. See section 4 — it replaces the magnetometer by decision, not by equivalence |

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
| **470 µF electrolytic**, ≥ 6.3 V (16 V is what shops stock) — **or 2 × 100 µF ceramic X5R/X7R ≥ 10 V** | 2 | Across the microSD reader's own `3V3` and `GND` pins. Takes the write spike locally so it never reaches the regulator. The requirement is [~50 µF effective](../design/electrical-architecture.md#how-much-bulk-is-actually-needed); the electrolytic is cheap margin, and ceramics work if derated for DC bias | 20 |
| **100 µF electrolytic**, ≥ 10 V | 2 | Pico `VSYS`, near the battery input. Optional — fit if the rail looks noisy under load | 15 |
| **10 µF**, ceramic X5R/X7R ≥ 10 V or electrolytic | 4 | Across the RA-02's `3.3V` and `GND`. PA key-up is 1.5 mA to 87 mA in microseconds | 20 |
| **100 nF ceramic**, marked `104` | 20 | One at **every** module's supply pins. They cost pennies; buy the strip | 30 |

> Electrolytics are polarised — the stripe marks the **negative** leg, to GND. Backwards they
> heat and can vent. Ceramics have no polarity.

**If only some are fitted, fit these two:** the 470 µF at the microSD and the 10 µF at the
RA-02. They are the two that map to failures this project actually had, and together they are
about ₹40. The `104`s are ₹30 for twenty and take seconds each. Only the `VSYS` 100 µF is
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
| **330 Ω**, ¼ W | 10 | Status LED on GP14. Gives ~4 mA at 3.3 V | 10 |
| **1 kΩ**, ¼ W | 10 | Power LED. Dimmer than 330 Ω but it is lit for the whole flight, so the ~1.3 mA matters against a rail with no headroom | 10 |
| **100 kΩ**, ¼ W, 1 % if offered | 10 | Battery divider on GP26 (bring-up row 2.6). Two in series across the battery gives a 2:1 ratio and draws only ~21 µA. **1 % tolerance is worth it here** — this divider is the only thing standing between the telemetry and a wrong battery reading | 20 |

## 4 · Scoring — the cheapest points left

| Item | Qty | Why | ~₹ |
|---|---:|---|---:|
| ~~QMC5883L / HMC5883L magnetometer~~ | — | **Declined 2026-09-05.** A hall effect sensor already held will be integrated instead, for the same additional-sensor credit | — |

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
