# Hardware Reference

This is the single hardware reference for the confirmed CanSat BOM. Robu SKU and product-page information identify the purchased product. Manufacturer documents describe a chip or reference module only where their scope is explicit; they do not automatically describe an assembled breakout board.

`TBD` means that the exact purchased board, its documentation, or a required measurement is still missing. No GPIO pins or regulator have been selected.

## Flight Computer

### Raspberry Pi Pico

- **Exact product:** Raspberry Pi Pico
- **Robu SKU:** 894292
- **Quantity:** 2 total; one onboard and one ground station
- **Manufacturer:** Raspberry Pi
- **Robu product page:** [Raspberry Pi Pico](https://robu.in/product/raspberry-pi-pico/)
- **Manufacturer document:** [Raspberry Pi Pico datasheet](datasheets/raspberry_pi_pico_datasheet.pdf)

- **Photographs:** [`894292-pico-front.jpg`](photos/894292-pico-front.jpg), [`894292-pico-back.jpg`](photos/894292-pico-back.jpg)

Verified from the delivered board, receiving inspection 2026-09-04:

| Item | Observed | Status |
|---|---|---|
| Board identity | `Raspberry Pi Pico © 2020` silkscreen. **A Pico, not a Pico W** - no radio module | VERIFIED FROM HARDWARE |
| RP2040 marking | `RP2-B2` - the B2 stepping | VERIFIED FROM HARDWARE |
| Laminate marking | `DC-136 94V-0`; CE and FCC marks on the underside | VERIFIED FROM HARDWARE |
| Pin labels | Full underside legend: `GP0`-`GP28`, `GP26_A0`/`GP27_A1`/`GP28_A2`, `ADC_VREF`, `AGND`, `VBUS`, `VSYS`, `3V3`, `3V3_EN`, `RUN`, `GND` | VERIFIED FROM HARDWARE |
| Debug access | `SWCLK`/`GND`/`SWDIO` 3-pad row, plus test points `TP1`-`TP6` | VERIFIED FROM HARDWARE |
| Headers | **Not fitted, and none supplied.** Through-holes and castellations bare | VERIFIED FROM HARDWARE |
| USB connector | Micro-B, intact | VERIFIED FROM HARDWARE |

Headers are a purchase, not an observation: two 20-pin strips per Pico. The vehicle Pico may
be better soldered flat to the prototype board than socketed - decide before assembly, since
it changes the board's height budget.

The downloaded Raspberry Pi datasheet is the source for the Pico board pinout and power-input details. The following board-level items must be transcribed and checked against the purchased board before design:

| Item | Value/status | Source |
|---|---|---|
| Board supply inputs | VSYS/VBUS behavior and limits must be taken from the exact datasheet revision | Raspberry Pi Pico datasheet |
| Logic voltage | GPIO electrical levels and absolute maximums - verify from datasheet | Raspberry Pi Pico datasheet |
| Interfaces | SPI, I2C, UART, ADC, PWM, USB, and simultaneous-use constraints - verify from datasheet | Raspberry Pi Pico datasheet |
| Current | Board current under the actual firmware/peripheral load - TBD | Datasheet plus measurement |
| Peak current | Startup and peripheral-load peaks - TBD | Measurement |
| Pinout | Datasheet pinout is available; final project allocation - TBD | Raspberry Pi Pico datasheet |
| Pull-ups | External bus pull-ups depend on connected boards - TBD | Connected-module documents |
| Decoupling | Board and rail requirements - verify from datasheet and schematic | Raspberry Pi Pico datasheet |
| Level shifting | Required for each connected board - TBD | Module documentation |

No final GPIO pins are assigned.

## Telemetry

### SX1278 RA-02

- **Exact product:** SX1278 LoRa Module RA-02 433 MHz Wireless Spread Spectrum Transmission
- **Robu SKU:** 1150780
- **Quantity:** 2 total; one onboard and one ground station
- **Robu product page:** [SX1278 RA-02](https://robu.in/product/sx1278-lora-module-ra-02-433mhz-wireless-spread-spectrum-transmission/)
- **Manufacturer:** RA-02 module manufacturer - TBD; SX1278 transceiver manufacturer is associated with Semtech
- **Manufacturer document:** [Semtech SX1276/77/78/79 datasheet](https://www.semtech.com/uploads/documents/DS_SX1276-7-8-9_W_APP_V7.pdf)

The Semtech document is chip-level. It must not be used as proof of the complete RA-02 module's regulator, pin header, antenna connector, logic level, or current behavior.

Photographs: [`1150780-ra02-front.jpg`](photos/1150780-ra02-front.jpg),
[`1150780-ra02-back.jpg`](photos/1150780-ra02-back.jpg),
[`1150780-ra02-antenna-mated.jpg`](photos/1150780-ra02-antenna-mated.jpg).

| Item | Value/status | Source |
|---|---|---|
| Carrier marking | `LoRa-02  SX1278  433MHz`; headers labelled `J1` and `J2` | Board silkscreen, 2026-09-04 |
| Shield marking | `Ra-02`, `ISM:410-525MHz`, `LoRa/FSK/OOK`, `PA:+18dBm` | Module shield, 2026-09-04 |
| Module supply voltage | Supply pin is printed `3.3V`. **No range is printed and no regulator is fitted**, so the carrier passes the pin straight to the module | Board silkscreen and inspection, 2026-09-04 |
| Chip supply voltage | SX1278 chip-level range - verify in Semtech datasheet | Semtech datasheet; not module proof |
| Logic voltage | **No level shifter is fitted**, so signal levels are the module's own. Value itself - TBD | Board inspection, 2026-09-04; module documentation still required |
| TX current | RA-02 current - TBD | Exact module documentation and measurement |
| TX power | Shield states `PA:+18dBm` as the module's PA capability. Configured output is set by `link_profile.hpp`, not by the shield | Module shield, 2026-09-04 |
| Frequency range | Shield states `ISM:410-525MHz`, which contains 433 MHz. Not a certification claim | Module shield, 2026-09-04 |
| Interface | SPI. `SCK`, `MOSI`, `MISO`, `NSS` are all broken out and printed | Board silkscreen, 2026-09-04 |
| SPI pins | J2: `GND GND 3.3V RST DIO0 DIO1 DIO2 DIO3`. J1: `GND NSS MOSI MISO SCK DIO5 DIO4 GND`, both read from the u.FL end | Board silkscreen, 2026-09-04 |
| Control pins | `RST`, `DIO0`, `DIO1` all present on the header; `DIO2`-`DIO5` also available | Board silkscreen, 2026-09-04 |
| Operating modes | Sleep, standby, receive, transmit, and mode-control details - TBD at module level | Module and Semtech documents |
| RF connection | **u.FL / IPEX socket** on the carrier, board-edge beside the `GND` end of J2. Mates with the supplied cable | Inspection and mated photograph, 2026-09-04 |
| Onboard passives | `C1` and `C2` outside the shield; no regulator, no translator | Board inspection, 2026-09-04 |
| RF requirements | Correct antenna, grounding, supply stability, and launch configuration - TBD | Module/RF documentation and competition requirements |

The supply pin sits **third from one end, with `GND` on either side of it**. A one-pin offset
when the module is pressed into the prototype board puts 3.3 V onto `RST`, or a ground pin
onto the rail. Mark pin 1 before wiring.

The launch sync words are competition requirements and are recorded in `requirements/requirements.md`; they are not electrical specifications.

### Antenna

- **Exact product:** LoRa Antenna 433 MHz with SMA Male Connector, as supplied by the project BOM
- **Robu SKU:** 1121334
- **Quantity:** 2
- **Robu product page:** [Robu antenna page](https://robu.in/product/lora-antenna-433mhz/)
- **Manufacturer:** TBD
- **Datasheet:** TBD

The live Robu page title observed during this pass says “RP-SMA Female Connector,” which conflicts with the supplied BOM description “SMA Male Connector.”

Verified from the delivered part - photographs
[`1121334-antenna-front.jpg`](photos/1121334-antenna-front.jpg) and
[`1150780-ra02-antenna-mated.jpg`](photos/1150780-ra02-antenna-mated.jpg), receiving
inspection 2026-09-04:

| Item | Observed | Status |
|---|---|---|
| Form | White rubber-duck whip on a hinged swivel base | VERIFIED FROM HARDWARE |
| Connector shell | **Female** - knurled coupling nut with internal threads | VERIFIED FROM HARDWARE |
| Centre contact | **Not resolvable.** No straight-on photograph of the mating face exists | TBD - blocking the SMA/RP-SMA question |
| Mates with the supplied cable | **Yes**, hand-tight, threads fully engaged, no adapter | VERIFIED FROM HARDWARE |
| Markings | **None.** The part carries no printed identification | VERIFIED FROM HARDWARE |
| Impedance, gain, power handling, frequency response, dimensions, mass | TBD | TBD |

**The shell is female, which agrees with the supplier page's gender and contradicts the
BOM's.** SMA versus RP-SMA turns on the centre contact, which no photograph shows, so the
nomenclature stays open even though assembly does not: the chain fits end to end.

This matters for procurement rather than for this build. An RP-SMA antenna screwed onto an
SMA pigtail mates perfectly and connects nothing. Photograph both mating faces before anyone
orders a replacement antenna against the BOM's description.

### IPEX-to-SMA Cable

- **Exact product:** 10CM IPEX1 to SMA Female Connector Cable 11mm RG1.13
- **Robu SKU:** 1674982
- **Quantity:** 2
- **Robu product page:** [Robu IPEX-to-SMA cable](https://robu.in/product/10cm-ipex1-to-sma-female-connector-cable-11mm-rg1-13/)
- **Manufacturer:** TBD
- **Datasheet:** TBD

Cable length and RG1.13 description come from the supplied product name.

Verified from the delivered part - photograph
[`1674982-ipex-sma-cable.jpg`](photos/1674982-ipex-sma-cable.jpg), receiving inspection
2026-09-04:

| Item | Observed | Status |
|---|---|---|
| SMA-end shell | **Male** - bulkhead body, external threads, hex flange | VERIFIED FROM HARDWARE |
| Supplied hardware | Panel nut, plain washer, star lock washer | VERIFIED FROM HARDWARE |
| SMA-end centre contact | **Not resolvable** from the supplied photograph | TBD |
| IPEX end | Mates with the RA-02's u.FL socket, so it is the IPEX-1 / u.FL generation | VERIFIED FROM HARDWARE |
| Mates with the antenna | **Yes**, hand-tight, no adapter | VERIFIED FROM HARDWARE |
| Continuity and isolation | TBD - meter | TBD |
| Markings | **None** | VERIFIED FROM HARDWARE |
| Impedance, loss, power handling, RF frequency suitability | TBD | TBD |

The bulkhead hardware means this cable is meant to be **panel-mounted**: the nut and star
washer clamp it through the airframe wall, which is also the strain relief. Plan a hole for
it rather than leaving the joint hanging on the coax.

## Sensors

### MPU-9250 Module

- **Exact product:** MPU-9250 9-Axis Accelerometer, Gyroscope and Magnetometer Sensor
- **Robu SKU:** 2846
- **Quantity:** 1
- **Robu reference:** [Robu SKU search](https://robu.in/?s=2846&post_type=product)
- **Manufacturer:** InvenSense/TDK for the MPU-9250; AKM for the AK8963 magnetometer die; breakout-board manufacturer - TBD
- **Manufacturer documents:** MPU-9250 Product Specification (PS-MPU-9250A-01) and MPU-9250 Register Map (RM-MPU-9250A-00); AK8963 datasheet for the magnetometer

The exact Robu breakout page and board schematic were not resolved. The manufacturer documents apply to the ICs, not necessarily to the purchased carrier board.

Verified from the delivered board — photographs
[`2846-mpu9250-front.jpg`](photos/2846-mpu9250-front.jpg) and
[`2846-mpu9250-back.jpg`](photos/2846-mpu9250-back.jpg), receiving inspection 2026-09-04:

| Item | Observed | Status |
|---|---|---|
| Board marking | Front `MPU-9250/6500`; back `GY-6500  GY-9250` and `V356` | VERIFIED FROM HARDWARE |
| Die marking | **`MP92`** / `163LA1` / `1719` on a 24-pin QFN. `MP92` is the MPU-9250 marking; an MPU-6500 die reads `MP65` | VERIFIED FROM HARDWARE |
| Pin labels | 10 pins: `VCC  GND  SCL  SDA  EDA  ECL  AD0  INT  NCS  FSYNC`. The underside names three of them dually — `SCL/SCLK`, `SDA/SDI`, `ADD/SDO` — for SPI use | VERIFIED FROM HARDWARE |
| Onboard regulator | **Present** — one SOT-23-5 beside `VCC`. Part marking not legible in the supplied photograph | VERIFIED FROM HARDWARE (presence); TBD (identity) |
| Fitted pull-ups | Five resistors marked `103` (10 kΩ) around the `SCL`/`SDA` and `AD0`/`INT`/`NCS` groups, plus unmarked capacitors | VERIFIED FROM HARDWARE |
| INT exposure | **`INT` is on the header** — the GP7 reservation is real | VERIFIED FROM HARDWARE |
| Printed axes | Axis cross printed beside the die. Component-side up, header to the right: X away from the header, Y towards `VCC`, Z out of the board | VERIFIED FROM HARDWARE |

The board carries a regulator, so `VCC` is not necessarily the die's supply and the board may
accept more than 3.3 V. **Its input range is unknown until that SOT-23-5 is identified**, so
feed it 3.3 V — which the design intends anyway — rather than assuming 5 V tolerance.

The die marking makes this a genuine nine-axis part, so the AK8963 and the absolute-yaw path
apply. `WHO_AM_I` at bring-up remains the final word: a photograph of a package is not a
register read.

**The MPU-9250 is two dies in one package.** The accelerometer and gyroscope answer at the module's own address; the AK8963 magnetometer is a separate I2C slave at `0x0C` that is invisible from outside until the MPU is told to bridge to it. Two consequences that are easy to get wrong and hard to notice afterwards:

- The magnetometer's axes are **not** the accelerometer's. Magnetometer X lies along the MPU's Y, magnetometer Y along the MPU's X, and magnetometer Z is inverted. The firmware rotates them in `mag_axes_to_body()` before anything else sees the sample; skipping that produces a heading that moves smoothly as the vehicle turns and is completely wrong.
- Modules sold as MPU-9250 are frequently MPU-6500 dies with no magnetometer at all. `WHO_AM_I` distinguishes them: `0x71`/`0x73` is a real MPU-9250/9255, `0x70` is an MPU-6500. The firmware accepts both and reports which it found.

| Item | IC-level documented value or status | Breakout-board status | Source |
|---|---|---|---|
| Supply | Verify MPU-9250 VDD range in the product specification | Board input and onboard regulator - TBD | MPU-9250 product specification |
| Logic | Verify VDDIO range in the product specification | Board signal levels - TBD | MPU-9250 product specification |
| Interface | I2C and SPI at IC level; the AK8963 is I2C only | Exposed bus and board wiring - TBD | Register map and board schematic |
| I2C address | `0x68` or `0x69` based on AD0; AK8963 at `0x0C` behind the pass-through bridge | AD0 wiring and available address - TBD | MPU-9250 register map |
| `WHO_AM_I` | `0x71` MPU-9250, `0x73` MPU-9255, `0x70` MPU-6500 (no magnetometer) | Value on the delivered board - TBD | MPU-9250 register map |
| Accelerometer ranges | +/-2, +/-4, +/-8, and +/-16 g | Configured: +/-16 g | Product specification |
| Gyroscope ranges | +/-250, +/-500, +/-1000, and +/-2000 degrees/s | Configured: +/-2000 deg/s | Product specification |
| Magnetometer range | +/-4912 uT, 14-bit (0.6 uT/LSB) or 16-bit (0.15 uT/LSB) | Configured: 16-bit, continuous mode 2 at 100 Hz | AK8963 datasheet |
| Magnetometer sensitivity adjustment | Per-axis ASA values in the AK8963 fuse ROM | Read at initialisation and applied per axis | AK8963 datasheet |
| Filters | Gyroscope `DLPF_CFG` (register 26) and accelerometer `A_DLPF_CFG` (register 29) are separate | Configured: 4 and 4, giving 20 Hz and 21.2 Hz | Register map |
| Output data rates | 1 kHz internal with DLPF 1..6, divided by (1 + `SMPLRT_DIV`); magnetometer free-runs | Configured: 200 Hz inertial, 100 Hz magnetic | Register map |
| Temperature | `TEMP_OUT`/333.87 + 21 degrees C; **not** the MPU-6050 transfer function | Diagnostic use only | Product specification |
| Interrupt | INT output exists | Header exposure and electrical behavior - TBD | Register map and board schematic |
| Current | IC and board current under selected mode - TBD | Board current - TBD | Product specification and measurement |
| Pull-ups | Required bus pull-ups - board-dependent | Fitted values/presence - TBD | Board schematic/inspection |
| Decoupling | IC requirements from the product specification | Existing board capacitors - TBD | Product specification and board inspection |
| Inertial calibration | Gyro bias and an accelerometer scale are estimated on the pad while stationary | Implemented in `startup_calibration.cpp` | Project firmware |
| Magnetic calibration | Hard and soft iron are properties of the **airframe**, not the sensor, and are only observable while rotating | Figure-of-eight sweep on the assembled vehicle - **NOT YET PERFORMED** | Project procedure |

### GY-BMP280-3.3

- **Exact product:** GY-BMP280-3.3 Precision Altimeter Atmospheric Pressure Sensor Module
- **Robu SKU:** 835813
- **Quantity:** 1
- **Robu reference:** [Robu SKU search](https://robu.in/?s=835813&post_type=product)
- **Manufacturer:** Bosch Sensortec for BMP280 IC; breakout-board manufacturer - TBD
- **Manufacturer document:** [Bosch BMP280 datasheet](datasheets/bmp280_datasheet.pdf)
- **Photographs:** [`835813-bmp280-front.jpg`](photos/835813-bmp280-front.jpg), [`835813-bmp280-back.jpg`](photos/835813-bmp280-back.jpg)

Verified from the delivered board, receiving inspection 2026-09-04:

| Item | Observed | Status |
|---|---|---|
| Board marking | The shared purple artwork `GY-BM ☐E/☐P 280`, one tick box per variant | VERIFIED FROM HARDWARE |
| **BMP280 or BME280** | **Unresolved.** Neither variant box is legibly marked and the die text is below the photograph's resolution | TBD - blocking |
| Pin count and labels | **Six pins**: `VCC  GND  SCL  SDA  CSB  SDO` - the I2C/SPI variant, not the 4-pin I2C-only board | VERIFIED FROM HARDWARE |
| Onboard regulator | **None.** The board carries the sensor, four resistors and two capacitors, and nothing else | VERIFIED FROM HARDWARE |
| Fitted pull-ups | Four resistors marked `103` (10 kΩ) | VERIFIED FROM HARDWARE |
| SDO strap direction | TBD - continuity check | TBD |

**No regulator means no 5 V tolerance.** This board must be fed 3.3 V, which is what the
design intends; a 5 V source would destroy it. That is now a verified constraint rather than
an inference from the product name.

The BMP280-or-BME280 question is not cosmetic. A BME280 answers chip-ID `0x60` where a
BMP280 answers `0x58`, and it reports a humidity channel the telemetry format has no field
for. Resolve it by a macro photograph of the die or by reading the chip-ID register at
bring-up - **not from the product name**.

| Item | IC-level documented value or status | Breakout-board status | Source |
|---|---|---|---|
| Supply | BMP280 VDD and VDDIO ranges - use the downloaded datasheet | Board input and regulator - TBD | Bosch BMP280 datasheet |
| Logic | VDDIO-dependent at IC level | Board signal levels - TBD | Bosch BMP280 datasheet |
| Interface | I2C and SPI at IC level | Exposed interface - TBD | Bosch BMP280 datasheet and board schematic |
| I2C address | `0x76` or `0x77` based on SDO | SDO wiring - TBD | Bosch BMP280 datasheet |
| Pressure range | 300 to 1100 hPa at IC level | Board operating conditions - TBD | Bosch BMP280 datasheet |
| Temperature range | -40 to +85 degrees C at IC level | Board operating conditions - TBD | Bosch BMP280 datasheet |
| Accuracy | Depends on operating mode and conditions; exact required value must be taken from the datasheet | Board-level accuracy - TBD | Bosch BMP280 datasheet |
| Sampling/data rate | Configurable oversampling and standby/filter settings | Project configuration - TBD | Bosch BMP280 datasheet |
| Current | Mode-dependent IC and board current - TBD for project load | Board current - TBD | Datasheet and measurement |
| Pull-ups | Bus pull-ups and fitted values - TBD | Board schematic/inspection | Exact board documentation |
| Decoupling | IC requirements from datasheet | Existing board capacitors - TBD | Datasheet and board inspection |

### Sensor Integration Rule

The BMP280 and MPU-9250 documents describe the ICs. They do not establish the purchased breakout-board supply path, level shifting, pull-ups, capacitors, header labels, dimensions, or weight. Those values remain TBD until the exact boards are photographed and identified.

One value in particular cannot come from any datasheet: the magnetometer's hard and soft iron correction describes the **assembled vehicle** — its battery, its radio, its wiring — not the sensor. It must be measured on the finished airframe and re-measured whenever the layout changes.

## GPS

### NEO-6M GPS Module with EPROM

- **Exact product:** NEO-6M GPS Module with EPROM
- **Robu SKU:** 11782
- **Quantity:** 1
- **Robu reference:** [Robu SKU search](https://robu.in/?s=11782&post_type=product)
- **Controller manufacturer:** u-blox for NEO-6 series; breakout-board manufacturer - TBD
- **Manufacturer documentation:** [u-blox NEO-6 series](https://www.u-blox.com/en/product/neo-6-series)
- **Photographs:** [`11782-neo6m-front.jpg`](photos/11782-neo6m-front.jpg), [`11782-neo6m-back.jpg`](photos/11782-neo6m-back.jpg)

Verified from the delivered board, receiving inspection 2026-09-04:

| Item | Observed | Status |
|---|---|---|
| Board marking | `GY-NEO6MV2` | VERIFIED FROM HARDWARE |
| Module label | `u-blox`, `NEO-6M-0-001`, lot `1702`, serial `2422187473 8`, `0300 3`, with data-matrix code | VERIFIED FROM HARDWARE |
| Pin labels and order | **Four pins, printed `VCC  RX  TX  GND`** - `VCC` at one end, `GND` at the other | VERIFIED FROM HARDWARE |
| Onboard regulator | **Present** - one SOT-23-5 beside the header. Part marking not legible in the supplied photograph | VERIFIED FROM HARDWARE (presence); TBD (identity) |
| Stated supply range | **None printed anywhere on the board** | VERIFIED FROM HARDWARE (that nothing is stated) |
| EEPROM | `24C32A` (`FT5N4D`), 8-pin - the "with EPROM" of the product name | VERIFIED FROM HARDWARE |
| Backup cell | Present, a coin cell mounted on its side | VERIFIED FROM HARDWARE |
| Antenna | u.FL / IPEX socket; **active patch antenna supplied and already mated** | VERIFIED FROM HARDWARE |

**Do not assume this board runs from the 3.3 V rail.** It prints no voltage, its regulator is
unidentified, and GY-NEO6MV2 boards exist in both 5 V and 3.3 V arrangements. Identify the
regulator, or measure the module supply and the UART idle level, before it is wired.

`TX` and `RX` name the **board's own** pins, so the board's `TX` goes to the Pico's `RX`.

The backup cell holds almanac and time across power cycles, so the first cold fix after
delivery will be far slower than every fix after it. Take bring-up row 4.2 on a genuinely
cold receiver, then repeat it.

The following module-level details remain TBD until the purchased board is identified:

- Supply voltage range the board tolerates
- Logic levels
- Alternate interfaces beyond the exposed UART
- Default baud rate and supported baud rates
- Default and maximum update rate
- Position accuracy under stated conditions
- Antenna connector, antenna type, and active-antenna power
- EEPROM/flash configuration behavior
- Startup, fix, backup, and reset behavior
- Board pinout, dimensions, weight, current, and decoupling

GPS is planned as an additional sensor; no scoring result is claimed until it is integrated and demonstrated working.

## Data Storage

### Micro SD Card Reader Module

- **Exact product:** Micro SD Card Reader Module
- **Robu SKU:** 11566
- **Quantity:** 1
- **Robu reference:** [Robu SKU search](https://robu.in/?s=11566&post_type=product)
- **Manufacturer:** TBD
- **Datasheet/schematic:** TBD

Verified from the delivered board — photographs
[`11566-sd-reader-front.jpg`](photos/11566-sd-reader-front.jpg) and
[`11566-sd-reader-back.jpg`](photos/11566-sd-reader-back.jpg), receiving inspection
2026-09-04:

| Item | Observed | Status |
|---|---|---|
| Supply pin marking | Printed `3V3`. **No voltage range is printed anywhere on the board** | VERIFIED FROM HARDWARE |
| Onboard regulator | **None**, in any package, on either face | VERIFIED FROM HARDWARE |
| Onboard level shifter | **None.** No buffer, translator or transistor; the board has no active component | VERIFIED FROM HARDWARE |
| Pin labels and order | `GND  MISO  CLK  MOSI  CS  3V3` — six pins, printed on the underside | VERIFIED FROM HARDWARE |
| Fitted passives | Four resistors marked `103` (10 kΩ), silkscreened `10K`, and two unmarked capacitors | VERIFIED FROM HARDWARE |
| Interface | SPI — `CLK`, `MOSI`, `MISO`, `CS` broken out; no SDIO pins | VERIFIED FROM HARDWARE |
| Card retention | Friction / slide-in holder. No spring eject, no hinged tray | VERIFIED FROM HARDWARE |
| Operating voltage range | **TBD.** The board is a 3.3 V board, but the *range* it tolerates is a property of the card and the tracks, and nothing on the board states it | TBD |

The delivered module is a 3.3 V board. It runs from the vehicle's 3.3 V rail alongside every
other peripheral, needs no level shifting, and needs no second rail or boost stage. Earlier
revisions of this database recorded a 4.5-5.5 V input and an onboard 3.3 V regulator, taken
from the supplier listing; **the delivered board has neither**.

Two consequences follow from the absence of the level shifter, and neither is a
simplification:

- **The host must be 3.3 V.** With a translator the module tolerated a 5 V host. Without one,
  the Pico's 3.3 V logic is a requirement, and any 5 V feed reaches the card directly.
- **Nothing buffers MISO.** Only the card itself releases the line when `CS` goes high. A card
  that holds it corrupts the *radio's* next transaction on the shared SPI0 bus, and the
  symptom presents as a dead radio. The 10 kΩ pull-up defines an undriven line; it cannot
  overcome a card that is still driving one.

Still to be established, by measurement rather than by lookup:

- Idle, initialization, write, and peak current, with the intended card
- The same with the radio transmitting, since they share the rail
- MISO behaviour with CS inactive, on the shared SPI0 bus with the RA-02
- Continuity from the `3V3` pin to the socket's supply pad
- Which nets the four 10 kΩ resistors pull up
- Supported card type/capacity limits
- The tolerated supply range, if a manufacturer document for this board is ever found

Full analysis: [sd-module-analysis.md](sd-module-analysis.md).

## Power

### 1S LiPo Battery

- **Exact product ordered:** Orange 3.7 V 1500 mAh 25C 1S Lithium Polymer Battery Pack
- **Exact product delivered:** **Pro-Range** 3.7 V 1500 mAh 25C 1S Lithium Polymer Battery Pack
- **Robu SKU:** 1125094
- **Quantity:** 1
- **Robu product page:** [Orange 1500 mAh LiPo](https://robu.in/product/orange-1500mah-1s-25c-3-7-v-lithium-polymer-battery-pack-li-po/)
- **Manufacturer:** Pro-Range. Made in P.R.C. No manufacturer document located
- **Datasheet:** TBD
- **Photographs:** [`1125094-lipo-front.jpg`](photos/1125094-lipo-front.jpg), [`1125094-lipo-back.jpg`](photos/1125094-lipo-back.jpg)

**A different brand arrived from the one ordered.** Capacity, cell count, nominal voltage and
C-rating all match the BOM, so this is a substitution rather than a wrong part — but the
manufacturer document this database has been waiting on is a Pro-Range document, not an
Orange one.

Verified from the delivered pack's label, receiving inspection 2026-09-04:

| Item | Observed | Status |
|---|---|---|
| Brand and type | `Pro-Range` Lithium Polymer Battery, `Ω MATCHED`, `TRUE BALANCE` | VERIFIED FROM HARDWARE |
| Cell count and nominal voltage | `1 Cell 3.7V` | VERIFIED FROM HARDWARE |
| Capacity marking | `1500` mAh | VERIFIED FROM HARDWARE |
| Discharge marking | `25C`, i.e. a claimed 37.5 A continuous | VERIFIED FROM HARDWARE |
| Serial | `18726 52478` | VERIFIED FROM HARDWARE |
| Main discharge connector | Red 2-pin JST-RCY (BEC) style | VERIFIED FROM HARDWARE |
| Balance connector | White 2-pin JST-XH style | VERIFIED FROM HARDWARE |
| Physical condition as delivered | Flat, square, no puffing or discolouration | VERIFIED FROM HARDWARE |
| Protection circuit | **Not visible.** Leads exit under heat-shrink; absence cannot be proven from outside | TBD |
| Charge current and cutoff voltage | **Not printed on the label.** The label says only "Charge Battery only with recommended charger" | TBD |
| Burst discharge | Not printed | TBD |
| Polarity | **TBD — meter only.** Lead colour is a convention, not evidence | TBD |
| Open-circuit voltage as delivered | TBD | TBD |

Assumed from the BOM, not from the pack:

- Approximate full-charge voltage: 4.2 V

**Neither connector mates with anything in this project**, and **no 1S charger was supplied
or is on the BOM**. Both are purchases before any powered test.

Still required from the exact battery documentation or label:

- Safe operating voltage range and cutoff
- Recommended and maximum continuous discharge current
- Permitted peak discharge current and duration
- Charging voltage, current, and termination method
- Built-in protection status
- Connector type and polarity
- Cell construction and safety restrictions
- Dimensions and weight

No battery protection, charger, cutoff, or regulator is selected.

### 3.3 V Regulated Power Supply

- **Quantity:** TBD
- **Product/model:** TBD
- **Status:** Planned, not selected

The regulator cannot be selected until the exact Pico, RA-02, sensor-board, GPS-board, and SD-reader requirements and measured transients are known. No current rating, efficiency, dropout, capacitor, or protection specification is invented here.

## Antennas and RF

The onboard and ground-station RA-02 modules each require their corresponding antenna and IPEX-to-SMA cable. The antenna SKU description and live Robu page title conflict on SMA versus RP-SMA terminology. Physical connector mating, cable polarity, RF impedance, frequency response, installation, strain relief, and antenna ground/reference requirements must be verified before radio power-up.

## Prototyping Hardware

### Universal Prototype PCB

- **Exact product:** 10 x 10 cm Universal PCB Prototype Board, Single-Sided, 2.54 mm Hole Pitch
- **Robu SKU:** 1031002
- **Quantity:** 2 total; one intended onboard
- **Robu product page:** [Robu prototype PCB](https://robu.in/product/10-x-10-cm-universal-pcb-prototype-board-single-sided-2-54mm-hole-pitch/)
- **Manufacturer:** TBD
- **Datasheet:** TBD

Verified from the delivered board - photographs
[`1031002-protoboard-front.jpg`](photos/1031002-protoboard-front.jpg) and
[`1031002-protoboard-back.jpg`](photos/1031002-protoboard-back.jpg), receiving inspection
2026-09-04:

| Item | Observed | Status |
|---|---|---|
| Size and pitch | Silkscreened `10*10CM` and `2.54MM`, matching the product description | VERIFIED FROM HARDWARE |
| Sidedness | **Single-sided.** Copper on one face; the reverse is bare laminate carrying the grid silkscreen | VERIFIED FROM HARDWARE |
| Copper pattern | **Individually isolated round pads.** No strips, no linked rows. One row of elongated pads along the top and bottom edges | VERIFIED FROM HARDWARE |
| Grid reference | Columns `A`-`Z` then `A`-`K` (36); rows `01`-`35` | VERIFIED FROM HARDWARE |
| Mounting | Four corner holes | VERIFIED FROM HARDWARE |
| Thickness, material, current capability, mass | TBD - not measurable from a photograph | TBD |

**Isolated pads on a single-sided board mean every connection is a wire, and there are no
power or ground rails.** A 3.3 V bus and a GND bus must be created by hand - a soldered bus
wire, or a run of bridged pads - before any module is placed. Plan those two runs first;
retrofitting them under a populated board is unpleasant.

The printed grid is worth using: recording each module's corner pad in the assembly notes
lets the layout survive being taken apart. This is a generic prototype board, not evidence of
a custom PCB.

## Source and Verification Rules

1. Use the exact Robu SKU to identify the purchased product.
2. Use manufacturer documentation for chip-level limits only when its scope is clear.
3. Do not transfer chip specifications to a breakout board without a board schematic or manufacturer documentation.
4. Record the exact document revision and board marking used for each electrical decision.
5. Measure current and rail behavior after documentation review; datasheet typical values are not a substitute for a system measurement.
6. Do not create the Pico pin map until all `TBD` interface, voltage, logic, pinout, pull-up, capacitor, and current items are resolved.
