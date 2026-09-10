# Max-rate commands — design

**Status: proposed, 2026-09-10. Nothing in this document is implemented.**

Two ground commands, each a one-way switch that takes the vehicle to a faster packet rate
and closes the uplink behind itself. They exist because the rulebook scores packet rate
with no ceiling, and because a vehicle that can be commanded is a vehicle that can be
commanded by accident — so the same button that buys the rate also removes the ability to
send another one.

**What the rulebook actually says**, since every number here is chosen against it:

> **Transmission Capability & Reliability (5 Points):** "Higher packet rates will be
> rewarded with more points, provided transmissions remain consistent. Packet loss will
> reduce the score — teams must balance high transmission frequency with minimal loss. All
> performance will be measured using the Physics Club's official dual ground stations."

Rate and loss are one scored line, measured on receivers that are not ours. That is the
whole reason the period below is derived from a guard rather than from a duty target.

---

## Contents

- [The two commands](#the-two-commands)
- [The token must cover the command](#the-token-must-cover-the-command)
- [What maximum is](#what-maximum-is)
- [What latches](#what-latches)
- [What it refuses](#what-it-refuses)
- [How the operator knows it worked](#how-the-operator-knows-it-worked)
- [Consequences accepted](#consequences-accepted)
- [Testing](#testing)
- [Decisions recorded](#decisions-recorded)
- [Out of scope](#out-of-scope)

---

## The two commands

Two new `CommandKind` values alongside `erase_log`, in the same envelope, with the same
replay rules ([command.hpp](../../firmware/common/include/cansat/command.hpp)):

```text
CAN-Team-25; CMD-MAX_RATE_GPS;  PN-1234; KEY-3f9a1c04b7e25d68;
CAN-Team-25; CMD-MAX_RATE_LEAN; PN-1234; KEY-9c1d77a20e4b8fa3;
```

Both appear on the web console as separate buttons, each labelled with the rate it produces
and each stating that it is irreversible. The operator chooses at the moment of sending,
not at build time, because which one is right depends on what the flight is for: position
on the air for a recovery-critical flight, raw rate for a scoring run.

**Neither carries a rate value.** The vehicle computes both periods from constants it was
built with. A corrupted frame cannot name a period; the worst a valid-looking frame can do
is trigger one of the two transitions the operator already had buttons for. A command that
carried a number would need range checking, a refusal path, and a way to report which
number was applied — new surface on an unauthenticated link, to express something the
vehicle already knows.

The acceptance window is unchanged: **READY, `ARM-0`, on the ground, and only on a build
with `allow_ground_commands` set.**

## The token must cover the command

`command_token()` is currently FNV-1a over `password + "|" + packet_number`, and
`parse_command()` reads the `CMD-` text separately. **So any valid token authorises any
known command at that packet number** — a captured `CMD-ERASE_LOG` frame becomes a valid
`CMD-MAX_RATE_LEAN` frame by editing four words, with the key untouched.

With one command that was invisible. With three, one of which erases the log and two of
which are irreversible, it is not acceptable. The digest becomes:

```text
FNV-1a( password + "|" + command + "|" + packet_number )
```

so each token authorises exactly one action at exactly one packet number. This changes the
existing erase token too, and `test-data/command-tokens.tsv` is regenerated — the fixture
both language implementations read, so neither can drift. **The fixture keeps at least one
row per command kind and the existing non-ASCII password row**, which is what catches a
JavaScript implementation hashing UTF-16 code units instead of UTF-8 bytes.

This is still not cryptography, and the header must keep saying so.

## What maximum is

| | Normal | `MAX_RATE_GPS` | `MAX_RATE_LEAN` |
|---|---:|---:|---:|
| `MODE`/`FAULTS`/`CAL`/`ARM`/`YR` | on the air | **shed** | **shed** |
| `GP-Lat`/`GP-Lon`/`GP-Alt` | logged only | **on the air** | logged only |
| Worst-case packet | 199 B | **201 B** | **145 B** |
| Airtime, model | 317.70 ms | **317.70 ms** | **235.78 ms** |
| Airtime, +1.8 % hardware correction | 323.41 ms | **323.41 ms** | **240.02 ms** |
| Period | 700 ms | **363 ms** | **280 ms** |
| Rate | 1.43 Hz | **2.75 Hz** | **3.57 Hz** |
| Duty | 46 % | **89 %** | **86 %** |

**201 bytes costs exactly what 199 does.** LoRa quantises the payload into symbol blocks,
and both land on 298 symbols at SF7/125 kHz — so putting the three `GP-` fields on the air
in place of the five diagnostic tags is airtime-free against today's budget. That is why
the GPS variant exists at all: it nearly doubles the rate *and* transmits position, for
nothing.

**The period is `airtime + 40 ms`, not `airtime / duty`.** The 40 ms is the vehicle's own
work between transmits. [F-11](../testing/bring-up-record.md#findings) measured the SD block
write at 2.7 ms mean and **30 ms worst case, on two different boards, in two of five
sessions** — that is a healthy card's internal housekeeping, not a fault, and it does not go
away because recent runs were clean. The sensor loop and the watchdog feed share what is
left. A period shorter than this guard does not fail loudly; it makes that packet late,
which reaches the official ground stations as jitter on a line scored for consistency.

**The byte figures must be defended, not asserted.** 145 B is the repository's existing
arithmetic (199 B worst case less 54 B of worst-case tags) and 201 B is that plus the three
`GP-` fields. The implementation adds tests that construct the widest packet in each
configuration and check it against its constant. If a measurement disagrees, the constant
moves and the period moves with it. **This document fixes the derivation, not the number.**

Both periods sit in `link_profile.hpp` behind `static_assert`s: each exceeds its own
budget's airtime plus the guard, and each budget fits the 255-byte FIFO.

## What latches

One accepted command sets four things at once, and none can be undone:

1. **The period** — `PeriodicTask::set_period()` on the telemetry task.
2. **The packet** — diagnostic tags off; `GP-` fields on or off per variant. Both are set
   explicitly by the command rather than inherited from the build, so there is no
   combination of build flag and command that produces a packet the period was not sized
   for.
3. **The runtime cap** — `worst_case_packet_bytes` moves to the variant's budget, so the
   existing oversize-shedding logic keeps describing the packet actually being sent.
4. **The uplink** — `service_ground_commands()` returns immediately from then on. The
   vehicle never enters RX again.

**Whichever command arrives first wins, and the other becomes unreachable.** That is a
consequence of the latch rather than a separate rule: there is no way to switch from one
max mode to the other, because after either one the vehicle is no longer listening.

**The latch lives in RAM.** A power cycle or a watchdog reset returns the vehicle to its
flashed configuration — 1.43 Hz, uplink open. Persisting it would mean writing flight
configuration to the card, and a card that arrives at the pad already carrying "max rate,
no uplink" from a bench session applies it silently to the next flight. The RAM latch's
failure mode is visible instead: the rate is back at 1.43 Hz and you press the button
again. The cost is that a watchdog reboot in flight drops the rate for the rest of the
flight — which is also the only signal that a reboot happened.

## What it refuses

- **Armed, or not in READY.** The same gate as `erase_log`, unchanged.
- **A replayed, future or stale packet number.** The same rules, unchanged.
- **A token minted for a different command.** New, and the reason for the digest change.
- **A second command after the latch.** Not refused — *not heard*. The radio is no longer
  in RX.

## How the operator knows it worked

There is no acknowledgement packet, because the vehicle stops listening in the same breath
as it answers. The evidence is the telemetry, and it is unambiguous within one period: the
rate moves to 2.75 or 3.57 Hz, the `MODE`, `FAULTS`, `CAL`, `ARM` and `YR` tags stop
appearing, and — for the GPS variant — the three `GP-` fields start.

`health_.ground_commands_accepted` still increments, and the SD log still records it.

## Consequences accepted

| | Effect |
|---|---|
| **Log capacity** | ~30 hours falls to ~15.5 (GPS) or ~12 (lean). Irrelevant to a flight, relevant to a long bench session. |
| **Average current** | The radio averages 41 mA today and ~75-78 mA at these duties; the steady-state total moves ~130 → ~165 mA. **Peaks do not change** — they are set by coincident TX, SD write and GPS acquisition, not by rate, so the 300 mA analysis in [electrical-architecture.md](electrical-architecture.md) stands unaltered. |
| **Channel occupancy** | The vehicle transmits ~86-89 % of the time. Acceptable inside a reserved launch slot; antisocial during shared bench testing on `0xF3`, where other teams are listening on the same word. |
| **Diagnosis** | `MODE`/`FAULTS`/`CAL`/`ARM` leave the air in both variants. A fault during a max-rate flight is visible in the log after recovery, not live. |
| **Format compliance** | Unaffected, and checked: the rulebook's mandated packet is the twelve fields `CAN-Team-XX; P-; Ti-; A-; Pr-; T-; Ro-; Pi-; Ya-; AX-; AY-; AZ-`. The shed tags appear nowhere in it, and `GP-` fields are listed under *Optional Sensor Fields*. |

## Testing

**Host, C++:** each variant sets all four latches, with the right period and the right
packet; the tags stop; the GPS variant adds the `GP-` fields and the lean variant does not;
the uplink is closed afterwards and neither a repeat nor the other variant has any effect;
both are refused when armed, when not in READY, and on replayed, future and stale packet
numbers; **a token minted for one command is refused for another**; the widest packet in
each configuration matches its budget constant.

**Host, Node:** the console mints both tokens, the firmware's fixture agrees with each, and
the password never appears in a transmitted frame.

**Fixtures:** `test-data/command-tokens.tsv` regenerated for the command-bound digest, with
a row per kind and the non-ASCII password row kept.

**Documented claims:** both rates, both periods and both duties enter `check_doc_claims.py`
derived from the shipped constants rather than quoted, the way the 1.43 Hz figures are.

**Bench, on hardware:** the only test that matters. For each variant on a separate power
cycle — send it in READY with `ARM-0`, watch the station's rate move, confirm the tag and
`GP-` changes in the raw packets, confirm the other button then does nothing, and confirm
that over several minutes the packet numbering has no gaps at the new rate. Power-cycle and
confirm the vehicle returns to 1.43 Hz with the uplink open. Record both as rows under
Gate 8, with the observed rate and loss.

## Decisions recorded

- **Two commands on the console, not a build-time flag.** Which packet is right depends on
  the flight, and the decision costs nothing at the moment of sending.
- **Ground-only window, not in flight.** Keeps the property that the vehicle never listens
  once armed, which the README and the audit both rest on.
- **The digest covers the command.** See [the token section](#the-token-must-cover-the-command).
- **Period from a guard, not a duty target.** The rulebook scores consistency alongside
  rate; a late packet costs on the same line the rate earns on.
- **RAM latch, not persisted.** See [What latches](#what-latches).
- **Tag shedding rather than a bandwidth change.** 250 kHz would halve airtime, but the
  bridge's modem parameters are compiled in: the vehicle would switch, the ground station
  would go deaf, and the uplink that could have undone it is closed by the same command.

## Out of scope

Persisting the latch across a reset. Commanding in flight. Changing bandwidth or spreading
factor. Any command that lowers the rate again, or switches between the two max modes.
Raising the rate of normal flight — it stays at 700 ms and 46 %.

**Not settled by this document, and worth an answer before flying any of it:** the updated
guidelines add *"All participating CanSats must be compatible with at least one of the
Ground Stations provided by the Physics Club"*. Compatibility means matching frequency,
spreading factor, bandwidth, coding rate and sync word. This repository has never seen
those numbers.
