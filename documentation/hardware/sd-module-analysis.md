# Micro SD Reader Module Analysis

## Scope

This document analyzes the exact Micro SD Card Reader Module identified as Robu SKU 11566
for integration with the CanSat's Raspberry Pi Pico, shared SPI bus, and power
architecture.

The analysis does not assume that all Micro SD breakout boards are electrically
equivalent. The module in hand has been identified; what its individual components measure
under load has not.

## Status change: the supply question is answered

This document previously concluded that the reader required a 4.5–5.5 V input, that a 1S
LiPo therefore could not drive it, and that a separate boost-derived rail might be needed.
That conclusion came from a supplier listing.

**The module received is a 3.3 V board.** What the delivered hardware actually shows, from
[`11566-sd-reader-front.jpg`](photos/11566-sd-reader-front.jpg) and
[`11566-sd-reader-back.jpg`](photos/11566-sd-reader-back.jpg):

- The supply pin is printed **`3V3`**. No voltage *range* is printed anywhere on the board.
- **No regulator** of any package, on either face.
- **No level shifter** — no buffer, no translator, no transistors. The board has no active
  component at all.
- Its entire parts list is **four resistors marked `103` (10 kΩ)**, silkscreened `10K`, and
  two unmarked capacitors.
- Header, in printed order: **`GND  MISO  CLK  MOSI  CS  3V3`**.
- Friction / slide-in card holder — no spring eject.

It is therefore powered from the same 3.3 V rail as the rest of the vehicle. There is no
second rail, no boost converter, and no level shifting.

> The *tolerated supply range* is still `TBD`. The board is a 3.3 V board, but a range is a
> property of the card and the tracks, and this board states none. Do not record a range here
> that no source establishes — that is exactly the failure that produced the 4.5–5.5 V claim.

Everything downstream of that changes: the power tree loses a branch, the regulator
selection loses a constraint, and the bring-up sequence loses a gate. Two things get
*harder*, and both follow from the missing translator:

- **The host must be 3.3 V.** With a level shifter the module tolerated a 5 V host. Without
  one, that is a requirement, and a stray 5 V feed reaches the card directly.
- **Nothing buffers MISO.** Only the card releases the line when `CS` goes high. A card that
  holds it corrupts the *radio's* next transaction on the shared bus, and the symptom
  presents as a dead radio. The 10 kΩ pull-up defines an undriven line; it cannot overcome a
  driven one.

What does *not* change is the measurement work — current draw, decoupling and shared-bus
behaviour are still unmeasured, and are still what stands between "identified" and
"qualified".

## Evidence Classifications

- **CONFIRMED** - Directly verified from an authoritative exact-product source or project hardware record.
- **VERIFIED FROM HARDWARE** - Recorded from the delivered board during receiving inspection.
- **MANUFACTURER DOCUMENTED** - From manufacturer documentation, but not necessarily the exact breakout board.
- **INFERRED** - A conclusion derived from documented values; the reasoning is stated.
- **PHYSICAL VERIFICATION REQUIRED** - Cannot safely be determined without measurement on the actual board.

## Exact Product Identity

| Item | Value | Evidence |
|---|---|---|
| Product | Micro SD Card Reader Module | CONFIRMED project BOM |
| Robu SKU | 11566 | CONFIRMED project BOM |
| Quantity | 1 | CONFIRMED project BOM |
| Supply pin marking | `3V3`; no range printed on the board | VERIFIED FROM HARDWARE, receiving inspection |
| Onboard regulator | None, either face | VERIFIED FROM HARDWARE, receiving inspection |
| Onboard level shifter | None; no active component on the board | VERIFIED FROM HARDWARE, receiving inspection |
| Tolerated supply range | TBD — no source states one | PHYSICAL VERIFICATION REQUIRED |
| Interface | SPI | VERIFIED FROM HARDWARE, receiving inspection |
| Manufacturer | TBD | PHYSICAL VERIFICATION REQUIRED |
| Exact board revision | TBD | PHYSICAL VERIFICATION REQUIRED |
| Board schematic | TBD | PHYSICAL VERIFICATION REQUIRED |

