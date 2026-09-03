# Micro SD Reader Module Analysis

## Scope

This document analyzes the exact Micro SD Card Reader Module identified as Robu SKU 11566 for integration with the CanSat's Raspberry Pi Pico, shared SPI bus, and current power architecture.

The analysis does not assume that all Micro SD breakout boards are electrically equivalent. No wiring is approved, no level shifter is selected, and no regulator is selected.

## Evidence Classifications

- **CONFIRMED** - Directly verified from an authoritative exact-product source or project hardware record.
- **MANUFACTURER DOCUMENTED** - From manufacturer documentation, but not necessarily the exact Robu breakout board.
- **ROBU DOCUMENTED** - Explicitly stated by the Robu listing information for SKU 11566.
- **INFERRED** - A conclusion derived from documented values; the reasoning is stated.
- **PHYSICAL VERIFICATION REQUIRED** - Cannot safely be determined without the actual board, markings, schematic, or measurement.

## Exact Product Identity

| Item | Value | Evidence |
|---|---|---|
| Product | Micro SD Card Reader Module | ROBU DOCUMENTED / project BOM |
| Robu SKU | 11566 | ROBU DOCUMENTED |
| Quantity | 1 | CONFIRMED project BOM |
| Robu reference | [Robu SKU 11566 search](https://robu.in/?s=11566&post_type=product) | ROBU reference; stable canonical product page not resolved |
| Manufacturer | TBD | PHYSICAL VERIFICATION REQUIRED |
| Exact board revision | TBD | PHYSICAL VERIFICATION REQUIRED |
| Board schematic | TBD | PHYSICAL VERIFICATION REQUIRED |

## Electrical Summary

| Property | Current value/status | Evidence level |
|---|---|---|
| Required VCC input | 4.5–5.5 V | ROBU DOCUMENTED |
| Onboard regulator | Robu listing states an onboard 3.3 V regulator | ROBU DOCUMENTED; physical confirmation still required |
| SD-card supply voltage | TBD; the regulator output is not independently documented for this exact board | PHYSICAL VERIFICATION REQUIRED |
| Host logic voltage | TBD | PHYSICAL VERIFICATION REQUIRED |
| SD-card logic voltage | TBD | PHYSICAL VERIFICATION REQUIRED |
| Interface | GND, VCC, MISO, MOSI, SCK, CS are listed; SPI behavior is inferred but not board-verified | ROBU DOCUMENTED / INFERRED |
| SPI signal voltage at CS | TBD | PHYSICAL VERIFICATION REQUIRED |
| SPI signal voltage at SCK | TBD | PHYSICAL VERIFICATION REQUIRED |
| SPI signal voltage at MOSI | TBD | PHYSICAL VERIFICATION REQUIRED |
| SPI signal voltage at MISO | TBD | PHYSICAL VERIFICATION REQUIRED |
| Level shifting | Presence and type unknown | PHYSICAL VERIFICATION REQUIRED |
| Typical current | TBD | PHYSICAL VERIFICATION REQUIRED |
| Startup current | TBD | PHYSICAL VERIFICATION REQUIRED |
| Read current | TBD | PHYSICAL VERIFICATION REQUIRED |
| Write current | TBD | PHYSICAL VERIFICATION REQUIRED |
| Peak/transient current | TBD | PHYSICAL VERIFICATION REQUIRED |
| Pull-ups | TBD | PHYSICAL VERIFICATION REQUIRED |
| Decoupling | TBD | PHYSICAL VERIFICATION REQUIRED |
| Pinout | GND, VCC, MISO, MOSI, SCK, CS as listed by Robu | ROBU DOCUMENTED; physical order and electrical behavior TBD |

## Regulator and SD-Card Rail

The Robu listing states that SKU 11566 accepts 4.5–5.5 V input and contains an onboard 3.3 V regulator. That establishes the advertised input requirement and regulator claim at the listing level. It does not establish:

- The regulator part number
- Its output tolerance
- Its output current rating
- Whether the output powers only the card or also other circuitry
- The actual SD-card rail voltage under load
- The regulator dropout or thermal behavior
- Required input/output capacitors
- Whether host-side signals are level shifted

**Onboard regulator status:** ROBU DOCUMENTED, but physical verification and a schematic are still required before relying on it in the CanSat power design.

## Logic-Level and Level-Shifting Analysis

The board may contain any of the following, but none is currently documented for the exact purchased board:

- Resistor-based level shifting
- Transistor-based level shifting
- A dedicated level-shifter IC
- A regulator only, with direct host/card signal connections
- Some combination of the above

The voltage seen by each SPI signal cannot be safely inferred from the presence of a 3.3 V regulator. The exact voltage at CS, SCK, MOSI, and MISO is therefore:

- **CS:** PHYSICAL VERIFICATION REQUIRED
- **SCK:** PHYSICAL VERIFICATION REQUIRED
- **MOSI:** PHYSICAL VERIFICATION REQUIRED
- **MISO:** PHYSICAL VERIFICATION REQUIRED

**Logic-level compatibility: PHYSICAL VERIFICATION REQUIRED.**

The Pico must not be connected directly to the reader until the host-side input thresholds and MISO output level are established. In particular, the board's 4.5–5.5 V input requirement does not prove that its signal pins are 5 V tolerant, 3.3 V compatible, or level shifted.

## Power-Source Compatibility

### 1S LiPo Directly

**Decision: NO, based on the documented input requirement.**

Reasoning:

- The project battery is approximately 3.7 V nominal and approximately 4.2 V when fully charged.
- Robu specifies 4.5–5.5 V input for SKU 11566.
- The battery maximum stated for the project is below the reader's stated minimum input.
- Battery voltage variation and load sag make direct operation even less defensible.

A direct connection would violate the stated input range unless the exact product documentation proves that the listing is inaccurate or the board has a different input path. No such evidence exists.

### Pico VSYS

**Decision: NO, based on the documented input requirement.**

The Pico VSYS path follows the switched 1S LiPo in the accepted architecture. VSYS therefore does not provide the reader's stated 4.5–5.5 V input. The Pico's onboard 3.3 V regulator does not raise VSYS to the required reader input voltage.

### Pico 3.3 V Output

**Decision: NO, based on the documented input requirement.**

The reader is listed as requiring at least 4.5 V input. The Pico's 3.3 V output is below that stated minimum. The reader's onboard regulator does not change the fact that its input must be supplied within the listed range.

The Pico 3.3 V output must also not be assumed capable of supplying the reader's unknown startup or write current.

### Separate 5 V Rail

**Decision: Potentially YES, with a separate rail, but not yet approved.**

A separate regulated rail within the Robu-stated 4.5–5.5 V input range could satisfy the reader's advertised VCC requirement. However, integration still depends on:

- Confirming the actual board accepts that rail
- Confirming the onboard regulator and its output
- Confirming host-side CS, SCK, MOSI, and MISO logic levels
- Confirming whether level shifting is present and correctly directed
- Confirming typical, startup, read, write, and peak current
- Providing required decoupling
- Confirming shared SPI MISO behavior when CS is inactive

No separate 5 V rail or converter is selected by this document.

## SPI Architecture Compatibility

The logical shared SPI arrangement remains:

```text
Pico SPI bus
    SCK  -> RA-02 + SD reader
    MOSI -> RA-02 + SD reader
    MISO <- RA-02 + SD reader
    RA-02 CS -> dedicated selection
    SD CS    -> dedicated selection
```

This is logically compatible with a standard SPI reader because the reader has the listed MISO, MOSI, SCK, and CS signals. Electrical shared-bus compatibility is not established until the reader's signal voltage and inactive-MISO behavior are verified.

Only one CS should be asserted at a time. The reader must not drive MISO while its CS is inactive if it is to share the bus safely with the RA-02.

## Current and Startup Behavior

No typical, maximum, startup, read, write, or peak current value has been verified for SKU 11566. The following loads must be measured or documented before power-converter sizing:

- Board startup
- SD-card initialization
- Idle state
- Sequential reads
- Sequential writes
- File creation and flush operations
- Worst-case write transient
- Behavior with the intended card

No battery-life or regulator-capacity conclusion can be made from the current information.

## Decoupling

Required decoupling is **PHYSICAL VERIFICATION REQUIRED**. Obtain the exact board schematic or inspect the board for:

- VCC input capacitors
- Regulator input capacitor
- Regulator output capacitor
- SD-card local bypass capacitor
- Capacitor values and voltage ratings
- Capacitor placement relative to regulator and card socket

Additional capacitors must not be selected until the board circuit and measured write transients are known.

## Integration Verdict

### Can the SD module safely operate with our 1S LiPo + Raspberry Pi Pico architecture?

**UNKNOWN pending physical verification.**

The power-source conclusions are more specific:

- **LiPo directly:** NO, because 3.7 V nominal and approximately 4.2 V full charge are below the Robu-stated 4.5–5.5 V input range.
- **Pico VSYS:** NO for the same input-range reason.
- **Pico 3.3 V:** NO for the same input-range reason.
- **Separate 5 V rail:** Potentially YES, but only after the exact board's logic levels, level shifting, current, regulator behavior, and SPI bus behavior are verified.

The module cannot currently be declared safe for direct Pico SPI connection. The onboard regulator claim does not establish logic-level compatibility.

## Required Physical Verification

The minimum information needed to resolve the remaining uncertainty is:

1. **Front photograph of SKU 11566** showing the complete board, labels, card socket, and all visible components.
2. **Back photograph** showing traces, components, jumpers, and any regulator or level-shifter packages.
3. **All IC markings** including regulator, level-shifter, buffer, transistor, and controller markings.
4. **Exact pin labels and physical pin order** for GND, VCC, MISO, MOSI, SCK, and CS.
5. **Board input documentation or schematic** confirming the 4.5–5.5 V input path.
6. **Regulator identification** and confirmation of its output voltage, tolerance, current rating, and capacitor requirements.
7. **Level-shifting identification**: resistor network, transistor network, dedicated IC, or direct traces.
8. **Signal-voltage measurements** at CS, SCK, MOSI, and MISO relative to board ground, including MISO with CS inactive.
9. **Current measurements** during startup, initialization, read, write, and worst-case write activity using the intended SD card.
10. **Decoupling inspection or schematic evidence** for the VCC input, regulator, and card rail.

Until these checks are complete, do not connect the module to Pico power or SPI signals.
