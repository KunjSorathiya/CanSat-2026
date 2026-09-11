# Documentation Index

Every engineering document for CanSat 2026, grouped by what you are trying to do.

---

## Start here

| I want to… | Read |
|---|---|
| **Build one of these from nothing** | **[Quick Start Guide](quick-start.md)** |
| **Record the parts that just arrived** | **[Receiving Inspection Record](hardware/receiving-inspection.md)** |
| **Solder the vehicle board** | **[Assembly Procedure](hardware/assembly-procedure.md)** |
| **Understand what actually happens during a flight** | **[Concept of Operations](mission/concept-of-operations.md)** |
| **Size the parachute** | **[simulations/](../simulations/README.md)** |
| **Build the structure** | **[mechanical/](../mechanical/README.md)** |
| Understand the whole project | [Root README](../README.md) |
| Understand how the software works | [Software Architecture](design/software-architecture.md) |
| Wire the hardware | [Wiring Diagrams](design/wiring.md) |
| Know what the competition requires | [Requirements Checklist](requirements/requirements.md) |
| Know what is built and what is next | [Project Timeline](project/timeline.md) |
| Run the ground station or a launch | [Operations Runbook](operations/runbook.md) |
| Know what is tested | [Test Plan](testing/test-plan.md) |
| Measure the vehicle on hardware day | [Bring-Up Record](testing/bring-up-record.md) |
| Know what one subsystem's state is | [Avionics](../avionics/README.md) · [Electrical](../electrical/README.md) · [Mechanical](../mechanical/README.md) |
| Know where the points are | [Scoring Assessment](project/scoring-assessment.md) |
| Know what is still to buy | [Purchase List](hardware/purchase-list.md) |
| Understand why telemetry runs at 1.43 Hz | [Link Budget](design/link-budget.md) |
| Understand why sensors run at 30 Hz | [Sensor Rates](design/sensor-rates.md) |
| See what changed recently | [Changelog](../CHANGELOG.md) |
| Check the project against its own claims | [Repository Audit](audit/2026-09-04-repository-audit.md) · [Continuous Review](audit/2026-09-05-continuous-review.md) |

---

## Getting started

| Document | Contents |
|---|---|
| [quick-start.md](quick-start.md) | Zero to a working CanSat: PC setup, BOM, ordering, tools, wiring, assembly, SDK, flashing, subsystem bring-up, end-to-end test, fault injection, launch, recovery, troubleshooting, and time estimates for three experience levels |

---

## Requirements

| Document | Contents |
|---|---|
| [requirements.md](requirements/requirements.md) | Every extracted requirement with status and evidence, hardware gap analysis, nine development gates, and the open questions for the organizers |
| `updated CanSat Final Guidelines 2026.pdf` | **The current rulebook.** It resolves the dimension and altitude contradictions the original carried |
| `CanSat Final Guidelines.PDF` | The originally supplied rulebook, kept because several documents record what it contradicted |

## Mission

