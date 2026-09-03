# CanSat 2026

## Project Overview

This repository contains the design and development work for our CanSat 2026 competition vehicle. It covers the onboard electronics, flight-computer firmware, telemetry, ground station, mechanical design, electrical design, simulations, testing, and engineering documentation.

The hardware described here is the confirmed project bill of materials. Detailed wiring, pin assignments, software interfaces, and some aspects of the mission are still being designed and are marked `TBD` where they are not yet confirmed.

## Mission Overview

The CanSat mission objective, competition-specific requirements, flight sequence, and success criteria have not yet been documented in this repository. These items are `TBD` and must be confirmed against the current competition rules before mission software and test procedures are finalized.

## System Architecture

The confirmed system has two computing nodes:

- A Raspberry Pi Pico is intended to be the CanSat flight computer.
- A second Raspberry Pi Pico is intended to be the ground-station controller.
- One SX1278 LoRa RA-02 module and antenna assembly is intended for each node.
- The CanSat node is expected to collect sensor data, obtain GPS data, store mission data, and transmit telemetry.
- The ground-station node is expected to receive telemetry and make it available to ground-station software and the operator.

This is the intended architecture, not a completed implementation. The electrical connections, packet format, timing, fault handling, and ground-station application behavior remain `TBD`.

## Hardware Overview

The CanSat uses a Raspberry Pi Pico, an inertial measurement unit, a GPS receiver, an atmospheric pressure sensor, removable storage, and a LoRa radio. A single 1S LiPo battery supplies the system. Two prototype boards are available for the electronics build.

### Detailed Hardware Table

| Subsystem | Component | Quantity | Purpose | Interface | Status |
|---|---|---:|---|---|---|
| Flight computer | Raspberry Pi Pico | 2 | One flight computer and one ground-station controller | TBD | Confirmed hardware; roles intended as described above |
| Telemetry | SX1278 LoRa Module RA-02, 433 MHz | 2 | Wireless telemetry link, one onboard and one at the ground station | TBD | Confirmed hardware; integration TBD |
| Telemetry | 433 MHz LoRa antenna with SMA male connector | 2 | Radio antennas, one for each LoRa module | SMA connection; exact installation TBD | Confirmed hardware |
| Telemetry | 10 cm IPEX to SMA female cable, 11 mm, RG1.13 | 2 | Connects each radio module to its antenna | IPEX to SMA; exact module connector compatibility TBD | Confirmed hardware |
| Sensors | MPU-6050 3-axis accelerometer and gyro sensor | 1 | Measures acceleration and angular motion | TBD | Confirmed hardware; integration TBD |
| Sensors | NEO-6M GPS module with EEPROM | 1 | Provides position and timing data | TBD | Confirmed hardware; integration TBD |
| Sensors | GY-BMP280-3.3 precision altimeter atmospheric pressure sensor module | 1 | Measures atmospheric pressure for altitude estimation | TBD | Confirmed hardware; integration TBD |
| Data storage | Micro SD card reader module | 1 | Stores flight and sensor data | TBD | Confirmed hardware; integration and filesystem TBD |
| Power | Orange 3.7 V 1500 mAh 25C 1S LiPo battery pack | 1 | Primary electrical power source | TBD | Confirmed hardware; protection, charging, and switching TBD |
| Prototyping | 10 x 10 cm universal single-sided PCB, 2.54 mm pitch | 2 | Prototyping and mounting of electronics | 2.54 mm through-hole pitch | Confirmed hardware |
| Power | 3.3 V regulated power supply | TBD | Supplies the planned 3.3 V peripheral rail | TBD | Planned; regulator selection and circuit TBD |

## Flight Computer

The onboard Raspberry Pi Pico is intended to coordinate sensor sampling, data validation, data storage, and telemetry transmission. Startup behavior, task scheduling, watchdog use, reset recovery, calibration handling, and the precise firmware architecture are `TBD`.

The second Pico is intended for the ground station. It is not an additional flight computer; its exact role in the ground-station data path remains `TBD`.

## Sensor Subsystem

The sensor subsystem consists of the MPU-6050, NEO-6M GPS module, and GY-BMP280-3.3 module. Sensor timestamps, sampling rates, calibration procedures, validity checks, and behavior when a sensor is unavailable are `TBD`.

### GPS

The NEO-6M is intended to provide position and timing information to the flight computer. Antenna placement, startup and fix handling, update rate, parsing, coordinate representation, and loss-of-fix behavior are `TBD`.

### Altitude Sensing

The BMP280 is intended to provide atmospheric pressure measurements that can be used for altitude estimation. The reference pressure, calibration method, filtering, sampling rate, and altitude algorithm are `TBD`. No altitude accuracy or performance claim is made here.

### IMU

The MPU-6050 is intended to provide three-axis acceleration and angular-rate measurements. Axis orientation, calibration, sample rate, filtering, mounting, and handling of invalid readings are `TBD`.

## LoRa Telemetry System

The two SX1278 RA-02 modules are intended to provide the wireless link between the CanSat and the ground station. Each module has a 433 MHz antenna and an IPEX-to-SMA cable in the confirmed hardware set.

