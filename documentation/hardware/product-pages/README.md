# Robu Product References

These entries identify the purchased items by the supplied Robu SKU. Robu is the source for exact product identity; manufacturer documents are used for component-level electrical specifications only when the product identity and document scope are clear.

| Component | Robu SKU | Robu Product Page | Datasheet | Status |
|---|---:|---|---|---|
| SX1278 LoRa Module RA-02 433 MHz Wireless Spread Spectrum Transmission | 1150780 | [Robu product page](https://robu.in/product/sx1278-lora-module-ra-02-433mhz-wireless-spread-spectrum-transmission/) | [Semtech SX1276/77/78/79 datasheet](https://www.semtech.com/uploads/documents/DS_SX1276-7-8-9_W_APP_V7.pdf) | Robu page identified; Semtech document is chip-level, not a complete RA-02 module document |
| LoRa Antenna 433 MHz with SMA Male Connector | 1121334 | [Robu product page](https://robu.in/product/lora-antenna-433mhz/) | TBD | Robu page found, but its live title says “RP-SMA Female Connector”; supplied BOM says SMA male. Connector identity requires physical verification |
| 10CM IPEX1 to SMA Female Connector Cable 11mm RG1.13 | 1674982 | [Robu product page](https://robu.in/product/10cm-ipex1-to-sma-female-connector-cable-11mm-rg1-13/) | TBD | Robu page identified; cable manufacturer and detailed RF specification TBD |
| MPU-9250 9-Axis Accelerometer, Gyroscope and Magnetometer | 2846 | [Robu SKU search](https://robu.in/?s=2846&post_type=product) | InvenSense MPU-9250 product specification and register map; AKM AK8963 datasheet | SKU appears in Robu indexed results; current canonical product page not resolved; breakout variant TBD, and `WHO_AM_I` must be read to confirm the part is not an MPU-6500 |
| NEO-6M GPS Module with EPROM | 11782 | [Robu SKU search](https://robu.in/?s=11782&post_type=product) | [u-blox NEO-6 series documentation](https://www.u-blox.com/en/product/neo-6-series) | SKU appears in Robu indexed results; current canonical product page not resolved; breakout variant TBD |
| Micro SD Card Reader Module | 11566 | [Robu SKU search](https://robu.in/?s=11566&post_type=product) | TBD | SKU appears in Robu indexed results; current canonical product page not resolved; exact reader breakout documentation unavailable |
| GY-BMP280-3.3 Precision Altimeter Atmospheric Pressure Sensor Module | 835813 | [Robu SKU search](https://robu.in/?s=835813&post_type=product) | [Bosch BMP280 datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp280-ds001.pdf) | SKU appears in Robu indexed results; current canonical product page not resolved; Bosch document is chip-level, breakout details TBD |
| Raspberry Pi Pico | 894292 | [Robu product page](https://robu.in/product/raspberry-pi-pico/) | [Local Raspberry Pi Pico datasheet](../datasheets/raspberry_pi_pico_datasheet.pdf) and [official online copy](https://datasheets.raspberrypi.com/pico/pico-datasheet.pdf) | Product and manufacturer datasheet identified |
| Orange 3.7V 1500mAh 25C 1S Lithium Polymer Battery Pack | 1125094 | [Robu product page](https://robu.in/product/orange-1500mah-1s-25c-3-7-v-lithium-polymer-battery-pack-li-po/) | TBD | Product page identified; battery manufacturer/model datasheet and connector details TBD |
| 10 x 10 cm Universal PCB Prototype Board, Single-Sided, 2.54 mm Hole Pitch | 1031002 | [Robu product page](https://robu.in/product/10-x-10-cm-universal-pcb-prototype-board-single-sided-2-54mm-hole-pitch/) | TBD | Product page identified; manufacturer and material/electrical datasheet TBD |

## Downloaded Documents

The following manufacturer documents were downloaded into `documentation/hardware/datasheets/`:

- `raspberry_pi_pico_datasheet.pdf` - Raspberry Pi Pico datasheet from Raspberry Pi.
- `bmp280_datasheet.pdf` - BMP280 datasheet from Bosch Sensortec.

The Semtech and InvenSense URLs returned HTML/error content in the current environment rather than PDFs, so invalid files were not retained. The u-blox datasheet URL attempted returned 404. No unauthorized mirror was downloaded.

## Documentation Limits

The RA-02, sensor boards, GPS board, SD reader, battery, antenna, cable, and prototype PCB are purchased products or breakouts. A chip or controller datasheet does not by itself identify the complete breakout-board circuit. Board-level supply voltage, logic levels, regulators, level shifting, pull-ups, capacitors, pin labels, current, dimensions, and weight remain `TBD` unless documented by the exact board source or verified from photographs and measurements.
