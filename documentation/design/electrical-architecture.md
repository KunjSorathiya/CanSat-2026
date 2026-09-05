# Electrical Architecture

## Purpose and Scope

This document defines the preliminary electrical architecture for the CanSat before final Pico GPIO assignment, firmware, or peripheral-driver implementation. It covers the onboard CanSat electronics only unless a section explicitly identifies the separate ground station.

The architecture is based on the confirmed hardware list and the current requirements baseline in `documentation/requirements/requirements.md`. It does not select a regulator, assign final pins, or assume that similarly named breakout boards have identical electrical characteristics.

## Confirmed Onboard Hardware

| Item | Quantity | Intended role | Confirmation status |
|---|---:|---|---|
| Raspberry Pi Pico | 1 | CanSat flight computer | Confirmed hardware; electrical integration TBD |
| SX1278 RA-02 433 MHz LoRa module | 1 | Onboard telemetry radio | Confirmed hardware; electrical integration TBD |
| MPU-9250 (delivered: **MPU-6500**) | 1 | Gyroscope and accelerometer | Sold as a nine-axis MPU-9250; `WHO_AM_I` read `0x70` on 2026-09-05, an MPU-6500 with six axes and no magnetometer ([F-1](../hardware/receiving-inspection.md#findings)). Answers at `0x68`; `0x0C` never does |
| NEO-6M GPS with EEPROM | 1 | GPS sensor and possible additional telemetry source | Confirmed hardware; exact board documentation TBD |
| GY-BMP280-3.3 | 1 | Pressure, altitude-related, and temperature measurement | Confirmed hardware. Chip ID `0x58` on 2026-09-05 — a BMP280, not a BME280 ([F-4](../hardware/receiving-inspection.md#findings)); answers at `0x76`, so SDO is strapped low |
| Micro SD card reader | 1 | Onboard data storage | Confirmed hardware; breakout variant and documentation TBD |
| Orange 3.7 V 1500 mAh 25C 1S LiPo | 1 | Primary power source | Confirmed hardware |
| AMS1117-3.3 regulator module | TBD | Previously planned 3.3 V peripheral rail | Not recommended for direct 1S-to-3.3 V regulation |
| Manual power switch | 1 required | Main power control | Hardware not selected |
| Power LED | 1 required | Visible power indication | Hardware not selected |
| Universal single-sided prototype PCB | 1 | Onboard prototype assembly | Confirmed hardware |

## Separate Ground Station Hardware

The ground station is electrically separate from the CanSat:

- Raspberry Pi Pico x1
- SX1278 RA-02 x1
- 433 MHz antenna x1
- IPEX-to-SMA cable x1

The ground-station power supply, switch, LED, wiring, and computer connection are outside this onboard architecture and remain TBD. Its RA-02 interface and configuration must remain compatible with the onboard telemetry design.

## Power Source

The onboard source is one 1S LiPo battery. The BOM ordered an Orange pack; **the pack that arrived is Pro-Range**, with the same capacity, cell count, nominal voltage and C-rating.

From the delivered pack's label:

- Nominal voltage: 3.7 V
- Discharge voltage: lower than nominal as the battery discharges; the usable lower limit is TBD
- Capacity marking: 1500 mAh
- C-rating marking: 25C
- Main discharge lead: red 2-pin JST-RCY (BEC) style
- Balance lead: white 2-pin JST-XH style

From the BOM rather than the pack:

- Approximate full-charge voltage: 4.2 V

The electrical design must treat the battery as a variable-voltage source. No battery life, safe cutoff voltage, charging current, protection method, or allowable load is assumed from the battery label alone - and in this case the label states none of them. **Neither lead mates with anything in this project, and no 1S charger was supplied or is on the BOM.**

## Recommended Preliminary Power Topology

The recommended architecture for evaluation is:

```text
1S LiPo battery
    -> manual ON/OFF switch
    -> switched battery distribution node
         -> Raspberry Pi Pico VSYS (Pico onboard 3.3 V regulator)
         -> peripheral power conversion - TBD
              -> verified 3.3 V peripheral rail - TBD
                   -> MPU-6500 (sold as MPU-9250), BMP280, NEO-6M, RA-02, microSD reader
                      (the microSD reader is a 3.3 V board: same rail, no second stage)
         -> power LED branch - TBD
```

This is a topology recommendation, not an approved schematic. The Pico VSYS path is based on the official Raspberry Pi Pico documentation. The switch position, LED connection, protection elements, peripheral conversion, and rail implementation remain subject to review.

The Pico's onboard regulator generates the 3.3 V rail for the RP2040 and GPIO. The Pico 3.3 V output must not be assumed capable of powering all external peripherals; that is a current question, and it is still open. It is no longer a voltage question for any load on this vehicle: the microSD reader received has no regulator and a supply pin printed `3V3`, so every peripheral runs from one 3.3 V rail and the separate reader rail that earlier revisions carried is removed from the design. With no level shifter anywhere in the vehicle, that single rail being 3.3 V is a requirement rather than a convenience. See [sd-module-analysis.md](../hardware/sd-module-analysis.md).

## Battery Voltage Range

Only the following voltage points are currently available from the confirmed project information:

| Condition | Voltage | Status |
|---|---:|---|
| Nominal battery label | 3.7 V | Confirmed label information |
| Fully charged approximation | Approximately 4.2 V | Confirmed project assumption |
| During discharge | Lower than 3.7 V | Exact range and cutoff TBD |
| Switched battery node under load | TBD | Requires measurement and load analysis |
| Pico VSYS input | Approximately 1.8-5.5 V supported by the Pico documentation | Confirmed Pico documentation; final battery protection remains TBD |
| 3.3 V regulated rail | 3.3 V target | Regulator and tolerance TBD |

The Pico is intended to be powered from the switched battery through VSYS. The design must not assume that the battery can directly power every external module. The permitted supply path for each peripheral requires exact board documentation.

## Power Switching

A manual ON/OFF switch is a mandatory competition function but has not been selected. The preliminary recommendation is to place it in the main battery feed so that the CanSat has one deliberate power-off state. The switch rating, contact behavior, mounting, location, and wiring are TBD.

The switch design must support:

- A clearly defined OFF state with no unintended energized loads
- A repeatable ON state
- Immediate power indication through the required visible LED
- No accidental interruption from vibration or impact
- Safe access before launch

The switch is not yet present in the confirmed hardware BOM.

## Voltage Regulation

A dedicated peripheral power-conversion solution is still required, but no replacement regulator has been selected. The following cannot be determined safely until exact module documentation and a load estimate are available:

- Required input-voltage range
- Required output-voltage tolerance
- Continuous output current
- Transient or peak output current
- Thermal dissipation
- Dropout or minimum headroom
- Efficiency
- Short-circuit and over-temperature behavior
- Reverse-current behavior
- Required input and output capacitors
- Battery cutoff behavior

The conversion solution must be selected only after the total measured or documented load, LoRa transmission peaks, SD-card transients, Pico VSYS behavior, and safety margins are known.

### AMS1117-3.3 Direct-Regulation Assessment

The planned AMS1117-3.3 module is rejected as the direct regulator from the 1S LiPo to a 3.3 V peripheral rail.

- The AMS1117 is a linear regulator. It needs input headroom above 3.3 V equal to its load-dependent dropout voltage.
- The AMS1117 manufacturer documentation specifies dropout on the order of 1.1 V at high load; the exact purchased module's dropout curve and current rating are not confirmed.
- Using that documented high-load order of magnitude, the input required to maintain 3.3 V is approximately 4.4 V or higher. A fully charged 1S LiPo is only approximately 4.2 V.
- At lower loads the dropout may be lower, but the project cannot guarantee 3.3 V throughout discharge without the exact module curve, load profile, and battery discharge limits.
- As the battery voltage falls, the regulator will lose regulation when the battery approaches 3.3 V plus the actual dropout. The usable battery capacity before the 3.3 V rail falls out of regulation cannot be calculated from the 1500 mAh label alone.
- Linear-regulator loss is approximately `(Vin - 3.3 V) x load current`; heat therefore depends on the actual load and voltage difference. No thermal result is claimed.
- Idealized conversion efficiency is approximately `3.3 V / Vin`, before regulator ground current and other losses. This does not establish system efficiency or battery life.
- A 3.3 V output now suits every peripheral including the microSD reader, which is a 3.3 V board. That removes a constraint from the regulator choice; it does not rescue this regulator, whose dropout is the problem.

**Decision:** Do not use the AMS1117-3.3 for direct 1S LiPo to 3.3 V regulation. A buck-boost or other suitable conversion architecture may be more appropriate for maintaining a regulated rail across the battery range, but no replacement regulator is selected here.

**Source:** [AMS1117 manufacturer documentation](https://www.advanced-monolithic.com/pdf/ds1117.pdf). This assessment does not establish the specifications of the exact module until its documentation is identified.

## 3.3 V Rail

A 3.3 V peripheral rail remains a possible peripheral rail, but its final scope is not approved. Before connecting it, verify for every module:

- Whether the module accepts 3.3 V supply
- Whether the module's signal pins are 3.3 V logic
- Whether it contains an onboard regulator or level shifter
- Its allowed voltage tolerance
- Its startup and transient current behavior
- Its required local bypass capacitors

The rail must have a defined distribution point, return path, decoupling plan, measurement point, and load test. The regulator output tolerance, current rating, and protection remain TBD.

## Grounding

The preliminary recommendation is a common electrical ground between the Pico and all onboard peripherals, with short and clearly documented return paths. The actual topology is TBD until the schematic and current paths are reviewed.

The design review must address:

- Battery return and regulator return
- Pico ground connection
- Sensor grounds
- RA-02 high-current return path
- SD-reader return path
- GPS return path
- Ground continuity across the prototype PCB
- Connector and cable return conductors
- Separation of sensitive sensor wiring from noisy or pulsed loads

No isolated ground, star ground, plane, or other specific layout is claimed at this stage.

## Decoupling

Each module's exact board documentation must be checked for required input, output, and local bypass capacitors. No capacitor values are selected in this document.

The prototype review must identify:

- Regulator input and output capacitor requirements
- Local bypass requirements for the Pico
- Local bypass requirements for the RA-02
- Local bypass requirements for MPU-9250, BMP280, GPS, and SD reader
- Placement relative to module supply pins
- Capacitor voltage ratings
- Effects of wiring length and connector resistance

Decoupling is a design and verification item, not evidence that a stable rail already exists.

## Power Distribution

The preliminary distribution order is:

1. Battery connector and battery protection/charging interface, all TBD.
2. Manual ON/OFF switch.
3. Switched battery distribution node.
4. Peripheral power-conversion solution, model and circuit TBD.
5. Verified 3.3 V peripheral distribution to only compatible loads.
6. Power LED branch with any required current-limiting element, value TBD.

The final schematic must show fuse or protection decisions, connectors, polarity, test points, return paths, and all loads. No fuse, resistor, capacitor, connector type, or wire gauge is selected here.

## Power Monitoring

No power-monitoring hardware is confirmed. The following are therefore TBD:

- Battery-voltage measurement
- 3.3 V rail measurement
- Current measurement
- ADC connection and scaling
- Thresholds and warnings
- Logging and telemetry of power status

Power monitoring is recommended for mission diagnostics, but it must not be implemented with guessed divider values or unverified input limits.

## Brownout Considerations

Potential brownouts must be treated as a mission risk. Likely stress cases include LoRa transmission, SD-card writes, startup, battery voltage sag, connector resistance, and impact-related wiring disturbance. No current or voltage margin is claimed.

The architecture must define and test:

- Minimum acceptable battery voltage
- Regulator dropout behavior
- 3.3 V rail behavior during radio transmission
- SD-card write transients
- Pico reset behavior
- Recovery after a brownout
- Whether data is corrupted by interrupted writes
- Whether telemetry restarts automatically after reset
- Whether the power LED reflects the actual switched state

## Module Power and Interface Verification

The exact board or breakout documentation must be obtained before schematic approval. The team must record the document revision, board marking, or other identifier used for each verification.

| Module | Supply voltage to verify | Logic voltage to verify | Maximum current to verify | Interface to verify | Pull-ups to verify | Capacitors to verify | Pin functions to verify |
|---|---|---|---|---|---|---|---|
| Raspberry Pi Pico | Board input options and limits - TBD | GPIO levels and limits - TBD | Board and USB/regulator current - TBD | Programming and peripheral connections - TBD | GPIO-specific requirements - TBD | Board requirements - TBD | Power, ground, GPIO, and reset functions - TBD |
| SX1278 RA-02 | Module supply range - TBD | Signal levels - TBD | Transmit peak and idle current - TBD | SPI or other supported control interface - TBD | Required control-line pull-ups - TBD | Local bypass requirements - TBD | Power, ground, antenna, control, and interrupt pins - TBD |
| MPU-9250 board | Board supply range - TBD | Signal levels - TBD | Operating and peak current - TBD | I2C, SPI, or supported alternatives - TBD | Bus pull-ups and values - TBD | Local bypass requirements - TBD | Power, ground, bus, interrupt, and configuration pins - TBD |
| NEO-6M board | Board supply range - TBD | UART signal levels - TBD | Acquisition and tracking current - TBD | UART or other available interface - TBD | Required pull-ups - TBD | Local bypass requirements - TBD | Power, ground, TX, RX, and control pins - TBD |
| GY-BMP280-3.3 board | Board supply range - TBD | Signal levels - TBD | Operating and peak current - TBD | I2C, SPI, or supported alternatives - TBD | Bus pull-ups and values - TBD | Local bypass requirements - TBD | Power, ground, bus, address, and control pins - TBD |
| Micro SD card reader | Reader-board input range - TBD | Card and host signal levels - TBD | Initialization, read/write, and peak current - TBD | SPI, SDIO, or other interface - TBD | Required bus pull-ups - TBD | Reader and card bypass requirements - TBD | Power, ground, chip select, clock, data, and control pins - TBD |
| Manual power switch | Rated voltage and current - TBD | Not applicable | Contact and switching current - TBD | Series power connection - TBD | Not applicable | Contact suppression requirements - TBD | Pole, throw, and terminal functions - TBD |
| Power LED | LED and branch voltage - TBD | Not applicable | LED current - TBD | Switched power indicator branch - TBD | Current-limiting requirement - TBD | Not applicable | Anode, cathode, and indicator wiring - TBD |

## Preliminary Interface Architecture

The project expects to use I2C, SPI, and UART somewhere in the system, but the exact interfaces must be confirmed from the exact module documentation. The table below therefore records candidate roles without assigning a bus or GPIO.

| Device | Interface | Pico Pins | Supply | Logic Level | Interrupt/Control | Status |
|---|---|---|---|---|---|---|
| SX1278 RA-02 | TBD; candidate control interface requires module verification | TBD | TBD; planned 3.3 V rail only after verification | TBD | Interrupt/control pins TBD | Requires datasheet and exact board verification |
| MPU-9250 | TBD; I2C/SPI availability and board wiring require verification | TBD | TBD | TBD | Interrupt and configuration pins TBD | Requires datasheet and exact board verification |
| NEO-6M GPS | TBD; UART availability and board levels require verification | TBD | TBD | TBD | Enable/reset/control pins TBD | Requires datasheet and exact board verification |
| GY-BMP280-3.3 | TBD; I2C/SPI availability and board wiring require verification | TBD | TBD | TBD | Address/control pins TBD | Requires datasheet and exact board verification |
| Micro SD card reader | TBD; SPI/SDIO/other interface must be identified from the breakout documentation | TBD | TBD | TBD | Chip-select and other control pins TBD | High risk; breakout variation must be resolved first |
| Power LED | Switched power indicator connection TBD | TBD | TBD | Not applicable | Manual switch relationship TBD | Required hardware not selected |
| Manual power switch | Series battery connection TBD | TBD | Not applicable | Not applicable | Main power control | Required hardware not selected |

No final GPIO assignments are made in this document. Bus sharing, chip-select allocation, interrupt routing, pull-up ownership, and cable lengths are all deferred until the exact interfaces are documented.

## Power Budget

Every load in this vehicle runs from the Pico's `3V3(OUT)` pin. The binding constraint is
therefore not the battery and not a regulator selection - it is what that pin is allowed to
supply, and the design sits close enough to that limit that the number matters.

**Raspberry Pi's guidance for `3V3(OUT)` is 300 mA**, and the RP2040 and the rest of the
Pico board draw from the same RT6150 buck-boost. The RP2040 at 125 MHz with peripherals
running is roughly 35 mA, so **about 250 mA is available to everything else.**

### Per-device worst case

Datasheet figures are the chip manufacturer's; the carrier boards add pull-ups, an LED or a
regulator that these do not include, so they are floors rather than ceilings. Where this
project has measured something, the measurement is named.

| Device | Worst-case draw | Condition | Duty in flight | Source |
|---|---:|---|---|---|
| **SX1278 / RA-02** | **87 mA** | TX, +17 dBm on PA_BOOST | **33 %** — 333.7 ms of every 1000 ms (row 5.2) | SX1276/78 datasheet, Table 10. +20 dBm would be 120 mA; this vehicle transmits at +17 |
| | 12 mA | RX continuous | the other 67 % | Same |
| | 1.5 mA | standby | — | Same |
| **microSD card + reader** | **~100 mA** | block write | **~1 %** — 2 writes ≈ 10 ms per telemetry second | SD Physical Layer spec permits 100 mA in default speed. The HP mx310 has no published figure. **Sub-millisecond spikes run higher** |
| | ~1.3 mA | the reader's four 10 kΩ pull-ups | continuous | [F-3](../hardware/receiving-inspection.md#findings) — 3.3 V board, no regulator, two capacitors |
| **NEO-6M module** | **~70 mA** | acquisition, cold start | **startup only** | u-blox NEO-6 datasheet, ~67 mA peak at 3.0 V; the carrier adds an LED |
| | ~45 mA | tracking | continuous once fixed | Same |
| **MPU-6500** | **~4 mA** | gyro + accel active | continuous | MPU-6500 datasheet: 3.2 mA gyro, 450 µA accel |
| **BMP280** | **~1 mA** | 83 Hz, high oversampling (row 3.5) | continuous | BMP280 datasheet, 720 µA at maximum rate |
| **Status LED, 330 Ω** | **~4 mA** | lit | ≤ 50 % duty, it blinks | (3.3 − 2.0) / 330 |
| **Battery divider, GP26** | ~21 µA | continuous | continuous | 100 kΩ / 100 kΩ from 4.2 V. Draws from the **battery**, not this rail |

### What that totals

| Case | Peripherals | + RP2040 | Against the 300 mA pin guidance |
|---|---:|---:|---|
| **Everything at once** — TX, SD write, GPS acquiring, all sensors, LED | **267 mA** | **302 mA** | **over** |
| **GPS tracking rather than acquiring** — otherwise the same | **242 mA** | **277 mA** | under, with 23 mA to spare |
| **Steady state** — TX at 33 % duty, RX otherwise, GPS tracking | **92 mA** | **127 mA** | comfortable |

Both columns are given because both get quoted. The peripheral column is what leaves the
`3V3(OUT)` pin; the second adds the RP2040's own draw, which shares the regulator, and is the
one to compare against Raspberry Pi's 300 mA figure.

> **An earlier revision of this tally read 316 mA.** The difference is almost entirely the
> radio: **120 mA is the SX1278's +20 dBm figure and this vehicle transmits at +17 dBm, which
> is 87 mA** — 33 mA less. Against that, the RP2040 estimate rose from 25 to 35 mA, and the
> status LED and the reader's pull-ups were missing from the old tally altogether. Net −14 mA,
> and the conclusion did not change: the all-at-once case exceeds what the pin is rated to
> supply, either way.

The first row is not hypothetical: a telemetry append writes two blocks immediately around a
transmit, so radio TX and an SD write genuinely coincide once a second. What makes it
survivable is that GPS acquisition is a startup condition and the SD spike lasts
milliseconds.

### What this requires of the board

1. **The 470 µF bulk capacitor across the microSD module's own 3V3 and GND.** Its job is to
   supply the write spike locally so it never reaches the Pico's regulator and never adds to
   the radio's peak. [F-10](../testing/bring-up-record.md#findings) is the evidence that this
   module's supply path is the weak point: a long jumper was enough to make every write fail
   while every read passed.
2. **A local capacitor at the RA-02's supply pins too** - 100 nF plus 10 µF. The same fault
   killed the radio, on the same kind of wire.
3. **Short, direct supply tracks to both.** Not a design nicety: it cost five bench runs.
4. **Do not add a load to this rail without re-doing this table.** There is no headroom left
   for one.

### What has been measured

| Measurement | Result | Row |
|---|---|---|
| Rail under 100 % radio TX duty | **3.26–3.27 V**, from 3.30 V idle | 5.4a |
| Rail under 100 % microSD write duty | **3.28–3.30 V** | 6.3b |
| Both together | **not taken** | Gate 2 |

Each load has been shown to be carried alone, at a duty far harsher than the mission's. The
combined case is the one still open, and it is the one the table says is tight. A current
figure has never been taken for any device - only rail voltage - because the series
connection would not hold on a breadboard. **Take it on the soldered board**, where a meter
can sit in the supply track.

## Electrical Risks

| Risk | Concern | Required control |
|---|---|---|
| LiPo voltage variation | The battery ranges from approximately 4.2 V when full to a lower discharge voltage. | Verify every load's allowable supply and define cutoff behavior. |
| 3.3 V rail stability | An unsuitable or undersized regulator could reset sensors, the Pico, or the radio. | Determine documented and measured continuous/peak load before selection and test the rail under load. |
| LoRa transmission peaks | Radio transmission may create a transient load. | Obtain exact peak current and measure rail behavior during transmission. |
| SD-card current spikes | Reader boards and cards vary, especially during writes. | Identify the breakout, verify voltage/logic, measure write transients, and provide documented decoupling. |
| Brownouts | Battery sag or load transients may reset the flight computer or corrupt logging. | Test startup, transmission, SD writes, low-voltage conditions, and reset recovery. |
| Logic-level incompatibility | Module boards may expose different signal levels or include different level shifting. | Verify exact boards before connecting signals. |
| Grounding problems | Shared returns and pulsed loads can disturb sensor readings or radio operation. | Review return paths and test the integrated system. |
| Connector and wiring problems | Incorrect IPEX/SMA, loose connections, polarity errors, or vibration can interrupt power or RF. | Verify connector compatibility, polarity, strain relief, and retention. |
| Power loss during impact | Impact can open a switch, connector, solder joint, or battery connection. | Secure the power path and perform impact and post-impact telemetry tests. |
| Unverified SD breakout | A module label does not establish its input voltage, signal levels, or interface. | Obtain exact documentation and test the reader independently. |
| Inadequate Pico supply path | The Pico is intended to use VSYS, but battery protection and the switched path remain unresolved. | Verify VSYS implementation, protection, startup, and brownout behavior. |

## Required Hardware Before Prototype

The following items or decisions are required before assembling the onboard electrical prototype:

- 3.3 V voltage regulator or regulated power supply, selected only after the power budget and module requirements are verified.
- Manual power switch with suitable voltage/current and mechanical ratings; exact part TBD.
- Visible power LED; current-limiting component and branch design TBD.
- Required resistors and capacitors identified from each exact module's documentation; values TBD until verified.
- Connectors compatible with the selected modules, prototype board, battery, and antenna cable; exact parts TBD.
- Wiring and suitable strain relief for power, signals, and antenna connections; sizes and types TBD.
- LiPo charging and protection solution appropriate for the confirmed battery; exact solution TBD.
- Any required level-shifting or signal-protection components after logic-level review; hardware TBD.
- Test points or measurement access for battery and 3.3 V rail; implementation TBD.

The egg, parachute, and mechanical hardware are required by the competition but are outside this electrical prototype list. They still affect packaging, wiring, antenna placement, and impact survivability.

## Electrical Architecture Decision

### Confirmed

- One Raspberry Pi Pico is intended as the onboard flight computer.
- One SX1278 RA-02, one 433 MHz antenna, and one IPEX-to-SMA cable are intended for the CanSat.
- The MPU-9250, NEO-6M, GY-BMP280-3.3, Micro SD reader, 1S LiPo, and one prototype PCB are onboard hardware.
- The battery is a 3.7 V nominal 1S LiPo and is approximately 4.2 V when fully charged.
- A dedicated regulated 3.3 V peripheral rail is planned.
- Manual power switching and a visible power LED are required by the competition, but the hardware is not selected.
- Final Pico GPIO pins are not assigned.

### Recommended

- Evaluate a switched-battery distribution node feeding a separately regulated 3.3 V peripheral rail.
- Power the Pico from the switched LiPo through VSYS, subject to battery protection and brownout review.
- Use one documented common ground with deliberate return paths and local module decoupling.
- Measure startup and transient loads before selecting the regulator.
- Treat the Micro SD breakout as an unknown electrical device until its exact documentation and behavior are verified.
- Add battery and rail measurement access before prototype testing.

### TBD

- Regulator model, current rating, efficiency, protection, capacitor requirements, and circuit.
- Battery protection, VSYS implementation, and allowable input behavior under load.
- Battery charging, protection, cutoff, and switching details.
- Manual switch and power LED part selection and wiring.
- Grounding, decoupling, connectors, wiring, test points, and mechanical retention.
- Exact interface, logic level, pull-ups, capacitors, pin functions, and power requirements for every module.
- All GPIO assignments, bus sharing, chip selects, and interrupt/control lines.
- Complete power budget and brownout margins.

### Requires Datasheet Verification

Before Gate 2 can be considered complete, the team must identify the exact board/module variants and obtain documentation for the Pico, RA-02, MPU-9250 board, NEO-6M board, BMP280 board, and Micro SD reader. Two of those are now settled by measurement rather than by the label on the bag: the IMU is an **MPU-6500** (`WHO_AM_I` `0x70`, no magnetometer), and the microSD reader is a 3.3 V board with no level shifter. Both are recorded in [receiving-inspection.md](../hardware/receiving-inspection.md#findings). The documentation review must record supply voltage, logic levels, maximum current, interface, pull-ups, capacitor requirements, and pin functions for each device. Until then, this document is a preliminary architecture and not an approved build schematic.
