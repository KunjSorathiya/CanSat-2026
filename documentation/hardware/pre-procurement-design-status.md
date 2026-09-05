# Pre-Procurement Electrical Design Status

This document records the CanSat electrical design baseline before the purchased hardware arrives. It consolidates the current hardware reference, compatibility analysis, electrical architecture, logical resource map, and preliminary GPIO map.

No component is physically verified. Exact Robu SKU selection is treated as procurement identity, not as proof of the delivered board's circuit. Chip-level manufacturer specifications are not silently promoted to breakout-board specifications.

## Status Model

- **VERIFIED FROM DOCUMENTATION** - Sufficiently supported by an official manufacturer document or exact product information for the stated scope. This does not mean physically inspected.
- **PROVISIONALLY ACCEPTED** - A design choice is suitable for pre-procurement planning, but depends on later electrical or physical verification.
- **PENDING PHYSICAL VERIFICATION** - The decision requires the actual board, markings, connector, or measurement.
- **BLOCKED** - Work cannot be released for the next design stage until a prerequisite or external decision is resolved.

## 1. Documentation-Verified Facts

The following facts are supported by the current documentation. `VERIFIED FROM DOCUMENTATION` applies only to the scope stated in each row.

| Item | Documentation-verified fact | Source | Status |
|---|---|---|---|
| Pico identity | Raspberry Pi Pico, Robu SKU 894292, quantity 2 | Robu product reference and Raspberry Pi Pico datasheet | VERIFIED FROM DOCUMENTATION |
| Pico GPIO | Pico GPIO operates in the 3.3 V logic domain; GPIO limits must follow the Pico/RP2040 documentation | Raspberry Pi Pico datasheet | VERIFIED FROM DOCUMENTATION |
| Pico power input | VSYS supports approximately 1.8 V to 5.5 V according to the accepted project interpretation of the Pico documentation | Raspberry Pi Pico documentation | VERIFIED FROM DOCUMENTATION |
| Pico peripherals | I2C, SPI, UART, ADC, USB, and configurable GPIO resources are available | Raspberry Pi Pico datasheet | VERIFIED FROM DOCUMENTATION |
| Battery identity | **Pro-Range** 1S LiPo delivered against Robu SKU 1125094, labelled 3.7 V nominal, 1500 mAh, 25C. The BOM ordered an Orange pack; capacity, cell count, voltage and C-rating match, the brand does not | Receiving inspection of the delivered pack | VERIFIED FROM HARDWARE |
| Battery full-charge assumption | Approximately 4.2 V when fully charged | Confirmed project power information | VERIFIED FROM DOCUMENTATION |
| RA-02 identity | SX1278 RA-02 433 MHz module, Robu SKU 1150780, quantity 2 | Robu product reference | VERIFIED FROM DOCUMENTATION |
| RA-02 IC scope | Semtech documentation describes SX1278 IC behavior, not necessarily the complete RA-02 carrier | Semtech SX127x documentation and compatibility analysis | VERIFIED FROM DOCUMENTATION |
| MPU-9250 IC scope | InvenSense/TDK documentation describes the MPU-9250 IC; the Robu breakout circuit remains separate | MPU-9250 manufacturer documentation | VERIFIED FROM DOCUMENTATION |
| BMP280 IC scope | Bosch documentation describes the BMP280 IC; the GY-BMP280-3.3 breakout circuit remains separate | Bosch BMP280 datasheet | VERIFIED FROM DOCUMENTATION |
| GPS identity | NEO-6M GPS module with EPROM, Robu SKU 11782, quantity 1 | Confirmed project BOM and Robu reference | VERIFIED FROM DOCUMENTATION |
| SD input | The delivered SKU 11566 has a supply pin printed `3V3`, no regulator and no level shifter | Receiving inspection of the board in hand, [`photos/`](photos/) | VERIFIED FROM HARDWARE |
| SD signal labels | Robu lists GND, VCC, MISO, MOSI, SCK, and CS | Supplied exact Robu product information | VERIFIED FROM DOCUMENTATION |
| SD logic path | The Robu information does not establish level shifting, host logic voltage, MISO release behavior, or SD-card rail voltage | SD module analysis | VERIFIED FROM DOCUMENTATION |
| Antenna identity | 433 MHz antenna, Robu SKU 1121334, quantity 2 | Confirmed BOM and Robu page | VERIFIED FROM DOCUMENTATION |
| Antenna conflict | Supplied BOM says SMA male while the observed Robu page title says RP-SMA female | Robu page observation and confirmed BOM | VERIFIED FROM DOCUMENTATION |
| Cable identity | 10 cm IPEX1 to SMA female RG1.13 cable, Robu SKU 1674982, quantity 2 | Confirmed BOM and Robu product reference | VERIFIED FROM DOCUMENTATION |
| Prototype board | 10 x 10 cm, single-sided, 2.54 mm prototype PCB, Robu SKU 1031002 | Confirmed BOM and Robu product reference | VERIFIED FROM DOCUMENTATION |
| Competition telemetry | Required format, mandatory fields, sequential packet numbering, continuous transmission, and minimum 1 packet/second are documented | Official rulebook extract | VERIFIED FROM DOCUMENTATION |

