# Robu Product References

These entries identify the purchased items by the supplied Robu SKU. Robu is the source for exact product identity; manufacturer documents are used for component-level electrical specifications only when the product identity and document scope are clear.

The Status column now records the **delivered** board where receiving inspection on 2026-09-04 disagreed with the listing. Three entries did. Photographs are in [`../photos/`](../photos/); the transcription is in [receiving-inspection.md](../receiving-inspection.md).

| Component | Robu SKU | Robu Product Page | Datasheet | Status |
|---|---:|---|---|---|
| SX1278 LoRa Module RA-02 433 MHz Wireless Spread Spectrum Transmission | 1150780 | [Robu product page](https://robu.in/product/sx1278-lora-module-ra-02-433mhz-wireless-spread-spectrum-transmission/) | [Semtech SX1276/77/78/79 datasheet](https://www.semtech.com/uploads/documents/DS_SX1276-7-8-9_W_APP_V7.pdf) | **Delivered board inspected.** Shield reads `Ra-02  ISM:410-525MHz  LoRa/FSK/OOK  PA:+18dBm`; carrier `LoRa-02 SX1278 433MHz`. u.FL socket, `3.3V` supply pin, no regulator, no level shifter, header order transcribed. Semtech document remains chip-level |
| LoRa Antenna 433 MHz with SMA Male Connector | 1121334 | [Robu product page](https://robu.in/product/lora-antenna-433mhz/) | TBD | **Delivered part inspected. The listing's *gender* is right and the BOM's is wrong**: the antenna shell is female. SMA versus RP-SMA still unresolved — the centre contact was not photographed. It mates with the supplied cable and the RA-02 with no adapter |
| 10CM IPEX1 to SMA Female Connector Cable 11mm RG1.13 | 1674982 | [Robu product page](https://robu.in/product/10cm-ipex1-to-sma-female-connector-cable-11mm-rg1-13/) | TBD | **Delivered part inspected.** Male bulkhead shell with nut and star washer; IPEX-1 / u.FL plug at the other end; mates with both the antenna and the RA-02. Unmarked. Centre contact not photographed; RF specification TBD |
| MPU-9250 9-Axis Accelerometer, Gyroscope and Magnetometer | 2846 | [Robu SKU search](https://robu.in/?s=2846&post_type=product) | InvenSense MPU-9250 product specification and register map; AKM AK8963 datasheet | **A different part from the one ordered: the BOM said MPU-6050, an MPU-9250 arrived.** Board `GY-6500 / GY-9250  V356`, die marked `MP92` (an MPU-6500 die reads `MP65`), 10-pin header, regulator fitted but unidentified, five 10 kΩ pull-ups. `WHO_AM_I` at bring-up is still the final confirmation |
| NEO-6M GPS Module with EPROM | 11782 | [Robu SKU search](https://robu.in/?s=11782&post_type=product) | [u-blox NEO-6 series documentation](https://www.u-blox.com/en/product/neo-6-series) | **Delivered board inspected.** `GY-NEO6MV2`, module labelled `u-blox NEO-6M-0-001`, `24C32A` EEPROM, backup cell, u.FL socket with the active patch antenna supplied. 4-pin `VCC RX TX GND`. Regulator fitted but unidentified, and **the board prints no supply range** |
| Micro SD Card Reader Module | 11566 | [Robu SKU search](https://robu.in/?s=11566&post_type=product) | TBD | **A different board from the one the listing describes.** The listing says 4.5-5.5 V input with an onboard 3.3 V regulator; **the delivered board has neither**. Supply pin printed `3V3`, no regulator, no level shifter, four 10 kΩ pull-ups, header `GND MISO CLK MOSI CS 3V3`, friction card holder |
| GY-BMP280-3.3 Precision Altimeter Atmospheric Pressure Sensor Module | 835813 | [Robu SKU search](https://robu.in/?s=835813&post_type=product) | [Bosch BMP280 datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp280-ds001.pdf) | **Delivered board inspected, and it is the shared-artwork variant**: purple 6-pin `GY-BM ☐E/☐P 280` with `VCC GND SCL SDA CSB SDO`. No regulator, so 3.3 V only. **Neither the E nor the P box is legibly marked and the die text is illegible — BMP280 versus BME280 is unresolved** |
| Raspberry Pi Pico | 894292 | [Robu product page](https://robu.in/product/raspberry-pi-pico/) | [Local Raspberry Pi Pico datasheet](../datasheets/raspberry_pi_pico_datasheet.pdf) and [official online copy](https://datasheets.raspberrypi.com/pico/pico-datasheet.pdf) | Product and manufacturer datasheet identified. **Delivered board inspected**: `Raspberry Pi Pico © 2020`, RP2040 marked `RP2-B2`, a Pico rather than a Pico W, **headers not fitted and none supplied** |
| Orange 3.7V 1500mAh 25C 1S Lithium Polymer Battery Pack | 1125094 | [Robu product page](https://robu.in/product/orange-1500mah-1s-25c-3-7-v-lithium-polymer-battery-pack-li-po/) | TBD | **A different brand arrived: the pack is Pro-Range, not Orange.** Capacity, cell count, nominal voltage and 25C rating all match. Red JST-RCY main lead, white 2-pin JST-XH balance lead, neither of which mates with anything in this project. **The label states no charge current and no cutoff voltage** |
| 10 x 10 cm Universal PCB Prototype Board, Single-Sided, 2.54 mm Hole Pitch | 1031002 | [Robu product page](https://robu.in/product/10-x-10-cm-universal-pcb-prototype-board-single-sided-2-54mm-hole-pitch/) | TBD | **Delivered board inspected.** Silkscreened `10*10CM  2.54MM`, single-sided, individually isolated round pads with an edge rail top and bottom, `A`-`Z`/`A`-`K` by `01`-`35` grid, four corner mounting holes. Thickness and material TBD |

## Downloaded Documents

The following manufacturer documents were downloaded into `documentation/hardware/datasheets/`:

- `raspberry_pi_pico_datasheet.pdf` - Raspberry Pi Pico datasheet from Raspberry Pi.
- `bmp280_datasheet.pdf` - BMP280 datasheet from Bosch Sensortec.

The Semtech and InvenSense URLs returned HTML/error content in the current environment rather than PDFs, so invalid files were not retained. The u-blox datasheet URL attempted returned 404. No unauthorized mirror was downloaded.

## Documentation Limits

The RA-02, sensor boards, GPS board, SD reader, battery, antenna, cable, and prototype PCB are purchased products or breakouts. A chip or controller datasheet does not by itself identify the complete breakout-board circuit. Board-level supply voltage, logic levels, regulators, level shifting, pull-ups, capacitors, pin labels, current, dimensions, and weight remain `TBD` unless documented by the exact board source or verified from photographs and measurements.

## What the delivery proved about listings

Three of the ten entries above were contradicted by the parts that arrived: the IMU was a
different device from the one ordered, the microSD reader had neither the input range nor
the regulator its listing advertised, and the battery was a different brand. A fourth, the
antenna, had the BOM and the listing disagreeing with each other, and the delivered part
sided with the listing.

That is the case for photographing every board before designing around it, stated as a
result rather than as a policy. **A SKU identifies what was bought. It does not describe what
was built.**

Photographs establish silkscreen text, component packages and physical fit. They do not
establish voltages, strap directions, continuity or current — those still need a meter, and
the rows waiting on one are marked `TBD` in
[receiving-inspection.md](../receiving-inspection.md) rather than filled in by inference.
