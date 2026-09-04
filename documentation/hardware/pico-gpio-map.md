# Preliminary Raspberry Pi Pico GPIO Map

## Scope

This is the first preliminary GPIO-number assignment for the onboard Raspberry Pi Pico. It is a resource allocation for planning, not final wiring approval.

The map is based on the Raspberry Pi Pico/RP2040 alternate-function model and the logical bus architecture in `pico-resource-map.md`. It remains subject to:

- Physical verification of every breakout board
- Electrical compatibility verification
- Power architecture approval
- Final firmware validation
- Final PCB and wiring review

No regulator is selected and no PCB or firmware is created here.

## Proposed Logical Architecture

| Bus or function | Preliminary allocation | Rationale |
|---|---|---|
| I2C0 | GPIO4/GPIO5 | Matched I2C0 SDA/SCL alternate functions for the MPU-9250 and BMP280 |
| SPI0 | GPIO18/GPIO19/GPIO16 | Matched SPI0 SCK/TX/RX alternate functions for shared RA-02 and SD access |
| RA-02 CS/NSS | GPIO17 | Dedicated selection line; also supports SPI0 CSn alternate function |
| SD CS | GPIO6 | Separate ordinary GPIO selection line; not shared with RA-02 CS |
| UART0 | GPIO12/GPIO13 | Dedicated GPS UART TX/RX pair |
| RA-02 RESET | GPIO20 | Dedicated ordinary GPIO control line |
| RA-02 DIO0 | GPIO21 | Dedicated input/event line for radio interrupt handling |
| RA-02 DIO1 | GPIO22 | Optional dedicated radio event line; may be released if not needed |
| MPU-9250 INT | GPIO7 | Reserved interrupt-capable ordinary GPIO input |
| Status LED | GPIO14 | Dedicated ordinary GPIO; external LED polarity and resistor remain TBD |
| Battery ADC | GPIO26 / ADC0 | Reserved ADC-capable GPIO; divider is not designed |

The alternate functions listed above must still be checked against the exact Pico datasheet revision and final board configuration. A valid Pico mux assignment does not prove that the connected breakout boards are electrically safe.

## Pico Documentation Basis