## Electrical Summary

| Property | Current value/status | Evidence level |
|---|---|---|
| Required VCC input | 3.3 V. The pin is printed `3V3` and nothing on the board could step a higher voltage down | VERIFIED FROM HARDWARE |
| Nominal supply used by this project | 3.3 V, the Pico's own regulated rail | CONFIRMED design decision |
| SD-card supply voltage | 3.3 V; with no regulator, the header pin is the card's rail | INFERRED from the absence of a regulator; confirm with a meter |
| Host logic voltage | 3.3 V, matching the Pico. **Required, not merely convenient** | INFERRED from the absence of a level shifter |
| Header, in printed order | `GND  MISO  CLK  MOSI  CS  3V3` | VERIFIED FROM HARDWARE |
| Level shifting | **None fitted.** Host signals reach the card unbuffered | VERIFIED FROM HARDWARE |
| Pull-ups | Four resistors marked `103` (10 kΩ), silkscreened `10K`. Which nets they serve: TBD | VERIFIED FROM HARDWARE (value); PHYSICAL VERIFICATION REQUIRED (nets) |
| Decoupling | Two unmarked capacitors; values TBD | VERIFIED FROM HARDWARE (presence) |
| Card retention | Friction / slide-in holder, no spring eject | VERIFIED FROM HARDWARE |
| Typical current | TBD | PHYSICAL VERIFICATION REQUIRED |
| Startup current | TBD | PHYSICAL VERIFICATION REQUIRED |
| Write current and worst-case transient | TBD | PHYSICAL VERIFICATION REQUIRED |
| MISO behaviour with CS inactive | TBD — **and unbuffered, so it depends entirely on the card** | PHYSICAL VERIFICATION REQUIRED |

## Supply Architecture

A 3.3 V module with no regulator, on a vehicle whose logic rail is 3.3 V, is the simple case:

```text
1S LiPo -> Pico VSYS -> Pico 3V3 regulator -> 3.3 V rail
                                               |
                                               +-- MPU-9250
                                               +-- BMP280
                                               +-- NEO-6M
                                               +-- RA-02
                                               +-- microSD reader   <- 3.3 V, in range
```

The module's own pin marking is `3V3`, so the rail and the requirement are the same number
rather than a boundary case. What the board *tolerates* either side of that is unstated by
any source and is not assumed here.

