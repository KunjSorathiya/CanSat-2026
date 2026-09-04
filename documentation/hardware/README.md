# CanSat Hardware Documentation

This directory is the controlled reference for the hardware used by the CanSat 2026 project.

## Contents

- [Hardware Reference](hardware.md) - Single hardware database for the confirmed BOM, electrical specifications, interfaces, integration notes, and unresolved items.
- [Receiving Inspection Record](receiving-inspection.md) - The fill-in record for the delivered hardware: inventory, photographs, per-board identification, and the questions that must be answered before power is applied.
- [Robu Product References](product-pages/README.md) - Exact Robu SKU references, product-page links, source status, and documentation limits.
- `datasheets/` - Manufacturer datasheets or official technical PDFs downloaded for this project.
- [`photos/`](photos/README.md) - Photographs of the delivered boards, the evidence behind every fact promoted out of `TBD`.

## Source Policy

The source order is:

1. Manufacturer datasheet
2. Manufacturer technical documentation
3. Exact Robu product page
4. Other reputable technical source only when the higher-priority sources are unavailable

Chip-level documents are not treated as breakout-board documentation. Board-level supply voltage, logic levels, regulators, level shifting, pull-ups, capacitors, pin labels, current, dimensions, and weight remain `TBD` unless the exact board source identifies them or the team verifies them from the physical hardware.

## Current Documentation Status

The Raspberry Pi Pico and Bosch BMP280 manufacturer PDFs are stored locally in `datasheets/`. The exact breakout-board documentation for the RA-02, MPU-9250, NEO-6M, GY-BMP280-3.3, and Micro SD reader is incomplete. The Micro SD reader, SKU 11566, is a blocking item because breakout boards can differ in supply voltage, regulation, level shifting, interface, and pinout.

The IMU is a nine-axis MPU-9250: accelerometer, gyroscope and an AK8963 magnetometer on a second die at I2C address `0x0C`. Two things about it must be confirmed physically rather than assumed — that the part is a real MPU-9250 and not an MPU-6500 sold as one, and which way the board's printed axes point.

The antenna description also requires physical confirmation: the supplied BOM says SMA male, while the live Robu page title observed for SKU 1121334 says RP-SMA female.

## Before Electrical Design

Do not create the Pico pin map, select a regulator, or connect module power until the exact board variants, product documentation, supply ranges, logic levels, interfaces, current requirements, pinouts, pull-ups, and capacitor requirements are recorded in [hardware.md](hardware.md).

Now that the hardware has been delivered, most of those items are resolved by unpowered inspection rather than by further searching. Work through [receiving-inspection.md](receiving-inspection.md) and feed each observation back into [hardware.md](hardware.md).
