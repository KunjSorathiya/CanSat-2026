# Radio Link Budget and Telemetry Rate

Why the CanSat transmits at 1 Hz, why the modem runs at SF7, and why no amount of software
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
python tools/link_budget.py --sweep --payload 200 --target-rate 1
```

## Packet size

The transmitted payload is the rulebook telemetry string, not a binary struct. Measured
from the flight computer's own output:

| Packet content | Bytes |
|---|---:|
| Team id + 12 mandatory fields | ~130 |
| \+ GPS latitude, longitude, altitude | ~172 |
| \+ `MODE` and `FAULTS` diagnostic tags | **188** |
| Budgeted worst case (`kWorstCasePacketBytes`) | **200** |

200 bytes carries headroom for a longer registered team id and a five-digit packet number.
The LoRa FIFO limit is 255 bytes, so the packet fits in one transmission with margin — but
only just, and the diagnostic tags are the first thing to drop if the packet ever grows.

## Airtime results

One 200-byte packet, coding rate 4/5, 8-symbol preamble, explicit header, CRC on. "Rate @
50 % duty" is the packet rate that leaves half the channel free for radio recovery, retries
and receiver timing — the budget the firmware enforces.

| Config | ToA (ms) | Max rate (Hz) | Rate @ 50% duty (Hz) | Min period (ms) | Meets 1 Hz? |
|---|---:|---:|---:|---:|---|
| SF7/BW125k/CR4-5 | 317.7 | 3.15 | 1.57 | 635 | **yes** |
| SF8/BW125k/CR4-5 | 563.7 | 1.77 | 0.89 | 1127 | over duty |
| SF9/BW125k/CR4-5 | 1004.5 | 1.00 | 0.50 | 2009 | **no** |
| SF10/BW125k/CR4-5 | 1845.2 | 0.54 | 0.27 | 3690 | no |
| SF11/BW125k/CR4-5 | 4018.2 | 0.25 | 0.12 | 8036 | no |
| SF12/BW125k/CR4-5 | 7217.2 | 0.14 | 0.07 | 14434 | no |
| SF7/BW250k/CR4-5 | 158.8 | 6.30 | 3.15 | 318 | yes |
| SF8/BW250k/CR4-5 | 281.9 | 3.55 | 1.77 | 564 | yes |
| SF9/BW250k/CR4-5 | 502.3 | 1.99 | 1.00 | 1005 | over duty |
| SF10/BW250k/CR4-5 | 922.6 | 1.08 | 0.54 | 1845 | over duty |
| SF11/BW250k/CR4-5 | 1681.4 | 0.59 | 0.30 | 3363 | no |
| SF12/BW250k/CR4-5 | 3608.6 | 0.28 | 0.14 | 7217 | no |

> **The finding that changed the project.** The previous provisional default was
> **SF9 / 125 kHz**. At that setting one full telemetry packet takes **1004 ms** of airtime.
> The scheduler was configured for a 500 ms period — a rate the radio could not have
> delivered, and the vehicle would have transmitted at roughly 1 Hz while every document
> claimed 2 Hz. It would not even have met the rulebook minimum with any margin.

## The 30 Hz question

30 Hz of full RF telemetry is not achievable, and nothing in the software can make it so.
The best case in the table — SF7 at 250 kHz — reaches 6.3 Hz at 100 % channel occupancy,
five times short, and that is before considering interference, retries or the receiver.
Reaching 30 Hz would require roughly a 33 ms packet, which at 200 bytes implies a data rate
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
| Spreading factor | **SF7** | The only SF that meets 1 Hz with duty margin on a 200-byte packet |
| Bandwidth | 125 kHz | Narrower is more interference-tolerant; 250 kHz is the upgrade path for 2 Hz |
| Coding rate | 4/5 | Lowest overhead; 4/6–4/8 cost airtime for error correction not yet shown to be needed |
| Preamble | 8 symbols | SX127x default; the receiver must match |
| CRC | on | Corruption must be detectable, not silently accepted |
| TX power | 17 dBm | RA-02 PA_BOOST maximum without PA_DAC |
| Sync word | 0xF3 test / 0xA5 official | **Rulebook-fixed** |
| Telemetry period | 1000 ms | 1 Hz rulebook minimum at ~32 % channel occupancy |

Resulting budget: **317.7 ms airtime, 31.8 % channel duty, 68 % of the channel free.**

The 2 Hz option is real but conditional: SF7 at **250 kHz** gives 158.8 ms airtime and a
comfortable 32 % duty at a 500 ms period. It costs about 3 dB of receiver sensitivity, which
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

## Guards in the code

Three layers, so an impossible configuration cannot reach the pad:

1. **Compile time.** `link_profile.hpp` `static_assert`s that the profile's worst-case
   airtime fits inside its own telemetry period at the duty limit, that the packet fits the
   LoRa FIFO, and that the period satisfies the 1 Hz minimum. An impossible default is a
   build error.
2. **Startup.** `flight::validate_config()` recomputes the airtime for the *actual* runtime
   configuration and refuses to start the mission loop with a message naming the airtime,
   the required minimum period and this document.
3. **Test.** `test_link_profile_is_shared_by_both_ends()` asserts the vehicle's radio
   configuration and the bridge's default `Sx1278Settings` are identical field by field. A
   modem mismatch receives nothing and is indistinguishable from dead hardware, so it is
   worth a dedicated test.

## Changing the profile

1. Edit the constants in `firmware/common/include/cansat/link_profile.hpp`.
2. Rebuild. If the `static_assert` fires, the combination cannot meet the rulebook minimum —
   the message says which constraint failed.
3. Re-run `python tools/link_budget.py --sweep --payload 200` and update the table above.
4. Run `bash tools/build_host.sh`. The shared-profile test proves both ends still agree.
5. Reflash **both** Picos. Flashing only one produces a dead link.

## What is not verified

Nothing in this document has been measured on hardware. Specifically:

| Claim | Status |
|---|---|
| Airtime formula | Verified against two published reference vectors, in two languages |
| 317.7 ms airtime for the real packet | Computed, never measured on a radio |
| Sensitivity figures | Datasheet typicals, not measured for the RA-02 carrier |
| Range margin | Free-space calculation; no field test has been run |
| Packet loss at 1 Hz | Unknown — requires a range test |
| Regulatory duty-cycle limits at 433 MHz | **Open question**: local regulations and any competition-imposed limit are unconfirmed |
| Exact channel frequency | **Open question for the organisers** |

The first field test to run is a static range test at the chosen profile, logging RSSI, SNR
and packet loss against distance. Until then, every number here is arithmetic.

---

## Related documents

- [Telemetry Protocol](telemetry-protocol.md) — the packet whose size drives all of this
- [Software Architecture](software-architecture.md) — where the telemetry scheduler sits
- [Hardware](../hardware/hardware.md) — the RA-02 module and antenna
- [Requirements](../requirements/requirements.md) — the 1 Hz rulebook minimum
- [Test Plan](../testing/test-plan.md) — the range test this document asks for
