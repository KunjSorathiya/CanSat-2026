# Documentation Index

Every engineering document for CanSat 2026, grouped by what you are trying to do.

---

## Start here

| I want to… | Read |
|---|---|
| **Build one of these from nothing** | **[Quick Start Guide](quick-start.md)** |
| **Record the parts that just arrived** | **[Receiving Inspection Record](hardware/receiving-inspection.md)** |
| Understand the whole project | [Root README](../README.md) |
| Understand how the software works | [Software Architecture](design/software-architecture.md) |
| Wire the hardware | [Wiring Diagrams](design/wiring.md) |
| Know what the competition requires | [Requirements Checklist](requirements/requirements.md) |
| Know what is built and what is next | [Project Timeline](project/timeline.md) |
| Run the ground station or a launch | [Operations Runbook](operations/runbook.md) |
| Know what is tested | [Test Plan](testing/test-plan.md) |
| Measure the vehicle on hardware day | [Bring-Up Record](testing/bring-up-record.md) |
| Understand why telemetry runs at 1 Hz | [Link Budget](design/link-budget.md) |
| Understand why sensors run at 30 Hz | [Sensor Rates](design/sensor-rates.md) |
| See what changed recently | [Changelog](../CHANGELOG.md) |
| Check the project against its own claims | [Repository Audit](audit/2026-09-04-repository-audit.md) |

---

## Getting started

| Document | Contents |
|---|---|
| [quick-start.md](quick-start.md) | Zero to a working CanSat: PC setup, BOM, ordering, tools, wiring, assembly, SDK, flashing, subsystem bring-up, end-to-end test, fault injection, launch, recovery, troubleshooting, and time estimates for three experience levels |

---

## Requirements

| Document | Contents |
|---|---|
| [requirements.md](requirements/requirements.md) | 30 extracted requirements with status, hardware gap analysis, nine development gates, ten open questions for the organizers |
| `CanSat Final Guidelines.PDF` | The supplied official rulebook |

---

## Design

| Document | Contents |
|---|---|
| [software-architecture.md](design/software-architecture.md) | Layer model, module map, flight-loop and pipeline flowcharts, fault model, timing budget, design rules |
| [telemetry-protocol.md](design/telemetry-protocol.md) | Wire format, field precision, validation policy, radio configuration, ground-station contract |
| [wiring.md](design/wiring.md) | Signal wiring for both Picos, pin table, bus-sharing rules, power tree, bring-up order |
| [electrical-architecture.md](design/electrical-architecture.md) | Power topology, regulation analysis, grounding, decoupling, power budget, electrical risks |
| [link-budget.md](design/link-budget.md) | LoRa airtime arithmetic, the spreading-factor and packet-rate decision, range margin, and the guards that enforce them |
| [sensor-rates.md](design/sensor-rates.md) | The 30 Hz acquisition loop: barometer conversion time, IMU anti-aliasing, bus budget, and why over-sampling corrupts vertical speed |

## Hardware

| Document | Contents |
|---|---|
| [hardware/README.md](hardware/README.md) | Hardware documentation policy and source hierarchy |
| [hardware.md](hardware/hardware.md) | The single hardware database: confirmed BOM, specifications, interfaces, open items |
| [receiving-inspection.md](hardware/receiving-inspection.md) | Unpowered inspection of the delivered parts: inventory, photographs, per-board identification, the four questions blocking the electrical design |
| [electrical-compatibility.md](hardware/electrical-compatibility.md) | Per-component compatibility assessment and what must be physically verified |
| [pico-gpio-map.md](hardware/pico-gpio-map.md) | GPIO reservation and preliminary assignment |
| [pico-resource-map.md](hardware/pico-resource-map.md) | Peripheral-level resource analysis: I2C, SPI, UART, ADC, interrupts |
| [sd-module-analysis.md](hardware/sd-module-analysis.md) | Deep analysis of the microSD reader — the highest-risk integration item |
| [pre-procurement-design-status.md](hardware/pre-procurement-design-status.md) | Verified facts, provisional decisions, post-procurement verification plan, design-freeze criteria |
| [product-pages/README.md](hardware/product-pages/README.md) | Exact supplier SKUs, product pages, datasheet status |
| `datasheets/` | Manufacturer PDFs stored locally |

## Project management

| Document | Contents |
|---|---|
| [timeline.md](project/timeline.md) | History, phase plan, gate status, critical path, blocked work, risk register |
| [../CHANGELOG.md](../CHANGELOG.md) | What changed, when, and why |

## Testing

| Document | Contents |
|---|---|
| [test-plan.md](testing/test-plan.md) | Automated coverage, per-suite descriptions, hardware and mission test plans |

## Operations

| Document | Contents |
|---|---|
| [runbook.md](operations/runbook.md) | Configuration, builds, ground-station operation, launch-day checklist, troubleshooting, analysis |

## Audit

| Document | Contents |
|---|---|
| [2026-09-04-repository-audit.md](audit/2026-09-04-repository-audit.md) | File-by-file verification of code, documentation and claims |

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
