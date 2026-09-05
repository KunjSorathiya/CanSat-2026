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

---

## 1 · Blocking the soldered board

Without these the board cannot be built.

| Item | Qty | Why | ~₹ |
|---|---:|---|---:|
| **Perfboard**, 7×9 cm or larger, ideally double-sided plated-through | 2 | The vehicle board, plus one spare. Buy the spare — the first attempt at a soldered board is rarely the one that flies | 100 |
| **Female headers**, 2.54 mm, 40-pin strips | 4 | **Socket every module rather than soldering it down.** A dead RA-02 or a swapped IMU then costs a minute, not a rebuild. This is also how the Pico itself should mount | 120 |
| **Slide or toggle switch**, SPST or SPDT, latching, ≥ 1 A | 2 | **Mandatory** — the rulebook requires a manual power switch, and its absence is part of why section C scores 0. It breaks the **battery positive** lead so there is one deliberate off state. Latching, never momentary. 1 A is ample against a ~300 mA peak | 40 |
| **LEDs**, 3 mm or 5 mm, two colours | 5 each | One is the **mandatory power LED**, one is the firmware status LED on GP14 (bring-up rows 1.1–1.3, still unmeasured because there is no LED) | 30 |
| **Solid-core hookup wire**, 22 AWG, several colours | 1 set | Board wiring. Solid core, not stranded — it seats in perfboard holes | 150 |
| **Silicone stranded wire**, 22 AWG, red and black | 1 m each | Battery leads only. Stranded survives flexing where solid core work-hardens and snaps | 80 |

## 2 · Capacitors — from [F-10](../testing/bring-up-record.md#findings)

A long supply jumper made **every** microSD write fail while every read passed, and the same
fault on the RA-02's supply made a healthy radio fail eight transmits in a row. These are
what stop that happening on the soldered board. Values and reasoning:
[electrical-architecture.md](../design/electrical-architecture.md#decoupling).

| Item | Qty | Where it goes | ~₹ |
|---|---:|---|---:|
| **470 µF electrolytic**, ≥ 6.3 V (16 V is what shops stock) | 2 | Across the microSD reader's own `3V3` and `GND` pins. Takes the write spike locally so it never reaches the regulator | 20 |
| **100 µF electrolytic**, ≥ 10 V | 2 | Pico `VSYS`, near the battery input. Optional — fit if the rail looks noisy under load | 15 |
| **10 µF**, ceramic X5R/X7R ≥ 10 V or electrolytic | 4 | Across the RA-02's `3.3V` and `GND`. PA key-up is 1.5 mA to 87 mA in microseconds | 20 |
| **100 nF ceramic**, marked `104` | 20 | One at **every** module's supply pins. They cost pennies; buy the strip | 30 |

> Electrolytics are polarised — the stripe marks the **negative** leg, to GND. Backwards they
> heat and can vent. Ceramics have no polarity.

## 3 · Resistors

| Item | Qty | Why | ~₹ |
|---|---:|---|---:|
| **330 Ω**, ¼ W | 10 | Status LED on GP14. Gives ~4 mA at 3.3 V | 10 |
| **1 kΩ**, ¼ W | 10 | Power LED. Dimmer than 330 Ω but it is lit for the whole flight, so the ~1.3 mA matters against a rail with no headroom | 10 |
| **100 kΩ**, ¼ W, 1 % if offered | 10 | Battery divider on GP26 (bring-up row 2.6). Two in series across the battery gives a 2:1 ratio and draws only ~21 µA. **1 % tolerance is worth it here** — this divider is the only thing standing between the telemetry and a wrong battery reading | 20 |

## 4 · Scoring — the cheapest points left

| Item | Qty | Why | ~₹ |
|---|---:|---|---:|
| **QMC5883L or HMC5883L magnetometer** breakout, 3.3 V I²C | 1 | **+5 points**, and the single best-value part remaining. The delivered IMU is a six-axis MPU-6500 with no magnetometer ([F-1](receiving-inspection.md#findings)), so the vehicle has no absolute yaw. Sits on the I²C0 bus already proven at GP4/GP5 — no new pins. Needs ~150 lines of driver | 150 |

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
