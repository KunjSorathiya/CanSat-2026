# Contributing

How to work on this repository without breaking flight-critical behaviour or the
documentation discipline the project depends on.

---

## Before you change anything

```bash
bash tools/build_host.sh
```

Every suite must pass before you start, so you know a later failure is yours.

---

## The loop

```mermaid
flowchart LR
    A["Read the relevant doc"] --> B["Change the code"]
    B --> C["bash tools/build_host.sh"]
    C --> D{"pass?"}
    D -- no --> B
    D -- yes --> E["bash tools/check_pico_syntax.sh<br/>if you touched src/pico/"]
    E --> F["Update the docs that claim something<br/>about what you changed"]
    F --> G["Commit"]
```

---

## Project layout

```text
firmware/common/            shared telemetry format + SX1278 driver (cansat::)
firmware/flight-computer/   flight core (flight::) + Pico HAL (src/pico/)
firmware/ground-station/    bridge firmware + USB framing (ground::)
ground-station/software/    Python receive pipeline
ground-station/web/         single-file browser console
tools/                      build, syntax-check, SDK stubs
documentation/              all engineering documentation
```

---

## Rules that are not negotiable

These are the invariants the flight software is built around. Changing one is a design
decision that belongs in [CHANGELOG.md](CHANGELOG.md), not a refactor.

1. **The flight core never includes a Pico SDK header.** Hardware reaches it only through
   the six interfaces in `interfaces.hpp`. Anything device-specific belongs under
   `src/pico/`, behind `#ifdef PICO_BUILD`.
2. **Nothing in the flight loop blocks or allocates.** No `new`, no growing containers, no
   unbounded waits, no retry loop that can spin. Recovery is always bounded.
3. **Telemetry never stops.** No peripheral failure and no mission state, `FAULT`
   included, may suppress telemetry that can still be produced correctly.
4. **Wrong data is worse than no data.** If a mandatory field cannot be trusted, suppress
   the packet. Never transmit a plausible-looking wrong value.
5. **Suppression does not consume a packet number.** Numbering must stay strictly
   sequential over transmitted packets, so gaps mean radio loss and nothing else.
6. **Provisional values stay labelled.** Anything the rulebook or the hardware has not
   fixed keeps its `PROVISIONAL` or `TBD` marker. Only the sync words `0xF3` and `0xA5`
   are rulebook-fixed.
7. **The `CAN-Team-XX` placeholder must keep failing validation.** It is what stops the
   vehicle flying with an unset identity.

---

## Things that exist in more than one language

Change one, change all of them, and update both test suites:

| Logic | Lives in |
|---|---|
| Packet format and parsing | `firmware/common/src/telemetry.cpp`, `ground-station/software/src/telemetry.py`, `ground-station/web/index.html` |
| CRC-16/CCITT framing | `firmware/ground-station/src/framing.cpp`, `ground-station/software/src/transport.py`, `ground-station/web/index.html` |
| Validation semantics | `ground-station/software/src/validator.py`, `ground-station/web/index.html` |
| Pin assignment | `firmware/flight-computer/include/flight/config.hpp` (authoritative), `documentation/hardware/pico-gpio-map.md`, `documentation/design/wiring.md` |

The web console has no automated tests. If you change the format or the framing, open it
and check it by hand.

---

## Code style

**C++ (C++17)**

- Two-space indent, 100-column soft limit, `snake_case` for functions and variables,
  `PascalCase` for types, trailing `_` on private members.
- `-Wall -Wextra -Wpedantic` must stay clean.
- Comment *why*, not *what*. Existing comments explain the reasoning behind thresholds and
  failure policy — match that.
- New tunables go in `Configuration`, never as scattered literals.

**Python (3.10+)**

- Standard library only in the core. `pyserial` and `matplotlib` stay optional, and the
  code must run without them.
- `from __future__ import annotations`, type hints on public functions, dataclasses for
  records.
- Docstrings state the module's responsibility and its boundary with other modules.

---

## Testing

Add a test with every behavioural change. Both suites use plain assertions and no
frameworks.

- **C++** — add a `test_*` function in `firmware/flight-computer/tests/flight_tests.cpp`
  and call it from `main()`. Use `CHECK(...)`; the runner prints a pass count.
- **Python** — add a `unittest` case under `ground-station/software/tests/`.

What a good test covers here: the failure path, not just the happy path. Most of this
codebase is about behaving correctly when a sensor, a card or a radio fails.

---

## Documentation

Documentation is part of the change, not a follow-up.

| If you change… | Update… |
|---|---|
| Pin assignment | `config.hpp`, `pico-gpio-map.md`, `wiring.md` |
| Packet format | `telemetry-protocol.md`, both parsers, the web console |
| Mission behaviour or thresholds | `software-architecture.md`, `flight-computer/README.md` |
| Test coverage | `test-plan.md` |
| Operating procedure | `runbook.md` |
| Anything notable | `CHANGELOG.md` |

Follow the [documentation rules](documentation/README.md#documentation-rules): evidence
before claims, contradictions surfaced rather than resolved locally, provisional values
labelled, and code as the truth when a document disagrees with it.

---

## Commits

Present-tense, imperative, and specific about behaviour:

```text
Add sensor plausibility gating to the acquisition path
Fix precision pattern rejecting well-formed packets
Document the shared SPI bus constraints
```

Not `update code`, `fixes`, or `wip`.

---

## Hardware changes

Anything that touches wiring, power or a pin assignment additionally requires:

1. The change reflected in `BoardPins` and in both hardware documents.
2. A note on what was physically verified, and how.
3. Bring-up re-run from the affected step in the
   [bring-up order](documentation/design/wiring.md#bring-up-order).

Never mark a hardware item verified because the code compiles.
