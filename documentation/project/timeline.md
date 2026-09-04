# Project Timeline

Where the project has been, where it stands today, and what has to happen next.

**Status date: 2026-09-04.**

> [!NOTE]
> No competition deadline appears in the supplied rulebook text, so the forward plan is
> written in **phases and gates, not calendar dates**. Deadlines are open question 9 in
> [requirements.md](../requirements/requirements.md#open-questions-for-organizers) and must
> be confirmed with the organizers before this page can carry real dates.

---

## Contents

- [At a glance](#at-a-glance)
- [What has happened](#what-has-happened)
- [Phase plan](#phase-plan)
- [Development gates](#development-gates)
- [Critical path](#critical-path)
- [Blocked work](#blocked-work)
- [Risk register](#risk-register)

---

## At a glance

| Phase | Scope | Status |
|---|---|---|
| 0 · Project setup | Repository, structure, rulebook capture | ✅ Complete |
| 1 · Requirements | Requirement extraction, gap analysis, gates, organizer questions | ✅ Complete |
| 2 · Hardware study | BOM identification, datasheets, compatibility and resource analysis | ✅ Complete |
| 3 · Software | Flight core, telemetry protocol, ground station, web console, test suites | ✅ Complete on host |
| 4 · Documentation | Architecture, wiring, timeline, test plan, runbook, audit | ✅ Complete |
| 5 · Procurement + bring-up | Verify boards, resolve power, bench each subsystem | ⬜ Not started — **blocking** |
| 6 · Electrical build | Regulator, switch, LED, divider, PCB, harness | ⬜ Not started |
| 7 · Mechanical build | Structure, egg chamber, parachute, recovery | ⬜ Not started |
| 8 · Integration + flight test | Full-system, drop and range testing | ⬜ Not started |
| 9 · Competition | Launch, analysis, reports, media | ⬜ Not started |

Phases 0–4 are complete. **Every remaining phase depends on hardware verification that has
not begun.**

---

## What has happened

All development so far is recorded in the repository history.

```mermaid
timeline
    title Development to date (2026-09-03 to 2026-09-04)
    Repository setup : Project directory structure : Hardware overview drafted
    Requirements : Rulebook captured : 30 requirements extracted : 9 development gates defined : 10 organizer questions raised
    Hardware analysis : BOM identified against Robu SKUs : Pico and BMP280 datasheets stored : Electrical compatibility assessed : GPIO and resource maps drafted : AMS1117-3.3 rejected for direct regulation : microSD supply flagged as blocking
    Software : Telemetry protocol specified : Flight core implemented : SX1278 driver written : Pico HAL written : Ground bridge and framing built : Python ground pipeline built : Web console built : Host test suites written
    Documentation and hardening : regex removed from the shared library : CI workflow added : Architecture, wiring, timeline, test plan, runbook written : Full repository audit
```

### Commit history

| Commit | Date | Content |
|---|---|---|
| `c6c500b` | 2026-09-03 | Initial project structure |
| `e71706c` | 2026-09-03 | Hardware project overview |
| `a915247` | 2026-09-03 | Competition requirements and rulebook |
| `27495df` | 2026-09-03 | Initial CanSat software and engineering documentation |
| *(working tree)* | 2026-09-04 | Flight core, ground station, web console, tests, full documentation set — see [CHANGELOG.md](../../CHANGELOG.md) |

### What exists now

| Area | Delivered | Evidence |
|---|---|---|
| Telemetry protocol | Rulebook format, strict parser, precision rules, optional fields | [telemetry-protocol.md](../design/telemetry-protocol.md) |
| Flight core | Controller, state machine, scheduler, orientation, calibration, faults, builder, block log, NMEA parser, link profile, airtime and sensor-rate guards | 32 C++ suites, 464 assertions |
| Sensor drivers | MPU6050, BMP280, NEO-6M, microSD, SX1278 | Compile-checked against SDK stubs |
| Ground bridge | Continuous RX, CRC framing, status lines, watchdog | Framing unit-tested |
| Ground software | Transport, parser, validator, health, logger, orchestrator, Tk dashboard, CLI | 42 Python tests |
| Web console | Framing, parser, validator and link health extracted from `index.html` and run under Node | 30 Node tests |
| Tooling | LoRa time-on-air calculator used for the packet-rate decision | 33 Python tests |
| Web console | Single-file console with demo, file replay and Web Serial | Manual use |
| Documentation | Requirements, hardware, electrical, protocol, architecture, wiring, testing, operations | This directory |

---

## Phase plan

```mermaid
gantt
    title Forward plan — sequence and dependencies, not calendar dates
    dateFormat X
    axisFormat Phase %s

    section Blocking
    Procurement verification        :crit, p5a, 0, 2
    Power architecture resolved     :crit, p5b, after p5a, 2
    Subsystem bench bring-up        :crit, p5c, after p5b, 3

    section Electrical
    Switch, LED, divider            :p6a, after p5c, 2
    PCB layout and assembly         :p6b, after p6a, 3
    Harness and cable management    :p6c, after p6b, 1

    section Mechanical
    Structure and layout            :p7a, after p5a, 3
    Egg chamber                     :p7b, after p7a, 2
    Parachute and deployment        :p7c, after p7a, 3
    Drop testing                    :p7d, after p7c, 2

    section Integration
    Full-system integration         :p8a, after p6c, 2
    Range and link testing          :p8b, after p8a, 2
    Descent-rate verification       :p8c, after p7d, 2
    Mission rehearsal               :p8d, after p8b, 2

    section Competition
    Preliminary report              :p9a, after p8a, 2
    Launch                          :milestone, p9b, after p8d, 0
    Analysis within 4 hours         :p9c, after p9b, 1
    Final report and media          :p9d, after p9c, 2
```

### Phase 5 — Procurement verification and bring-up *(blocking everything else)*

- Photograph and identify every purchased breakout; record the exact variant
- Confirm supply voltage, logic levels, regulators, level shifters, pull-ups and pinouts
- Resolve the microSD reader supply (its listing states 4.5–5.5 V input)
- Verify the antenna and IPEX cable connector genders
- Follow the [bring-up order](../design/wiring.md#bring-up-order), one subsystem at a time

### Phase 6 — Electrical build

- Select the peripheral regulator with a measured load budget
- Add the manual ON/OFF switch and the immediate power LED (both mandatory, neither in the BOM)
- Design, build and measure the battery divider, then set `battery_divider_ratio`
- Lay out the PCB; the rulebook awards points for original design, routing and assembly quality

### Phase 7 — Mechanical build

- Structure sized to the resolved dimension limit (currently contradictory — organizer question 1)
- Cushioned, secure egg chamber
- Parachute and deployment for a descent rate of no more than 5 m/s
- Drop testing for egg survival and structural integrity

### Phase 8 — Integration and flight test

- Full-system integration on battery power
- Range and link testing at the launch configuration
- Descent-rate measurement
- End-to-end rehearsal following the [runbook](../operations/runbook.md)

### Phase 9 — Competition

- Preliminary report (excludes analysis), final report (includes it)
- Launch, then analysis inside the four-hour window
- Required photographs, video, and social-media posts tagging Physics Club, SVNIT

---

## Development gates

The nine gates are defined in
[requirements.md](../requirements/requirements.md#development-gates). Current position:

```mermaid
flowchart LR
    G1["Gate 1<br/>Requirements locked"] --> G2["Gate 2<br/>Electrical architecture"]
    G2 --> G3["Gate 3<br/>Power tested"]
    G3 --> G4["Gate 4<br/>Sensors verified"]
    G4 --> G5["Gate 5<br/>Telemetry verified"]
    G5 --> G6["Gate 6<br/>Ground station verified"]
    G6 --> G7["Gate 7<br/>Mechanical + recovery"]
    G7 --> G8["Gate 8<br/>Full integration"]
    G8 --> G9["Gate 9<br/>Competition ready"]

    classDef done fill:#1b5e20,stroke:#1b5e20,color:#fff
    classDef partial fill:#e65100,stroke:#e65100,color:#fff
    classDef todo fill:#37474f,stroke:#37474f,color:#fff
    class G1 partial
    class G2,G3,G4,G5,G6,G7,G8,G9 todo
```

| Gate | Status | What is missing |
|---|---|---|
| 1 · Requirements locked | 🟠 Partial | Requirements extracted and gates defined, but ten organizer questions are unanswered — including the dimension and altitude contradictions |
| 2 · Electrical architecture approved | ⬜ Not passed | No regulator selected; module supplies unresolved |
| 3 · Power system tested | ⬜ Not passed | No switch, no LED, no divider, no measurement |
| 4 · Sensors individually verified | ⬜ Not passed | Drivers written and compile-checked; no hardware reads |
| 5 · Telemetry verified | ⬜ Not passed | Format and rate verified in software; no radio link tested |
| 6 · Ground station verified | ⬜ Not passed | Pipeline tested against files and loopback; never against a real bridge |
| 7 · Mechanical and recovery verified | ⬜ Not passed | Nothing built |
| 8 · Full system integration | ⬜ Not passed | Blocked by gates 2–7 |
| 9 · Competition readiness | ⬜ Not passed | Blocked by gate 8 |

---

## Critical path

```mermaid
flowchart TD
    A["Identify exact breakout variants"] --> B["Resolve the microSD supply"]
    A --> C["Select the peripheral regulator"]
    B --> C
    C --> D["Build and measure the power system"]
    D --> E["Bench-verify each sensor"]
    E --> F["Verify the radio link end to end"]
    F --> G["Full integration on battery power"]
    H["Resolve the dimension contradiction"] --> I["Freeze the mechanical design"]
    I --> J["Build the structure and egg chamber"]
    J --> K["Parachute and drop testing"]
    K --> G
    G --> L["Mission rehearsal"] --> M["Launch"]

    classDef blocker fill:#b71c1c,stroke:#b71c1c,color:#fff
    class A,C,H blocker
```

Three items gate everything: **exact board identification**, **regulator selection**, and
**the organizers' answer on dimensions**. Nothing downstream can be frozen until they
resolve.

---

## Blocked work

| Blocked | Blocked by | Owner |
|---|---|---|
| Mechanical design freeze | Contradictory dimension limits (organizer questions 1 and 4) | Organizers |
| Descent-system sizing | Launch altitude contradiction, 100 ft vs 150 ft (question 3) | Organizers |
| Yaw compliance claim | No definition of valid yaw data; the vehicle has no magnetometer (question 6) | Organizers |
| Radio parameter freeze | Only the sync words are prescribed (question 7) | Organizers |
| Peripheral rail design | Exact breakout documentation | Team |
| microSD integration | Reader supply and MISO tri-state behaviour | Team |
| Battery-life estimate | Regulator selection plus a measured load | Team |
| Report and media schedule | No deadlines in the supplied text (question 9) | Organizers |

---

## Risk register

| Risk | Impact | Current mitigation |
|---|---|---|
| microSD reader is incompatible with the available rails | Loses onboard logging | Flagged as the top hardware risk; SD failure already degrades gracefully in firmware |
| No regulator selected | Blocks the whole power build | AMS1117-3.3 assessed and rejected with reasoning recorded; replacement still open |
| Yaw cannot be produced credibly without a magnetometer | Mandatory field may be judged non-compliant | Yaw is implemented and documented as a *relative* gyro-integrated angle, never claimed as magnetic heading |
| Antenna connector gender mismatch | Cannot connect the RF chain | Flagged for physical verification before assembly |
| Dimension contradiction unresolved | Mechanical rework, or disqualification on size | No value invented locally; escalated to the organizers |
| Radio link untested at range | Telemetry loss during flight | Link testing is a named gate; firmware already recovers from radio failure with bounded back-off |
| Hardware bring-up not started | Compresses every later phase | Bring-up is documented and sequenced so it can start the moment parts are verified |

---

Related: [requirements.md](../requirements/requirements.md) ·
[test-plan.md](../testing/test-plan.md) · [runbook.md](../operations/runbook.md) ·
[CHANGELOG.md](../../CHANGELOG.md)
