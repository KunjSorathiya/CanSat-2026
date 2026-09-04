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
| A.1 | Raspberry Pi Pico | 894292 | 2 | | | Headers fitted or loose? |
| A.2 | SX1278 RA-02 LoRa module | 1150780 | 2 | | | Headers fitted or loose? |
| A.3 | 433 MHz LoRa antenna | 1121334 | 2 | | | See [D.2](#d2--antenna-sma-or-rp-sma) |
| A.4 | IPEX1 to SMA cable, 10 cm RG1.13 | 1674982 | 2 | | | |
| A.5 | MPU-9250 module | 2846 | 1 | | | |
| A.6 | NEO-6M GPS with EPROM | 11782 | 1 | | | Patch antenna included? |
| A.7 | GY-BMP280-3.3 | 835813 | 1 | | | |
| A.8 | Micro SD card reader module | 11566 | 1 | | | See [D.1](#d1--the-microsd-reader-sku-11566) |
| A.9 | Orange 1S 3.7 V 1500 mAh 25C LiPo | 1125094 | 1 | | | Do not charge yet. See [D.4](#d4--battery) |
| A.10 | Universal prototype PCB, 10 x 10 cm | 1031002 | 2 | | | |

Not on the BOM, but Part C cannot be completed without them. Record what you actually have:

| Tool | Have it? | Notes |
|---|---|---|
| Multimeter with continuity | | Required for every polarity and continuity check below |
| Magnifier or phone macro lens | | Required to read regulator and level-shifter markings |
| Soldering iron and solder | | Headers arrive loose on most of these boards |
| microSD card | | Not on the BOM — check whether one was supplied |
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
| B.1 | Pico #1 and #2 | Front, back, board marking | | |
| B.2 | RA-02 #1 and #2 | Front, back, header labels, RF connector | | |
| B.3 | Antenna | Both ends, connector mating face | | |
| B.4 | IPEX cable | Both ends, close-up of each connector | | |
| B.5 | MPU-9250 | Front, back, regulator, pull-ups, AD0 strap | | |
| B.6 | NEO-6M | Front, back, controller, regulator, antenna socket | | |
| B.7 | GY-BMP280-3.3 | Front, back, regulator, SDO strap | | |
| B.8 | microSD reader | Front, back, **every** component marking | | |
| B.9 | LiPo | Full label, connector, both faces | | |
| B.10 | Prototype PCB | Front, back, copper pattern close-up | | |

---

## Part C · Per-board identification

These rows come from
[section 5, the post-procurement verification plan](pre-procurement-design-status.md#5-post-procurement-verification-plan).
Every row is unpowered: reading, measuring geometry, or checking continuity on the meter's
continuity range. Rows needing a live rail belong in the bring-up record instead.

### C.1 · Raspberry Pi Pico, quantity 2

| # | Record | How | Value | Date/by |
|---|---|---|---|---|
| C.1.1 | Board marking and revision | Read the silkscreen | | |
| C.1.2 | Pin labels match the datasheet pinout | Compare board with datasheet | | |
| C.1.3 | VSYS, VBUS, 3V3, 3V3_EN, RUN, GND present and undamaged | Visual | | |
| C.1.4 | Debug pads present | Visual | | |
| C.1.5 | Headers fitted, or to be soldered | Visual | | |
| C.1.6 | USB connector condition | Visual | | |

### C.2 · SX1278 RA-02, quantity 2

| # | Record | How | Value | Date/by |
|---|---|---|---|---|
| C.2.1 | Board marking and revision | Silkscreen | | |
| C.2.2 | **Header pin order, exactly as printed** | Transcribe every pin, in order | | |
| C.2.3 | NSS/CS, RESET, DIO0, DIO1 present and labelled | Silkscreen | | |
| C.2.4 | SCK, MOSI, MISO labels | Silkscreen | | |
| C.2.5 | Onboard regulator present? Part marking | Magnifier | | |
| C.2.6 | Onboard level shifter present? Part marking | Magnifier | | |
| C.2.7 | Antenna connector type: IPEX/u.FL or SMA | Visual, compare with the cable | | |
| C.2.8 | Supply pin labelled 3.3 V, or a range | Silkscreen | | |

> C.2.2 matters most on this board. RA-02 carriers ship with more than one header
> arrangement, and the module has no reverse-polarity protection worth relying on.
> Transcribe the printed order; do not copy a pinout from a web image.

### C.3 · MPU-9250

| # | Record | How | Value | Date/by |
|---|---|---|---|---|
| C.3.1 | Board marking, MPU-9250 breakout or other | Silkscreen | | |
| C.3.2 | **IC marking on the die itself: MPU-9250, MPU-9255, or MPU-6500** | Magnifier | | |
| C.3.3 | Pin labels: VCC, GND, SCL, SDA, XDA, XCL, AD0, INT | Silkscreen | | |
| C.3.4 | Onboard regulator present? Part marking | Magnifier | | |
| C.3.5 | Bus pull-ups fitted? Marked value | Magnifier | | |
| C.3.6 | AD0 strapped high or low as delivered | Continuity to VCC/GND | | |
| C.3.7 | Expected I2C address implied by C.3.6 | `0x68` or `0x69` | | |
| C.3.8 | INT exposed on the header | Visual | | |
| C.3.9 | Silkscreen axis arrows present? Which way do X, Y and Z point? | Visual, photograph | | |

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
| C.4.1 | Board marking; confirm BMP280, **not** BME280 | Silkscreen and die marking | | |
| C.4.2 | Pin count and labels: 4-pin I2C or 6-pin I2C/SPI | Silkscreen | | |
| C.4.3 | Onboard regulator present? Part marking | Magnifier | | |
| C.4.4 | Bus pull-ups fitted? Marked value | Magnifier | | |
| C.4.5 | SDO strapped high or low as delivered | Continuity to VCC/GND | | |
| C.4.6 | Expected I2C address implied by C.4.5 | `0x76` or `0x77` | | |

> C.4.6 must agree with the `0x76` that the wiring and bring-up documents assume. If the
> board straps SDO high, either the strap or the document changes — decide deliberately and
> record which.

### C.5 · NEO-6M GPS

| # | Record | How | Value | Date/by |
|---|---|---|---|---|
| C.5.1 | Board marking and revision | Silkscreen | | |
| C.5.2 | Controller marking: NEO-6M and variant | Magnifier | | |
| C.5.3 | Pin labels and order: VCC, GND, TX, RX | Silkscreen | | |
| C.5.4 | Onboard regulator present? Part marking | Magnifier | | |
| C.5.5 | Stated supply range on the silkscreen, if any | Silkscreen | | |
| C.5.6 | Antenna connector type; patch antenna supplied? | Visual | | |
| C.5.7 | Backup battery or supercapacitor present? | Visual | | |

> C.5.3: **TX and RX on a GPS breakout name the board's own pins**, so the board's TX goes
> to the Pico's RX. Record the label, not your interpretation of it.

### C.6 · Micro SD card reader, SKU 11566

The highest-risk item in the BOM — [sd-module-analysis.md](sd-module-analysis.md) explains
why. Complete this section before anything else touches SPI0.

| # | Record | How | Value | Date/by |
|---|---|---|---|---|
| C.6.1 | Board marking and revision | Silkscreen | | |
| C.6.2 | Pin labels and physical order | Transcribe, left to right | | |
| C.6.3 | Regulator present? Full part marking | Magnifier | | |
| C.6.4 | Level shifter present? Full part marking | Magnifier | | |
| C.6.5 | If shifting is discrete: resistors, transistors, or a buffer IC? | Magnifier, trace the tracks | | |
| C.6.6 | Every resistor and capacitor marking | Magnifier | | |
| C.6.7 | Stated input range on the silkscreen | Silkscreen | | |
| C.6.8 | Continuity: VCC pin to regulator input | Meter | | |
| C.6.9 | Continuity: card supply to regulator output, or to VCC directly | Meter | | |
| C.6.10 | Card retention: push-push, push-pull, or friction | Visual | | |

> C.6.3 and C.6.9 together answer the question blocking the entire power design: is the card
> fed from an onboard regulator — so the module needs 4.5 to 5.5 V in, which a 1S LiPo
> cannot supply — or directly from VCC, so 3.3 V drives it? Two photographs and two
> continuity checks decide a whole branch of the schematic.

### C.7 · Antenna and IPEX cable

| # | Record | How | Value | Date/by |
|---|---|---|---|---|
| C.7.1 | Antenna connector: SMA or RP-SMA, male or female | Inspect the centre contact | | |
| C.7.2 | Cable SMA end: SMA or RP-SMA, male or female | Inspect the centre contact | | |
| C.7.3 | Cable IPEX end variant | Compare with the RA-02 socket | | |
| C.7.4 | Antenna and cable mate without force | Hand-tight, **no power** | | |
| C.7.5 | IPEX end mates with the RA-02 socket | Gentle, **no power** | | |
| C.7.6 | Cable continuity: centre to centre, shield to shield | Meter | | |
| C.7.7 | Cable isolation: centre to shield reads open | Meter | | |
| C.7.8 | Markings on antenna and cable | Read and photograph | | |

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
| C.8.1 | Full label text | Photograph and transcribe | | |
| C.8.2 | Connector type: JST-PH 2.0, JST-XH, or other | Compare with a known connector | | |
| C.8.3 | Polarity: which lead is positive | **Meter, not wire colour** | | |
| C.8.4 | Protection circuit present? | Visual at the tab end | | |
| C.8.5 | Open-circuit voltage as delivered | Meter across the terminals | | |
| C.8.6 | Stated continuous and burst discharge | Label | | |
| C.8.7 | Stated charge current and cutoff | Label or manufacturer document | | |
| C.8.8 | Physical damage, puffing, or smell | Visual | | |

> C.8.3: LiPo lead colours are conventional, not guaranteed, and a reversed 1S pack will
> destroy the Pico faster than you can disconnect it. Twenty seconds with a meter.
>
> C.8.5: a cell delivered below roughly 3.0 V, or visibly puffed, is one to set aside and
> raise with the supplier — not to charge.

### C.9 · Prototype PCB, quantity 2

| # | Record | How | Value | Date/by |
|---|---|---|---|---|
| C.9.1 | Dimensions | Ruler | | |
| C.9.2 | Hole pitch | Ruler over 10 holes, divide by 10 | | |
| C.9.3 | Single- or double-sided | Visual | | |
| C.9.4 | Pad pattern: isolated pads, strips, or bus rails | Visual | | |
| C.9.5 | Adjacent pads are isolated | Meter | | |
| C.9.6 | Board thickness and mounting holes | Ruler, visual | | |

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
| Is there an onboard regulator, and what is it? | C.6.3, C.6.8 | |
| Is the card fed from that regulator, or from VCC directly? | C.6.9 | |
| Is there level shifting, and of what kind? | C.6.4, C.6.5 | |
| Does the module need 4.5 to 5.5 V, or will 3.3 V drive it? | All of the above | **3.3 V drives it. The module is a 2.6–3.6 V SPI board.** |

**Answered.** 3.3 V drives it, so the second rail disappears from the power tree and no
boost converter is specified. The supplier listing that said 4.5–5.5 V described a
different board from the one that arrived, which is the entire reason this document exists.

The rows above are still worth filling in. Knowing the module runs at 3.3 V settles the
*voltage*; the regulator, the level-shifting arrangement and the card's own rail still
determine how it behaves on a bus it shares with the radio, and its write-transient current
is what the power budget now waits on.

### D.2 · Antenna: SMA or RP-SMA

**Blocks:** RF assembly, and any range test.

| Question | Answered by | Answer |
|---|---|---|
| What is actually on the antenna? | C.7.1 | |
| What is actually on the cable? | C.7.2 | |
| Do they mate? | C.7.4 | |
| Does the cable mate with the RA-02? | C.7.5 | |

The supplied BOM says SMA male; the supplier's live page title says RP-SMA female. One of
them is wrong about the parts in your hands, and only the parts can say which. If they do
not mate, order the adapter today — a cheap part with a long lead time relative to how late
you would otherwise discover you need it.

### D.3 · RA-02 carrier

**Blocks:** freezing the GPIO map, and the schematic.

| Question | Answered by | Answer |
|---|---|---|
| What is the exact printed header order? | C.2.2 | |
| Are RESET and DIO0 available? Is DIO1? | C.2.3 | |
| Is there a regulator or level shifter on the carrier? | C.2.5, C.2.6 | |
| Does the RF connector match the supplied cable? | C.2.7, C.7.5 | |

### D.4 · Battery

**Blocks:** every powered test, and the power budget.

| Question | Answered by | Answer |
|---|---|---|
| Which lead is positive? | C.8.3 | |
| Is there protection, or must the design provide cutoff? | C.8.4 | |
| Is the pack healthy as delivered? | C.8.5, C.8.8 | |
| What are the real charge and discharge limits? | C.8.6, C.8.7 | |
| Is there a charger for it? | Part A tools table | |

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
| 1 | | | | |
| 2 | | | | |
| 3 | | | | |
| 4 | | | | |
| 5 | | | | |

A finding that changes what the project believes about a component also gets a CHANGELOG
entry — Part E, step 6.

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
