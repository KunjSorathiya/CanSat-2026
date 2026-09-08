# Electrical

The vehicle's electrical design: what is connected to what, and what carries the current.

**Status: 2026-09-08.** The board is **built and working**. Every device on it answers, and
the 3.3 V rail has been measured under load. Three parts of the power path are still
missing, and all three are cheap.

| | |
|---|---|
| 🟢 **Signal wiring** | Complete and verified. I2C0, SPI0 and UART0 all pass on the soldered board |
| 🟢 **3.3 V rail** | **No separate regulator is needed.** Every load runs from the Pico's own `3V3(OUT)`, measured at **3.28–3.29 V through 45 back-to-back transmits** and 3.28–3.30 V at 100 % microSD write duty |
| 🔴 **Battery path** | The **switch**, the **Schottky** and the **divider** are not fitted. Until the Schottky is in, the battery switch must be OFF whenever USB is connected |
| 🔴 **Indicators** | The power LED and the status LED are not fitted. They are **5 of the cheapest points in the rulebook** |

---

## Contents

- [The design in one paragraph](#the-design-in-one-paragraph)
- [What is here](#what-is-here)
- [The authoritative sources](#the-authoritative-sources)
- [Open items](#open-items)

---

## The design in one paragraph

A single 1S LiPo feeds a manual switch, a Schottky diode and then the Pico's `VSYS`. The
Pico's own regulator produces the **one and only 3.3 V rail**, and every peripheral — radio,
microSD, IMU, barometer, GPS and microphone — hangs off it. There is no second rail and no
external regulator: the AMS1117-3.3 was assessed and rejected, and the measurements then
showed that nothing needed replacing it. Three buses leave the Pico: I2C0 to the two
environmental sensors, SPI0 shared between the radio and the card, and UART0 to the GPS. Two
ADC channels read the battery divider and the microphone. Ground is a ring around the board
edge with the analogue ground tied to it exactly once.

**The one thing that surprised this project:** a long supply jumper made *every* microSD
write fail while every read passed, and the same fault made a healthy radio fail eight
transmits in a row ([F-10](../documentation/testing/bring-up-record.md#findings)). The bulk
and ceramic decoupling in the netlist is not decoration; it is the fix for a failure this
board actually had.

---

## What is here

| Path | Contents |
|---|---|
| [`schematics/`](schematics/) | The machine-readable netlist — every net, every part, every pin. Generated from the firmware so it cannot drift |
| [`PCB/`](PCB/) | Board layout. The vehicle is perfboard today; this is where a fabricated PCB would live |

---

## The authoritative sources

Electrical facts live in four places, and they are ranked. When two disagree, the higher
one wins and the lower one is the bug.

1. **The hardware itself**, as recorded in
   [bring-up-record.md](../documentation/testing/bring-up-record.md). A measurement beats
   everything below it.
2. **`BoardPins`** in [`config.hpp`](../firmware/flight-computer/include/flight/config.hpp).
   The firmware decides which pin does what; a document that disagrees is wrong.
3. **[`schematics/vehicle-netlist.tsv`](schematics/vehicle-netlist.tsv)**, generated from 2
   and refusing to build if it disagrees with it.
4. **The prose and drawings** — [wiring.md](../documentation/design/wiring.md),
   [electrical-architecture.md](../documentation/design/electrical-architecture.md),
   [pico-gpio-map.md](../documentation/hardware/pico-gpio-map.md).

| Document | What it is for |
|---|---|
| [electrical-architecture.md](../documentation/design/electrical-architecture.md) | Power topology, the regulator analysis and its rejection, grounding, decoupling, power budget, risks |
| [wiring.md](../documentation/design/wiring.md) | Signal wiring for both Picos, the pin table, bus-sharing rules, bring-up order |
| [pico-gpio-map.md](../documentation/hardware/pico-gpio-map.md) | GPIO reservation, pin by pin |
| [pico-resource-map.md](../documentation/hardware/pico-resource-map.md) | Peripheral-level analysis: I2C, SPI, UART, ADC, interrupts |
| [electrical-compatibility.md](../documentation/hardware/electrical-compatibility.md) | Per-component voltage and logic-level assessment |
| [assembly-procedure.md](../documentation/hardware/assembly-procedure.md) | The floorplan and the sixteen gated build steps |
| [wiring-schedule.svg](../documentation/hardware/diagrams/wiring-schedule.svg) | Every Pico pin and every module pin, on one sheet |
| [power-path-battery-to-pico.svg](../documentation/hardware/diagrams/power-path-battery-to-pico.svg) | The battery path, drawn, including the gap where the switch goes |

---

## Open items

| Item | Why it is open | Cost |
|---|---|---|
| **Schottky diode** between the switch and `VSYS` | Not bought. Required by the Pico datasheet §4.5 for a second supply; without it USB back-powers the LiPo | ~₹10 |
| **Manual ON/OFF switch** | Held, not fitted. [PWR-001](../documentation/requirements/requirements.md), mandatory | held |
| **Power LED**, lit the instant the switch closes | Held, not fitted. PWR-002 and PWR-003, mandatory, **and it must be immediate** — so it goes on the rail, not on a GPIO | held |
| **Status LED** on GP14 | Held, not fitted. Not a requirement, but it is the only diagnostic visible on a sealed vehicle, and bring-up rows 1.1–1.3 cannot be taken without it | held |
| **Battery divider**, 33 kΩ / 33 kΩ into GP26 | Resistors held, not fitted. `battery_divider_ratio` stays 0 until it is measured, and the firmware reports the raw pin voltage rather than inventing a scale | held |
| **Series current draw** | Four bench sessions have skipped it. The 3.3 V test link (`J1`) exists to make it a one-minute measurement | — |
| **Radio and card drawing together** | Each has been measured alone and the rail holds. Nothing has yet run both at once — that is bring-up gate 7 | — |

Related: [electrical/schematics/](schematics/) · [electrical/PCB/](PCB/) ·
[bring-up-record.md](../documentation/testing/bring-up-record.md) ·
[purchase-list.md](../documentation/hardware/purchase-list.md)