Source: [Raspberry Pi Pico datasheet](datasheets/raspberry_pi_pico_datasheet.pdf) and [official online copy](https://datasheets.raspberrypi.com/pico/pico-datasheet.pdf).

The selected pins use these documented RP2040 function roles:

- GPIO4/GPIO5: I2C0 SDA/SCL pair
- GPIO16/GPIO17/GPIO18/GPIO19: SPI0 RX/CSn/SCK/TX functions
- GPIO12/GPIO13: UART0 TX/RX pair
- GPIO26: ADC0
- GPIO6, GPIO7, GPIO14, GPIO20, GPIO21, and GPIO22: ordinary GPIO resources for control or input functions

The GPIOs used for control and interrupts do not need a special alternate function for this preliminary map. GPIO interrupt capability remains a firmware and final electrical validation item.

The Pico's onboard LED is not used as the competition-visible external LED. GPIO14 is reserved for a separate visible status/power LED so that its physical visibility can be designed into the CanSat structure.

## GPIO Reservation and Preliminary Assignment

| Pico GPIO | Function | Peripheral | Direction | Connected Device | Priority | Status | Notes |
|---:|---|---|---|---|---|---|---|
| GPIO4 | I2C SDA | I2C0 | Bidirectional | MPU-9250 + BMP280 | REQUIRED | Provisional | Shared bus; breakout pull-ups and voltage TBD |
| GPIO5 | I2C SCL | I2C0 | Output/open-drain bus | MPU-9250 + BMP280 | REQUIRED | Provisional | Shared bus; breakout pull-ups and voltage TBD |
| GPIO18 | SPI SCK | SPI0 | Output | RA-02 + Micro SD reader | REQUIRED | Provisional | Shared clock; exact board interfaces TBD |
| GPIO19 | SPI MOSI | SPI0 TX | Output | RA-02 + Micro SD reader | REQUIRED | Provisional | Shared controller-to-device data |
| GPIO16 | SPI MISO | SPI0 RX | Input | RA-02 + Micro SD reader | REQUIRED | Provisional | Only selected device may drive the bus |
| GPIO17 | RA-02 CS/NSS | SPI0 CSn / GPIO | Output | SX1278 RA-02 | REQUIRED | Provisional | Dedicated radio chip select |
| GPIO6 | SD CS | GPIO | Output | Micro SD reader | REQUIRED | Provisional | Separate from RA-02 CS; SD logic behavior TBD |
| GPIO20 | RA-02 RESET | GPIO | Output | SX1278 RA-02 | REQUIRED | Provisional | Carrier reset pin availability TBD |
| GPIO21 | RA-02 DIO0 | GPIO interrupt/input | Input | SX1278 RA-02 | REQUIRED | Provisional | Reserved for radio event/interrupt handling |
| GPIO22 | RA-02 DIO1 | GPIO interrupt/input | Input | SX1278 RA-02 | OPTIONAL | Provisional | Release if the verified radio design does not need it |
| GPIO12 | GPS TX path | UART0 TX | Output | NEO-6M RX | REQUIRED | Provisional | Pico TX -> GPS RX; breakout labels and levels TBD |
| GPIO13 | GPS RX path | UART0 RX | Input | NEO-6M TX | REQUIRED | Provisional | Pico RX <- GPS TX; breakout labels and levels TBD |
| GPIO7 | MPU-9250 INT | GPIO interrupt/input | Input | MPU-9250 INT | USEFUL | Provisional | Only if the breakout exposes INT and the design uses it |
| GPIO14 | Status LED | GPIO | Output | External visible LED | REQUIRED | Provisional | Proposed GPIO -> resistor -> LED -> GND; polarity and resistor value TBD |
| GPIO26 | Battery monitoring | ADC0 | Analog input | Battery-voltage monitor reservation | USEFUL | Reserved | Divider and protection not designed; never connect LiPo directly |

## Connection Summary

```text
I2C:
GPIO4 -> SDA -> MPU-9250 + BMP280
GPIO5 -> SCL -> MPU-9250 + BMP280

SPI:
GPIO18 -> SCK  -> RA-02 + Micro SD
GPIO19 -> MOSI -> RA-02 + Micro SD
GPIO16 <- MISO <- RA-02 + Micro SD
GPIO17 -> CS/NSS -> RA-02
GPIO6  -> CS     -> Micro SD

UART:
GPIO12 -> TX -> GPS RX
GPIO13 <- RX <- GPS TX

RA-02:
GPIO20 -> RESET
GPIO21 <- DIO0
GPIO22 <- DIO1 (optional)

MPU-9250:
GPIO7 <- INT

LED:
GPIO14 -> status LED

Battery:
GPIO26 <- ADC reservation
```

### LED Connection

The preliminary logical polarity is:

```text
GPIO14 -> current-limiting resistor - TBD -> external visible LED -> GND
```

An opposite-polarity arrangement remains possible if required by the selected hardware. The LED part, resistor value, current, power source, and immediate-on behavior must be verified separately. This GPIO assignment does not by itself satisfy the competition power-indicator requirement.

## ADC and Battery Monitoring

### Battery ADC Reservation

GPIO26/ADC0 is reserved for future battery-voltage monitoring. This is only a resource reservation:

- No resistor divider is designed.
- No resistor values are selected.
- No protection circuit is selected.
- The LiPo must not be connected directly to GPIO26.
- Battery voltage range and Pico ADC limits must be checked before implementation.

## I2C Analysis and Design

The MPU-9250 and BMP280 share GPIO4/GPIO5 as one I2C0 bus.

| Device | Expected IC-level address | Address control | Map implication |
|---|---|---|---|
| MPU-9250 accelerometer + gyroscope | `0x68` or `0x69` | AD0 | No conflict with the other two |
| AK8963 magnetometer (second die in the MPU-9250) | `0x0C` | Fixed; visible only once `INT_PIN_CFG.BYPASS_EN` bridges it to the primary bus | No conflict with the other two |
| BMP280 | `0x76` or `0x77` | SDO | No conflict with the other two |

The address choices are IC-level documentation. The selected breakout's AD0/SDO wiring, pull-ups, voltage domain, and exposed interface remain unresolved.

- Both devices must operate at a compatible bus voltage.
- Pull-up implementation: **TBD / physical verification required.**
- No external pull-up values are selected.
- No hardware address modification is selected.
- If the actual boards expose conflicting or fixed behavior, the bus map must be revisited.

**I2C result:** The logical bus assignment is valid and has no documented address conflict. Electrical approval remains provisional.

## SPI Analysis and Design

The RA-02 and Micro SD reader share SPI0:

```text
GPIO18 -> SCK  -> RA-02 + Micro SD
GPIO19 -> MOSI -> RA-02 + Micro SD
GPIO16 <- MISO <- RA-02 + Micro SD
GPIO17 -> CS/NSS -> RA-02 only
GPIO6  -> CS     -> SD reader only
```

Only one CS should be asserted at a time. The non-selected device must release MISO or behave as documented for shared-bus operation.

The RA-02 additionally receives:

- GPIO20 -> RESET
- GPIO21 <- DIO0
- GPIO22 <- DIO1, optional

DIO0 is reserved because it is useful for receive-complete, transmit-complete, and other configured radio event handling. The exact event mapping depends on the final radio configuration and carrier-board availability.

The SD reader is a 2.6-3.6 V SPI module, so its signals are 3.3 V like the Pico's and its supply comes from the same rail. What remains unresolved is its MISO release behaviour when deselected on the bus it shares with the RA-02, and its write-transient current. **GPIO assignment does not imply electrical approval.**

**SPI result:** Shared SPI with separate CS lines is logically valid and preserves the second SPI controller for future use. Electrical bus operation remains provisional.

## UART Design

The GPS connection is:

```text
GPIO12 / UART0 TX -> NEO-6M RX
GPIO13 / UART0 RX <- NEO-6M TX
```

The directions are intentionally not reversed. The exact NEO-6M breakout labels and logic levels remain subject to verification.

UART1 remains unassigned for debugging, a future sensor, or expansion if the final pin multiplexing permits. USB remains available for programming and development/debugging.

## Pico Resource Table

| Resource | Device/Function | Proposed allocation | Status | Notes |
|---|---|---|---|---|
| I2C0 | MPU-9250, AK8963 and BMP280 | GPIO4 SDA / GPIO5 SCL | Provisional | Three devices on one bus; addresses are logically distinct; pull-ups and voltage TBD |
| SPI0 | RA-02 and Micro SD reader | GPIO18 SCK / GPIO19 MOSI / GPIO16 MISO | Provisional | Shared bus; separate CS lines; SD electrical behavior TBD |
| UART0 | NEO-6M GPS | GPIO12 TX / GPIO13 RX | Provisional | Pico TX -> GPS RX; Pico RX <- GPS TX |
| UART1 | Debug or future expansion | Unassigned | Reserved | Preserve if final pin multiplexing permits |
| ADC0 | Battery-voltage monitoring | GPIO26 | Reserved | Divider and protection not designed |
| GPIO | RA-02 controls, SD CS, MPU-9250 INT, status LED | GPIO17, GPIO6, GPIO20-GPIO22, GPIO7, GPIO14 | Provisional | Exact breakout pins and logic levels TBD |
| USB | Programming and development debug | USB interface | Preserved | Not consumed by mission peripherals |
| SWD/debug | Low-level development/debug | Pico debug interface | Preserved where practical | Access and header arrangement remain TBD |

## Debugging and Programming

- USB remains available for programming and development/debugging.
- UART1 is kept unassigned for a debug console, future sensor, or expansion if final pin multiplexing permits.
- SWD/debug access is preserved where practical through the Pico debug interface.
- GPIO0, GPIO1, GPIO2, GPIO3, GPIO8, GPIO9, GPIO10, GPIO11, GPIO15, GPIO27, and GPIO28 remain available for testing or expansion.
- No debug resource is considered a mission connection until the final wiring review.

## Reserved and Spare Pins

### Reserved Resources

- GPIO17: RA-02 CS/NSS
- GPIO20: RA-02 RESET
- GPIO21: RA-02 DIO0
- GPIO22: optional RA-02 DIO1
- GPIO26/ADC0: battery monitoring reservation
- GPIO14: external visible status LED
- GPIO7: MPU-9250 INT reservation
- UART1-capable GPIO resources: preserved for debug or expansion
- USB: preserved for development and programming
- SWD/debug access: preserved where practical through the Pico debug interface

### Reserved / Spare Pins

The following exposed GPIOs are intentionally not assigned in this preliminary map:

| GPIO | Reason preserved |
|---:|---|
| GPIO0 | Future UART/SPI/I2C/GPIO expansion or test access |
| GPIO1 | Future UART/SPI/I2C/GPIO expansion or test access |
| GPIO2 | Future UART/SPI/I2C/GPIO expansion or test access |
| GPIO3 | Future UART/SPI/I2C/GPIO expansion or test access |
| GPIO8 | Preserve UART1/SPI1/I2C0-capable resource for debug or expansion |
| GPIO9 | Preserve UART1/SPI1/I2C0-capable resource for debug or expansion |
| GPIO10 | Preserve UART1/SPI1/I2C1-capable resource for debug or expansion |
| GPIO11 | Preserve UART1/SPI1/I2C1-capable resource for debug or expansion |
| GPIO15 | Spare ordinary GPIO and alternate-function resource |
| GPIO27 | Spare ADC-capable GPIO and expansion resource |
| GPIO28 | Spare ADC-capable GPIO and expansion resource |

GPIO25 is left for the Pico onboard LED function and is not used as the competition-visible external LED. SWD/debug pads and non-GPIO power/control pins are not treated as spare mission GPIOs.

## Resource Conflict Analysis

| Check | Result | Notes |
|---|---|---|
| No GPIO assigned twice | PASS | Each listed GPIO has one preliminary function |
| I2C alternate functions | PASS | GPIO4/GPIO5 are an I2C0 SDA/SCL pair |
| SPI alternate functions | PASS | GPIO16/18/19 provide SPI0 RX/SCK/TX |
| Separate SPI chip selects | PASS | GPIO17 for RA-02 and GPIO6 for SD |
| I2C address sharing | PASS logically | MPU-9250 `0x68/0x69`; AK8963 `0x0C`; BMP280 `0x76/0x77` |
| GPS directions | PASS | Pico TX goes to GPS RX; Pico RX receives GPS TX |
| GPS UART allocation | PASS | UART0 reserved; UART1 remains available |
| ADC capability | PASS | GPIO26 is ADC0-capable |
| LED GPIO capability | PASS | GPIO14 is assigned as ordinary GPIO |
| Interrupt resources | PASS provisionally | RA-02 DIO0 required; DIO1 optional; MPU-9250 INT useful |
| GPIO capacity | PASS logically | Required functions fit while preserving spare exposed GPIOs |
| Debugging | PASS provisionally | USB and SWD/debug access preserved; UART1 unassigned |
| Electrical compatibility | NOT PASSED | Breakout supply, logic, pull-ups, MISO behavior, and pinouts remain unresolved |
| Power architecture | NOT PASSED | Regulator and complete load/transient budget remain unresolved |

## Provisional Power Relationship

This pin map does not design the power circuit. The eventual power architecture must consider the Pico, RA-02, MPU-9250, BMP280, NEO-6M, Micro SD reader, and status LED.

- The Pico supply input path remains subject to the electrical-compatibility review.
- The RA-02 and sensor breakout supply and logic levels remain subject to physical verification.
- The SD reader's 2.6-3.6 V range settles its supply, but not its behaviour on a shared SPI bus: confirm that it releases MISO when deselected before trusting it beside the RA-02.
- No regulator, level shifter, resistor, capacitor, or power wiring is selected here.
- Power uncertainty can invalidate a physical connection, but it does not change the logical bus reservation by itself.

## Assignments That May Change

The following GPIO assignments may change after physical verification:

- GPIO17 if the RA-02 carrier does not expose the assumed CS/NSS function or requires a different control arrangement
- GPIO20 if RA-02 RESET is not exposed or has different electrical behavior
- GPIO21 and GPIO22 if the carrier does not expose DIO0/DIO1 or the selected radio mode needs different event lines
- GPIO6 if the SD reader CS label or host interface differs from the Robu listing
- GPIO16, GPIO18, and GPIO19 if the SD reader cannot share SPI electrically
- GPIO4 and GPIO5 if either sensor breakout does not expose I2C or its bus voltage/pull-ups are incompatible
- GPIO7 if the MPU-9250 breakout does not expose INT
- GPIO12 and GPIO13 if the GPS breakout uses different exposed pins or levels
- GPIO26 if battery monitoring is removed or a different ADC arrangement is required
- GPIO14 if the physical LED circuit requires a different control arrangement

These are hardware dependencies, not evidence of a logical Pico resource conflict.

## Dependencies Before Final GPIO Map

The following unresolved hardware questions can force a pin reassignment:

- RA-02 carrier pin availability and actual header pinout
- RA-02 RESET, DIO0, and optional DIO1 exposure
- SD module pin labels, CS behavior, and host-side logic-level implementation
- Whether the SD reader can share SPI electrically with the RA-02
- MPU-9250 breakout interface selection and INT availability
- BMP280 breakout interface selection and address/control pins
- GPS breakout TX/RX arrangement and any enable/reset controls
- Breakout-board logic levels and required level shifting
- Pico board variant and final pin-multiplexing constraints
- Whether battery monitoring is implemented and which ADC resource is suitable

The following uncertainties do not prevent this logical allocation, although they remain necessary for final wiring:

- Regulator selection
- Battery protection design
- Exact resistor and capacitor values
- RF connector type

## Engineering Decision

**YELLOW - The GPIO allocation is technically valid according to the Pico alternate-function model, but it depends on physical breakout verification and electrical compatibility.**

There is no identified GPIO or peripheral conflict in this preliminary map:

- I2C0 supports the MPU-9250 and BMP280 with distinct logical addresses.
- SPI0 supports shared RA-02 and SD signals with separate CS lines.
- UART0 supports the GPS with correct TX/RX directions.
- UART1, USB, SWD/debug, ADC capacity, and multiple GPIOs remain available.
- The proposed allocation does not require a regulator, level shifter, resistor, capacitor, or PCB decision.

The map must not be treated as final wiring approval.

## Final Question

**Can we now proceed to the actual Pico GPIO pin map?**

Yes, for this preliminary GPIO-number map. The final map remains subject to the dependencies above.

## Final Answers

1. **Is every required device assigned?** Yes, every listed mission device and control function has a preliminary logical GPIO or bus assignment. Electrical approval is still pending.
2. **Are any GPIOs conflicting?** No logical GPIO or alternate-function conflict was identified.
3. **How many GPIOs remain spare?** Eleven exposed GPIOs are intentionally listed as spare: GPIO0, GPIO1, GPIO2, GPIO3, GPIO8, GPIO9, GPIO10, GPIO11, GPIO15, GPIO27, and GPIO28. GPIO25 remains reserved for the Pico onboard LED and is not used for the external competition LED.
4. **Is the SPI sharing architecture valid?** Yes logically, with shared SCK/MOSI/MISO and separate RA-02 and SD CS lines. Electrical validation remains pending.
5. **Is the I2C sharing architecture valid?** Yes logically; MPU-9250 and BMP280 address options do not conflict. Pull-ups and bus voltage remain pending.
6. **Is the GPS UART allocation valid?** Yes; Pico TX is connected logically to GPS RX and Pico RX to GPS TX using UART0. Breakout labels and levels remain pending.
7. **Is battery ADC reserved?** Yes, GPIO26/ADC0 is reserved. No divider or direct battery connection is designed.
8. **Are debugging resources preserved?** Yes; USB, SWD/debug access, UART1 capacity, spare GPIO, and spare ADC-capable GPIOs are preserved.
9. **What physical checks could force reassignment?** RA-02 carrier pin availability, SD pin labels and level shifting, MPU-9250 INT exposure, GPS TX/RX arrangement, BMP280 interface selection, breakout logic levels, and Pico board/pin-mux verification.

This is a preliminary GPIO-number map only. No final wiring, regulator, PCB, firmware, or competition compliance claim is made.
