# Preliminary Pico Resource Map

## Purpose and Scope

This document is a provisional logical resource allocation for the onboard Raspberry Pi Pico. It answers which buses and control resources the CanSat needs, without assigning GPIO numbers or finalizing electrical wiring.

The logical architecture can be defined before every breakout board is electrically verified. The following remain dependencies for final wiring and the final GPIO map:

- Exact breakout-board voltage and logic levels
- Exposed interfaces and pin labels
- Pull-ups, level shifting, and interrupt availability
- SD reader signal circuitry
- RA-02 carrier-board pinout

No regulator is selected, no GPIO number is assigned, and no PCB or firmware is created by this document.

## Proposed Logical Architecture

```text
Raspberry Pi Pico
|
+-- I2C bus
|   +-- MPU-9250
|   +-- BMP280
|
+-- SPI bus
|   +-- SX1278 RA-02
|   |   +-- dedicated CS/NSS
|   |   +-- RESET control
|   |   +-- DIO0 event input
|   |   +-- optional DIO1 event input
|   +-- Micro SD reader
|       +-- dedicated CS
|
+-- UART
|   +-- NEO-6M GPS
|   +-- second UART retained for debug or expansion if pin mux permits
|
+-- GPIO
|   +-- Status LED
|   +-- RA-02 CS/NSS, RESET, DIO0, optional DIO1
|   +-- SD CS
|   +-- MPU-9250 INT
|
+-- ADC
|   +-- one channel reserved for future battery monitoring
|
+-- USB/SWD
    +-- development, programming, and debugging
```

This is a logical map, not a wiring diagram. The Pico pin map must be created only after the affected breakout-board dependencies are resolved.

## I2C Analysis

### Logical Sharing

The MPU-9250 and BMP280 can logically share one I2C bus if both breakout boards expose I2C and operate at a compatible bus voltage. Their documented IC-level address options do not create a required conflict:

| Device | IC-level address information | Address-selection signal | Conflict assessment |
|---|---|---|---|
| MPU-9250 | `0x68` or `0x69` | AD0 | Does not conflict with the BMP280 address range |
| BMP280 | `0x76` or `0x77` | SDO | Does not conflict with the MPU-9250 address range |

The address values above are IC-level documentation. The actual breakout wiring for AD0, SDO, pull-ups, and exposed pins remains TBD.

### I2C Conditions

- SDA is one shared logical bus signal.
- SCL is one shared logical bus signal.
- Both devices must be connected to the same compatible bus-voltage domain.
- Each device must have a unique address on the bus.
- Pull-up implementation: **TBD / physical verification required.**
- Pull-up voltage and resistance must be compatible with the Pico and both breakout boards.
- A breakout-board regulator or level shifter must not be assumed from the sensor IC datasheet.
- If an address is changed, the physical AD0 or SDO connection and resulting address must be documented.

### I2C Decision

**Logical result: GREEN.** There is no address conflict between the documented IC-level addresses. The electrical result remains **YELLOW** until the breakout supply, logic levels, pull-ups, and exposed interface are verified.

## SPI Analysis

### Shared SPI Bus

The RA-02 and Micro SD reader can logically share one SPI peripheral. The shared signals are:

- SCK: shared clock
- MOSI: shared controller-to-device data
- MISO: shared device-to-controller data
- CS/NSS: separate device-selection signal for each device

The proposed logical arrangement is:

```text
SPI bus
|
+-- SX1278 RA-02
|   +-- dedicated CS/NSS
|   +-- RESET control
|   +-- DIO0 event input
|   +-- optional DIO1 event input
|
+-- Micro SD reader
    +-- dedicated CS
```

Only the selected device should actively use the bus. The non-selected device must release MISO or otherwise behave as documented for shared-bus operation. This must be checked against the actual RA-02 carrier and SD-reader circuitry.

### RA-02 Reservations