The remaining supply question is not voltage but **current**. The Pico's onboard regulator
also feeds the radio and three sensors, and an SD card's write transient is the largest
short-duration load on this vehicle. That is a measurement, not a specification lookup:
see [Current and Startup Behavior](#current-and-startup-behavior).

## Logic-Level Analysis

With the module and the host both at 3.3 V, CS, CLK, MOSI and MISO are 3.3 V signals on both
sides and no shifting is required — which is fortunate, because **the board fits none**. The
host lines reach the card through nothing but track and a 10 kΩ pull-up.

That makes 3.3 V a requirement rather than a convenience. There is no translator between a
mistake and the card.

Two things are still worth measuring on the bench before the vehicle is assembled, because
they are board properties rather than voltage-domain properties:

- **MISO with CS inactive.** SPI0 is shared with the RA-02. A reader that keeps driving
  MISO after its chip select is released corrupts the radio's next transaction — a fault
  that presents as a dead radio, not a dead card. The driver already clocks an extra byte
  with CS high for exactly this reason (`firmware/flight-computer/src/pico/sd_card.cpp`),
  but the board's behaviour should be confirmed rather than assumed.
- **Bus pull-ups.** Four are fitted and all four are marked `103` (10 kΩ), silkscreened
  `10K`. Which nets they serve is still unread, and they interact with the shared bus and
  with the RA-02's own pins.

## Power-Source Compatibility

### Pico 3.3 V output

**Decision: YES on voltage; current not yet qualified.**

The module's supply pin is printed `3V3` and there is no regulator to step anything down, so
the 3.3 V rail is exactly what it wants. This is the intended supply for the vehicle. What remains open is whether the Pico's regulator can carry the reader's write
transient on top of the radio and the sensors, which is a measurement.

### 1S LiPo directly

**Decision: NO.**

A cell reaches about 4.2 V fully charged and sits near 3.7 V for most of its discharge, and
with no regulator on the board that voltage would land on the card and on the unbuffered
signal lines directly. Use the regulated 3.3 V rail.

### Pico VSYS

**Decision: NO.**

VSYS follows the battery, so it carries the same 4.2 V down to 3.0 V swing as the cell
itself. A module with no regulator wants a regulated input, not a battery-tracking one.

### Separate rail or boost converter

**Not required, and removed from the design.** The earlier analysis carried a possible
second rail purely to satisfy a 4.5–5.5 V input requirement that this module does not
have. Nothing else on the vehicle needs more than 3.3 V, so the power tree is now a single
regulated rail.

## SPI Architecture Compatibility

The shared SPI arrangement is:

```text
Pico SPI0
    SCK  (GP18) -> RA-02 + SD reader
    MOSI (GP19) -> RA-02 + SD reader
    MISO (GP16) <- RA-02 + SD reader
    RA-02 CS (GP17) -> dedicated selection
    SD CS    (GP6)  -> dedicated selection
```

Only one chip select is asserted at a time. The driver sets the bus clock it needs at the
start of every transfer rather than assuming whatever the previous user of the bus left
behind, initialises the card at 400 kHz as the SD specification requires, and raises the
clock to its run rate afterwards.

Electrical shared-bus behaviour is still the item to verify on the bench, per the MISO
note above.

## Current and Startup Behavior

No current value has been measured for this module. These loads must be measured before
the power budget is closed:

- Board startup
- SD-card initialisation (the 400 kHz phase)
- Idle with a card inserted
- Sequential writes at the run clock
- Worst-case write transient, with the intended card
- The same measurements with the radio transmitting, since they share the rail

The write transient is the one that matters: it is short, it is large, and it lands on the
same regulator as a radio that is transmitting once a second.

## Decoupling

Required decoupling is **PHYSICAL VERIFICATION REQUIRED**. Inspect the board for VCC input
capacitance and local bypass at the card socket, and add bulk capacitance at the module's
supply pin if the measured write transient calls for it. Do not select capacitors before
the transient is measured.

## Failure Behaviour

The SD log is a recorder, not a flight-critical function, and the firmware treats it that
way:

- A card that fails to initialise raises an `sd_unavailable` warning and the mission
  continues.
- Consecutive write failures disable logging after `sd_max_failures` rather than retrying
  into the flight loop.
- No SD failure path can suppress a telemetry packet.

This is deliberate. Telemetry is the graded deliverable; the card is the data that makes
the flight worth analysing afterwards.

## Integration Verdict

**Voltage: resolved.** The module is a 3.3 V SPI board with no regulator and no level
shifter, and it runs from the vehicle's 3.3 V rail alongside everything else. The *range* it
tolerates is still unstated by any source, and is deliberately not recorded here.

**Remaining work is measurement, not specification:**

1. Current during startup, initialisation, idle and worst-case write, with the intended card.
2. The same, with the radio transmitting, against the Pico regulator's capability.
3. MISO behaviour with CS inactive, on the shared bus with the RA-02.
4. Bus pull-up presence and values.
5. Decoupling adequacy under the measured write transient.

Until items 1 to 3 are done, the module may be bench-tested on the shared bus but the
power budget is not closed.

## Related documents

- [Receiving Inspection Record](receiving-inspection.md) — section C.6, where this board was identified from its photographs
- [Hardware Reference](hardware.md) — the single hardware database
- [Electrical Architecture](../design/electrical-architecture.md) — the power tree this simplifies
- [Wiring](../design/wiring.md) — the bring-up order