| Document | Contents |
|---|---|
| [concept-of-operations.md](mission/concept-of-operations.md) | The mission from power-on to recovery: the profile, phase by phase, the data budget, what is autonomous, failure behaviour, and what is still unknown. **It is also where [F-20](testing/bring-up-record.md#findings) was found** |

**Launch logs, flight records and post-flight reports belong in this directory too.** There
are none yet, because nothing has flown.

---

## Design

| Document | Contents |
|---|---|
| [software-architecture.md](design/software-architecture.md) | Layer model, module map, flight-loop and pipeline flowcharts, fault model, timing budget, design rules |
| [telemetry-protocol.md](design/telemetry-protocol.md) | Wire format, field precision, validation policy, radio configuration, ground-station contract |
| [wiring.md](design/wiring.md) | Signal wiring for both Picos, pin table, bus-sharing rules, power tree, bring-up order |
| [electrical-architecture.md](design/electrical-architecture.md) | Power topology, regulation analysis, grounding, decoupling, power budget, electrical risks |
| [link-budget.md](design/link-budget.md) | LoRa airtime arithmetic, the spreading-factor and packet-rate decision, range margin, and the guards that enforce them |
| [max-rate-command.md](design/max-rate-command.md) | **Approved 2026-09-11, implementation in progress.** Telemetry cadence: GPS and sound on the air in every packet of normal flight, and the one ground command that takes the vehicle to 3.17 Hz with the sensors at 1.06 Hz and closes the uplink behind it. The two packet shapes, the slot arithmetic, what latches, and the open bench fallback |
| [sensor-rates.md](design/sensor-rates.md) | The 30 Hz acquisition loop: barometer conversion time, IMU anti-aliasing, bus budget, and why over-sampling corrupts vertical speed |

## Hardware

| Document | Contents |
|---|---|
| [hardware/README.md](hardware/README.md) | Hardware documentation policy and source hierarchy |
| [hardware.md](hardware/hardware.md) | The single hardware database: confirmed BOM, specifications, interfaces, open items |
| [receiving-inspection.md](hardware/receiving-inspection.md) | Unpowered inspection of the delivered parts: inventory, photographs, per-board identification, the four questions blocking the electrical design |
| [assembly-procedure.md](hardware/assembly-procedure.md) | The pre-solder checks, the five decisions they force, the floorplan, and the sixteen gated build steps |
| [electrical-compatibility.md](hardware/electrical-compatibility.md) | Per-component compatibility assessment and what must be physically verified |
| [pico-gpio-map.md](hardware/pico-gpio-map.md) | GPIO reservation and preliminary assignment |
| [pico-resource-map.md](hardware/pico-resource-map.md) | Peripheral-level resource analysis: I2C, SPI, UART, ADC, interrupts |
| [sd-module-analysis.md](hardware/sd-module-analysis.md) | Deep analysis of the microSD reader — the highest-risk integration item |
| [pre-procurement-design-status.md](hardware/pre-procurement-design-status.md) | Verified facts, provisional decisions, post-procurement verification plan, design-freeze criteria |
| [purchase-list.md](hardware/purchase-list.md) | Everything still to buy, costed, each line traced to a requirement, a finding or a scoring recommendation |
| [product-pages/README.md](hardware/product-pages/README.md) | Exact supplier SKUs, product pages, datasheet status |
| [photos/README.md](hardware/photos/README.md) | The photographic record of every delivered board |
| `datasheets/` | Manufacturer PDFs stored locally |
| `diagrams/` | Generated SVGs: board layout to scale, wiring schedule, power path |

## Analysis and simulation

| Document | Contents |
|---|---|
| [simulations/README.md](../simulations/README.md) | The descent model: canopy sizing, descent time, telemetry yield, and the three findings that came out of it |
| [`simulations/descent.py`](../simulations/descent.py) | The model itself, run by the host test suite and pinned to closed-form limits |
| [`tools/link_budget.py`](../tools/link_budget.py) | LoRa time-on-air, pinned to published Semtech reference vectors |

## Subsystem summaries

Short, current status pages that live next to the thing they describe. They quote the
documents above rather than competing with them.

| Path | Contents |
|---|---|
| [avionics/README.md](../avionics/README.md) | The three onboard subsystems at a glance |
| [avionics/sensors/](../avionics/sensors/README.md) | Every sensor, what it measured, and why the IMU is the wrong part |
| [avionics/telemetry/](../avionics/telemetry/README.md) | The link, the packet, the log, and why the period is 700 ms |
| [avionics/power/](../avionics/power/README.md) | One rail, and the four parts not yet on it |
| [electrical/README.md](../electrical/README.md) | The electrical design and its authority hierarchy |
| [electrical/schematics/](../electrical/schematics/README.md) | The generated netlist, and how to run a continuity check from it |
| [electrical/PCB/](../electrical/PCB/README.md) | Why the vehicle is on perfboard, and what a fabricated board would need |
| [mechanical/README.md](../mechanical/README.md) | Envelope, the board-fit constraint, canopy sizing, mass budget |
| [mechanical/drawings/](../mechanical/drawings/README.md) | Dimensioned drawings, generated where they can be |
| [mechanical/CAD/](../mechanical/CAD/README.md) | What goes there, and the constraints a model must satisfy |

## Project management

| Document | Contents |
|---|---|
| [timeline.md](project/timeline.md) | History, phase plan, gate status, critical path, blocked work, risk register |
| [scoring-assessment.md](project/scoring-assessment.md) | Where the project stands against the 200-point rulebook, section by section, and the cheapest points left |
| [../CHANGELOG.md](../CHANGELOG.md) | What changed, when, and why |

## Testing

| Document | Contents |
|---|---|
| [test-plan.md](testing/test-plan.md) | Automated coverage, per-suite descriptions, hardware and mission test plans |
| [bring-up-record.md](testing/bring-up-record.md) | Every prediction this repository makes, paired with what was measured against it, across nine gates — and twenty findings |

## Operations

| Document | Contents |
|---|---|
| [runbook.md](operations/runbook.md) | Configuration, builds, ground-station operation, launch-day checklist, troubleshooting, analysis |

## Audit

| Document | Contents |
|---|---|
| [2026-09-04-repository-audit.md](audit/2026-09-04-repository-audit.md) | File-by-file verification of code, documentation and claims |
| [2026-09-05-continuous-review.md](audit/2026-09-05-continuous-review.md) | Seventeen findings from one continuous pass: what the repository claimed, and whether each claim was still true |

---

## Component READMEs

Documentation that lives next to the code it describes:

| Path | Contents |
|---|---|
| [firmware/flight-computer/README.md](../firmware/flight-computer/README.md) | Flight-core layout, build instructions, calibration and safeguards |
| [firmware/ground-station/README.md](../firmware/ground-station/README.md) | Bridge firmware and the framing contract |
| [ground-station/software/README.md](../ground-station/software/README.md) | Python module responsibilities and CLI usage |
| [ground-station/web/README.md](../ground-station/web/README.md) | Web console sources and features |
| [tools/pico_sdk_stubs/README.md](../tools/pico_sdk_stubs/README.md) | What the SDK stubs are, and what they are not |
| [test-data/README.md](../test-data/README.md) | The fixture files the C++, Python and JavaScript parsers are all held to |

---

## Generated artifacts

Nothing in this list should ever be hand-edited. Each is produced by a script, and the
script is what to change.

| Artifact | Generator |
|---|---|
| [`electrical/schematics/vehicle-netlist.tsv`](../electrical/schematics/vehicle-netlist.tsv) | [`tools/gen_netlist.py`](../tools/gen_netlist.py) — refuses to run if it disagrees with `flight::BoardPins` |
| [`hardware/diagrams/board-layout-to-scale.svg`](hardware/diagrams/board-layout-to-scale.svg) | [`tools/gen_board_layout.py`](../tools/gen_board_layout.py) |
| [`hardware/diagrams/wiring-schedule.svg`](hardware/diagrams/wiring-schedule.svg) | [`tools/gen_wiring_schedule.py`](../tools/gen_wiring_schedule.py) |
| [`mechanical/drawings/envelope-and-board-fit.svg`](../mechanical/drawings/envelope-and-board-fit.svg) | [`tools/gen_envelope_drawing.py`](../tools/gen_envelope_drawing.py) |

---

## Documentation rules

These rules apply to every document in this directory. They are the reason the
documentation is useful rather than optimistic.

1. **Evidence before claims.** A requirement is `Complete` only when this repository holds
   evidence. Owning a component is not integration; compiling code is not verification.
2. **Contradictions are surfaced, never resolved locally.** Where the rulebook conflicts
   with itself, both readings are recorded and the question is escalated. No value is
   invented to make a table look finished.
3. **Provisional values are labelled.** Anything the rulebook or the hardware has not fixed
   is marked `PROVISIONAL` or `TBD`, in documents and in code alike.
4. **Chip documentation is not board documentation.** A datasheet for the sensor die says
   nothing about the breakout's regulator, level shifters or pull-ups.
5. **Documentation and code must agree.** Pin maps, timing values and thresholds appear in
   both; when they diverge, the code is the truth and the document is the bug.
6. **A number a document states should be checked by something.**
   [`tools/check_doc_claims.py`](../tools/check_doc_claims.py) holds 233 documented claims to
   the source that defines them, and it runs in the same build as the tests. When a document
   states a figure that matters, add a check for it rather than trusting the next reader to
   notice.
7. **Where a figure can be generated, generate it.** A drawing whose dimensions were typed by
   hand is correct on the day it is drawn. See [Generated artifacts](#generated-artifacts).