- **CS/NSS:** Required dedicated selection resource.
- **RESET:** Required control resource for initialization and recovery.
- **DIO0:** Required or strongly useful event input reservation for LoRa interrupt/event handling, such as transmit completion, receive completion, or other configured radio events.
- **DIO1:** Optional event input reservation. Keep available if the final radio mode or receive handling benefits from a second DIO event.

The exact number and header availability of DIO pins on the RA-02 carrier are **TBD / physical verification required**.

### SD Reservations

- **CS:** Required dedicated selection resource for the SD reader.
- **SCK, MOSI, MISO:** Shared SPI signals, subject to verification of the reader's host-side logic behavior.

SKU 11566 as delivered is a 2.6-3.6 V SPI module, so it shares the 3.3 V rail and the Pico's signal levels. The remaining SPI0 question is behavioural rather than electrical: a reader that keeps driving MISO after its chip select is released corrupts the RA-02's next transaction, which presents as a dead radio rather than a dead card.

### Shared Versus Separate SPI Controllers

Sharing one SPI controller is preferable as the initial logical architecture because:

- Both devices use the same fundamental synchronous serial signals.
- Separate CS/NSS resources isolate transactions.
- It preserves the other SPI controller for future expansion or a recovery/debug use.
- It avoids consuming additional bus resources without evidence that separate controllers are necessary.

A separate SPI controller should be considered only if the exact SD reader, RA-02 carrier, timing requirements, or bus electrical behavior makes shared operation unreliable. That decision is deferred until physical verification and testing.

**Logical result: GREEN.** Shared SPI has no fundamental resource conflict. Electrical shared-bus behavior remains **YELLOW** because the SD reader and RA-02 carrier are not fully verified.

## UART Analysis

One Pico UART is reserved for the NEO-6M GPS:

- GPS TX and GPS RX are reserved as one UART connection.
- The exact breakout pin labels and logic levels remain TBD.
- A second UART should remain available for debugging, a future sensor, or expansion if the Pico pin-multiplexing and final GPIO map permit.
- USB remains available for programming and development/debugging.

The GPS UART must not consume every serial/debug resource. The exact use of the second UART is intentionally not assigned.

**Logical result: GREEN.** One dedicated GPS UART plus retained debug/expansion capacity is practical at the resource level. Pin-mux conflicts remain a final-map dependency.

## GPIO Reservation

The following table reserves logical functions only. It intentionally contains no GPIO numbers.

| Function | Priority | Resource Type | Allocation | Status |
|---|---|---|---|---|
| I2C SDA | REQUIRED | I2C | I2C bus | Provisional; breakout interface and pull-ups TBD |
| I2C SCL | REQUIRED | I2C | I2C bus | Provisional; breakout interface and pull-ups TBD |
| SPI SCK | REQUIRED | SPI | SPI bus | Provisional; shared by RA-02 and SD |
| SPI MOSI | REQUIRED | SPI | SPI bus | Provisional; shared by RA-02 and SD |
| SPI MISO | REQUIRED | SPI | SPI bus | Provisional; shared by RA-02 and SD |
| RA-02 CS/NSS | REQUIRED | GPIO control | Dedicated GPIO | Carrier pin availability TBD |
| RA-02 RESET | REQUIRED | GPIO control | Dedicated GPIO | Carrier pin availability TBD |
| RA-02 DIO0 | REQUIRED | GPIO interrupt/input | Dedicated GPIO interrupt-capable resource | Carrier pin availability and event mapping TBD |
| RA-02 DIO1 | OPTIONAL | GPIO interrupt/input | Reserved optional GPIO interrupt-capable resource | Carrier pin availability TBD |
| SD CS | REQUIRED | GPIO control | Dedicated GPIO | SD board signal level and CS behavior TBD |
| GPS TX | REQUIRED | UART | GPS UART | Breakout TX label and logic level TBD |
| GPS RX | REQUIRED | UART | GPS UART | Breakout RX label and logic level TBD |
| MPU-9250 INT | USEFUL | GPIO interrupt/input | Reserved GPIO interrupt-capable resource | Breakout INT exposure TBD |
| Status LED | REQUIRED | GPIO or power-indicator control | Dedicated status/power-indicator resource | LED circuit and competition power-indicator implementation TBD |
| Battery ADC | USEFUL | ADC | One reserved ADC channel | Divider and voltage protection TBD; no circuit designed |
| Spare GPIO | SPARE | GPIO | Preserve from final allocation | Exact count and pin mux TBD |

