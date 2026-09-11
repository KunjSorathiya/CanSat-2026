# Radio Link Budget and Telemetry Rate

Why the CanSat transmits at 1.43 Hz, why the modem runs at SF7, and why no amount of software
makes the radio go faster.

This document is the source of the numbers in `firmware/common/include/cansat/link_profile.hpp`.
If you change a modem parameter, change it there — the flight computer and the ground-station
bridge both read that one definition, and the build fails if the profile cannot meet the
rulebook minimum.

---

## Contents

- [The question](#the-question)
- [How airtime is computed](#how-airtime-is-computed)
- [Packet size](#packet-size)
- [Airtime results](#airtime-results)
- [The 30 Hz question](#the-30-hz-question)
- [The chosen profile](#the-chosen-profile)
- [Range margin](#range-margin)
- [Guards in the code](#guards-in-the-code)
- [Changing the profile](#changing-the-profile)
- [What is not verified](#what-is-not-verified)

---

## The question

The rulebook requires **at least one telemetry packet per second**, continuously, from
power-on through five seconds past impact. A packet rate is not something firmware can
simply request: LoRa trades data rate for sensitivity, and at a high spreading factor a
single packet can occupy the channel for longer than the interval it is supposed to fit in.

So the question is not "what rate do we want" but "what rate can this radio physically
deliver with this packet".

## How airtime is computed

From the Semtech SX1276/77/78/79 datasheet (rev. 7, section 4.1.1.7):

```text
Tsym       = 2^SF / BW
Tpreamble  = (n_preamble + 4.25) * Tsym
n_payload  = 8 + max(ceil((8*PL - 4*SF + 28 + 16*CRC - 20*IH) / (4*(SF - 2*DE))) * (CR + 4), 0)
ToA        = Tpreamble + n_payload * Tsym
```

`PL` is payload bytes, `CRC` is 1 when the payload CRC is on, `IH` is 1 in implicit-header
mode, `CR` is 1–4 for coding rates 4/5–4/8, and `DE` is 1 when low-data-rate optimisation is
active — mandatory above a 16 ms symbol period.

Two implementations of this formula exist, deliberately:

| Where | What it is for |
|---|---|
| [`tools/link_budget.py`](../../tools/link_budget.py) | Design-time analysis and the tables below |
| [`cansat/lora_airtime.hpp`](../../firmware/common/include/cansat/lora_airtime.hpp) | Startup validation on the vehicle, `constexpr` |

Both are pinned to the same published reference vectors, in
[`tools/tests/test_link_budget.py`](../../tools/tests/test_link_budget.py) and in
`flight_tests.cpp` respectively:

| Configuration | Published airtime |
|---|---:|
| SF7 / 125 kHz / CR 4-5 / 13-byte payload | 46.336 ms |
| SF12 / 125 kHz / CR 4-5 / 13-byte payload | 1155.072 ms |

Reproduce any number in this document with:

```bash
python tools/link_budget.py --sweep --payload 255 --target-rate 1
```

## Packet size

The transmitted payload is the rulebook telemetry string, not a binary struct. Measured by
running the formatter, not estimated:

| Packet content | Bytes |
|---|---:|
| Team id + 12 mandatory fields | **118** |
| \+ GPS latitude, longitude, altitude | **163** |
| \+ `MODE`, `FAULTS`, `CAL`, `ARM`, `YR` diagnostic tags | **208** |
| Budgeted (`kWorstCasePacketBytes`) — the organizers' receiver limit | **200** |

The first three rows are asserted by `test_measured_packet_sizes_match_the_link_budget`, so
a change to the packet format that makes this table wrong fails the build.

The budget is the organizers' ground station's limit: their receiver discards any packet over
200 bytes, and it is their station that scores. That, not the 255-byte FIFO, is the size a
packet cannot exceed, so it is the only honest basis for an airtime figure. The tagged row
above is over it, which is one more reason the tags are off the air.

There is no meaningful "absolute worst case" row, and the earlier one was misleading: the
team identifier has no length limit in the rulebook format, so the worst case a packet can
reach is not bounded by the format at all. It is bounded by the flight computer, which
sheds optional content in priority order rather than letting the radio truncate a packet
silently. A packet built from extreme values with every tag attached does exceed 255 bytes,
which is exactly why that shedding path exists and is tested.

> **The nine-axis upgrade cost six bytes per packet.** The `YR-` tag says whether the
> mandatory yaw field is an absolute magnetic angle or a relative gyro integration. Six
> bytes of airtime to stop a receiver mistaking one for the other is the cheapest part of
> this whole design.
>
> Those six bytes are still spent on the delivered hardware, which is an MPU-6500 with no
> magnetometer ([F-1](../hardware/receiving-inspection.md#findings)) and therefore always
> sends `YR-G`. A tag that always says the same thing still earns its airtime: it says the
> yaw is relative, which is the fact a receiver most needs and would otherwise assume.

> **A second finding.** An earlier revision of this document budgeted **200 bytes**, from an
> estimate rather than a measurement. The real in-flight packet with GPS and every
> diagnostic tag is **212 bytes** — so the airtime budget sat below the *typical* packet
> and under-estimated occupancy on every single transmission. The numbers above now come
> from running `format_packet()` and reading the lengths, in a test.
>
> The budget is 200 again now, for a better reason than an estimate: it is the organizers'
> receiver limit, and the packet was reshaped to fit it.

Because a packet can approach the 200-byte ceiling, the flight computer sheds optional content
rather than letting the radio truncate it silently. The rulebook sets the priority —
mandatory fields first, optional fields "only if bandwidth allows" — so the ladder is:

1. Drop the diagnostic tags (`MODE`, `FAULTS`, `CAL`, `ARM`, `YR`): project-local, least valuable.
2. Drop `SN-`: mandatory + GPS + sound are 209 bytes at their widest, so this one is reachable,
   though only at widths no flight produces together.
3. Drop the GPS fields: unreachable in a valid configuration, and recoverable from the SD log.
4. If the mandatory block alone would overflow, suppress the packet and raise
   `packet_oversize` — a truncated packet reads as corruption at the ground station, which
   is worse than a missing one.

Every step raises the `packet_oversize` fault, so the ground station sees that it happened.

## Airtime results

One 255-byte packet, coding rate 4/5, 8-symbol preamble, explicit header, CRC on. "Rate @
50 % duty" is the packet rate that leaves half the channel free for radio recovery, retries
and receiver timing — the budget the firmware enforces.

| Config | ToA (ms) | Max rate (Hz) | Rate @ 50% duty (Hz) | Min period (ms) | Meets 1 Hz? |
|---|---:|---:|---:|---:|---|
| SF7/BW125k/CR4-5 | 399.6 | 2.50 | 1.25 | 799 | **yes** |
| SF8/BW125k/CR4-5 | 707.1 | 1.41 | 0.71 | 1414 | over duty |
| SF9/BW125k/CR4-5 | 1250.3 | 0.80 | 0.40 | 2501 | **no** |
| SF10/BW125k/CR4-5 | 2295.8 | 0.44 | 0.22 | 4592 | no |
| SF11/BW125k/CR4-5 | 5001.2 | 0.20 | 0.10 | 10002 | no |
| SF12/BW125k/CR4-5 | 9019.4 | 0.11 | 0.06 | 18039 | no |
| SF7/BW250k/CR4-5 | 199.8 | 5.00 | 2.50 | 400 | yes |
| SF8/BW250k/CR4-5 | 353.5 | 2.83 | 1.41 | 707 | yes |
| SF9/BW250k/CR4-5 | 625.2 | 1.60 | 0.80 | 1250 | over duty |
| SF10/BW250k/CR4-5 | 1147.9 | 0.87 | 0.44 | 2296 | no |
| SF11/BW250k/CR4-5 | 2091.0 | 0.48 | 0.24 | 4182 | no |
| SF12/BW250k/CR4-5 | 4509.7 | 0.22 | 0.11 | 9019 | no |

> **The finding that changed the project.** The previous provisional default was
> **SF9 / 125 kHz**. At that setting a full telemetry packet takes **1250 ms** of airtime —
> longer than the interval it was supposed to fit inside. The scheduler was configured for
> a 500 ms period, a rate the radio could not have delivered, and the vehicle would have
> transmitted at well under 1 Hz while every document claimed 2 Hz. It would not have met
> the rulebook minimum at all.

## The 30 Hz question

30 Hz of full RF telemetry is not achievable, and nothing in the software can make it so.
The best case in the table — SF7 at 250 kHz — reaches 5 Hz at 100 % channel occupancy, six
times short, and that is before considering interference, retries or the receiver. Reaching
30 Hz would require roughly a 33 ms packet, which at this payload size implies a data rate
this modem does not offer in any legal configuration of the 433 MHz band.

The architecture therefore decouples the rates rather than faking one:

| Stage | Rate | Bounded by |
|---|---|---|
| Sensor acquisition, orientation, state estimation | 30 Hz | Barometer conversion time — see [sensor-rates.md](sensor-rates.md) |
| Canonical telemetry model | Built on demand from the latest snapshot | — |
| SD raw block logging | Per telemetry record, independent of the radio | SD write latency |
| **RF telemetry** | **1 Hz** | **Airtime — this document** |
| Ground-station parse, validate, log, display | As received, plus a 5 s sliding link-health window | — |

Raising the internal sensor rate is an independent change from raising the radio rate, and
neither forces the other. See
[software-architecture.md](software-architecture.md) for the loop structure.

## The chosen profile

Defined once, in [`cansat/link_profile.hpp`](../../firmware/common/include/cansat/link_profile.hpp):

| Parameter | Value | Why |
|---|---|---|
| Frequency | 433 MHz | RA-02 carrier band; **exact channel is an open question for the organisers** |
| Spreading factor | **SF7** | The only SF that meets 1 Hz with duty margin on a full-FIFO packet |
| Bandwidth | 125 kHz | Narrower is more interference-tolerant; 250 kHz is the upgrade path for 2 Hz |
| Coding rate | 4/5 | Lowest overhead; 4/6–4/8 cost airtime for error correction not yet shown to be needed |
| Preamble | 8 symbols | SX127x default; the receiver must match |
| CRC | on | Corruption must be detectable, not silently accepted |
| TX power | 17 dBm | RA-02 PA_BOOST maximum without PA_DAC |
| Sync word | 0xF3 test / 0xA5 official — **both images fly 0xA5**, the organizers' station's word | **Rulebook-fixed** |
| Worst-case airtime | **317.7 ms** | The 200-byte budget at SF7/125 kHz/CR4-5 — the organizers' receiver limit, which every packet is held to. Measured airtime runs ~1.8 % above the model (bring-up 5.2/5.3), so budget ~323 ms |
| Telemetry period | 700 ms | **1.43 Hz at ~45 % worst-case channel occupancy** on the model, ~46 % measured, on the 200-byte budget. Every normal-flight packet is rich: the organizers count only transmitted telemetry for extra-sensor points, and at a 700 ms cadence carrying the sensors in every packet is the only way to put GPS and sound on the air at least once a second. The five diagnostic tags that used to fill that space are logged instead. The rulebook's 1 Hz is a minimum, and the period is derived from the **measured** airtime (bring-up 5.2/5.3), 1.8 % above the model. After the max-rate command the vehicle leaves this period for the slot pattern in [max-rate-command.md](max-rate-command.md) |

Resulting budget: **317.7 ms worst-case airtime, 45 % channel duty, 55 % of the channel
free.** A typical 180-byte rich packet costs about 287 ms, or 41 %.

The 2 Hz option is real but conditional: SF7 at **250 kHz** gives 199.8 ms worst-case
airtime and a comfortable 40 % duty at a 500 ms period. It costs about 3 dB of receiver sensitivity, which
the range margin below can absorb. It is not the default because it has never been tested on
hardware; promote it after a successful range test, not before.

## Range margin

Free-space path loss at 433 MHz:

```text
FSPL(dB) = 32.45 + 20*log10(f_MHz) + 20*log10(d_km)
         = 32.45 + 52.7 + 20*log10(d_km)
```

| Distance | FSPL | Received power (17 dBm TX, 0 dBi antennas both ends) |
|---|---:|---:|
| 500 m | 79.1 dB | −62 dBm |
| 1 km | 85.2 dB | −68 dBm |
| 2 km | 91.2 dB | −74 dBm |

Typical SX127x sensitivity at 125 kHz: about **−123 dBm at SF7**, about −129 dBm at SF9. Even
at 2 km with quarter-wave whips and no ground-plane assumptions, SF7 leaves roughly **49 dB**
of margin. Dropping from SF9 to SF7 costs about 6 dB of that margin and buys a 3× airtime
reduction — a clearly favourable trade for this mission profile.

These are datasheet and free-space figures. They are an argument that SF7 is *not*
range-limited for a CanSat descent; they are **not** a measured link budget. Antenna gain,
polarisation mismatch during tumbling, body blockage by the vehicle structure, and the
ground station's own noise floor are all unmeasured.

## The 1 Hz minimum is a floor this vehicle cannot be configured onto

The rulebook requires **at least** one packet per second, and the 2026 revision separately
scores rates above it. A period of exactly 1000 ms satisfies the letter of that and is still
the wrong number to build, for two independent reasons:

1. **It sits *on* the requirement.** What a ground station measures is the transmit period
   plus whatever jitter the loop, the radio and the receiver add. At exactly 1000 ms every
   one of those pushes an interval past a second, and the vehicle is momentarily below a
   requirement that is *checked*, not estimated. Measured mission-clock jitter is under
   **4 ms** (bring-up row 5.4), so the 50 ms margin below is more than an order of magnitude
   of headroom.
2. **It scores nothing.** Rate above 1 Hz is a scored line, and 1.00 Hz is the floor of it.

So the profile carries a hard ceiling rather than a default:

| Constant | Value | Meaning |
|---|---:|---|
| `kRulebookMinRatePeriodMs` | 1000 ms | The rulebook's 1 Hz, as a period |
| `kTelemetryJitterMarginMs` | 50 ms | Margin against measured jitter of under 4 ms |
| **`kMaxTelemetryPeriodMs`** | **950 ms** | **The slowest period any build may carry — 1.053 Hz** |
| `kTelemetryPeriodMs` | 700 ms | What this vehicle actually ships — **1.43 Hz** |

**Nothing can be configured past that ceiling**, at compile time or at run time. A period of
exactly 1000 ms is refused, and that is the point of the guard rather than an off-by-one.

## Guards in the code

Four layers, so an impossible or merely compliant-on-paper configuration cannot reach the pad:

1. **Compile time.** `link_profile.hpp` `static_assert`s that the profile's worst-case
   airtime fits inside its own telemetry period at the duty limit, that the packet fits the
   LoRa FIFO, and that the period is **at or below `kMaxTelemetryPeriodMs`** — not merely at
   or below the rulebook's 1000 ms. An impossible *or* borderline default is a build error.
2. **Startup.** `flight::validate_config()` recomputes the airtime for the *actual* runtime
   configuration and refuses to start the mission loop with a message naming the airtime,
   the required minimum period and this document. It applies the same 950 ms ceiling, which
   is the layer that catches a period written by hand at a call site rather than taken from
   the profile.
3. **Test.** `test_link_profile_is_shared_by_both_ends()` asserts the vehicle's radio
   configuration and the bridge's default `Sx1278Settings` are identical field by field. A
   modem mismatch receives nothing and is indistinguishable from dead hardware, so it is
   worth a dedicated test. `test_the_link_profile_is_compulsorily_faster_than_1_hz()` holds
   the three layers to each other, and
   `test_a_default_vehicle_transmits_faster_than_1_hz()` flies a mission and measures the
   gaps between transmitted packets — because a guard on a constant is only worth having if
   the packets actually come out at that spacing.
4. **The receiving end.** The ground station reports whether the rate that *arrived* cleared
   1 Hz (`LinkHealth.rate_meets_rulebook`), which is a different question from what the
   vehicle transmitted once the link starts losing packets. It answers three ways —
   compliant, not compliant, or not yet enough link to judge — because "not measured" and
   "measured, and too slow" call for very different reactions.

> **If a live link reads 1 Hz**, the vehicle is not the cause: no build can be configured
> that way. The usual explanation is an image flashed before the period changed. The vehicle's
> startup summary prints the rate in Hz for exactly this reason.

## Changing the profile

1. Edit the constants in `firmware/common/include/cansat/link_profile.hpp`.
2. Rebuild. If a `static_assert` fires, the combination either cannot meet the rulebook
   minimum or only just meets it — the message says which constraint failed.
3. Re-run `python tools/link_budget.py --sweep --payload 255` and update the table above.
4. Run `bash tools/build_host.sh`. The shared-profile test proves both ends still agree.
5. Reflash **both** Picos. Flashing only one produces a dead link.

## What is not verified

The airtime half is now measured. Everything about propagation still is not:

| Claim | Status |
|---|---|
| Airtime formula | Verified against two published reference vectors, in two languages, **and against a radio on 2026-09-05** |
| 399.6 ms worst-case airtime | **MEASURED: 406.9 ms** on the delivered RA-02, mean of five transmits — 1.8 % over prediction, in the direction the driver's overhead explains |
| Sensitivity figures | Datasheet typicals, not measured for the RA-02 carrier |
| Range margin | Free-space calculation; no field test has been run |
| Packet loss at 1 Hz | Unknown — requires a range test |
| Regulatory duty-cycle limits at 433 MHz | **Open question**: local regulations and any competition-imposed limit are unconfirmed |
| Exact channel frequency | **Open question for the organisers** |

**What the airtime measurement settles.** A 206-byte packet took **333.7 ms** against 327.9
predicted, and a 255-byte one **406.9 ms** against 399.6 — both about 1.7 % over, with the
excess growing slightly with payload, which is what filling a larger FIFO over SPI looks
like. The model this document computes from, `lora_time_on_air_ms()`, is therefore sound on
the real modem and not merely self-consistent.

That matters more than the two numbers. **The 1 Hz telemetry rate, the SF7 choice, the
channel-occupancy figures and the build-time `static_assert` that refuses a profile which
cannot meet 1 Hz all rest on this one function.** It has now been checked against a radio.

Channel occupancy follows directly: **33.4 % typical and 40.7 % worst case**, against the
~33 % and 40 % predicted.

The first field test to run is a static range test at the chosen profile, logging RSSI, SNR
and packet loss against distance. Sensitivity, range margin and packet loss remain pure
arithmetic until then — **airtime was the only part of this document a bench could settle.**

---

## Related documents

- [Telemetry Protocol](telemetry-protocol.md) — the packet whose size drives all of this
- [Software Architecture](software-architecture.md) — where the telemetry scheduler sits
- [Hardware](../hardware/hardware.md) — the RA-02 module and antenna
- [Requirements](../requirements/requirements.md) — the 1 Hz rulebook minimum
- [Test Plan](../testing/test-plan.md) — the range test this document asks for