## 2. Provisional Design Decisions

These decisions can be made before the components arrive. They are not physical verification claims.

| Decision | Rationale | Dependency | Status |
|---|---|---|---|
| Power Pico from switched LiPo through VSYS | The accepted Pico architecture uses VSYS within the documented input range | Battery protection, switch behavior, startup, and brownout testing | PROVISIONALLY ACCEPTED |
| Do not use AMS1117-3.3 for direct 1S-to-3.3 V regulation | Its dropout/headroom requirement can exceed the fully charged 1S battery voltage | Final peripheral power-conversion decision remains open | PROVISIONALLY ACCEPTED |
| Treat Pico 3.3 V output as RP2040/GPIO supply, not an automatic whole-system supply | External peripheral current and transients are not yet budgeted | Measured/documented peripheral loads | PROVISIONALLY ACCEPTED |
| ~~Reserve a separate power path for the SD reader~~ | Withdrawn. The delivered board carries no regulator and its supply pin is printed `3V3`, so it runs from the 3.3 V rail; the separate path existed only to satisfy a supplier-listed 4.5-5.5 V requirement the board does not have | None | WITHDRAWN, superseded by hardware |
| Share I2C0 between MPU-9250 and BMP280 | Documented IC address options are logically distinct: MPU-9250 `0x68/0x69`, BMP280 `0x76/0x77` | Breakout I2C exposure, bus voltage, and pull-ups | PROVISIONALLY ACCEPTED |
| Share SPI0 between RA-02 and SD | Shared SCK/MOSI/MISO with separate CS lines is resource-efficient | SD level shifting, MISO release, and carrier pinouts | PROVISIONALLY ACCEPTED |
| Reserve separate CS lines | Prevents simultaneous selection on the shared SPI bus | Exact board CS labels | PROVISIONALLY ACCEPTED |
| Reserve RA-02 RESET and DIO0 | Supports radio initialization, recovery, and event/interrupt handling | Carrier pin availability and signal behavior | PROVISIONALLY ACCEPTED |
| Keep RA-02 DIO1 optional | Preserves an event input without making it mandatory | Final radio mode and carrier pins | PROVISIONALLY ACCEPTED |
| Reserve UART0 for GPS | Preserves UART1 capacity for debug or expansion | GPS board TX/RX labels and logic levels | PROVISIONALLY ACCEPTED |
| Reserve GPIO26/ADC0 for battery monitoring | Preserves a future diagnostic path without designing a divider yet | Battery range and ADC protection design | PROVISIONALLY ACCEPTED |
| Use GPIO14 for an external visible LED | Separates the competition-visible LED from the Pico onboard LED | LED circuit, resistor, visibility, and power behavior | PROVISIONALLY ACCEPTED |
| Preserve USB, SWD/debug, UART1, and spare GPIO | Maintains development and troubleshooting capability | Final physical access and pin-mux review | PROVISIONALLY ACCEPTED |
| Retain the current preliminary GPIO numbers | The map has no identified Pico alternate-function conflict | Breakout pinouts and electrical approval | PROVISIONALLY ACCEPTED |

The current preliminary GPIO allocation is recorded in [pico-gpio-map.md](pico-gpio-map.md). It is a planning baseline, not released wiring.

## 3. Pending Physical Verification

These items genuinely require the delivered boards, markings, photographs, schematics, or measurements. They do not prevent documentation-level planning, but they prevent electrical release.

