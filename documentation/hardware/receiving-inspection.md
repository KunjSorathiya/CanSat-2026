# Receiving Inspection Record

The parts have arrived. This is where what was *ordered* becomes what is *held*, and it is
the only document in this project allowed to say a board-level fact was verified on the
bench.

Everything in [hardware.md](hardware.md) marked `TBD` is marked that way for one reason: a
chip datasheet does not describe a breakout board, and a supplier listing is procurement
identity, not a schematic. The boards are now on the desk. Most of those `TBD`s can be
closed by looking at them — with a camera, a magnifier and a multimeter, before any power
is applied.

**Fill in the value, the date, and who recorded it.** A blank row is honest. A row filled
in from the supplier page rather than from the board in your hand is exactly the failure
this project's documentation rules exist to prevent.

---

## Contents

- [The rule](#the-rule)
- [Status vocabulary](#status-vocabulary)
- [Part A · Inventory against the BOM](#part-a--inventory-against-the-bom)
- [Part B · Photographs](#part-b--photographs)
- [Part C · Per-board identification](#part-c--per-board-identification)
- [Part D · The four blocking questions](#part-d--the-four-blocking-questions)
- [Part E · Closing a TBD](#part-e--closing-a-tbd)
- [Findings](#findings)
- [Sign-off](#sign-off)

---

## The rule

**No component is powered until Parts A, B and C are complete for that component, and
Part D is answered for anything it shares a rail or a bus with.**

This document ends where [bring-up-record.md](../testing/bring-up-record.md) begins.
Receiving inspection is unpowered: identity, markings, pin labels, connector geometry,
continuity. The bring-up record is powered: rails, currents, logic levels, timing. The
split is the point — a wrong assumption found with a magnifier costs nothing, and the same
assumption found with a battery connected can cost a board.

Work Part A completely first. A missing or wrong item found on day one is a supplier
conversation; found in week three it is a schedule problem.

---

## Status vocabulary

[pre-procurement-design-status.md](pre-procurement-design-status.md) defines four statuses.
This document adds the fifth — the one that could not exist before the parts arrived:

- **VERIFIED FROM HARDWARE** — observed on the delivered board, with the observation, the
  date, the person, and a photograph or measurement behind it.

`VERIFIED FROM DOCUMENTATION` is not upgraded to this in bulk. Each item is promoted
individually, by being looked at.

---

## Part A · Inventory against the BOM

Count and condition only. Do not open antistatic bags further than needed to read a label,
and do not connect the battery to anything.

| # | Item | SKU | Ordered | Received | Physical condition | Notes |
|---|---|---:|---:|---:|---|---|
| A.1 | Raspberry Pi Pico | 894292 | 2 | **2** | No visible damage | **Headers not fitted, and none supplied in the photograph.** Genuine board, `© 2020` silkscreen |
| A.2 | SX1278 RA-02 LoRa module | 1150780 | 2 | **2** | No visible damage | Headers loose in the bag, 2 × 8-pin. See [D.3](#d3--ra-02-carrier) |
| A.3 | 433 MHz LoRa antenna | 1121334 | 2 | **2** | No visible damage | White rubber-duck, hinged base. See [D.2](#d2--antenna-sma-or-rp-sma) |
| A.4 | IPEX1 to SMA cable, 10 cm RG1.13 | 1674982 | 2 | **2** | No visible damage | Supplied with panel nut, plain washer and star washer |
| A.5 | MPU-9250 module | 2846 | 1 | **1** | No visible damage | **Delivered as MPU-9250/6500, not the MPU-6050 the BOM named.** Header loose, 10-pin |
| A.6 | NEO-6M GPS with EPROM | 11782 | 1 | **1** | No visible damage | Active patch antenna **supplied and already fitted** to the u.FL socket |
| A.7 | GY-BMP280-3.3 | 835813 | 1 | **1** | No visible damage | Purple 6-pin board. Header loose, 6-pin. See [F-4](#findings) |
| A.8 | Micro SD card reader module | 11566 | 1 | **1** | No visible damage | **3.3 V board, not the 4.5–5.5 V one the listing described.** Header loose, 6-pin. See [D.1](#d1--the-microsd-reader-sku-11566) |
| A.9 | 1S 3.7 V 1500 mAh 25C LiPo | 1125094 | 1 | **1** | Pack flat, no puffing visible | **Delivered as Pro-Range, not Orange.** Do not charge yet. See [D.4](#d4--battery) |
| A.10 | Universal prototype PCB, 10 x 10 cm | 1031002 | 2 | **2** | No visible damage | Single-sided, isolated pads, edge rails |
| A.11 | LM393 sound detection sensor | TBD | 1 | **1** | No visible damage | **Second batch, received 2026-09-06.** Four-pin board: `AO DO GND VCC`. See [D.5](#d5--the-lm393-sound-module) |
| A.12 | Electrolytic capacitors, 10 µF and 100 µF | TBD | — | **As ordered, counted by hand 2026-09-06** | No visible damage | Second batch. 50 V and 25 V parts, sleeve print legible. The photograph shows one of each for identification and is **not** the count — see [D.7](#d7--the-capacitors) |
| A.13 | Ceramic capacitors, 0.1 µF `104` | TBD | 20 | **~20, counted by hand 2026-09-06** | No visible damage | Second batch. Orange discs, **print worn illegible**. **Three** are consumed by the build as fitted, so there is ample margin. See [D.7](#d7--the-capacitors) |
| A.14 | Resistors: 100 kΩ, 33 kΩ, 1 kΩ | TBD | — | **~10 of each, taped in three groups** | No visible damage | Second batch. **Values read from the colour bands** in the three close-ups — see [D.6](#d6--the-resistors). **No 330 Ω arrived**; 1 kΩ serves both LEDs instead |

> **Counted by hand on 2026-09-05 / KS. Every line matches the quantity ordered.** The
> photographs show one unit of each item, and a photograph of one board is never evidence that
> two arrived — A.1, A.2, A.3, A.4 and A.10 were each ordered in pairs and each pair is
> present. Nothing is short, and nothing arrived that was not ordered.

Not on the BOM, but Part C cannot be completed without them. Record what you actually have:

| Tool | Have it? | Notes |
|---|---|---|
| Multimeter with continuity | **Yes, the second one** | The original meter's resistance range is faulty — see the note below; its Ω readings are discarded. **A replacement was obtained 2026-09-05.** Short its probes and confirm ≤0.5 Ω, steady, before it is used for anything |
| Magnifier or phone macro lens | | Required to read regulator and level-shifter markings |
| Soldering iron and solder | **Yes** | Headers arrive loose on most of these boards. All headers fitted 2026-09-04 |
| Power switch and LEDs | **Yes, obtained 2026-09-06** | An I/O switch and **one red and one green 5 mm LED** (colours recorded 2026-09-09; the batch was described only as "two colours" until then, and the colour is what decides whether a 1 kΩ series resistor works — see [electrical-architecture.md](../design/electrical-architecture.md#the-indicator-leds)). Both are rulebook items: the switch is mandatory, and the power LED must light **immediately on power-on**, so it is wired across the 3.3 V rail through a resistor and never from a GPIO |
| microSD card | **Yes, HP mx310 64 GB, obtained 2026-09-05** | Not on the BOM, and none was in the reader's bag in `11566-sd-reader-front.jpg`. **This row said 32 GB until 2026-09-07 and was wrong** — the purchase list and bring-up rows 6.1/6.2 have said 64 GB throughout, and the card in the slot is 64 GB. That makes it **SDXC, not SDHC**, which changes nothing the driver does: row 6.2 confirmed block addressing via `high_capacity()`, which is what that row actually tests. It does mean the card ships exFAT and has to be reformatted — see [Gate 6](../testing/bring-up-record.md#gate-6--storage) |
| USB micro-B cable, data-capable | | For the Pico. A charge-only cable is a classic wasted afternoon |
| 1S LiPo charger | **Yes — USB-powered 1S charger, obtained 2026-09-05** | Not on the BOM. **Never charge a LiPo without one.** Being 1S there is no cell balancing to do, so a single-cell CC/CV charger is the right part. **Record its charge current and its output connector** — the pack's leads are JST-RCY and JST-XH, and the charger must mate with one of them or via the new pigtail. Check it terminates at 4.20 V |

> Anything answered "no" here is a purchase to make today, not on the day it blocks work.

> **The meter is part of the evidence chain, so its condition is recorded here too.** On
> 2026-09-05 the resistance range produced ~51 Ω between *every* pair of pins on the
> MPU-9250, including pins with no possible connection, and 18 Ω across its own shorted probe
> tips — climbing steadily from zero, on a new battery. Those numbers described the
> instrument, not the board, and **none of them are recorded in Part C.** The MPU-9250 is not
> implicated by them.
>
> Continuity and DC volts were verified working and were used for [C.6.9](#c6--micro-sd-card-reader-sku-11566),
> [C.7.6](#c7--antenna-and-ipex-cable), [C.7.7](#c7--antenna-and-ipex-cable),
> [C.8.3](#c8--lipo-battery), [C.8.5](#c8--lipo-battery) and [C.9.5](#c9--prototype-pcb-quantity-2).
> **The four strap rows were never taken with a meter at all**, and they did not need to be.
> `C.3.6`/`C.3.7` and `C.4.5`/`C.4.6` were closed on 2026-09-05 by the address each device
> answers at on a live bus scan: a part replying at `0x68` can only have AD0 low, and one
> replying at `0x76` can only have SDO low. **That is better evidence than a resistance
> reading** — it is the strap's effect rather than its cause, measured through the same bus the
> firmware will use.
>
> **What closing those four rows by address does *not* establish is impedance.** A part answering
> at `0x76` proves `SDO` is *low*; it says nothing about whether it is low through a 10 kΩ resistor
> or tied hard to ground — and the difference matters the moment anyone drives that pin. A
> resistor strap overridden with 3.3 V costs ~330 µA and moves the address; **a hard tie driven to
> 3.3 V is a short across the rail.** C.4.7 and C.3.8 exist to close that with a meter, now that a
> working one is available. Neither pin has any reason to be connected on this vehicle — both are
> strapped correctly and there is one of each part — so the cheapest control is to fit **4-pin
> strips** and leave those positions with no pad to land on.
>
> **Before trusting any meter here, short its probes: it must read ≤0.5 Ω and hold steady.**

---

## Part B · Photographs

Photographs are the evidence behind every claim in Part C, and the only way a reviewer — or
you in three months — can check a transcription without unbolting the vehicle.

Store them in `photos/`, named:

```text
<sku>-<short-name>-<view>.jpg      e.g. 11566-sd-reader-front.jpg
                                        11566-sd-reader-back.jpg
                                        1121334-antenna-connector.jpg
```

For every board: front, back, and a close-up of every marked component — regulators, level
shifters, controllers, crystal, address straps. For connectors: a straight-on shot of the
mating face. Include something for scale, keep the label text in focus, and shoot in even
light. A photograph in which you cannot read the regulator's part number has recorded
nothing.

| # | Subject | Views required | Taken | File(s) |
|---|---|---|---|---|
| B.1 | Pico #1 and #2 | Front, back, board marking | 2026-09-04 | `894292-pico-front.jpg`, `894292-pico-back.jpg` |
| B.2 | RA-02 #1 and #2 | Front, back, header labels, RF connector | 2026-09-04 | `1150780-ra02-front.jpg`, `1150780-ra02-back.jpg`, `1150780-ra02-antenna-mated.jpg` |
| B.3 | Antenna | Both ends, connector mating face | Partial | `1121334-antenna-front.jpg` — **mating face not shot straight on**, see [F-5](#findings) |
| B.4 | IPEX cable | Both ends, close-up of each connector | Partial | `1674982-ipex-sma-cable.jpg` — **neither centre contact resolvable**, see [F-5](#findings) |
| B.5 | MPU-9250 | Front, back, regulator, pull-ups, AD0 strap | 2026-09-04 | `2846-mpu9250-front.jpg`, `2846-mpu9250-back.jpg` — regulator marking illegible |
| B.6 | NEO-6M | Front, back, controller, regulator, antenna socket | 2026-09-04 | `11782-neo6m-front.jpg`, `11782-neo6m-back.jpg` — regulator marking illegible |
| B.7 | GY-BMP280-3.3 | Front, back, regulator, SDO strap | 2026-09-04 | `835813-bmp280-front.jpg`, `835813-bmp280-back.jpg` — die marking illegible, but the **package footprint is measurable** and identifies the part, see [F-4](#findings) |
| B.8 | microSD reader | Front, back, **every** component marking | 2026-09-04 | `11566-sd-reader-front.jpg`, `11566-sd-reader-back.jpg` |
| B.9 | LiPo | Full label, connector, both faces | 2026-09-04 | `1125094-lipo-front.jpg`, `1125094-lipo-back.jpg` |
| B.10 | Prototype PCB | Front, back, copper pattern close-up | 2026-09-04 | `1031002-protoboard-front.jpg`, `1031002-protoboard-back.jpg` |

Two re-shoots are outstanding, both of them macro shots the phone can take today:

1. **The antenna's mating face and the cable's SMA mating face, straight on.** Without them
   [D.2](#d2--antenna-sma-or-rp-sma) cannot be closed on nomenclature, only on fit.
2. **The MPU-9250 and NEO-6M regulator markings.** Both are SOT-23 parts whose text is below
   this photograph's resolution, and they are the only two supply questions the vehicle has
   left.

The third — **the BMP280 die marking** — has been struck off, but not because it was read.
The die text is still illegible; the part was identified from its **package outline**
instead, which this photograph does resolve. See [C.4.1](#c4--gy-bmp280-33).

> **What "illegible" now means here.** Each remaining marking was re-examined at the file's
> full 12 MP resolution, cropped to the bare package and enhanced (grayscale, autocontrast,
> histogram equalisation, unsharp mask) before being called illegible. On the two regulators
> there is no character-shaped structure to recover at all — the limit is the capture, not
> the processing, and no amount of further work on **these files** will produce a part
> number. A macro re-shoot is the only route.
>
> The antenna and cable are a **different failure, and an easier one to fix**: those files are
> sharp, but both connectors are photographed side-on, so the bore is not in view at any
> resolution. That is a framing problem, not a lens problem — the same phone, turned to face
> the mating end, closes [C.7.1](#c7--antenna-and-ipex-cable) and [C.7.2](#c7--antenna-and-ipex-cable).

---

## Part C · Per-board identification

These rows come from
[section 5, the post-procurement verification plan](pre-procurement-design-status.md#5-post-procurement-verification-plan).
Every row is unpowered: reading, measuring geometry, or checking continuity on the meter's
continuity range. Rows needing a live rail belong in the bring-up record instead.

> **Rows dated `Photo 2026-09-04` were transcribed from the photographs in [`photos/`](photos/),
> not from a supplier page.** They are silkscreen text, component packages and physical
> geometry — the things a camera can actually establish. **Every row that needs a meter is
> still blank**, because a photograph cannot show continuity, a strap's direction, or a
> voltage. Do not fill those in from the pattern of the rows around them.

### C.1 · Raspberry Pi Pico, quantity 2

| # | Record | How | Value | Date/by |
|---|---|---|---|---|
| C.1.1 | Board marking and revision | Read the silkscreen | `Raspberry Pi Pico © 2020`; RP2040 marked `RP2-B2`; laminate code `DC-136 94V-0` | Photo 2026-09-04 |
| C.1.2 | Pin labels match the datasheet pinout | Compare board with datasheet | Yes — GP0–GP28, `GP26_A0`/`GP27_A1`/`GP28_A2`, `ADC_VREF`, `AGND` all printed on the underside | Photo 2026-09-04 |
| C.1.3 | VSYS, VBUS, 3V3, 3V3_EN, RUN, GND present and undamaged | Visual | All six present and legible on the underside | Photo 2026-09-04 |
| C.1.4 | Debug pads present | Visual | Yes — `SWCLK`, `GND`, `SWDIO` as a 3-pad row on the underside, plus `TP1`–`TP6` | Photo 2026-09-04 |
| C.1.5 | Headers fitted, or to be soldered | Visual | **As delivered: not fitted, none supplied**, through-holes and castellations bare. **Headers bought separately and soldered to both Picos on 2026-09-04**; joints inspected and adjacent-pin isolation checked before any power | Photo 2026-09-04; fitted 2026-09-04 / KS |
| C.1.6 | USB connector condition | Visual | Micro-B, intact, no bent shell | Photo 2026-09-04 |

> C.1.5 was a purchase, not an observation: two 20-pin strips per Pico. **Both Picos are now
> headered**, which settles one half of the mounting question — soldering the vehicle Pico
> flat to the prototype board, castellations down, is no longer an option. **The other half
> was settled on 2026-09-05: the pins are passed through the prototype board and soldered
> directly**, so no socket has to be retained against launch loads. See
> [Module mounting](../design/wiring.md#module-mounting).
>
> This is a **Pico, not a Pico W** — no radio module, and the `DEBUG` pad row sits where the
> W's antenna would be. Nothing in this project wants Wi-Fi, but it is worth having recorded.

### C.2 · SX1278 RA-02, quantity 2

| # | Record | How | Value | Date/by |
|---|---|---|---|---|
| C.2.1 | Board marking and revision | Silkscreen | Carrier: `LoRa-02 SX1278 433MHz`, headers `J1`/`J2`. Shield: `Ra-02`, `ISM:410-525MHz`, `LoRa/FSK/OOK`, `PA:+18dBm` | Photo 2026-09-04 |
| C.2.2 | **Header pin order, exactly as printed** | Transcribe every pin, in order | See the transcription below. 2 × 8 pins | Photo 2026-09-04 |
| C.2.3 | NSS/CS, RESET, DIO0, DIO1 present and labelled | Silkscreen | All present: `NSS`, `RST`, `DIO0`, `DIO1`. `DIO2`–`DIO5` also broken out | Photo 2026-09-04 |
| C.2.4 | SCK, MOSI, MISO labels | Silkscreen | All three present and printed as `SCK`, `MOSI`, `MISO` | Photo 2026-09-04 |
| C.2.5 | Onboard regulator present? Part marking | Magnifier | **None.** Only `C1` and `C2` outside the shield; the supply pin is printed `3.3V` | Photo 2026-09-04 |
| C.2.6 | Onboard level shifter present? Part marking | Magnifier | **None visible.** Carrier is a breakout, not a translator | Photo 2026-09-04 |
| C.2.7 | Antenna connector type: IPEX/u.FL or SMA | Visual, compare with the cable | **IPEX / u.FL socket**, board-edge, next to the `GND` end of J2 | Photo 2026-09-04 |
| C.2.8 | Supply pin labelled 3.3 V, or a range | Silkscreen | `3.3V` — a single value, no range | Photo 2026-09-04 |

Transcribed header order, taken with the **u.FL connector at the top right**:

```text
J2 (row nearest the u.FL socket, reading away from it)
  GND   GND   3.3V   RST   DIO0   DIO1   DIO2   DIO3

J1 (opposite row, reading from the same end)
  GND   NSS   MOSI   MISO   SCK   DIO5   DIO4   GND
```

> This is the canonical Ra-02 arrangement, and it is now confirmed against the delivered
> board rather than assumed. Note that **the supply pin sits third from the u.FL end, with
> `GND` immediately before it and `RST` immediately after** — a one-pin offset when the module
> is pressed into the prototype board puts 3.3 V onto `RST` one way, or a `GND` pin onto the
> rail the other. Mark pin 1 on the board before wiring.
>
> **`PA:+18dBm` on the shield is not the 17 dBm the link budget assumes.** The shield states
> the module's PA capability, not the configured output;
> [`link_profile.hpp`](../../firmware/common/include/cansat/link_profile.hpp) sets what is
> actually transmitted. The two are not in conflict, but the 1 dB is worth knowing about
> before anyone reads a range measurement as a link-budget failure.
>
> `ISM:410-525MHz` comfortably contains 433 MHz.

> C.2.2 matters most on this board. RA-02 carriers ship with more than one header
> arrangement, and the module has no reverse-polarity protection worth relying on.
> Transcribe the printed order; do not copy a pinout from a web image.

### C.3 · MPU-9250

| # | Record | How | Value | Date/by |
|---|---|---|---|---|
| C.3.1 | Board marking, MPU-9250 breakout or other | Silkscreen | Front `MPU-9250/6500`; back `GY-6500  GY-9250` and `V356` | Photo 2026-09-04 |
| C.3.2 | **IC marking on the die itself: MPU-9250, MPU-9255, or MPU-6500** | Magnifier, **then `WHO_AM_I`** | Die reads **`MP92`** / `163LA1` / `1719` on a 24-pin QFN, which is the MPU-9250 marking. **The part itself disagrees: `WHO_AM_I` returns `0x70` — an MPU-6500. Six axes, no magnetometer.** The register wins; see the note below | Photo 2026-09-04; **register read 2026-09-05 / KS** |
| C.3.3 | Pin labels: VCC, GND, SCL, SDA, XDA, XCL, AD0, INT | Silkscreen | 10 pins. Front: `VCC GND SCL SDA EDA ECL AD0 INT NCS FSYNC`. Back names two of them dually: `SCL/SCLK`, `SDA/SDI`, `ADD/SDO` | Photo 2026-09-04 |
| C.3.4 | Onboard regulator present? Part marking | Magnifier | **Yes** — one **SOT-23-5** beside the `VCC` pin: three pads one side, two the other, counted at full sensor resolution. **Marking still not legible**, at any enhancement this photograph supports. The package class rules out SOT-223 | Photo 2026-09-04 |
| C.3.5 | Bus pull-ups fitted? Marked value | Magnifier | **Yes** — five resistors marked `103` (10 kΩ), grouped around the `SCL`/`SDA` and `AD0`/`INT`/`NCS` pins, plus unmarked 0402 passives and one tantalum marked `C106` (**10 µF**) beside the regulator | Photo 2026-09-04 |
| C.3.6 | AD0 strapped high or low as delivered | Continuity to VCC/GND — **closed instead by the address the part answers at** | **LOW.** The IMU answers at `0x68` on a live bus scan, which is only possible with AD0 pulled low | Bus scan 2026-09-05 / KS |
| C.3.7 | Expected I2C address implied by C.3.6 | `0x68` or `0x69` | **`0x68`** — matches `Mpu9250::Options::address`. No firmware change needed | Bus scan 2026-09-05 / KS |
| C.3.8 | INT exposed on the header | Visual | **Yes**, `INT` is on the header — GP7 in the pin map is real | Photo 2026-09-04 |
| C.3.9 | Silkscreen axis arrows present? Which way do X, Y and Z point? | Visual, photograph | **Yes** — an axis cross is printed beside the die. With the board component-side up and the pin header on the right: **X points away from the header, Y towards the `VCC` end, Z out of the board** | Photo 2026-09-04 |

> **C.3.2 was answered wrongly from the photograph, and the register overturned it on
> 2026-09-05.** The silkscreen hedges (`MPU-9250/6500`, and the back carries *both* board
> names). The die reads `MP92`, which is the MPU-9250 marking. But `WHO_AM_I` returns
> **`0x70`** — an MPU-6500. **There is no magnetometer in this package**, and the second bus
> scan confirms it: `0x0C` does not appear after `INT_PIN_CFG.BYPASS_EN` is set, because there
> is nothing behind the bridge to answer.
>
> The old note said "`WHO_AM_I` at bring-up is still the final word — a photograph of a
> package is not a register read." That was correct, and it is why this row was never signed
> off on the photograph alone. **The caveat did its job; the conclusion above it did not.**
>
> Two readings of the discrepancy, and the vehicle behaves the same under both: the die text
> was misread at that resolution (`MP92` against `MP65` is four characters of laser marking
> near the limit of the capture), or the die is genuinely remarked. Either way the silicon
> answers `0x70`, and what the silicon answers is what flies. Recording it as unresolved is
> more honest than picking one.
>
> **What this costs.** The AK8963 and the whole absolute-yaw path do not apply to this
> vehicle. Yaw is gyro-integrated and will drift; telemetry reports `YR-G`, never `YR-M`. The
> firmware already handles this deliberately — it accepts `0x70` as a six-axis part rather
> than refusing to boot — so the vehicle flies degraded and says so. That design decision has
> now been exercised on real hardware rather than argued about.
>
> C.3.4: the regulator's presence is what matters for the rail decision, and it is visible.
> Its identity decides the input range, and that needs a macro re-shoot or a measurement.
>
> C.3.5: **five 10 kΩ pull-ups on this board and four more on the BMP280** both hang on the
> same I2C0 pair. Two 10 kΩ pull-ups in parallel is 5 kΩ, which is still a legal bus but a
> stiffer one than either board was designed around. Record the sink current at bring-up
> before assuming it is harmless.
>
> Two of those five read `E0I` rather than `103`. **They are the same part, placed rotated
> 180°** — `103` upside down is `E0I`. Do not record a phantom component from them.
>
> The `C106` tantalum is the regulator's bulk output capacitor, and it is worth knowing about
> for a different reason than decoupling: **10 µF of tantalum across the LDO output is an
> inrush load at switch-on**, on a rail that [C.3.4](#c3--mpu-9250) shows is fed from the
> board's own regulator rather than straight from `VCC`.
>
> C.3.9 is recorded from the printed cross, and orientation read off a photograph is easy to
> get wrong. Confirm it against the board in your hand before the airframe is built around it.

> C.3.2 was the row that mattered most on this board, and it is now answered: **this is one
> of the MPU-6500 dies sold as an MPU-9250.** Pin-compatible, electrically identical for the
> accelerometer and gyroscope, and with **no magnetometer at all**. The silkscreen was never
> evidence. The die marking looked like evidence and was not. `WHO_AM_I` = `0x70`, read on the
> bench 2026-09-05, is the evidence — and it is recorded in bring-up gate 8.8.
>
> The accelerometer and gyroscope on this part measure well: |a| 9.8675 m/s² with a 0.0092
> standard deviation, gyro noise between 0.096 and 0.142 dps against a 2 dps limit, 100 valid
> samples out of 100. **Nothing is wrong with the part that arrived. It is simply not the part
> the listing described**, and one of its three sensors does not exist.
>
> C.3.9 matters because the firmware's body frame is defined against those arrows. The
> AK8963 magnetometer inside the package has its own, different axes; the firmware already
> corrects for that, but it can only be checked against a board whose printed axes are
> recorded.

### C.4 · GY-BMP280-3.3

| # | Record | How | Value | Date/by |
|---|---|---|---|---|
| C.4.1 | Board marking; confirm BMP280, **not** BME280 | Silkscreen, die marking, package footprint, **then chip ID** | **BMP280, confirmed by register.** Chip ID `0xD0` returned **`0x58`** on 2026-09-05 — a BME280 answers `0x60`. The photograph agreed: silkscreen and die text both fail — shared `GY-BM ☐E/☐P 280` artwork with neither box legibly ticked, and a laser mark below this photograph's resolution. **The package outline settles it instead: 2.04 × 2.50 mm measured**, against 2.00 × 2.50 mm for a BMP280 and 2.50 × 2.50 mm for a BME280. See [F-4](#findings) | Photo 2026-09-04; **register read 2026-09-05 / KS** |
| C.4.2 | Pin count and labels: 4-pin I2C or 6-pin I2C/SPI | Silkscreen | **6 pins**, in order `VCC GND SCL SDA CSB SDO` — the I2C/SPI variant, not the 4-pin I2C-only board | Photo 2026-09-04 |
| C.4.3 | Onboard regulator present? Part marking | Magnifier | **None.** The board carries only the sensor, four resistors and two capacitors — consistent with the 3.3 V-only `GY-BMP280-3.3` the BOM ordered | Photo 2026-09-04 |
| C.4.4 | Bus pull-ups fitted? Marked value | Magnifier | **Yes** — four resistors marked `103` (10 kΩ) | Photo 2026-09-04 |
| C.4.5 | SDO strapped high or low as delivered | Continuity to VCC/GND — **closed instead by the address the part answers at** | **LOW.** The barometer answers at `0x76`, which requires SDO pulled low. **Level only — the strap's *impedance* was never measured** and remains unknown, see the note below | Bus scan 2026-09-05 / KS — level closed, impedance open |
| C.4.6 | Expected I2C address implied by C.4.5 | `0x76` or `0x77` | **`0x76`** — matches `Bmp280::Options::address` and what wiring.md assumes. No firmware change needed | Bus scan 2026-09-05 / KS |
| C.4.7 | **`SDO` strap impedance** — resistor or hard tie | ~10 kΩ, or near 0 Ω | Resistance from `SDO` to `GND` on the module, unpowered | | |
| C.3.8 | **`AD0` strap impedance** on the IMU — the same question | ~10 kΩ, or near 0 Ω | Resistance from `AD0` to `GND` on the module, unpowered | | |

> C.4.3 is the useful half of this table: **no regulator means no 5 V tolerance.** This board
> must be fed 3.3 V, which is what the design already intends, and feeding it from a 5 V
> source would destroy it. That is now a verified constraint rather than a hopeful reading of
> the product name.
>
> C.4.1 was closed by measuring the part rather than by reading it, because the two candidates
> differ in **shape** and not only in text. Both are LGA-8 under a metal lid, but a BMP280 is
> **2.00 × 2.50 mm** and a BME280 is **2.50 × 2.50 mm** — one rectangular, one square.
>
> The method, so the number can be re-checked or overturned: in `835813-bmp280-front.jpg` the
> six header pads set an in-frame scale of **89.6 px per 2.54 mm, i.e. 35.3 px/mm**, taken
> across the whole pad 1 → pad 6 span so one mis-centred pad cannot move it. The sensor's lid
> fills a 78 × 93 px bounding box; the board lies about 4° off axis, and un-rotating that box
> gives a true **71.9 × 88.1 px = 2.04 × 2.50 mm**. The aspect ratio alone — **0.82**, and
> free of any scale assumption — already separates a BMP280's 0.80 from a BME280's 1.00; the
> absolute figures then agree to 2 %. The short side would have to be wrong by 16 px, several
> times the edge blur, for this part to be a BME280.
>
> **This is still a measurement from a photograph, not a register read.** A BME280 answers
> chip-ID `0x60` where a BMP280 answers `0x58`, and it also reports humidity that the
> telemetry format has no field for. Read the chip ID at bring-up and record it here; that,
> not this paragraph, is the final word. **Do not resolve this row from the product name.**

> C.4.6 must agree with the `0x76` that the wiring and bring-up documents assume. If the
> board straps SDO high, either the strap or the document changes — decide deliberately and
> record which.

### C.5 · NEO-6M GPS

| # | Record | How | Value | Date/by |
|---|---|---|---|---|
| C.5.1 | Board marking and revision | Silkscreen | `GY-NEO6MV2` | Photo 2026-09-04 |
| C.5.2 | Controller marking: NEO-6M and variant | Magnifier | `u-blox` `NEO-6M-0-001`, lot `1702`, serial `2422187473 8`, `0300 3`. Genuine u-blox label with data matrix | Photo 2026-09-04 |
| C.5.3 | Pin labels and order: VCC, GND, TX, RX | Silkscreen | **4 pins, printed `VCC RX TX GND`** — `VCC` at one end, `GND` at the other, `RX` and `TX` between them in that order | Photo 2026-09-04 |
| C.5.4 | Onboard regulator present? Part marking | Magnifier | **Yes** — one **SOT-23-class** package beside the header, **measured 1.4 × 2.6 mm** against this board's own 2.54 mm header pitch. **Marking still not legible.** The size rules out SOT-223 | Photo 2026-09-04 |
| C.5.5 | Stated supply range on the silkscreen, if any | Silkscreen, **then operation** | **None printed** — the board states no voltage anywhere. **Answered by running it: the module works on 3.3 V from the Pico's 3V3 rail.** Clean NMEA at 9600 baud, and it acquired a satellite indoors, so the receiver and its antenna path are both live | Photo 2026-09-04; **operated 2026-09-05 / KS** |
| C.5.6 | Antenna connector type; patch antenna supplied? | Visual | **u.FL / IPEX socket**, and the **active patch antenna is supplied and already mated** | Photo 2026-09-04 |
| C.5.7 | Backup battery or supercapacitor present? | Visual | **Yes** — a coin cell on its side next to the module, plus a `24C32A` (`FT5N4D`) 8-pin EEPROM. This is the "with EPROM" the BOM named | Photo 2026-09-04 |

> **C.5.5 is closed, and closed by observation rather than by the listing.** On 2026-09-05 the
> module was powered from the Pico's own 3V3 output and produced clean, checksum-valid NMEA at
> 9600 baud within seconds — `$GPRMC`, `$GPVTG`, `$GPGGA`, `$GPGSA`, `$GPGSV`, `$GPGLL`, one
> full cycle per second. A module browning out below its 2.7 V floor does not emit
> well-formed sentences at the right baud rate for minutes on end.
>
> **It also proved the antenna path**, which no supply test had to. Within five seconds
> `$GPGSV,1,1,01,04,,,28` reported one satellite in view — PRN 04 at 28 dB-Hz — from indoors.
> The receiver is not merely talking, it is hearing.
>
> **C.5.4 stays open and is now only a curiosity.** The regulator's part number is still
> unread, so its rated input range is still unknown. That mattered when it decided whether the
> module could be fed 3.3 V at all; it no longer does, because the module has been fed 3.3 V
> and works. Identify it if a macro shot is ever taken, but nothing waits on it.
>
> **The vehicle now has exactly one supply question left, and it is not a voltage.** Every
> module is confirmed 3.3 V: the RA-02 and microSD by inspection, the BMP280 by the absence of
> a regulator, the IMU by operation, and the GPS by this. What Gate 2 still needs is current,
> not volts.
>
> **The supplier listing says "Supply voltage: 3.3 V". That does not close this row**, and it
> is recorded in [product-pages](product-pages/README.md) as procurement information rather
> than here as an observation. The same order's listings have now been contradicted four
> times, most sharply on 2026-09-05 when the module sold, silkscreened and die-marked as an
> MPU-9250 answered `WHO_AM_I` as an MPU-6500. **This document's first page says a row filled
> in from the supplier page is exactly the failure it exists to prevent.**
>
> **It does, however, make the module safe to try at 3.3 V**, which is a different claim and
> worth stating because it unblocks Gate 4:
>
> - The u-blox NEO-6M itself is a **2.7–3.6 V** part. Whatever the carrier's regulator is
>   for, the module behind it is native 3.3 V.
> - **The risk is asymmetric.** Feeding 3.3 V to a board that wanted 5 V produces a module
>   that does not start or browns out — it does not damage anything. Feeding 5 V to a 3.3 V
>   board destroys it. Only the harmless direction is being proposed.
> - **Its TX cannot exceed its own supply**, so a module powered at 3.3 V cannot present the
>   Pico's GP13 with more than 3.3 V. There is no path to a damaged Pico here.
>
> So the test is: power it from the Pico's 3V3, watch for NMEA. Sentences arriving settles the
> question by observation. **Silence is a real result too** — if the carrier's regulator has
> meaningful dropout, 3.3 V in could leave the module near its 2.7 V floor, and the symptom
> would be flaky or absent output rather than clean failure. Record whichever happens.
>
> C.5.7 explains a behaviour worth expecting: the backup cell holds almanac and time across
> power cycles, so the *first* cold fix after delivery will be far slower than every fix
> after it. Bring-up row 4.2 should be taken on a genuinely cold receiver, then repeated.

> C.5.3: **TX and RX on a GPS breakout name the board's own pins**, so the board's TX goes
> to the Pico's RX. Record the label, not your interpretation of it.

### C.6 · Micro SD card reader, SKU 11566

The highest-risk item in the BOM — [sd-module-analysis.md](sd-module-analysis.md) explains
why. Complete this section before anything else touches SPI0.

| # | Record | How | Value | Date/by |
|---|---|---|---|---|
| C.6.1 | Board marking and revision | Silkscreen | Unnamed micro-SD breakout; only `10K`, `U1` and a resistor reference are printed. **Not the 6-pin 5 V adapter the supplier listing describes** | Photo 2026-09-04 |
| C.6.2 | Pin labels and physical order | Transcribe, left to right | **6 pins, in order `GND MISO CLK MOSI CS 3V3`** — printed on the underside | Photo 2026-09-04 |
| C.6.3 | Regulator present? Full part marking | Magnifier | **None.** No SOT-23, no SOT-89, no regulator of any package on either face | Photo 2026-09-04 |
| C.6.4 | Level shifter present? Full part marking | Magnifier | **None.** No buffer, no translator IC, no transistors — the board has no active component at all | Photo 2026-09-04 |
| C.6.5 | If shifting is discrete: resistors, transistors, or a buffer IC? | Magnifier, trace the tracks | Not applicable — nothing is shifted. Signals run from the header to the socket | Photo 2026-09-04 |
| C.6.6 | Every resistor and capacitor marking | Magnifier | **Four resistors marked `103` (10 kΩ)**, silkscreened `10K`, and two unmarked capacitors. That is the entire parts list | Photo 2026-09-04 |
| C.6.7 | Stated input range on the silkscreen | Silkscreen | **No range printed. The supply pin is labelled `3V3`** — a single value | Photo 2026-09-04 |
| C.6.8 | Continuity: VCC pin to regulator input | Meter | Not applicable — there is no regulator | Photo 2026-09-04 |
| C.6.9 | Continuity: card supply to regulator output, or to VCC directly | Meter | **Direct, and nothing in between.** The `3V3` header pin beeps to exactly one socket leg; `GND` beeps to a different single leg. No other leg responds to either | Meter 2026-09-05 / KS |
| C.6.10 | Card retention: push-push, push-pull, or friction | Visual | **Friction / slide-in holder** — no spring eject, no hinged tray | Photo 2026-09-04 |

> **This table closes [D.1](#d1--the-microsd-reader-sku-11566), the question that has blocked
> the power design since the BOM was written.** The board has no regulator and no level
> shifter; its power pin is printed `3V3`; its only components are four 10 kΩ pull-ups and two
> capacitors. It runs from the same 3.3 V rail as everything else, and the boost stage the
> earlier design reserved for it is not needed and should not be built.
>
> C.6.9 stays open even so: confirming with a meter that the `3V3` pin reaches the socket's
> supply pad directly is a ten-second check, and it is the difference between believing the
> board has no regulator and knowing where the card's power comes from.
>
> **The absence of a buffer makes bring-up row 7.4 more important, not less.** On a board like
> this, nothing but the card itself releases MISO when `CS` goes high. If a card holds the
> line, it corrupts the *radio's* next transaction on the shared SPI0 bus, and the symptom
> looks like a dead radio. The 10 kΩ pull-up defines the line when nobody drives it; it
> cannot fight a card that is still driving.
>
> C.6.10 is a mechanical note with a flight consequence: a friction holder has no positive
> retention. On a vehicle that will be shaken, spun and landed hard, the card must be
> restrained by something other than the socket — tape, a clamp, or foam.

### C.7 · Antenna and IPEX cable

| # | Record | How | Value | Date/by |
|---|---|---|---|---|
| C.7.1 | Antenna connector: SMA or RP-SMA, male or female | Inspect the centre contact | **Shell is female**: a knurled coupling nut with internal threads. **Centre contact not resolvable** — no straight-on photograph exists | Photo 2026-09-04 |
| C.7.2 | Cable SMA end: SMA or RP-SMA, male or female | Inspect the centre contact | **Shell is male**: bulkhead body, external threads, hex flange, supplied with nut and star washer. **Centre contact not resolvable** | Photo 2026-09-04 |
| C.7.3 | Cable IPEX end variant | Compare with the RA-02 socket | Mates with the RA-02's u.FL socket, so it is the IPEX-1 / u.FL generation the BOM named | Photo 2026-09-04 |
| C.7.4 | Antenna and cable mate without force | Hand-tight, **no power** | **Yes — mated in `1150780-ra02-antenna-mated.jpg`**, threads fully engaged, no adapter | Photo 2026-09-04 |
| C.7.5 | IPEX end mates with the RA-02 socket | Gentle, **no power** | **Yes — snapped onto the module's u.FL socket in the same photograph** | Photo 2026-09-04 |
| C.7.6 | Cable continuity: centre to centre, shield to shield | Meter | **Both continuous.** Centre to centre and shield to shield each beep | Meter 2026-09-05 / KS |
| C.7.7 | Cable isolation: centre to shield reads open | Meter | **Open at both ends.** No short between centre and shield | Meter 2026-09-05 / KS |
| C.7.8 | Markings on antenna and cable | Read and photograph | **None.** Neither part carries any printed identification | Photo 2026-09-04 |

> **The whole RF chain has been shown to fit: antenna → SMA joint → 10 cm pigtail → u.FL →
> RA-02.** That is the practically important half of [D.2](#d2--antenna-sma-or-rp-sma), and it
> means no adapter has to be ordered.
>
> **It does not close the nomenclature question.** The BOM says SMA male, the supplier page
> says RP-SMA female, and the photographs settle only the shells — antenna female, cable male
> — which agrees with the supplier's *gender* and contradicts the BOM's. Whether the pair is
> SMA or RP-SMA depends on the centre contacts, and neither mating face was photographed
> straight on.
>
> This is not pedantry with a spare antenna in play: **an RP-SMA antenna screwed onto an SMA
> pigtail mates mechanically and connects nothing**, because the centres cannot meet. Two
> parts from the same order will always agree with each other; a replacement bought later may
> not. Photograph both mating faces before anyone orders a second antenna.

> **Telling SMA from RP-SMA:** gender is named by the threads, but polarity is set by the
> centre. Standard SMA male has outer threads and a centre **pin**; RP-SMA male has outer
> threads and a centre **socket**. Look at the middle, not the shell — this is precisely how
> the BOM and the supplier page came to disagree.
>
> C.7.7 matters more than it looks. A short between centre and shield presents the radio's
> power amplifier with a load it is not designed to drive.

### C.8 · LiPo battery

**Do not connect, charge, or load this battery until every row is filled in.**

| # | Record | How | Value | Date/by |
|---|---|---|---|---|
| C.8.1 | Full label text | Photograph and transcribe | **`Pro-Range` Lithium Polymer Battery**, "Smartly Flavoured Li-Po Battery", `Ω MATCHED`, `TRUE BALANCE`, **`1500` mAh**, **`1 Cell 3.7V 25C`**. Back: serial `18726 52478`, `Made in P.R.C`, CE and crossed-bin marks, "Check Genuinity on lipo.robu.in" | Photo 2026-09-04 |
| C.8.2 | Connector type: JST-PH 2.0, JST-XH, or other | Compare with a known connector | **Two leads. Main discharge: red 2-pin JST-RCY (BEC) style. Balance: white 2-pin JST-XH style.** Neither is JST-PH 2.0 | Photo 2026-09-04 |
| C.8.3 | Polarity: which lead is positive | **Meter, not wire colour** | **Red is positive**, read on the meter at the JST-RCY discharge lead. Colour and polarity agree on this pack — confirmed, not assumed | Meter 2026-09-05 / KS |
| C.8.4 | Protection circuit present? | Visual at the tab end | **Not visible.** Leads exit under the red heat-shrink; no protection board can be seen, and its absence cannot be proven from outside | Photo 2026-09-04 |
| C.8.5 | Open-circuit voltage as delivered | Meter across the terminals | **3.92 V** at the JST-RCY, as delivered and never charged | Meter 2026-09-05 / KS |
| C.8.6 | Stated continuous and burst discharge | Label | **Only `25C`**, i.e. a claimed 37.5 A continuous. No burst figure, no amps printed anywhere | Photo 2026-09-04 |
| C.8.7 | Stated charge current and cutoff | Label or manufacturer document | **Neither is printed.** The label says only "Charge Battery only with recommended charger" | Photo 2026-09-04 |
| C.8.8 | Physical damage, puffing, or smell | Visual | Pack flat, corners square, no puffing or discolouration visible | Photo 2026-09-04 |

> **The delivered pack is Pro-Range, not the Orange pack the BOM named.** Capacity, cell
> count, nominal voltage and C-rating all match, so this is a brand substitution rather than
> a wrong part — but every document that says "Orange" is now naming something that did not
> arrive. See [F-2](#findings).
>
> C.8.2 is the row with a shopping list attached. **Nothing in this project mates with either
> connector.** The main lead is a JST-RCY, which is physically large for a CanSat; the balance
> lead is the natural tap for a 1S charger. Buy a matching female pigtail rather than cutting
> and re-terminating a charged cell — and if both leads are ever cut at once, one slip shorts
> the pack.
>
> C.8.7 leaves the charge parameters undocumented by the manufacturer. A 1S LiPo charges at
> 4.20 V with a standard CC/CV profile, and the safe default is 1C — 1.5 A — but **that is
> general knowledge, not this pack's specification**, and it stays out of the tables until a
> Pro-Range document says it. Use a proper 1S balance charger and its own defaults.

> C.8.3: LiPo lead colours are conventional, not guaranteed, and a reversed 1S pack will
> destroy the Pico faster than you can disconnect it. Twenty seconds with a meter.
>
> C.8.5: a cell delivered below roughly 3.0 V, or visibly puffed, is one to set aside and
> raise with the supplier — not to charge.

### C.9 · Prototype PCB, quantity 2

| # | Record | How | Value | Date/by |
|---|---|---|---|---|
| C.9.1 | Dimensions | Ruler | Silkscreened `10*10CM`. Grid runs `A`–`Z` then `A`–`K` across (36 columns) and `01`–`35` down | Photo 2026-09-04 |
| C.9.2 | Hole pitch | Ruler over 10 holes, divide by 10 | Silkscreened `2.54MM` | Photo 2026-09-04 |
| C.9.3 | Single- or double-sided | Visual | **Single-sided.** Copper pads on one face only; the reverse is bare laminate carrying the coordinate silkscreen | Photo 2026-09-04 |
| C.9.4 | Pad pattern: isolated pads, strips, or bus rails | Visual | **Individually isolated round pads** — no strips, no linked rows. One row of elongated pads along each of the top and bottom edges | Photo 2026-09-04 |
| C.9.5 | Adjacent pads are isolated | Meter | **Isolated.** Adjacent pads read open, both horizontally and vertically. **The elongated edge rows are isolated pad-to-pad as well** — they are not a bus | Meter 2026-09-05 / KS |
| C.9.6 | Board thickness and mounting holes | Ruler, visual | **Four corner mounting holes. Thickness 1.6 mm**, measured on the delivered board | Photo 2026-09-04; **measured 2026-09-05 / KS** |

> C.9.3 and C.9.4 together decide how the vehicle is built: **every connection is a wire.**
> Isolated pads on a single-sided board give no power or ground rails, so a 3.3 V and a GND
> bus have to be created by hand — a soldered bus wire, or a run of bridged pads — before any
> module is placed. Plan those two runs first; retrofitting them under a populated board is
> unpleasant.
>
> **C.9.5 confirms it on the meter, edge rows included.** The elongated pads along the top and
> bottom edges were the one place a ready-made rail could have been hiding — they are isolated
> pad-to-pad like every other pad on the board. There is no rail anywhere on this PCB, so both
> bus runs are hand-built with no exceptions.
>
> The coordinate grid is worth using. Record each module's corner pad as, say, `F-12` in the
> assembly notes, and the layout survives being taken apart.
>
> **1.6 mm is the standard FR-4 thickness**, which is the convenient answer: ordinary M2 or
> M3 standoffs, spacers and nylon hardware are all dimensioned around it, and the mass
> budget can use the usual figure for a 100 × 100 mm single-sided board rather than an
> estimate. **C.9.6 was the last blank row in Part C.**

> C.9.4 changes how the vehicle is laid out — strip board and isolated-pad board are wired
> quite differently — so record it before planning placement.

---

## Part D · The four blocking questions

These four are why the electrical design is frozen. Each is answered by Part C rows you can
complete today, unpowered. Answering them is the whole value of the parts arriving.

### D.1 · The microSD reader, SKU 11566

**Blocks:** regulator selection, the power tree, Gate 2 of the bring-up record, and every
shared-SPI decision.

| Question | Answered by | Answer |
|---|---|---|
| Is there an onboard regulator, and what is it? | C.6.3, C.6.8 | **No regulator, of any package, on either face.** |
| Is the card fed from that regulator, or from VCC directly? | C.6.9 | **From the header directly** — there is nothing in between. Confirm the track with a meter. |
| Is there level shifting, and of what kind? | C.6.4, C.6.5 | **None.** No buffer, no translator, no transistors. Four 10 kΩ pull-ups and two capacitors are the whole board. |
| Does the module need 4.5 to 5.5 V, or will 3.3 V drive it? | All of the above | **3.3 V. The supply pin is printed `3V3` and nothing on the board could step a higher voltage down.** |

**Answered.** The second rail disappears from the power tree and no boost converter is
specified. The supplier listing that said 4.5–5.5 V described a different board from the one
that arrived, which is the entire reason this document exists.

Two consequences that are *not* simplifications:

- **The host must be 3.3 V, and now there is no margin for anything else.** With a level
  shifter the module tolerated a 5 V host; without one, the Pico's 3.3 V logic is not a
  convenience but a requirement, and a stray 5 V feed reaches the card directly.
- **Nothing buffers MISO.** Bring-up row 7.4 — MISO released when `CS` is high — moves from a
  precaution to the single most important row in the shared-bus gate. See [C.6](#c6--micro-sd-card-reader-sku-11566).

The module's write-transient current is now what the power budget waits on, and no
photograph can supply it.

### D.2 · Antenna: SMA or RP-SMA

**Blocks:** RF assembly, and any range test.

| Question | Answered by | Answer |
|---|---|---|
| What is actually on the antenna? | C.7.1 | **Female shell** (internal-thread knurled coupling). Centre contact unresolved. |
| What is actually on the cable? | C.7.2 | **Male shell** (external threads, hex flange, bulkhead nut). Centre contact unresolved. |
| Do they mate? | C.7.4 | **Yes**, hand-tight, no adapter. |
| Does the cable mate with the RA-02? | C.7.5 | **Yes**, onto the module's u.FL socket. |

**Answered for assembly, open for procurement.** The chain fits end to end — no adapter is
needed and none has to be ordered — which is what blocked RF assembly.

The nomenclature is still unsettled, and it now leans the supplier's way rather than the
BOM's: the antenna shell is **female**, contradicting the BOM's "SMA Male". Whether the pair
is SMA or RP-SMA turns on the centre contacts, and neither mating face was photographed
straight on.

Leave this open until those two photographs exist. **A replacement antenna ordered on the
BOM's description could screw on perfectly and radiate nothing**, because RP-SMA and SMA
mate mechanically and connect nothing.

### D.3 · RA-02 carrier

**Blocks:** freezing the GPIO map, and the schematic.

| Question | Answered by | Answer |
|---|---|---|
| What is the exact printed header order? | C.2.2 | **`GND GND 3.3V RST DIO0 DIO1 DIO2 DIO3` / `GND NSS MOSI MISO SCK DIO5 DIO4 GND`**, transcribed in [C.2](#c2--sx1278-ra-02-quantity-2). |
| Are RESET and DIO0 available? Is DIO1? | C.2.3 | **All three, on the header.** `DIO2`–`DIO5` are broken out too. |
| Is there a regulator or level shifter on the carrier? | C.2.5, C.2.6 | **Neither.** Supply pin printed `3.3V`; the carrier is a plain breakout. |
| Does the RF connector match the supplied cable? | C.2.7, C.7.5 | **Yes** — u.FL socket, mated in the photograph. |

**Answered.** Every pin the GPIO map reserves for the radio exists on the delivered board:
`NSS` for GP17, `RST` for GP20, `DIO0` for GP21 and `DIO1` for GP22. No pin has to move, and
the map's radio half can be frozen.

The absence of a regulator is the fact to carry forward: **this module is 3.3 V only, on both
its supply and its logic**, so it sits on the same rail as the Pico's 3.3 V output with no
translation anywhere. Its transmit-current transient on that shared rail is now the open
question, and it belongs to bring-up gate 2.

### D.4 · Battery

**Blocks:** every powered test, and the power budget.

| Question | Answered by | Answer |
|---|---|---|
| Which lead is positive? | C.8.3 | **Red, confirmed on the meter** at the JST-RCY. Colour and polarity agree on this pack. |
| Is there protection, or must the design provide cutoff? | C.8.4 | **No protection board visible**, and absence cannot be proven from outside. Assume the design must provide cutoff until shown otherwise. |
| Is the pack healthy as delivered? | C.8.5, C.8.8 | **Yes.** Flat, square, no puffing, and **3.92 V open-circuit** — a normal storage voltage, well clear of the ~3.0 V floor. |
| What are the real charge and discharge limits? | C.8.6, C.8.7 | **Only `25C` is printed.** No charge current, no cutoff, no burst figure. |
| Is there a charger for it? | Part A tools table | **Yes** — a 1S balance charger was bought separately, 2026-09-05. |

**Half answered.** The pack itself is now characterised: polarity read on the meter, and
**3.92 V open-circuit** — healthy, and not a cell to set aside. The two rows that needed
nothing but a multimeter are closed.

**The procurement half is now closed too.** A 1S balance charger and a matching JST-RCY
pigtail were bought on 2026-09-05, so the pack can be charged and connected.

**What remains is a design obligation, not a purchase.** No protection circuit is visible and
its absence cannot be proven from outside, so **the design must provide undervoltage cutoff**
until something shows otherwise. The label prints no charge current and no cutoff either, so
the charger's own 1S profile governs — use it and record what it does. Charge the pack on a
non-flammable surface, never unattended.

The delivery also changed the question. The pack is **Pro-Range, not Orange**, so the
manufacturer document this table has been waiting on is a Pro-Range document — and the label
carries no charge parameters at all. **A 1S charger is now a purchase on the critical path**,
because there is no safe way to bring this cell up to voltage without one.

---

## Part E · Closing a TBD

A recorded observation is only useful once it reaches the documents waiting on it. For each
item verified, in this order:

1. **[hardware.md](hardware.md)** — replace the `TBD` with the observed value. Cite the
   source as `physical inspection, <date>, <initials>` and link the photograph. This is the
   single hardware database; while it still says `TBD`, the project still believes `TBD`.
2. **[product-pages/README.md](product-pages/README.md)** — update the Status column
   wherever physical inspection resolves a supplier ambiguity.
3. **[pre-procurement-design-status.md](pre-procurement-design-status.md)** — move the item
   from `PENDING PHYSICAL VERIFICATION` to `VERIFIED FROM HARDWARE`, then re-check the
   section 6 design-freeze criteria it feeds.
4. **[electrical-compatibility.md](electrical-compatibility.md)** — revisit any assessment
   that assumed the value.
5. If it changes a pin, an address, or a constant: change
   `firmware/flight-computer/include/flight/config.hpp` first — the code is the truth — then
   [pico-gpio-map.md](pico-gpio-map.md) and [wiring.md](../design/wiring.md), then run
   `bash tools/build_host.sh`, which will tell you which document now contradicts the code.
6. **[CHANGELOG.md](../../CHANGELOG.md)** — one entry per inspection session, saying what
   was verified and what it unblocked.

Do not batch this to the end of the week. The gap between observing and recording is where
"I'm sure it was 3.3 V" comes from.

---

### D.5 · The LM393 sound module

Received 2026-09-06 and photographed as
[`lm393-sound-front.jpg`](photos/lm393-sound-front.jpg), cropped from the batch shot
[`passives-2026-09-06.jpg`](photos/passives-2026-09-06.jpg).

**Established from the photograph:**

| Fact | Evidence |
|---|---|
| **Four-pin board: `AO DO GND VCC`** | Header silkscreen, legible |
| Analogue output present | The `AO` label above |
| Comparator output present | The `DO` label above |
| LM393 comparator fitted | 8-pin SOIC on the board |
| Gain trimpot fitted | Blue multi-turn part beside the IC |
| Electret capsule fitted | Cylindrical can on its own stalk |
| Two indicator LEDs | Silkscreen `PWR-LED` and `DO-LED` |
| Comparator part marking | `LM393` over `49M` over `BWQ64` |
| Capsule polarity marked | `+` and `−` on the two capsule pads |
| Onboard SMD resistors | Marked `102` (1 kΩ) and `201` (200 Ω) |

**Why this mattered.** The firmware supports both board variants because they are sold under
one name and only the pin count separates them — see
[wiring.md](../design/wiring.md#gp27-and-gp15--the-lm393-sound-module). A three-pin board has
no `AO` at all, and reading an unconnected ADC pin does not return zero: it returns a
floating level that looks exactly like a quiet room. **This board is the four-pin variant, so
both channels are real and `sound_analog_connected` and `sound_gate_connected` are both
correct at their defaults.** That closes bring-up row 3.13a.

**Not established, and not to be guessed:**

- The board's own part-number silkscreen. The visible legend is functional — pin names,   LED names — and carries no board type or revision.
- Whether `AO` is buffered or is the raw capsule bias. It affects the level's scale, not
  whether it works, and the trimpot makes it unrecordable anyway — see the millivolts-not-
  decibels note in [sound_level.hpp](../../firmware/flight-computer/include/flight/sound_level.hpp).
- Whether the analogue output is buffered or is the raw capsule bias. Two SMD resistors   are visible (`102`, `201`) but the network cannot be traced from one face.

---

### D.6 · The resistors

Three groups arrived taped together and were photographed separately so the bands could be read. **The bands were then checked against the meter on 2026-09-06 and all three agree.** That order matters: the photograph says what the part claims, the meter says what it is, and only the second belongs in a design. The `GP26` divider is the one place a wrong value would have produced a plausible number rather than an obvious failure, so it is the one that most needed checking.

| Photograph | Body | Bands | Value — **band-read and metered, agreeing** | Use |
|---|---|---|---|---|
| [`resistors-100k-5pct.jpg`](photos/resistors-100k-5pct.jpg) | Beige, carbon film | brown‑black‑yellow‑gold | **100 kΩ ±5 %** | Spare. The divider uses the 1 % parts instead |
| [`resistors-33k-1pct.jpg`](photos/resistors-33k-1pct.jpg) | Blue, metal film | orange‑orange‑black‑red‑brown | **33 kΩ ±1 %** | **The `GP26` battery divider**, two in series |
| [`resistors-1k-5pct.jpg`](photos/resistors-1k-5pct.jpg) | Mint | brown‑black‑red‑gold | **1 kΩ ±5 %** | **Both LEDs**, power and status |

**Two of these are four-band and one is five-band, and the four-band pair were photographed with the gold tolerance band on the left** — so they read right to left. That is not pedantry: read the wrong way a `brown‑black‑yellow‑gold` part becomes an invalid code rather than a wrong number, which is the good case. The five-band 33 kΩ is the dangerous one, because it reads as a plausible 1.2 kΩ backwards. Its tolerance band is the brown one set slightly apart at the right.

**No 330 Ω arrived**, which the purchase list had asked for. It does not matter: 1 kΩ drives both LEDs at about 1.3 mA, which is visible and *reduces* the load budget. If the power LED proves too dim outdoors, two 1 kΩ in parallel give 500 Ω and 2.6 mA.

---

### D.7 · The capacitors

Photographed as
[`capacitors-2026-09-06.jpg`](photos/capacitors-2026-09-06.jpg), cropped from the batch shot.

**The photograph is a layout for identification, not a count.** One of each part was put out to be photographed; the delivered quantities are as ordered and were counted by hand on 2026-09-06. This document already says a photograph of one board is never evidence that two arrived, and the same rule applies to a tray of passives.

| Part | Value read from | Where it goes |
|---|---|---|
| **10 µF 50 V** electrolytic | Sleeve print, legible | RA-02 supply pins, with a `104` beside it |
| **100 µF 50 V** electrolytic | Sleeve print, legible | microSD supply pins — **in parallel with the one below** |
| **100 µF 25 V** electrolytic | Sleeve print, legible | microSD supply pins, the other half of the pair |
| **0.1 µF ceramic disc** | **Print worn illegible.** The value comes from the purchase, not from the part | microSD, RA-02 and the sound board — **three of them.** The IMU, barometer and GPS are deliberately without, see [D-8](assembly-procedure.md#d-8-three-104s-are-deliberately-omitted) |

Two 100 µF in parallel is 200 µF at the microSD, against the
[~50 µF the write spike actually needs](../design/electrical-architecture.md#how-much-bulk-is-actually-needed).
Mixing a 50 V and a 25 V part in that pair is fine: both are far above a 3.3 V rail, and an
electrolytic has no DC-bias derating, so each contributes its full marking.

**Three `104` ceramics are consumed by the build as fitted** — microSD, RA-02 and the sound board. The IMU, barometer and GPS are deliberately without one ([D-8](assembly-procedure.md#d-8-three-104s-are-deliberately-omitted)); the parts are held against those three ever misbehaving. Against about twenty in hand, so there is ample margin. They are the one part here with no substitute, since a bulk capacitor of any type is too slow for the edges they cover.

**The disc print being unreadable is worth stating plainly.** The value on record is what
was ordered, not what was verified — an orange disc looks the same at 100 pF as at 0.1 µF.
Anything with a capacitance range on it will settle them in seconds; if the meter has none,
fit them and treat a rail that misbehaves at high frequency as a suspect.

---

## Findings

Anything that did not match the documentation: wrong item, wrong quantity, damage, a
supplier specification the board contradicts, a missing accessory.

| # | Item | Expected | Received/observed | Action taken |
|---|---|---|---|---|
| F-1 | IMU, SKU 2846 | MPU-6050, six axes | **An MPU-6500. Six axes, no magnetometer.** Silkscreen hedges `MPU-9250/6500`; die reads `MP92`, the 9250 marking; **`WHO_AM_I` returns `0x70`**, read on the bench 2026-09-05, and `0x0C` never appears after the bypass is enabled | Driver replaced and the estimator reworked for nine axes — **correct work, and it is what lets this part fly at all**: the firmware accepts `0x70` as a six-axis part rather than refusing to boot. **The vehicle has no absolute yaw reference.** Yaw is gyro-integrated and drifts; telemetry reports `YR-G`, never `YR-M`. Gate 8 rows 8.9–8.11, 8.13 and 8.14 are not takeable on this part |
| F-2 | Battery, SKU 1125094 | Orange 1S 1500 mAh 25C | **Pro-Range** 1S 1500 mAh 25C. Capacity, cell count and C-rating match; brand does not | Documentation renamed to the delivered brand. Charge parameters are absent from the label and remain undocumented |
| F-3 | microSD reader, SKU 11566 | 4.5–5.5 V input, onboard 3.3 V regulator, per the supplier listing | **3.3 V board. No regulator, no level shifter**, supply pin printed `3V3`, four 10 kΩ pull-ups and two capacitors | Second rail and boost stage removed from the power tree. Bring-up row 7.4 promoted, since nothing buffers MISO |
| F-4 | Barometer, SKU 835813 | GY-BMP280-3.3 | Purple **6-pin** `GY-BM ☐E/☐P 280` shared-artwork board, neither variant box legibly marked. Package measured 2.04 × 2.50 mm — a BMP280, not a BME280 | **Closed 2026-09-05. Chip ID `0xD0` returned `0x58`: a BMP280.** The geometric identification in [C.4.1](#c4--gy-bmp280-33) was right, and the register confirms it. It answers at `0x76`, so SDO is low and no firmware change is needed |
| F-5 | Antenna and cable | BOM: "SMA Male". Supplier page: "RP-SMA Female" | **They mate, hand-tight, no adapter.** Antenna shell female, cable shell male — agreeing with the supplier's gender, not the BOM's. Centre contacts not photographed | Assembly unblocked. Nomenclature open pending straight-on photographs of both mating faces |
| F-6 | Pico headers | — | **No headers fitted and none supplied** | **Closed.** Strips bought separately and soldered to both Picos, and to every sensor, the microSD, GPS and RA-02 carriers, on 2026-09-04. Joints inspected and adjacent-pin isolation checked before power |
| F-7 | Accessories | — | **No microSD card and no 1S charger** in the delivery; neither is on the BOM | **Closed.** Both bought separately 2026-09-05, along with a JST-RCY pigtail for the battery and a replacement multimeter. Card capacity/class and charger model still to be recorded in the Part A tools table |

A finding that changes what the project believes about a component also gets a CHANGELOG
entry — Part E, step 6.

> F-1, F-3 and F-5 are three supplier descriptions contradicted by three delivered parts, in
> a single order. That is the case for this document stated better than any argument for it:
> **a listing identifies what was bought, never what was built.**

---

## Sign-off

| Part | Completed by | Date | All rows filled? |
|---|---|---|---|
| A · Inventory | | | |
| B · Photographs | | | |
| C.1 · Pico | | | |
| C.2 · RA-02 | | | |
| C.3 · MPU-9250 | | | |
| C.4 · BMP280 | | | |
| C.5 · NEO-6M | | | |
| C.6 · microSD reader | | | |
| C.7 · Antenna and cable | | | |
| C.8 · Battery | | | |
| C.9 · Prototype PCB | | | |
| D · Blocking questions | | | |

**No part may be signed off with a blank row.** Either record it, or write down why it could
not be recorded. An unfilled row that looks filled is worse than an empty document.

---

## Related documents

- [Pre-Procurement Design Status](pre-procurement-design-status.md) — the verification plan these rows come from, and the design-freeze criteria they release
- [Hardware Reference](hardware.md) — the single hardware database these observations update
- [SD Module Analysis](sd-module-analysis.md) — why [D.1](#d1--the-microsd-reader-sku-11566) blocks the power design
- [Electrical Compatibility](electrical-compatibility.md) — the per-component assessments that assume values recorded here
- [Bring-Up Record](../testing/bring-up-record.md) — the powered measurements that start where this document ends
- [Wiring](../design/wiring.md) — the bring-up order to follow once the boards are verified
