# Electrical Compatibility Assessment

## Scope

This document evaluates whether the current CanSat BOM can electrically work with the Raspberry Pi Pico. It is a pre-pin-map assessment only. It does not assign GPIO pins, select a regulator, design a PCB, or authorize wiring.

Sources are separated by evidence level:

- **CONFIRMED** - Directly stated in the project BOM or an authoritative exact-product source.
- **MANUFACTURER DOCUMENTED** - Stated in a manufacturer datasheet or official technical document, but not necessarily describing the purchased breakout board.
- **ROBU DOCUMENTED** - Stated on the exact Robu listing supplied for the product.
- **INFERRED** - A technical conclusion derived from documented values; it is not a direct specification.
- **PHYSICAL VERIFICATION REQUIRED** - Cannot safely be determined without the actual board, markings, schematic, or connector inspection.

## Component Compatibility Summary

| Component | Robu SKU | Supply | Logic Level | Interface | Current | Pico Compatible? | Evidence Level | Status |
|---|---:|---|---|---|---|---|---|---|
| Raspberry Pi Pico | 894292 | VSYS/VBUS options are documented; exact project supply path TBD | 3.3 V GPIO; GPIO must remain within documented I/O limits | I2C, SPI, UART, ADC, USB and GPIO capabilities documented | System current TBD; no whole-system rail guarantee assumed | Yes as controller; peripheral power capability requires separate budget | MANUFACTURER DOCUMENTED / CONFIRMED | Controller confirmed; integration not verified |
| SX1278 RA-02 | 1150780 | SX1278 IC: 1.8-3.7 V; RA-02 carrier supply TBD | IC I/O is supply-dependent; carrier logic level TBD | SPI plus reset, NSS/CS, DIO controls at IC level | IC TX about 120 mA at +20 dBm and RX about 10.3 mA; carrier current TBD | Unknown until carrier board supply, logic, and peak current are verified | MANUFACTURER DOCUMENTED / PHYSICAL VERIFICATION REQUIRED | Carrier blocked |
| MPU-9250 module | 2846 | MPU-9250 IC VDD 2.375-3.46 V; breakout supply TBD | IC VLOGIC 1.71-3.46 V; breakout levels TBD | I2C/SPI at IC level; exposed breakout bus TBD | IC normal-mode current about 3.9 mA; breakout current TBD | Compatible in principle; board voltage and pull-ups must be verified | MANUFACTURER DOCUMENTED / PHYSICAL VERIFICATION REQUIRED | Breakout blocked |
| NEO-6M GPS module | 11782 | NEO-6 receiver supply 2.7-3.6 V; breakout supply TBD | Receiver interface levels are documented; breakout levels TBD | UART documented for NEO-6; exposed board interface TBD | NEO-6 receiver current is about 37 mA; breakout current TBD | Compatible in principle; board regulator and UART levels must be verified | MANUFACTURER DOCUMENTED / PHYSICAL VERIFICATION REQUIRED | Breakout blocked |
| GY-BMP280-3.3 | 835813 | BMP280 IC VDD 1.71-3.6 V; breakout supply TBD | IC VDDIO 1.2-3.6 V; breakout levels TBD | I2C/SPI at IC level; exposed breakout bus TBD | IC mode-dependent current; breakout current TBD | Compatible in principle; board wiring and pull-ups must be verified | MANUFACTURER DOCUMENTED / PHYSICAL VERIFICATION REQUIRED | Breakout blocked |
| Micro SD reader | 11566 | Supply pin printed `3V3`; no regulator fitted; SPI | 3.3 V, unbuffered - **no level shifter is fitted, so a 3.3 V host is required** | GND, MISO, CLK, MOSI, CS, 3V3 | TBD; write transient unmeasured | Supply-compatible with the 3.3 V rail; **MISO release depends entirely on the card**, since nothing buffers it | VERIFIED FROM HARDWARE / current PHYSICAL VERIFICATION REQUIRED | Supply resolved; current and shared-bus behaviour open |
| 1S 1500mAh 25C LiPo | 1125094 | 3.7 V nominal; approximately 4.2 V full charge; cutoff TBD | Not applicable | Battery connector and polarity TBD | Safe continuous and peak current TBD | Not a direct Pico peripheral supply until input path is verified | CONFIRMED / PHYSICAL VERIFICATION REQUIRED | Battery integration blocked |
| 433 MHz antenna | 1121334 | Not applicable | Not applicable | RF connector type conflicting | RF power handling TBD | Not a digital Pico connection | ROBU DOCUMENTED / CONFIRMED conflict | RF connector blocked |
| IPEX-to-SMA cable | 1674982 | Not applicable | Not applicable | IPEX1 to SMA female stated by product name | RF loss/power handling TBD | Not a digital Pico connection | CONFIRMED / PHYSICAL VERIFICATION REQUIRED | Connector mating blocked |
| Prototype PCB | 1031002 | Not applicable | Not applicable | Passive 2.54 mm prototype board | Current capability TBD | Passive carrier only; suitability requires inspection | ROBU DOCUMENTED | Board details TBD |

