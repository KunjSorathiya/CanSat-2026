# Telemetry

The radio link, the packet, and the onboard log.

**Status: 2026-09-08.** The link closed end to end on the bench on 2026-09-07. Nothing has
been tested beyond bench range.

---

## The link

| | |
|---|---|
| Radio | SX1278 RA-02, 433 MHz, one on the vehicle and one on the ground bridge |
| Modem | SF7, 125 kHz, CR 4/5, 8-symbol preamble, payload CRC on |
| Power | 17 dBm on PA_BOOST — the RA-02's maximum without PA_DAC |
| Sync words | **`0xF3` testing, `0xA5` official launch** — the only radio parameters the rulebook fixes |
| Telemetry period | **700 ms — 1.43 Hz** |
| Worst-case packet | 199 bytes with GPS logged rather than transmitted; 255 with it on the air |

Every one of those except the sync words is a project engineering choice, defined **once**
in [`link_profile.hpp`](../../firmware/common/include/cansat/link_profile.hpp) and used by
both ends of the link. Before that header existed the two ends carried separate copies and
agreed only by coincidence — a mismatch produces a silent, total link failure that looks
exactly like broken hardware.

**The header will not compile a profile that merely *meets* the rulebook's 1 Hz minimum.**
Three `static_assert`s enforce it: the packet must fit the FIFO, the worst-case airtime must
stay inside the 50 % duty cap, and the period must be at or below **`kMaxTelemetryPeriodMs`
= 950 ms** — the rulebook's 1000 ms less 50 ms of jitter margin. A period of exactly
1000 ms is refused, at compile time and again in `validate_config()` at startup.

**This vehicle cannot be built at 1 Hz.** If a live link reads 1 Hz, the cause is upstream of
the configuration — usually an image flashed before the period changed. The startup
summary prints the rate in Hz so that is one glance to confirm.

---

## What has been measured

From [bring-up-record.md](../../documentation/testing/bring-up-record.md), gates 5, 7 and 8.

| Quantity | Predicted | Measured | Verdict |
|---|---|---|---|
| Version register | `0x12` | `0x12`, re-read twice on the soldered board | ✅ |
| Airtime, 206-byte packet | 327.9 ms | **333.7 ms** | +1.8 % |
| Airtime, 255-byte packet | 399.6 ms | **406.9 ms** | +1.8 %, identical to 0.1 ms across four sessions |
| Sustained transmit | Every packet sent | **45 attempts in 15.0 s, 45 sent, 0 failed** | ✅ |
| End-to-end packets | Sequential from `P-001` | **66/66, no gaps, no duplicates** | ✅ |
| Packet loss, bench | 0 % | **0 % over 66 packets** | ✅ |
| RSSI, bench range | — | **−44 dBm** | — |
| Channel occupancy | ~33 % typical | **33.4 % typical, 40.7 % worst** | ✅ |

**The model reads 1.8 % low, consistently, on two boards.** That is not noise — it
reproduced to 0.1 ms across four sessions. The telemetry period is sized from the
**measured** airtime rather than the model because of it: 199 bytes costs ~323 ms once the
1.8 % is applied, so the 50 % duty cap puts the floor at ~647 ms, and 700 ms leaves the
real duty at 46 % rather than sitting on the limit.

---

## Why 700 ms and not 1000

The rulebook's 1 Hz is a **minimum**, and the 2026 revision scores rates above it. Two
changes took the vehicle from exactly the floor to 43 % above it:

1. **GPS moved out of the packet and into the log.** `GP-Lat`/`GP-Lon`/`GP-Alt` are 56
   bytes, and they take the worst case from 199 to exactly 255 — the FIFO limit. SEN-011
   scores an additional sensor on data *transmitted **or** logged*, and every SD row carries
   latitude, longitude, altitude, satellite count and HDOP, so the five points are untouched.
2. **The period sized from measured airtime**, as above.

**What it costs is recovery, not points.** With the position off the air the ground station
cannot say where the vehicle is — during descent or after landing — and the fix is on a card
inside the thing you are looking for. `Configuration::transmit_gps` puts it back at 1.18 Hz,
and `validate_config()` refuses the two settings apart. That is a launch-day judgement about
the site.

**Sitting at exactly 1000 ms was its own hazard.** Any jitter puts an interval over a second
and the vehicle momentarily below a requirement that is *checked, not estimated*. 700 ms
carries 300 ms of margin — and since 2026-09-08 the hazard is not merely avoided but
**unreachable**: no build can carry a period above 950 ms.

---

## The packet

```text
CAN-Team-25; P-042; Ti-00:01:23:450; A-118.4; Pr-99821.33; T-24.6; Ro-2.1; Pi--1.4;
Ya-15.9; AX-0.12; AY--0.31; AZ-9.79; MODE-FLIGHT; FAULTS-0; CAL-1; ARM-1; YR-G;
```

Mandatory fields first, in the rulebook's order and to its exact precision, then optional
fields. Three independent parsers — C++, Python and JavaScript — are held to one fixture
file, [`protocol-fixtures.tsv`](../../test-data/protocol-fixtures.tsv), so they cannot
disagree.

**Two behaviours worth knowing:**

- **Wrong data is never transmitted.** If any mandatory field cannot be trusted, no packet
  is produced — and the packet number is **not consumed**. Transmitted numbers stay strictly
  sequential, so a gap at the ground station means radio loss and nothing else.
- **Optional fields shed before the packet overruns.** The controller drops its diagnostic
  tags rather than let a packet reach the radio's silent 255-byte truncation.

Full specification: [telemetry-protocol.md](../../documentation/design/telemetry-protocol.md).

---

## The onboard log

The card is the primary record; the radio is the live view.

- **No filesystem.** Records go into raw 512-byte blocks, with the header rewritten after
  every record, so a brownout or an impact reset resumes at the correct block instead of
  overwriting flight data.
- **It carries more than the packet does** — GPS position, satellite count, HDOP, and the
  microphone columns, none of which are transmitted.
- **It degrades quietly.** Ten consecutive write failures disable logging; telemetry is
  untouched.
- Measured: **2.677 ms mean write, 297–367 writes/s sustained**, 100/100 written across
  three sessions.

Read it back with [`tools/read_flight_log.py`](../../tools/read_flight_log.py).

---

## Open items

| Item | Note |
|---|---|
| **The official sync word `0xA5` has never been tried** | Only `0xF3` has linked. `0xA5` is what the launch runs on |
| **Range** | Every RSSI row past bench range is empty: 10 m, 100 m, 500 m, 1 km, and the range at which loss reaches 5 % |
| **Loss over 500 packets** | The bench run was 66 |
| **CRC errors on the USB link** | Row 8.3, never counted over a long run |
| **SD log against received telemetry** | Row 8.15. The card should be complete where the radio has gaps; nobody has diffed them |
| **The official dual ground stations** | The 2026 revision names the radios but not the framing or the host-side format — [open question 7](../../README.md) |

Related: [telemetry-protocol.md](../../documentation/design/telemetry-protocol.md) ·
[link-budget.md](../../documentation/design/link-budget.md) ·
[bring-up-record.md](../../documentation/testing/bring-up-record.md) ·
[avionics/README.md](../README.md)
