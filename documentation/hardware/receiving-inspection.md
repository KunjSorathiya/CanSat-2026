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
| A.1 | Raspberry Pi Pico | 894292 | 2 | | No visible damage | **Headers not fitted, and none supplied in the photograph.** Genuine board, `© 2020` silkscreen |
| A.2 | SX1278 RA-02 LoRa module | 1150780 | 2 | | No visible damage | Headers loose in the bag, 2 × 8-pin. See [D.3](#d3--ra-02-carrier) |
| A.3 | 433 MHz LoRa antenna | 1121334 | 2 | | No visible damage | White rubber-duck, hinged base. See [D.2](#d2--antenna-sma-or-rp-sma) |
| A.4 | IPEX1 to SMA cable, 10 cm RG1.13 | 1674982 | 2 | | No visible damage | Supplied with panel nut, plain washer and star washer |
| A.5 | MPU-9250 module | 2846 | 1 | | No visible damage | **Delivered as MPU-9250/6500, not the MPU-6050 the BOM named.** Header loose, 10-pin |
| A.6 | NEO-6M GPS with EPROM | 11782 | 1 | | No visible damage | Active patch antenna **supplied and already fitted** to the u.FL socket |
| A.7 | GY-BMP280-3.3 | 835813 | 1 | | No visible damage | Purple 6-pin board. Header loose, 6-pin. See [F-4](#findings) |
| A.8 | Micro SD card reader module | 11566 | 1 | | No visible damage | **3.3 V board, not the 4.5–5.5 V one the listing described.** Header loose, 6-pin. See [D.1](#d1--the-microsd-reader-sku-11566) |
| A.9 | 1S 3.7 V 1500 mAh 25C LiPo | 1125094 | 1 | | Pack flat, no puffing visible | **Delivered as Pro-Range, not Orange.** Do not charge yet. See [D.4](#d4--battery) |
| A.10 | Universal prototype PCB, 10 x 10 cm | 1031002 | 2 | | No visible damage | Single-sided, isolated pads, edge rails |

> **The Received column is deliberately blank.** The photographs show one unit of each item;
> a photograph of one board is not evidence that two arrived. Count them by hand and fill the
> column in — A.1, A.2, A.3, A.4 and A.10 were each ordered in pairs.

Not on the BOM, but Part C cannot be completed without them. Record what you actually have:

| Tool | Have it? | Notes |
|---|---|---|
| Multimeter with continuity | | Required for every polarity and continuity check below |
| Magnifier or phone macro lens | | Required to read regulator and level-shifter markings |
| Soldering iron and solder | | Headers arrive loose on most of these boards |
| microSD card | **No** | Not on the BOM, and none is in the reader's bag in `11566-sd-reader-front.jpg`. Buy one |
| USB micro-B cable, data-capable | | For the Pico. A charge-only cable is a classic wasted afternoon |
| 1S LiPo charger | | Not on the BOM. **Never charge a LiPo without one** |

> Anything answered "no" here is a purchase to make today, not on the day it blocks work.

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
| B.7 | GY-BMP280-3.3 | Front, back, regulator, SDO strap | 2026-09-04 | `835813-bmp280-front.jpg`, `835813-bmp280-back.jpg` — **die marking illegible**, see [F-4](#findings) |
| B.8 | microSD reader | Front, back, **every** component marking | 2026-09-04 | `11566-sd-reader-front.jpg`, `11566-sd-reader-back.jpg` |
| B.9 | LiPo | Full label, connector, both faces | 2026-09-04 | `1125094-lipo-front.jpg`, `1125094-lipo-back.jpg` |
| B.10 | Prototype PCB | Front, back, copper pattern close-up | 2026-09-04 | `1031002-protoboard-front.jpg`, `1031002-protoboard-back.jpg` |

Three re-shoots are outstanding, all of them macro shots the phone can take today:

1. **The antenna's mating face and the cable's SMA mating face, straight on.** Without them
   [D.2](#d2--antenna-sma-or-rp-sma) cannot be closed on nomenclature, only on fit.
2. **The BMP280 die marking**, to settle BMP280 against BME280 ([C.4.1](#c4--gy-bmp280-33)).
3. **The MPU-9250, NEO-6M and microSD regulator markings.** All three are SOT-23 parts whose
   text is below this photograph's resolution.

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
| C.1.5 | Headers fitted, or to be soldered | Visual | **Not fitted, and no header strip in the photograph.** Plated through-holes and castellations bare | Photo 2026-09-04 |
| C.1.6 | USB connector condition | Visual | Micro-B, intact, no bent shell | Photo 2026-09-04 |

> C.1.5 is a purchase, not an observation: two 20-pin strips per Pico, and the vehicle Pico
> may be better soldered flat to the prototype board than socketed. Decide before assembly.
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
> board rather than assumed. Note that **the supply pin sits third from one end with `GND`
> either side of it** — a one-pin offset when the module is pressed into the prototype board
> puts 3.3 V onto `RST` or a `GND` pin onto the rail. Mark pin 1 on the board before wiring.
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
| C.3.2 | **IC marking on the die itself: MPU-9250, MPU-9255, or MPU-6500** | Magnifier | **`MP92`** / `163LA1` / `1719` on a 24-pin QFN. `MP92` is the MPU-9250 marking; an MPU-6500 reads `MP65` | Photo 2026-09-04 |
| C.3.3 | Pin labels: VCC, GND, SCL, SDA, XDA, XCL, AD0, INT | Silkscreen | 10 pins. Front: `VCC GND SCL SDA EDA ECL AD0 INT NCS FSYNC`. Back names two of them dually: `SCL/SCLK`, `SDA/SDI`, `ADD/SDO` | Photo 2026-09-04 |
| C.3.4 | Onboard regulator present? Part marking | Magnifier | **Yes** — one SOT-23-5 beside the `VCC` pin. **Marking not legible at this resolution** | Photo 2026-09-04 |
| C.3.5 | Bus pull-ups fitted? Marked value | Magnifier | **Yes** — five resistors marked `103` (10 kΩ), grouped around the `SCL`/`SDA` and `AD0`/`INT`/`NCS` pins, plus unmarked capacitors | Photo 2026-09-04 |
| C.3.6 | AD0 strapped high or low as delivered | Continuity to VCC/GND | | |
| C.3.7 | Expected I2C address implied by C.3.6 | `0x68` or `0x69` | | |
| C.3.8 | INT exposed on the header | Visual | **Yes**, `INT` is on the header — GP7 in the pin map is real | Photo 2026-09-04 |
| C.3.9 | Silkscreen axis arrows present? Which way do X, Y and Z point? | Visual, photograph | **Yes** — an axis cross is printed beside the die. With the board component-side up and the pin header on the right: **X points away from the header, Y towards the `VCC` end, Z out of the board** | Photo 2026-09-04 |

> C.3.2 settles the question C.3.1 could not: the silkscreen hedges (`MPU-9250/6500`, and the
> back carries *both* board names), but the die says `MP92`. This is a real nine-axis part,
> so the AK8963 and the absolute-yaw path in the firmware apply. `WHO_AM_I` at bring-up is
> still the final word — a photograph of a package is not a register read.
>
> C.3.4: the regulator's presence is what matters for the rail decision, and it is visible.
> Its identity decides the input range, and that needs a macro re-shoot or a measurement.
>
> C.3.5: **five 10 kΩ pull-ups on this board and four more on the BMP280** both hang on the
> same I2C0 pair. Two 10 kΩ pull-ups in parallel is 5 kΩ, which is still a legal bus but a
> stiffer one than either board was designed around. Record the sink current at bring-up
> before assuming it is harmless.
>
> C.3.9 is recorded from the printed cross, and orientation read off a photograph is easy to
> get wrong. Confirm it against the board in your hand before the airframe is built around it.

> C.3.2 is the row that matters most on this board. Modules sold as MPU-9250 are frequently
> MPU-6500 dies, which are pin-compatible, electrically identical for the accelerometer and
> gyroscope, and have **no magnetometer at all**. The silkscreen is not evidence; the die
> marking is, and `WHO_AM_I` settles it once the board is powered — `0x71` or `0x73` for a
> real MPU-9250/9255, `0x70` for an MPU-6500. Record what the magnifier says here, and the
> `WHO_AM_I` value in bring-up gate 8.8.
>
> C.3.9 matters because the firmware's body frame is defined against those arrows. The
> AK8963 magnetometer inside the package has its own, different axes; the firmware already
> corrects for that, but it can only be checked against a board whose printed axes are
> recorded.

### C.4 · GY-BMP280-3.3

| # | Record | How | Value | Date/by |
|---|---|---|---|---|
| C.4.1 | Board marking; confirm BMP280, **not** BME280 | Silkscreen and die marking | **Unresolved.** Silkscreen is the shared `GY-BM ☐E/☐P 280` artwork with a tick box per variant; **neither box is legibly marked, and the die text is below this photograph's resolution**. See [F-4](#findings) | Photo 2026-09-04 |
| C.4.2 | Pin count and labels: 4-pin I2C or 6-pin I2C/SPI | Silkscreen | **6 pins**, in order `VCC GND SCL SDA CSB SDO` — the I2C/SPI variant, not the 4-pin I2C-only board | Photo 2026-09-04 |
| C.4.3 | Onboard regulator present? Part marking | Magnifier | **None.** The board carries only the sensor, four resistors and two capacitors — consistent with the 3.3 V-only `GY-BMP280-3.3` the BOM ordered | Photo 2026-09-04 |
| C.4.4 | Bus pull-ups fitted? Marked value | Magnifier | **Yes** — four resistors marked `103` (10 kΩ) | Photo 2026-09-04 |
| C.4.5 | SDO strapped high or low as delivered | Continuity to VCC/GND | | |
| C.4.6 | Expected I2C address implied by C.4.5 | `0x76` or `0x77` | | |

> C.4.3 is the useful half of this table: **no regulator means no 5 V tolerance.** This board
> must be fed 3.3 V, which is what the design already intends, and feeding it from a 5 V
> source would destroy it. That is now a verified constraint rather than a hopeful reading of
> the product name.
>
> C.4.1 stays open, and it matters more than it looks. A BME280 answers chip-ID `0x60` where
> a BMP280 answers `0x58`, and it also reports humidity the telemetry format has no field
> for. Either re-shoot the die at macro range, or read the chip-ID register at bring-up and
> record it here. **Do not resolve this row from the product name.**

> C.4.6 must agree with the `0x76` that the wiring and bring-up documents assume. If the
> board straps SDO high, either the strap or the document changes — decide deliberately and
> record which.

### C.5 · NEO-6M GPS

| # | Record | How | Value | Date/by |
|---|---|---|---|---|
| C.5.1 | Board marking and revision | Silkscreen | `GY-NEO6MV2` | Photo 2026-09-04 |
| C.5.2 | Controller marking: NEO-6M and variant | Magnifier | `u-blox` `NEO-6M-0-001`, lot `1702`, serial `2422187473 8`, `0300 3`. Genuine u-blox label with data matrix | Photo 2026-09-04 |
| C.5.3 | Pin labels and order: VCC, GND, TX, RX | Silkscreen | **4 pins, printed `VCC RX TX GND`** — `VCC` at one end, `GND` at the other, `RX` and `TX` between them in that order | Photo 2026-09-04 |
| C.5.4 | Onboard regulator present? Part marking | Magnifier | **Yes** — one SOT-23-5 beside the header. **Marking not legible at this resolution** | Photo 2026-09-04 |
| C.5.5 | Stated supply range on the silkscreen, if any | Silkscreen | **None printed.** The board states no voltage anywhere | Photo 2026-09-04 |
| C.5.6 | Antenna connector type; patch antenna supplied? | Visual | **u.FL / IPEX socket**, and the **active patch antenna is supplied and already mated** | Photo 2026-09-04 |
| C.5.7 | Backup battery or supercapacitor present? | Visual | **Yes** — a coin cell on its side next to the module, plus a `24C32A` (`FT5N4D`) 8-pin EEPROM. This is the "with EPROM" the BOM named | Photo 2026-09-04 |

> C.5.5 is the row to be careful about. The board prints no supply range, the regulator is
> unidentified, and GY-NEO6MV2 boards exist in versions that want 5 V and versions happy on
> 3.3 V. **Do not assume this one runs from the 3.3 V rail.** Identify the regulator, or
> measure the module supply and the UART idle level, before it is wired to the Pico.
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
| C.6.9 | Continuity: card supply to regulator output, or to VCC directly | Meter | | |
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
| C.7.6 | Cable continuity: centre to centre, shield to shield | Meter | | |
| C.7.7 | Cable isolation: centre to shield reads open | Meter | | |
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
| C.8.3 | Polarity: which lead is positive | **Meter, not wire colour** | | |
| C.8.4 | Protection circuit present? | Visual at the tab end | **Not visible.** Leads exit under the red heat-shrink; no protection board can be seen, and its absence cannot be proven from outside | Photo 2026-09-04 |
| C.8.5 | Open-circuit voltage as delivered | Meter across the terminals | | |
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
| C.9.5 | Adjacent pads are isolated | Meter | | |
| C.9.6 | Board thickness and mounting holes | Ruler, visual | **Four corner mounting holes.** Thickness not measurable from a photograph | Photo 2026-09-04 |

> C.9.3 and C.9.4 together decide how the vehicle is built: **every connection is a wire.**
> Isolated pads on a single-sided board give no power or ground rails, so a 3.3 V and a GND
> bus have to be created by hand — a soldered bus wire, or a run of bridged pads — before any
> module is placed. Plan those two runs first; retrofitting them under a populated board is
> unpleasant.
>
> The coordinate grid is worth using. Record each module's corner pad as, say, `F-12` in the
> assembly notes, and the layout survives being taken apart.

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
| Which lead is positive? | C.8.3 | **Open — needs a meter.** Wire colour is a convention, not evidence. |
| Is there protection, or must the design provide cutoff? | C.8.4 | **No protection board visible**, and absence cannot be proven from outside. Assume the design must provide cutoff until shown otherwise. |
| Is the pack healthy as delivered? | C.8.5, C.8.8 | **Physically yes** — flat, square, no puffing. **Open-circuit voltage still unmeasured.** |
| What are the real charge and discharge limits? | C.8.6, C.8.7 | **Only `25C` is printed.** No charge current, no cutoff, no burst figure. |
| Is there a charger for it? | Part A tools table | **No** — not on the BOM and not in the delivery. |

**Still blocking, and the least advanced of the four.** Two of these rows need nothing but a
multimeter and five minutes; until they are done, no powered test can start.

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

## Findings

Anything that did not match the documentation: wrong item, wrong quantity, damage, a
supplier specification the board contradicts, a missing accessory.

| # | Item | Expected | Received/observed | Action taken |
|---|---|---|---|---|
| F-1 | IMU, SKU 2846 | MPU-6050, six axes | **MPU-9250**, nine axes — silkscreen `MPU-9250/6500`, die marked `MP92` | Driver replaced, attitude estimator reworked for nine axes, documentation migrated. `WHO_AM_I` at bring-up is the final confirmation |
| F-2 | Battery, SKU 1125094 | Orange 1S 1500 mAh 25C | **Pro-Range** 1S 1500 mAh 25C. Capacity, cell count and C-rating match; brand does not | Documentation renamed to the delivered brand. Charge parameters are absent from the label and remain undocumented |
| F-3 | microSD reader, SKU 11566 | 4.5–5.5 V input, onboard 3.3 V regulator, per the supplier listing | **3.3 V board. No regulator, no level shifter**, supply pin printed `3V3`, four 10 kΩ pull-ups and two capacitors | Second rail and boost stage removed from the power tree. Bring-up row 7.4 promoted, since nothing buffers MISO |
| F-4 | Barometer, SKU 835813 | GY-BMP280-3.3 | Purple **6-pin** `GY-BM ☐E/☐P 280` shared-artwork board. Neither variant box legibly marked; **BMP280 vs BME280 unresolved** | Open. Re-shoot the die, or read the chip ID at bring-up (`0x58` BMP280, `0x60` BME280) |
| F-5 | Antenna and cable | BOM: "SMA Male". Supplier page: "RP-SMA Female" | **They mate, hand-tight, no adapter.** Antenna shell female, cable shell male — agreeing with the supplier's gender, not the BOM's. Centre contacts not photographed | Assembly unblocked. Nomenclature open pending straight-on photographs of both mating faces |
| F-6 | Pico headers | — | **No headers fitted and none supplied** | Purchase. Two 20-pin strips per Pico, or solder the vehicle Pico flat to the prototype board |
| F-7 | Accessories | — | **No microSD card and no 1S charger** in the delivery; neither is on the BOM | Purchase. The charger is on the critical path — see [D.4](#d4--battery) |

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