## Raspberry Pi Pico

**Sources:** [Raspberry Pi Pico datasheet](datasheets/raspberry_pi_pico_datasheet.pdf) and [official online copy](https://datasheets.raspberrypi.com/pico/pico-datasheet.pdf).

Manufacturer-documented controller capabilities relevant to this project include 3.3 V GPIO operation, configurable I2C, SPI, UART, ADC, PWM, and general GPIO resources. The Pico board pinout and power-input options are defined by the manufacturer datasheet. The project has not assigned final GPIO pins.

| Item | Assessment | Evidence level |
|---|---|---|
| 3.3 V GPIO logic | Pico GPIO is 3.3 V logic; do not expose GPIO to an unverified 5 V signal | MANUFACTURER DOCUMENTED |
| GPIO voltage limits | Must follow the Pico/RP2040 electrical limits in the exact datasheet revision | MANUFACTURER DOCUMENTED; exact design limit must be recorded |
| Power input | The permitted VBUS/VSYS/3V3 paths and limits are in the Pico datasheet; project input path is TBD | MANUFACTURER DOCUMENTED |
| 3.3 V output | The onboard 3V3 output must not be assumed capable of powering all peripherals; allowable external load requires datasheet review and measurement | MANUFACTURER DOCUMENTED / INFERRED |
| I2C | Available as configurable Pico peripheral resources; bus assignment and pull-ups TBD | MANUFACTURER DOCUMENTED |
| SPI | Available as configurable Pico peripheral resources; bus and chip selects TBD | MANUFACTURER DOCUMENTED |
| UART | Available as configurable Pico peripheral resources; GPS UART assignment TBD | MANUFACTURER DOCUMENTED |
| ADC | Available for future monitored analog signals; no power-monitoring pin is assigned | MANUFACTURER DOCUMENTED |
| GPIO count | Exact usable GPIO count and reserved functions must be taken from the board pinout; no allocation is made | MANUFACTURER DOCUMENTED |
| Current limitations | Board current, 3V3 external-load allowance, and total peripheral load are not a substitute for a measured power budget | PHYSICAL VERIFICATION REQUIRED |

**Pico conclusion:** The Pico is suitable as the controller in principle. It is not yet proven safe to power the complete peripheral set from its 3V3 output, and it is not proven safe to connect any 5 V signal to its GPIO.

## SX1278 RA-02

**Sources:** [Semtech SX1276/77/78/79 datasheet](https://www.semtech.com/uploads/documents/DS_SX1276-7-8-9_W_APP_V7.pdf) and [Robu SKU 1150780 product page](https://robu.in/product/sx1278-lora-module-ra-02-433mhz-wireless-spread-spectrum-transmission/).

The Semtech document describes the SX1278 IC, not the complete RA-02 carrier. The following are IC-level facts or interfaces and must not be promoted to carrier-board specifications:

- SX1278 IC supply range: manufacturer documented in the Semtech datasheet.
- SX1278 frequency coverage: manufacturer documents the SX1278 family range; the purchased RA-02 is identified as 433 MHz by the Robu product description.
- Digital control uses SPI with chip select/NSS, reset, and DIO control/status lines at IC level.
- IC-level transmit and receive current, and maximum transmit power, are manufacturer-documented in the Semtech datasheet. The RA-02 carrier's actual current and RF output remain TBD.
- The carrier's header pin order, logic-level translation, regulator, antenna connector, and RF matching are unknown.

| RA-02 item | Current conclusion | Evidence level |
|---|---|---|
| Supply voltage | SX1278 IC range is documented; RA-02 carrier input range is unknown | MANUFACTURER DOCUMENTED / PHYSICAL VERIFICATION REQUIRED |
| Digital logic | IC signal limits are documented; carrier signal level and any translation are unknown | MANUFACTURER DOCUMENTED / PHYSICAL VERIFICATION REQUIRED |
| SPI | IC SPI operation is documented; carrier header mapping is unknown | MANUFACTURER DOCUMENTED / PHYSICAL VERIFICATION REQUIRED |
| NSS/CS | Required at IC level; carrier pin label and active behavior unknown | MANUFACTURER DOCUMENTED / PHYSICAL VERIFICATION REQUIRED |
| RESET | Required at IC level; carrier pin label and circuit unknown | MANUFACTURER DOCUMENTED / PHYSICAL VERIFICATION REQUIRED |
| DIO pins | Available at IC level; number and carrier header mapping required | MANUFACTURER DOCUMENTED / PHYSICAL VERIFICATION REQUIRED |
| TX/RX current | Semtech documents IC operating values; exact RA-02 peak and board current are unknown | MANUFACTURER DOCUMENTED / PHYSICAL VERIFICATION REQUIRED |
| TX power | Semtech documents IC limits; configured RA-02 output and carrier RF limits are unknown | MANUFACTURER DOCUMENTED / PHYSICAL VERIFICATION REQUIRED |
| Antenna | Carrier connector and matching must be inspected | PHYSICAL VERIFICATION REQUIRED |

**RA-02 conclusion:** Compatible with a 3.3 V Pico interface only in principle. Do not connect power or GPIO until the carrier documentation or physical inspection establishes its supply, logic, pinout, and RF connector.

## MPU-9250

**Sources:** InvenSense MPU-9250 Product Specification (PS-MPU-9250A-01), MPU-9250 Register Map (RM-MPU-9250A-00), the AKM AK8963 datasheet, and the [Robu SKU 2846 reference](https://robu.in/?s=2846&post_type=product).

Manufacturer IC documentation includes the following:

- VDD and VDDIO requirements are specified in the MPU-9250 product specification.
- I2C and SPI are supported at IC level for the inertial sensors; the AK8963 magnetometer is I2C only.
- The I2C address is selected by AD0, with the two documented address choices. The AK8963 answers separately at `0x0C`, reachable only once `INT_PIN_CFG.BYPASS_EN` bridges it to the primary bus.
- SDA, SCL, AD0, and INT are IC signals; exact Robu header labels and wiring are unknown.
- Accelerometer ranges are +/-2, +/-4, +/-8, and +/-16 g.
- Gyroscope ranges are +/-250, +/-500, +/-1000, and +/-2000 degrees/s.
- Magnetometer range is +/-4912 uT, at 14-bit or 16-bit resolution.
- Output-rate and divider behavior are configurable in the IC; the magnetometer free-runs in its own continuous mode.
- IC current is documented by operating mode; breakout-board current is unknown.
- **The magnetometer die does not share the inertial axes.** Magnetometer X lies along the MPU's Y, magnetometer Y along the MPU's X, and magnetometer Z is inverted. This is a fact about the package, not about the breakout, and the firmware corrects for it.
- **A module labelled MPU-9250 may be an MPU-6500 with no magnetometer.** `WHO_AM_I` is the only way to tell: `0x71`/`0x73` versus `0x70`. Read it during bring-up and record the value.

The exact Robu carrier board's supply input, onboard regulator, logic levels, pull-ups, capacitor population, pinout, dimensions, and current remain **PHYSICAL VERIFICATION REQUIRED**. Bias, scale, temperature, axis orientation, and installation calibration remain project verification tasks.

**MPU-9250 conclusion:** Compatible with the Pico in principle if the breakout exposes a Pico-safe bus and supply. The board cannot yet be approved for wiring.

## BMP280

**Sources:** [Bosch BMP280 datasheet](datasheets/bmp280_datasheet.pdf) and [Robu SKU 835813 reference](https://robu.in/?s=835813&post_type=product).

Manufacturer IC documentation includes:

- BMP280 VDD and VDDIO operating ranges.
- I2C and SPI interfaces.
- I2C address selection through SDO, with the two documented address choices.
- Pressure measurement range of 300 to 1100 hPa at IC level.
- Temperature operating range of -40 to +85 degrees C at IC level.
- Configurable oversampling, filtering, standby timing, and measurement mode.
- Mode-dependent current and accuracy specifications in the Bosch datasheet.

The GY-BMP280-3.3 carrier's input voltage, onboard regulator, VDDIO behavior, pull-ups, capacitors, pinout, current, dimensions, and weight are **PHYSICAL VERIFICATION REQUIRED**. The product name alone does not establish these values.

**BMP280 conclusion:** Compatible with the Pico in principle at IC level, but the exact breakout board must be verified before connection.

## NEO-6M GPS

**Sources:** [u-blox NEO-6 series documentation](https://www.u-blox.com/en/product/neo-6-series) and [Robu SKU 11782 reference](https://robu.in/?s=11782&post_type=product).

The u-blox NEO-6 documentation identifies the receiver family and documents UART operation, default configuration behavior, supported communication configuration, navigation update capability, supply requirements, current behavior, and positioning performance under stated conditions. The exact values, revision, and module variant must be recorded from the applicable u-blox document before design approval; the current local documentation set does not contain a valid downloaded u-blox PDF.

The purchased NEO-6M breakout's regulator, input range, UART logic levels, TX/RX header labels, antenna power, backup behavior, EEPROM/flash arrangement, current, capacitors, and pinout are **PHYSICAL VERIFICATION REQUIRED**.

**NEO-6M conclusion:** Compatible with the Pico in principle through a verified UART-level connection. The board is not approved for wiring, and GPS is an additional sensor rather than proof of a mandatory requirement.

## Micro SD Reader - Highest Priority

**Source:** receiving inspection of the delivered board, and the
[Robu SKU 11566 reference](https://robu.in/?s=11566&post_type=product).

The module received is:

- Supply pin printed `3V3`, with **no voltage range printed anywhere on the board**
- **No regulator**, in any package, on either face
- **No level shifter** - no buffer, translator or transistor; no active component at all
- Fitted passives: four resistors marked `103` (10 kOhm) and two unmarked capacitors
- Interface: SPI
- Pins, in printed order: `GND  MISO  CLK  MOSI  CS  3V3`
- Friction / slide-in card holder

These are **VERIFIED FROM HARDWARE**, from the photographs in
[`photos/`](photos/). The supplier listing this section previously quoted said 4.5-5.5 V with
an onboard regulator; the board in hand has neither, and the board wins.

The *tolerated* supply range is deliberately not recorded: no source states one, and
inventing a range from the module's class is what produced the 4.5-5.5 V figure in the first
place. What is established is that the board wants 3.3 V and cannot step anything down.

**Supply compatibility: resolved.** 3.3 V sits in the upper half of the module's range, so
it runs from the vehicle's regulated rail with margin at both ends of a discharge curve. No
second rail, no boost stage, no supply-driven level shifting.

The following are still unknown and still matter:

- Initialization, read, write, and peak current, and the same with the radio transmitting
- MISO behaviour with CS inactive, on the shared SPI0 bus with the RA-02
- Pull-up networks
- Required bypass capacitors
- Exact pinout beyond the supplied labels
- Board schematic and component markings

**Micro SD conclusion:** Supply-compatible with the 3.3 V rail. Not yet qualified on
current or on shared-bus behaviour. Bench-test it on the shared bus before it goes into the
airframe, and close the power budget with a measured write transient.

## Power Architecture

### Preliminary Power Tree

```text
1S LiPo, approximately 4.2 V full charge
    |
    +-- manual power switch
    |
    +-- regulator(s) - not selected
           |
           +-- Pico - input path TBD
           +-- RA-02 - carrier supply TBD
           +-- MPU-9250 - board supply TBD
           +-- BMP280 - board supply TBD
           +-- GPS - board supply TBD
           +-- SD reader - 3.3 V board, no regulator, runs from the 3.3 V rail
```

### Conceptual Rail Assessment

- The battery rail is variable, not a fixed 3.7 V rail.
- A regulated 3.3 V rail is electrically plausible for verified 3.3 V-compatible peripheral boards, but it is not yet proven adequate for the total current or transient load.
- The SD reader's `3V3` supply pin matches the planned 3.3 V rail, so it is one load on that rail like any other. Its **current** contribution, especially the write transient, is still unmeasured and is the open question for the regulator sizing.
- The SD reader's signal pins are 3.3 V on a 3.3 V board, so no shifting is required - and none is fitted, which makes a 3.3 V host mandatory rather than merely convenient. Its MISO behaviour when deselected still needs confirming, and matters more than usual: **nothing on the board buffers the line**, so only the card releases it, and a card that does not corrupts the RA-02's next transaction on the shared SPI0 bus.
- The RA-02, Pico supply path, GPS board, and sensor boards must not be connected directly to the LiPo until their exact board input limits are documented.
- Known current consumers include the Pico, radio, sensors, GPS, SD reader, regulator losses, and LED branch. Actual typical and peak values remain TBD for the purchased boards.
- Likely transient loads include RA-02 transmission, GPS startup/acquisition, SD-card initialization and writes, and Pico startup. This is an engineering risk, not a measured result.

**Provisional architecture conclusion:** A switched battery followed by separately verified regulation is plausible. The present 3.3 V architecture is not approved until the SD reader path, carrier logic levels, regulator load, and Pico supply path are resolved. No regulator is selected here.

## Battery

**Source:** [Robu SKU 1125094 product page](https://robu.in/product/orange-1500mah-1s-25c-3-7-v-lithium-polymer-battery-pack-li-po/).

Confirmed project information:

- Nominal voltage: 3.7 V
- Approximate full-charge voltage: 4.2 V
- Capacity marking: 1500 mAh
- Discharge marking: 25C

Still unverified:

- Actual manufacturer and model
- Connector type and polarity
- Safe cutoff voltage
- Recommended charger and charging current
- Built-in protection
- Safe continuous discharge current
- Permitted peak discharge current and duration
- Dimensions and weight

The 1500 mAh and 25C markings must not be multiplied to claim a safe project discharge current. The battery cannot be approved as a direct supply for any board until its connector, polarity, charge/protection requirements, and each load's input range are verified.

## RF Connection

The supplied BOM description for SKU 1121334 is **“SMA Male Connector.”** The live Robu page title observed during the product-page pass says **“RP-SMA Female Connector.”** This is unresolved.

**RF connector type must be physically verified before connecting the RA-02.** The team must also verify that the IPEX1 cable connector mates with the actual RA-02 board and that the cable's SMA/RP-SMA end mates with the actual antenna. Do not use an adapter or connect the radio based on appearance alone.

## What We Need to Physically Verify

Only the following checks are required to unblock the electrical design:

### A. Micro SD reader - SKU 11566

- Front and back photographs
- All chip and regulator markings
- Any resistor, transistor, or level-shifter IC markings
- Pin labels and board revision
- Exact Robu schematic or product documentation

### B. RA-02 - SKU 1150780

- Front and back photographs
- Manufacturer and board markings
- Header pin labels and pin order
- Antenna connector type
- Any onboard regulator or level-shifter markings

### C. MPU-9250 - SKU 2846

- Front and back photographs
- Board markings and revision
- Regulator and pull-up markings
- Header labels and schematic, if available

### D. NEO-6M - SKU 11782

- Front and back photographs
- Board/regulator markings
- Header labels and connector details
- Antenna connector and active-antenna arrangement

### E. GY-BMP280-3.3 - SKU 835813

- Front and back photographs
- Board markings and revision
- Regulator, pull-up, and level-shifter markings
- Header labels

### F. Battery - SKU 1125094

- Full label photograph
- Connector photograph
- Connector polarity photograph or continuity check
- Manufacturer/model and charging/protection information if present

### G. Antenna and cable

- Connector-end photographs of both antennas and both cables
- Markings identifying SMA versus RP-SMA and IPEX variant
- Physical mating check with the RA-02 connector

No additional photographs are required before these checks are completed. Datasheets or schematics should be provided wherever a marking identifies a manufacturer or board variant.

## Final Decision

### Already Electrically Understood

- Pico is a suitable 3.3 V-class controller in principle, subject to its documented input path and GPIO limits.
- BMP280 and MPU-9250 IC interfaces and electrical domains are documented at chip level.
- Semtech documents the SX1278 IC interface and operating limits at chip level.
- The delivered SD reader is a 3.3 V SPI board with no regulator and no level shifter, and runs from the 3.3 V rail.
- The battery is a 3.7 V nominal, approximately 4.2 V full-charge 1S LiPo by project confirmation.

### Electrically Compatible in Principle

- Raspberry Pi Pico as the controller
- MPU-9250 breakout, if its board is 3.3 V compatible and its bus pull-ups are safe
- BMP280 breakout, if its board is 3.3 V compatible and its bus pull-ups are safe
- NEO-6M breakout, if its UART logic and supply path are Pico compatible
- RA-02, if its carrier board exposes Pico-safe logic and a verified supply

“In principle” is not wiring approval or competition compliance.

### Blocked by Breakout Uncertainty

- RA-02 carrier board
- MPU-9250 breakout
- NEO-6M breakout
- GY-BMP280-3.3 breakout
- Micro SD reader, especially its host-side logic
- Battery connector, protection, cutoff, and discharge capability
- Antenna and cable connector mating

### Power Architecture Risks

- Variable LiPo voltage and unverified cutoff
- Unknown total and transient current
- 3.3 V rail instability
- SD reader's stated input voltage not matching the planned 3.3 V rail
- Possible Pico GPIO exposure to unverified 5 V signals
- Brownout or data corruption during radio transmission and SD writes
- Unverified regulator, capacitor, grounding, and connector requirements

### RF Connector Risks

- SMA versus RP-SMA conflict on antenna SKU 1121334
- Unknown IPEX connector variant on the RA-02 and cable
- Possible physical mismatch or incorrect RF connection
- Carrier-board antenna and RF matching details are unknown

### Can We Proceed to Preliminary Pin-Map Design?

**Not yet.** Proceed only after:

1. The exact breakout boards are identified by photographs, markings, schematics, or authoritative product documentation.
2. The SD reader's regulator and logic-level path are verified.
3. The RA-02 carrier pinout, supply, logic, and antenna connector are verified.
4. All supply and logic limits are recorded for every board.
5. The complete typical and peak power budget is available.
6. The battery connector, polarity, charging, protection, and cutoff are verified.
7. The antenna and cable connector mating is physically confirmed.

No GPIO pins, regulator, PCB design, firmware, or BOM changes are introduced by this document.