> **The parts arrived on 2026-09-04 and were photographed.** Items struck through below are
> closed, with the observation recorded in [receiving-inspection.md](receiving-inspection.md)
> and the photographs in [`photos/`](photos/). A fifth status now applies to them:
>
> - **VERIFIED FROM HARDWARE** - observed on the delivered board, with a photograph or a
>   measurement behind it.
>
> Everything not struck through needs a meter or a powered rail. A photograph cannot show
> continuity, a strap's direction, a voltage or a current, and nothing here has been inferred
> from one.

### Micro SD Reader - SKU 11566

- ~~Confirm the actual board matches the Robu listing.~~ **It does not.** The listing describes a 4.5-5.5 V board with an onboard regulator; the delivered board has neither.
- ~~Identify all regulator, level-shifter, resistor, transistor, buffer, and controller markings.~~ **There are none to identify** beyond four resistors marked `103` (10 kOhm) and two unmarked capacitors. No active component is fitted.
- ~~Confirm whether the onboard regulator is present and what it supplies.~~ **Absent.**
- Determine SD-card rail voltage. *(Meter: confirm the `3V3` pin reaches the socket's supply pad directly.)*
- ~~Determine whether signals are resistor shifted, transistor shifted, shifted by an IC, or directly connected.~~ **Directly connected**, through track and a 10 kOhm pull-up.
- Determine the voltage at CS, CLK, MOSI, and MISO. *(Meter, powered.)*
- Confirm MISO is released when CS is inactive. **Now the most important row in the shared-bus gate**, since nothing on the board buffers it.
- Measure startup, initialization, read, write, and peak current.
- Identify required decoupling. Two capacitors are fitted; their values are unread.

### RA-02 - SKU 1150780

- ~~Confirm header pin order and labels.~~ **Transcribed.** J2 `GND GND 3.3V RST DIO0 DIO1 DIO2 DIO3`, J1 `GND NSS MOSI MISO SCK DIO5 DIO4 GND`, both read from the u.FL end.
- Confirm carrier supply range and logic levels. The supply pin is printed `3.3V` with no range, and **no regulator or translator is fitted**, so the carrier passes the pin straight to the module. The module's own limits are still unread.
- ~~Identify any onboard regulator or level shifter.~~ **Neither is fitted.** `C1` and `C2` are the only parts outside the shield.
- ~~Confirm CS/NSS, RESET, DIO0, and optional DIO1 availability.~~ **All present**, with `DIO2`-`DIO5` also broken out.
- ~~Confirm the antenna connector.~~ **u.FL / IPEX socket**, mated with the supplied cable. RF matching arrangement is inside the shield and remains unknown.
- Measure idle, receive, startup, and transmit current on the actual carrier.

### Sensor and GPS Breakouts

- ~~Confirm board markings and revision.~~ **MPU-9250:** `GY-6500 / GY-9250`, `V356`, die marked `MP92`. **Barometer:** the shared `GY-BM E/P 280` artwork. **GPS:** `GY-NEO6MV2` with a `u-blox NEO-6M-0-001` module.
- ~~Confirm exposed interfaces and pin labels.~~ **Transcribed for all three** - see [wiring.md](../design/wiring.md#module-header-pinouts-as-printed).
- Confirm onboard regulators, pull-ups, level shifting, and capacitors. **Partly done:** the MPU-9250 and NEO-6M each carry an unidentified SOT-23-5 regulator; the barometer carries none; pull-ups are 10 kOhm on both I2C boards; no level shifting anywhere. **The two regulator part numbers are still unread, so two input ranges are still unknown.**
- ~~Confirm MPU-9250 INT exposure.~~ **`INT` is on the header.**
- ~~Resolve BMP280 against BME280.~~ **A BMP280.** Chip ID `0xD0` returned `0x58` on 2026-09-05; a BME280 answers `0x60`. The variant tick box is unmarked and the die text illegible, so the register settled what the silkscreen could not.
- ~~Confirm BMP280 SDO/address wiring and MPU-9250 AD0 strap direction.~~ **Both strapped low**, answered by bus scan rather than by meter: the barometer replies at `0x76` and the IMU at `0x68`, 2026-09-05.
- ~~Confirm GPS TX/RX arrangement.~~ **4 pins, `VCC RX TX GND`**, naming the board's own pins. Logic levels still unmeasured.

### Battery and RF Hardware

- ~~Confirm battery label and connector.~~ **Pro-Range, not Orange**; `1 Cell 3.7V 25C`, 1500 mAh; red JST-RCY main lead and white 2-pin JST-XH balance lead, **neither of which mates with anything in this project**.
- ~~Confirm battery polarity.~~ **Red is positive**, read on a meter at 3.92 V open-circuit, 2026-09-05. Lead colour is a convention, not evidence, and now it is not the evidence being relied on.
- Confirm protection and charging information. **Partly done.** No protection board is visible, the label states no charge current and no cutoff voltage, and no Pro-Range document has been located. A 1S charger was bought separately on 2026-09-05 ([F-7](receiving-inspection.md#findings)) — none was supplied and none is on the BOM — and its model still has to reach the Part A tools table. **The absent charge parameters are what keeps this item open.**
- **Resolve SMA versus RP-SMA.** The antenna shell is female and the cable's is male - agreeing with the supplier listing, contradicting the BOM - but neither centre contact was photographed. Open.
- ~~Confirm IPEX connector variant and mating with the RA-02.~~ **IPEX-1 / u.FL, mated.**
- Check connector retention and continuity before power or RF operation. *(Meter.)*

## 4. Procurement Checklist

The exact BOM is selected. Documentation availability does not imply physical verification.

| Item | SKU | Quantity | Documentation Available | Physical Verification Needed |
|---|---:|---:|---|---|
| Raspberry Pi Pico | 894292 | 2 | Robu page and Raspberry Pi datasheet | Board marking, revision, connector/pin condition, VSYS/3V3 measurements |
| SX1278 RA-02 LoRa module | 1150780 | 2 | Robu page and Semtech IC documentation | Carrier pinout, regulator, logic levels, DIO/RESET, RF connector, current |
| 433 MHz LoRa antenna | 1121334 | 2 | Robu page and supplied BOM description | SMA/RP-SMA type, gender, markings, physical mating |
| IPEX1 to SMA female RG1.13 cable | 1674982 | 2 | Robu product page and supplied description | IPEX variant, SMA/RP-SMA end, continuity, physical mating |
| MPU-9250 module | 2846 | 1 | Robu SKU reference and MPU-9250 IC documentation | Breakout revision, regulator, pull-ups, pins, INT, voltage |
| NEO-6M GPS with EPROM | 11782 | 1 | Robu SKU reference and u-blox documentation | Board revision, regulator, pins, UART levels, antenna, current |
| GY-BMP280-3.3 | 835813 | 1 | Robu SKU reference and Bosch BMP280 datasheet | Breakout revision, interface, address pins, pull-ups, voltage |
| Micro SD Card Reader Module | 11566 | 1 | Robu listing facts; exact schematic unavailable | Regulator, SD rail, level shifting, pinout, MISO behavior, current |
| Orange 1S 3.7 V 1500 mAh 25C LiPo | 1125094 | 1 | Robu page and supplied BOM markings | Label, connector, polarity, protection, charge/discharge limits |
| Universal prototype PCB | 1031002 | 2 | Robu product page and supplied dimensions | Board pattern, thickness, condition, continuity, mounting |

## 5. Post-Procurement Verification Plan

### Raspberry Pi Pico

- Photograph the board marking and revision.
- Confirm VSYS, VBUS, 3V3, 3V3_EN, GND, RUN, USB, and debug-pad condition.
- Measure VSYS and Pico 3.3 V behavior with the intended supply arrangement.
- Confirm GPIO and alternate-function labels against the board pinout.
- Confirm USB programming and debug access before mounting.

### SX1278 RA-02

- Photograph both sides and all markings.
- Record the header pin order from the physical board.
- Identify regulator, level-shifter, and RF-network components.
- Confirm CS/NSS, RESET, DIO0, DIO1, SCK, MOSI, and MISO labels.
- Verify antenna connector type and cable mating without applying power.
- Measure supply voltage and current in idle, receive, and transmit test modes after safe test setup.

### MPU-9250

- Photograph both sides and record board marking.
- Identify regulator, pull-up resistors, address strap, and bypass capacitors.
- Confirm SDA, SCL, AD0, INT, VCC, and GND labels.
- Measure board supply and bus logic levels.
- Confirm the observed I2C address with AD0 in its delivered configuration.

### NEO-6M GPS

- Photograph both sides, controller marking, regulator, antenna connector, and header labels.
- Confirm TX and RX direction labels.
- Measure board supply and UART idle/high levels.
- Confirm antenna power arrangement and connector type.
- Measure startup/acquisition and tracking current.

### GY-BMP280-3.3

- Photograph both sides and board markings.
- Identify regulator, pull-ups, address strap, and bypass capacitors.
- Confirm SDA, SCL, SDO, CSB, VCC, and GND labels if present.
- Confirm I2C address and selected interface without changing hardware prematurely.
- Measure board supply and bus levels.

### Micro SD Reader - SKU 11566

- Photograph the complete front and back of the board.
- Record regulator, level-shifter, transistor, resistor, and controller markings.
- Confirm the physical order and polarity of VCC, GND, MISO, MOSI, SCK, and CS.
- Confirm whether the onboard regulator powers the card and whether level shifting exists.
- Measure VCC input, SD-card rail, and every SPI signal relative to GND.
- Measure MISO with CS inactive to confirm shared-bus release.
- Measure startup, initialization, read, write, and worst-case write current.
- Record all fitted capacitors and compare them with the identified regulator/card requirements.

### LiPo Battery

- Photograph the complete label and connector.
- Identify positive and negative terminals with a continuity/polarity check using an appropriate instrument.
- Record protection-board presence or absence.
- Record charger, cutoff, continuous-discharge, and burst-discharge information from the label or manufacturer document.
- Measure open-circuit voltage before any load test.

### Antenna and IPEX Cable

- Photograph both antenna connector ends and both cable ends.
- Record SMA versus RP-SMA gender and IPEX variant markings.
- Confirm mechanical mating with the RA-02 without force.
- Check cable continuity and absence of shorts before RF power.

### Prototype PCB

- Confirm the board dimensions, hole pitch, copper pattern, thickness, and visible defects.
- Check continuity and isolation between intended power areas before assembly.

## 6. Design Freeze Criteria

### A. Power Design Is Frozen When

- The Pico VSYS path, switch, battery protection, and battery cutoff behavior are documented.
- The exact supply requirements of every peripheral board are known.
- The SD reader input requirement and onboard regulator behavior are verified.
- A regulator or conversion solution is selected from a measured/documented load budget, including startup and transient loads.
- 3.3 V and any SD-reader input rail tolerances are defined.
- Grounding, decoupling, protection, test points, and brownout behavior are reviewed.
- Startup, radio transmission, SD writes, and low-voltage tests pass with evidence.

### B. GPIO Map Is Frozen When

- Every connected board's actual pin labels and exposed controls are known.
- RA-02 CS/NSS, RESET, DIO0, and any DIO1 use is confirmed.
- SD CS and SPI signal behavior are confirmed.
- MPU-9250 INT availability is confirmed or formally removed from the design.
- BMP280 interface and address configuration are confirmed.
- GPS TX/RX arrangement is confirmed.
- Pico alternate functions and pin multiplexing are reviewed together.
- No pin is assigned twice and spare/debug resources remain acceptable.

### C. PCB Design Starts When

- The electrical schematic is complete and reviewed.
- Power and logic compatibility is approved for every module.
- Connector types, pin order, polarity, and mechanical placement are known.
- The power budget and decoupling requirements are documented.
- The physical dimensions and mass constraints are clarified for the competition.
- The team accepts the risk of using prototype boards or has approved a custom-PCB scope.

### D. Firmware Hardware Abstraction Is Frozen When

- The GPIO map and bus assignments are frozen.
- Supply and logic-level assumptions are verified.
- Device addresses and interface modes are confirmed.
- Reset, interrupt, chip-select, and startup behavior are documented.
- Sensor units, ranges, calibration inputs, and invalid-data behavior are defined.
- SD initialization and write-failure behavior are known.
- RA-02 packet and radio-control requirements are defined.
- Hardware-in-the-loop tests can exercise every mission-critical interface.

## Pre-Procurement Engineering Decision

The team can safely complete documentation-level schematic and resource planning before the components arrive. The current GPIO map can remain provisional and should not be changed without a real pin-mux conflict or a board-level interface dependency.

The power architecture can be finalized conceptually now:

```text
1S LiPo -> manual switch -> Pico VSYS
                         -> separate peripheral conversion, TBD
                              -> verified 3.3 V loads
```

It cannot be released as a component-level power design until the SD reader, RA-02 carrier, breakout boards, and battery details are verified. No physical verification is claimed in this document.
