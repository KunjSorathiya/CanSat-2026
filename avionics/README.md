# Avionics

The three onboard subsystems, each summarised against what has actually been measured.

**Status: 2026-09-14 — submitted, launch pending.** The vehicle board is built, every device
on it answers, and it flies inside the printed structure.

| Subsystem | State | Detail |
|---|---|---|
| [Sensors](sensors/) | 🟢 **All four verified on the soldered board** | IMU, barometer and GPS read; microphone fitted. One part is not what it was sold as |
| [Telemetry](telemetry/) | 🟢 **Link closed on the bench 2026-09-07** | 66 packets, no gaps, no duplicates, −44 dBm. Range untested |
| [Power](power/) | 🟢 **Rail measured; switch, power LED and divider fitted** | No regulator needed. The Schottky was not fitted, so USB and battery must never be connected together |

---

## What this directory is, and is not

Each subdirectory holds a **subsystem summary**: what the parts are, what has been measured
on them, what the firmware does with them, and what is still open. One page each, current
as of the date at the top.

**It is not the authority for any of it.** Every number here is quoted from somewhere that
owns it:

| Kind of fact | Owned by |
|---|---|
| Measurements taken on hardware | [bring-up-record.md](../documentation/testing/bring-up-record.md) |
| Pin assignment | [`BoardPins`](../firmware/flight-computer/include/flight/config.hpp) |
| Part identification and inspection | [receiving-inspection.md](../documentation/hardware/receiving-inspection.md) |
| Wire-level connections | [vehicle-netlist.tsv](../electrical/schematics/vehicle-netlist.tsv) |
| Wire format and radio parameters | [telemetry-protocol.md](../documentation/design/telemetry-protocol.md) |
| Requirement status | [requirements.md](../documentation/requirements/requirements.md) |

**When a summary here disagrees with one of those, the source wins and this page is the
bug.** These pages exist because "what is the state of the radio" is a question that
otherwise needs four documents to answer.

---

## The one thing to know about each

**Sensors — the IMU is not the part on the invoice.** It was sold as a nine-axis MPU-9250
and delivered as a six-axis MPU-6500: `WHO_AM_I` reads `0x70` and `0x0C` never answers.
There is no magnetometer, so yaw is a gyro integration with an arbitrary zero, declared
`YR-G` in every packet. It drifts, and it has been measured drifting **more than a full
revolution in a 36.8-minute stationary log**.

**Telemetry — the link works, at bench range only.** 66 packets end to end at −44 dBm with
zero loss. Nothing has been tested past a bench, and the official sync word `0xA5` has
never been tried.

**Power — the regulator question resolved itself.** Every load runs from the Pico's own
`3V3(OUT)`, which held 3.28–3.29 V through 45 back-to-back transmits. The AMS1117 that
blocked the design for weeks turned out not to be needed. What is left is the battery end:
a switch, a diode and two resistors. The switch and the divider went in before submission; the
diode did not.

---

Related: [documentation/README.md](../documentation/README.md) ·
[electrical/](../electrical/) · [mechanical/](../mechanical/) ·
[firmware/](../firmware/)