The status labels describe resource importance, not electrical compliance. `REQUIRED` means the logical function is needed by the intended architecture; it does not mean the physical board connection is approved.

## ADC and Battery Monitoring

Reserve one ADC channel for future battery-voltage monitoring. This is a resource reservation only.

No resistor divider, resistor value, protection component, scaling, threshold, or GPIO/ADC number is selected. Before using the reservation, verify:

- Pico ADC input limits
- Battery voltage range
- Divider loading and tolerance
- Input protection
- Measurement reference
- Whether battery status must be logged or transmitted

Reserving the ADC channel is worthwhile because it preserves a path for low-battery diagnostics and brownout investigation without consuming a bus. It does not establish that monitoring hardware is present.

## Debugging and Programming

The resource map preserves:

- USB for programming and development/debugging
- SWD/debug access where the physical development setup supports it
- A second UART for debug or future expansion if final pin multiplexing permits
- Spare GPIO capacity for test points, diagnostics, or future requirements

The debug resources should not be consumed by normal mission peripherals unless a later design review demonstrates that the tradeoff is necessary.

## Pico Resource Table

| Resource | Device/Function | Required/Optional | Proposed Allocation | Notes |
|---|---|---|---|---|
| I2C | MPU-9250 and BMP280 | Required | One shared I2C bus | Addresses are logically distinct; pull-ups and bus voltage TBD |
| SPI | RA-02 and Micro SD reader | Required | One shared SPI bus | Separate CS/NSS for each device; SD electrical behavior TBD |
| UART | NEO-6M GPS | Required | One dedicated UART | TX/RX breakout levels and labels TBD |
| UART | Debug/future expansion | Optional | Preserve second UART if pin mux permits | Do not consume during preliminary allocation |
| ADC | Battery-voltage monitoring | Optional/useful | Reserve one ADC channel | Divider and protection TBD |
| GPIO | RA-02 CS/NSS | Required | Dedicated GPIO resource | Number TBD |
| GPIO | RA-02 RESET | Required | Dedicated GPIO resource | Number TBD |
| GPIO | RA-02 DIO0 | Required | Interrupt-capable GPIO resource | Useful for radio event handling; number TBD |
| GPIO | RA-02 DIO1 | Optional | Optional interrupt-capable GPIO resource | Number TBD |
| GPIO | SD CS | Required | Dedicated GPIO resource | Number TBD |
| GPIO | MPU-9250 INT | Useful | Interrupt-capable GPIO resource | Only if breakout exposes it and firmware uses it |
| GPIO | Status LED | Required | Dedicated indicator-control resource | Number and electrical connection TBD |
| GPIO | Spare expansion | Spare | Preserve unallocated GPIO resources | Exact capacity depends on final pin mux |
| USB | Programming/debugging | Required for development | USB development interface | No mission pin assigned |
| SWD/debug | Low-level development/debug | Useful | Preserve debug access | Board/header access and use procedure TBD |

## Resource Conflict Analysis

### I2C Sharing

No logical address conflict exists between the MPU-9250 and BMP280 address options. The bus remains dependent on compatible voltage domains and verified pull-ups. This does not prevent logical allocation.

### SPI Sharing

The RA-02 and SD reader can use one logical SPI bus with separate CS lines. Shared-bus electrical behavior, especially SD MISO release and level shifting, must be verified before wiring. This does not justify abandoning the shared-bus allocation at this stage.

### UART Availability

One UART is sufficient for the GPS. Retaining another serial/debug resource is preferable to assigning every serial resource to mission hardware. The exact GPIO pin multiplexing remains TBD.

### GPIO Count