The radio configuration and telemetry protocol have not yet been defined. Frequency configuration, modulation settings, packet structure, sequence numbering, timestamps, checks, acknowledgements, retry behavior, link-loss indication, and regulatory or competition constraints are `TBD`. The ground station must distinguish missing or invalid packets from valid telemetry rather than silently accepting corrupted data.

## Ground Station

The ground station is intended to use the second Raspberry Pi Pico and second LoRa radio assembly to receive the CanSat telemetry. Ground-station software is intended to present mission-relevant values, connection status, sensor health, warnings, and recorded data, but the user interface and division of work between the Pico and the computer are `TBD`.

## Data Storage

The Micro SD card reader module is intended for onboard recording of sensor data, GPS data, system status, and telemetry-related information. The file format, logging rate, synchronization behavior, capacity assumptions, card initialization, write-failure handling, and recovery after reset are `TBD`.

## Power System

The confirmed battery is a 1S LiPo with a nominal voltage of 3.7 V, approximately 4.2 V when fully charged, and a lower voltage as it discharges. The battery must therefore not be treated as a fixed 3.7 V supply.

A dedicated regulated 3.3 V power rail is planned for the 3.3 V peripherals, particularly the SX1278, MPU-6050, and BMP280. The regulator is deliberately recorded as **3.3 V regulated power supply — TBD**. No regulator model, current rating, efficiency, protection circuit, battery life, or completed installation is confirmed.

Power distribution, charging, battery protection, voltage monitoring, power switching, grounding, decoupling, brownout behavior, and separation of noisy loads are `TBD` and require electrical design review before hardware operation.

## Mechanical System

The mechanical system will contain and protect the electronics, battery, antennas, sensors, and wiring while meeting the project and competition constraints. CAD models, dimensions, mounting details, antenna placement, sensor exposure, access for programming, and structural verification are `TBD`.

## Software and Firmware

The planned software areas are:

- Flight-computer firmware for sensor acquisition, validation, logging, state management, and telemetry.
- Ground-station Pico firmware for the radio interface and forwarding or processing received data.
- Ground-station software and UI for live monitoring, logging, and post-flight review.

The language choices, build systems, libraries, telemetry protocol, configuration management, and firmware update process are `TBD`. Hardware-facing code must account for communication failures, invalid sensor values, timing limits, power interruptions, resets, and unexpected states.

## Data Flow

The expected data flow is:

1. The flight-computer Pico reads the MPU-6050, GPS, and BMP280.
2. It validates and timestamps the measurements, then records selected data to the Micro SD card.
3. It packages selected mission data for transmission through the onboard SX1278.
4. The ground-station SX1278 receives the packets through the second antenna assembly.
5. The ground-station Pico and ground-station software process, display, and log the received data.

This flow describes the intended behavior. Exact interfaces, packet contents, rates, buffering, error handling, and acknowledgment behavior are `TBD`.

## Testing

No test results are claimed by this README. The repository is intended to separate and document unit tests, hardware interface tests, sensor tests, telemetry and communication tests, integration tests, system tests, and mission tests as implementation progresses.

Testing should include power and brownout behavior, sensor failure or invalid data, SD-card write failures, resets, missing and corrupted telemetry packets, radio range and antenna installation, and end-to-end data logging. Hardware test procedures, test fixtures, acceptance criteria, and results are `TBD`.

## Repository Structure

```text
avionics/
	sensors/             Sensor-related hardware and documentation
	telemetry/           Telemetry hardware and protocol work
	power/               Power-system design
firmware/
	flight-computer/     Onboard flight-computer firmware
	ground-station/      Ground-station Pico firmware
ground-station/
	software/            Ground-station application software
	UI/                  Ground-station user interface
mechanical/
	CAD/                 Mechanical CAD files
	drawings/            Mechanical drawings
electrical/
	schematics/          Electrical schematics
	PCB/                 PCB and board-design files
simulations/           Simulations and analysis
documentation/
	requirements/        Requirements and constraints
	design/              System and subsystem design
	testing/             Test procedures and results
	mission/             Mission planning and operations
test-data/             Test data and datasets
README.md              Project overview and current context
```

## Current Project Status

The hardware BOM listed in this README is confirmed. The two-Pico flight-computer and ground-station arrangement, paired SX1278 radio link, planned 3.3 V peripheral rail, and general sensor and storage roles are the current intended architecture.

Firmware, ground-station software, electrical implementation, mechanical implementation, telemetry protocol, mission definition, and test evidence are not yet documented as complete. This README does not claim that the system has been assembled, compiled, simulated, or tested on hardware.

## Future Development / TBD Items

- Confirm the mission objectives, requirements, and competition constraints.
- Define the electrical schematic, pin assignments, grounding, protection, and power distribution.
- Select and document the 3.3 V regulated power supply.
- Confirm every module voltage requirement and interface before connecting hardware.
- Define sensor orientation, calibration, sampling, filtering, and fault handling.
- Define and implement a validated telemetry packet format and link-loss behavior.
- Define onboard data logging format and SD-card recovery behavior.
- Implement flight-computer and ground-station firmware.
- Develop the ground-station software and mission-oriented UI.
- Complete mechanical packaging, antenna placement, and access provisions.
- Create test procedures and record results at unit, hardware, integration, system, and mission levels.