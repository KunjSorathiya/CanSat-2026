# Max-rate command — design

**Status: proposed, 2026-09-10. Nothing in this document is implemented.**

A ground-commanded, one-way switch that takes the vehicle from 1.43 Hz to 3.57 Hz and
closes the uplink behind itself. It exists because the 2026 revision scores packet rates
above 1 Hz, and because a vehicle that can be commanded is a vehicle that can be commanded
by accident — so the same button that buys the rate also removes the ability to send
another one.

---

## Contents

- [What the command is](#what-the-command-is)
- [What maximum is](#what-maximum-is)
- [What latches](#what-latches)
- [What it refuses](#what-it-refuses)
- [How the operator knows it worked](#how-the-operator-knows-it-worked)
- [Consequences accepted](#consequences-accepted)
- [Testing](#testing)
- [Decisions recorded](#decisions-recorded)
- [Out of scope](#out-of-scope)

---

## What the command is

A second `CommandKind` alongside `erase_log`, in the same envelope, with the same token
scheme and the same replay rules
([command.hpp](../../firmware/common/include/cansat/command.hpp)):

```text
CAN-Team-25; CMD-MAX_RATE; PN-1234; KEY-3f9a1c04b7e25d68;
```

**It carries no rate value, and that is the point.** The vehicle computes its own maximum
from constants it was built with. A corrupted frame, another team's traffic or a forged
token cannot name a period; the worst a valid-looking frame can do is trigger the one
transition the operator already intended. A command that carried a number would need range
checking, a refusal path, and a way to report which number was applied — all of it new
surface on an unauthenticated link, to express something the vehicle already knows.

`test-data/command-tokens.tsv` gains rows for the new kind so the C++ and JavaScript
implementations still cannot drift apart unnoticed.

The acceptance window is unchanged: **READY, `ARM-0`, on the ground, and only on a build
with `allow_ground_commands` set.** The replay rules are unchanged: a packet number already
accepted, not yet reached, or older than `command_replay_window` is refused.

## What maximum is

|  | Normal | Max mode |
|---|---:|---:|
| Worst-case packet | 199 B | **145 B** |
| Airtime, model | 317.70 ms | **235.78 ms** |
| Airtime, +1.8 % hardware correction | 323.41 ms | **240.02 ms** |
| Period | 700 ms | **280 ms** |
| Rate | 1.43 Hz | **3.57 Hz** |
| Duty | 46 % | **86 %** |

Two things produce the gain, and only one of them is the duty policy.

**The packet gets smaller.** Max mode stops appending the `MODE`, `FAULTS`, `CAL`, `ARM`
and `YR` tags — the same fields the oversize path already sheds first, on the grounds that
they are project-local diagnostics rather than rulebook data. They continue to reach the SD
log. The runtime cap (`Configuration::worst_case_packet_bytes`) drops to match, so the
shedding logic and the budget stay describing the same packet.

**The gap gets smaller.** 280 ms is `airtime + 40 ms`, not `airtime / duty`. The 40 ms is
the vehicle's own work between transmits: [F-11](../testing/bring-up-record.md#findings)
measured the SD block write at 2.7 ms mean and **30 ms worst case, in two of five
sessions**, and the sensor loop and the watchdog feed have to fit alongside it. At 86 % duty
the duty-derived period would be 279.1 ms and the guard-derived one 280.0 ms, so **the guard
is what binds** — which is the right way round. A period shorter than the guard does not
fail loudly; it makes packets late by however long the card stalls, and that arrives at the
ground station as jitter rather than as an error.

**The 145-byte figure must be defended, not asserted.** It is the repository's existing
arithmetic — the 199 B worst case minus 54 B of worst-case tags — and the implementation
adds a test that constructs the widest mandatory-only packet and checks it against the
constant, the way the 199 is defended today. If that measurement disagrees, the constant
moves and the period constant moves with it. **This document fixes the derivation, not the
number.**

Every figure above sits in `link_profile.hpp` behind `static_assert`s: that the max-mode
period exceeds the max-mode airtime plus the guard, and that the max-mode packet fits the
FIFO. A future change to the packet format that invalidates them fails the build rather
than the flight.

## What latches

One accepted command sets three things at once, and none of them can be undone:

1. **The period** — `PeriodicTask::set_period(kMaxRatePeriodMs)` on the telemetry task.
2. **The packet** — diagnostic tags suppressed, runtime cap lowered to the max-mode budget.
3. **The uplink** — `service_ground_commands()` returns immediately from then on. The
   vehicle never enters RX again, so no further command can be received, including another
   `MAX_RATE`.

**The latch lives in RAM.** A power cycle or a watchdog reset returns the vehicle to
1.43 Hz with the uplink open, on a build that had it enabled. That is a deliberate choice
and not a limitation to be worked around later: persisting it would mean writing flight
configuration to the card, and a card that arrives at the pad already carrying "no uplink,
max rate" from a bench session is a worse failure than re-sending a command.

## What it refuses

- **`transmit_gps` true.** That packet is 255 B and 406.81 ms of measured airtime, which is
  *longer than the 280 ms period*. Compile-time `static_assert` on the pair, plus a runtime
  refusal, because the two settings are configured in different places and a build that
  combines them must not fly.
- **Armed, or not in READY.** The same gate as `erase_log`, unchanged.
- **A replayed, future or stale packet number.** The same rules, unchanged.
- **A second command after the latch.** Not refused — *not heard*. The radio is no longer
  in RX.

## How the operator knows it worked

There is no acknowledgement packet, because the vehicle stops listening in the same breath
as it answers. The evidence is the telemetry itself, and it is unambiguous within one
period: the rate at the ground station goes from 1.43 to 3.57 Hz, and the `MODE`, `FAULTS`,
`CAL`, `ARM` and `YR` tags stop appearing in the packets.

`health_.ground_commands_accepted` still increments, and the SD log still records it.

## Consequences accepted

| | Effect |
|---|---|
| **Log capacity** | ~30 hours falls to **~12 hours**. Irrelevant to a flight, relevant to a long bench session. |
| **Average current** | The radio averages 41 mA today and **75 mA** at 86 % duty; the steady-state total moves ~130 → ~165 mA. **Peaks do not change** — they are set by coincident TX, SD write and GPS acquisition, not by rate, and the 300 mA analysis in [electrical-architecture.md](electrical-architecture.md) stands unaltered. |
| **Channel occupancy** | The vehicle transmits ~86 % of the time. Acceptable inside a reserved launch slot, antisocial during shared bench testing on `0xF3`, and a regulatory question this repository does not answer. |
| **Scoring** | If the rulebook's rate points cap below 3.57 Hz, the extra rate is spent for nothing. **Check the scoring table before flying this.** |
| **Diagnosis** | `MODE`/`FAULTS`/`CAL`/`ARM` leave the air. A fault during a max-rate flight is visible in the log after recovery, not live. |

## Testing

**Host, C++:** the accepted transition sets all three latches; the tags stop; the period
changes; the uplink is closed afterwards and a second command has no effect; the command is
refused when armed, when not in READY, and on replayed, future and stale packet numbers;
the `transmit_gps` combination is refused; the widest mandatory-only packet matches the
budget constant.

**Host, Node:** the console mints a `MAX_RATE` token the firmware's fixture agrees with, and
never transmits the password.

**Fixtures:** `test-data/command-tokens.tsv` rows for the new kind, read by both suites.

**Documented claims:** the rate, period and duty figures enter `check_doc_claims.py` the
same way the 1.43 Hz figures did — derived from the shipped constants rather than quoted.

**Bench, on hardware:** the only test that matters. Send it in READY with `ARM-0`, watch the
station's rate move to 3.57 Hz, confirm the tags stop, confirm a second press does nothing,
power-cycle and confirm the vehicle comes back at 1.43 Hz with the uplink open. Record it in
the bring-up record as a new row under Gate 8.

## Decisions recorded

- **Ground-only window, not in flight.** Keeps the property that the vehicle never listens
  once armed, which the README and the audit both rest on. A rate command is useful before
  launch; a vehicle that listens in flight is a different project.
- **86 % duty, not 50 %.** The launch slot is reserved, so channel courtesy is not the
  binding constraint there. The bench is a different matter — see the table above.
- **RAM latch, not persisted.** See [What latches](#what-latches).
- **Tag shedding rather than a bandwidth change.** 250 kHz would halve airtime, but the
  bridge's modem parameters are compiled in: the vehicle would switch, the ground station
  would go deaf, and the uplink that could have undone it is closed by the same command.
- **No rate value on the wire.** See [What the command is](#what-the-command-is).

## Out of scope

Persisting the latch across a reset. Commanding in flight. Changing bandwidth or spreading
factor. Any command that lowers the rate again. Raising the duty cap for normal flight —
normal flight stays at 700 ms and 46 %.