The listed functions require bus signals, CS/control lines, interrupt inputs, a status LED, and one optional ADC. The Pico provides configurable GPIO and peripheral resources documented by Raspberry Pi. There is no identified logical resource conflict, but the exact usable GPIO count after power, USB, debug, and pin-mux constraints must be checked during the final map.

**Result: no fundamental logical conflict; final capacity is a pin-mux verification item.**

### ADC Availability

One ADC resource can be reserved for battery monitoring without affecting the proposed I2C, SPI, UART, or debug architecture. The voltage-divider design is deliberately deferred.

### Interrupt Requirements

Reserve RA-02 DIO0 as the primary radio event input. Keep DIO1 optional. Reserve MPU-9250 INT as useful rather than mandatory because the sensor can be polled if the verified board and later firmware design support that approach. The actual interrupt pin availability on each breakout remains TBD.

### Debug and Programming

USB remains available. SWD/debug access and a second UART are preserved as development resources rather than consumed by the initial mission allocation.

### Future Expansion

Future expansion is protected by:

- Keeping the second UART unassigned where pin mux permits
- Keeping RA-02 DIO1 optional
- Reserving one ADC channel
- Preserving spare GPIO
- Using one shared SPI bus instead of consuming both SPI controllers initially
- Preserving USB and debug access

## Provisional Power Relationship

This resource map does not design the power circuit. The following devices must ultimately be considered in the verified power architecture:

- Raspberry Pi Pico
- SX1278 RA-02
- MPU-9250
- BMP280
- NEO-6M GPS
- Micro SD reader
- Status LED and its current-limiting path

The planned 3.3 V relationship is not automatically approved:

- The Pico supply path remains TBD.
- The RA-02 carrier supply remains TBD.
- Sensor breakout supply and logic levels remain TBD.
- The GPS breakout supply and logic levels remain TBD.
- The SD reader is a 2.6-3.6 V SPI module on the 3.3 V rail; its MISO release behaviour on the shared bus remains unconfirmed.
- The 3.3 V rail current and transient budget remains TBD.

The power dependencies affect final electrical wiring, but they do not prevent this logical resource allocation.

## Dependencies Before Final GPIO Map

These unresolved items can change the final GPIO map:

- SD reader interface and logic-level implementation
- SD reader CS behavior and any additional control pins
- RA-02 carrier pin availability and actual header pinout
- RA-02 RESET and DIO0/DIO1 availability and event requirements
- MPU-9250 breakout interface selection and INT pin availability
- BMP280 breakout interface selection and address/control pin availability
- GPS breakout UART pin arrangement and any enable/reset controls
- Breakout-board level shifting and pin-mux implications
- Pico board variant, reserved pins, and final pin-multiplexing constraints
- Whether battery monitoring is implemented and which ADC resource is practical

These uncertainties do not prevent logical bus allocation:

- Regulator selection
- Battery protection design
- Exact capacitor values
- RF connector type, provided RF wiring is not finalized
- Final wire routing

They still must be resolved before electrical assembly and final verification.

## Engineering Decision

**YELLOW - Logical architecture is sound, but hardware verification remains.**

There is no fundamental Pico resource conflict in the proposed architecture:

- MPU-9250 and BMP280 can logically share I2C.
- RA-02 and SD can logically share SPI with separate chip-select lines.
- One UART can be dedicated to the GPS while another is preserved for debug or expansion.
- One ADC can be reserved for battery monitoring.
- USB/SWD and spare GPIO capacity are preserved for development and future needs.

The architecture is not an electrical wiring approval. The SD reader and carrier-board uncertainties remain the primary dependencies.

## Final Question

**Can we now proceed to the actual Pico GPIO pin map?**

**Yes, for a provisional GPIO map, provided every unresolved electrical dependency is explicitly marked.**

The final pin map must remain subject to:

- Physical verification of all breakout boards
- Confirmed voltage and logic levels
- Confirmed pinouts and exposed control/interrupt pins
- SD reader level-shifting verification
- RA-02 carrier verification
- Pico pin-multiplexing review
- Final power and wiring review

This document intentionally contains no GPIO numbers and does not create final wiring.
