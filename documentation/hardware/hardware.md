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

| Item | Value/status | Source |
|---|---|---|
| Module supply voltage | RA-02 board range - TBD | Exact RA-02 module documentation required |
| Chip supply voltage | SX1278 chip-level range - verify in Semtech datasheet | Semtech datasheet; not module proof |
| Logic voltage | RA-02 signal levels - TBD | Exact module documentation required |
| TX current | RA-02 current - TBD | Exact module documentation and measurement |
| TX power | Configured/module output and limits - TBD | Exact module/RF documentation |
| Frequency range | RA-02 configuration range - TBD; chip-level information is not module certification | Module and Semtech documents |
| Interface | SPI/control interface - verify for RA-02 | Exact module documentation |
| SPI pins | SCK, MOSI, MISO, NSS/CS - exact header mapping TBD | RA-02 pinout required |
| Operating modes | Sleep, standby, receive, transmit, and mode-control details - TBD at module level | Module and Semtech documents |
| RF connection | Antenna connector and cable compatibility - TBD | RA-02 and cable/antenna documents |
| RF requirements | Correct antenna, grounding, supply stability, and launch configuration - TBD | Module/RF documentation and competition requirements |

The launch sync words are competition requirements and are recorded in `requirements/requirements.md`; they are not electrical specifications.

### Antenna

- **Exact product:** LoRa Antenna 433 MHz with SMA Male Connector, as supplied by the project BOM
- **Robu SKU:** 1121334
- **Quantity:** 2
- **Robu product page:** [Robu antenna page](https://robu.in/product/lora-antenna-433mhz/)
- **Manufacturer:** TBD
- **Datasheet:** TBD

The live Robu page title observed during this pass says “RP-SMA Female Connector,” which conflicts with the supplied BOM description “SMA Male Connector.” Connector gender/type, impedance, gain, dimensions, mounting, power handling, and frequency response require physical and product-page verification before RF assembly.

### IPEX-to-SMA Cable

- **Exact product:** 10CM IPEX1 to SMA Female Connector Cable 11mm RG1.13
- **Robu SKU:** 1674982
- **Quantity:** 2
- **Robu product page:** [Robu IPEX-to-SMA cable](https://robu.in/product/10cm-ipex1-to-sma-female-connector-cable-11mm-rg1-13/)
- **Manufacturer:** TBD
- **Datasheet:** TBD

Cable length and RG1.13 description come from the supplied product name. IPEX version, mating connector, SMA/RP-SMA identity, impedance, loss, power handling, and RF frequency suitability remain TBD.

## Sensors

### MPU-9250 Module

- **Exact product:** MPU-9250 9-Axis Accelerometer, Gyroscope and Magnetometer Sensor
- **Robu SKU:** 2846
- **Quantity:** 1
- **Robu reference:** [Robu SKU search](https://robu.in/?s=2846&post_type=product)
- **Manufacturer:** InvenSense/TDK for the MPU-9250; AKM for the AK8963 magnetometer die; breakout-board manufacturer - TBD
- **Manufacturer documents:** MPU-9250 Product Specification (PS-MPU-9250A-01) and MPU-9250 Register Map (RM-MPU-9250A-00); AK8963 datasheet for the magnetometer

The exact Robu breakout page and board schematic were not resolved. The manufacturer documents apply to the ICs, not necessarily to the purchased carrier board.

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

The exact Robu product page, board schematic, and board revision were not resolved. The following module-level details therefore remain TBD until the purchased board is identified:

- Supply voltage and whether the board includes a regulator
- Logic levels
- UART pins and any alternate interfaces
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

- **Operating voltage:** DC 2.6-3.6 V (VERIFIED FROM HARDWARE, receiving inspection)
- **Interface:** SPI (VERIFIED FROM HARDWARE, receiving inspection)

The module received is a 3.3 V board. It runs from the vehicle's 3.3 V rail alongside every
other peripheral, needs no level shifting for supply reasons, and needs no second rail or
boost stage. Earlier revisions of this database recorded a 4.5-5.5 V requirement taken from
a supplier listing; the delivered board does not agree with that listing.

Still to be established, by measurement rather than by lookup:

- Idle, initialization, write, and peak current, with the intended card
- The same with the radio transmitting, since they share the regulator
- MISO behaviour with CS inactive, on the shared SPI0 bus with the RA-02
- Pull-up presence and values
- Required capacitors and their placement
- Supported card type/capacity limits
- Exact pin labels and physical pin order
- Connector and card-retention details

Full analysis: [sd-module-analysis.md](sd-module-analysis.md).

## Power

### Orange 1S LiPo Battery

- **Exact product:** Orange 3.7 V 1500 mAh 25C 1S Lithium Polymer Battery Pack
- **Robu SKU:** 1125094
- **Quantity:** 1
- **Robu product page:** [Orange 1500 mAh LiPo](https://robu.in/product/orange-1500mah-1s-25c-3-7-v-lithium-polymer-battery-pack-li-po/)
- **Manufacturer:** TBD
- **Datasheet:** TBD

Known from the supplied BOM:

- Nominal voltage: 3.7 V
- Approximate full-charge voltage: 4.2 V
- Capacity marking: 1500 mAh
- Discharge marking: 25C

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

The 10 x 10 cm size and 2.54 mm hole pitch are from the supplied product description. Board thickness, copper pattern, current capability, material, weight, and mounting details are TBD. This is a generic prototype board, not evidence of a custom PCB.

## Source and Verification Rules

1. Use the exact Robu SKU to identify the purchased product.
2. Use manufacturer documentation for chip-level limits only when its scope is clear.
3. Do not transfer chip specifications to a breakout board without a board schematic or manufacturer documentation.
4. Record the exact document revision and board marking used for each electrical decision.
5. Measure current and rail behavior after documentation review; datasheet typical values are not a substitute for a system measurement.
6. Do not create the Pico pin map until all `TBD` interface, voltage, logic, pinout, pull-up, capacitor, and current items are resolved.
