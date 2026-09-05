# CanSat 2026 Engineering Requirements Checklist

This document is the controlled requirements baseline for the CanSat 2026 project. Competition requirements are based on the supplied extract from the official CanSat 2026 rulebook. The rulebook is authoritative; this checklist does not resolve contradictions or infer unstated specifications.

Confirmed hardware is recorded in the implementation column, but possession does not establish compliance. A requirement may be marked `Complete` or `Verified` only after objective evidence is added to the Evidence column.

## Status Definitions

- `Not Started` — The requirement is understood, but work has not begun or no implementation evidence exists.
- `In Progress` — Work or testing has begun, but acceptance evidence is incomplete.
- `Complete` — Implementation is complete, but formal verification evidence may still be pending.
- `Verified` — Acceptance criteria have been met and evidence is recorded.
- `Blocked` — Progress requires missing hardware, an external decision, or another prerequisite.
- `TBD` — The requirement or its acceptance condition is ambiguous and requires clarification.

## Requirements Checklist

**Status as of 2026-09-05:** 35 of the 116 requirement rows are marked
`Complete`, each with a named test in the Evidence column. None is
marked `Verified`: that word is reserved for evidence from hardware, and while the IMU, the
barometer, the GPS and the radio have now each read on the bench, no requirement has been
demonstrated end to end on a powered vehicle. Every remaining row depends on procurement,
the power design, the mechanical build or a launch.

The sensor rows were the last block still describing the pre-implementation project —
`SEN-001` to `SEN-009` said "integration - TBD" while the telemetry rows carrying the same
quantities were already `Complete`, so the same fact held two statuses on one page. They now
carry the implementation that exists and the tests that cover it.

Four rows outside that block were stale for the same reason. `PWR-004` (telemetry begins
automatically at power-on) and `GEN-008` (the vehicle includes a communication system) were
`Not Started` for behaviour that is implemented, tested, and in the radio's case already
transmitting on a bench. `MIS-003` and `MIS-004` are about what telemetry *shows during a
lift*, so they are marked implemented and awaiting that lift rather than complete: the
software produces the behaviour and no one has yet watched it happen.


| ID | Requirement | Source | Priority | Implementation | Verification Method | Status | Evidence |
|---|---|---|---|---|---|---|---|
| GEN-001 | Team must consist of 3 to 5 students. | Rulebook - General Rules | Mandatory | Team membership - TBD | Confirm team roster against registration records. | TBD | |
| GEN-002 | The CanSat must be self-built during the build phase. | Rulebook - General Rules | Mandatory | Team fabrication and assembly - TBD | Review build records, photographs, and assembled hardware. | Not Started | |
| GEN-003 | Prefabricated kits are not permitted. | Rulebook - General Rules | Mandatory | Component-level BOM is confirmed; kit status - TBD | Review procurement records and fabrication evidence. | Not Started | |
| GEN-004 | The CanSat must remain within the applicable physical size limit. | Rulebook 2026 - General Rules and section 8 | Mandatory | **21 cm (+7 cm maximum, egg chamber) x 12 cm.** Both pages of the 2026 revision agree; the earlier three-way contradiction is gone. Exceeding by >10% is a disqualification | Measure the completed CanSat against 21/28 cm x 12 cm. | **Requirement locked; hardware not built** | |
| GEN-005 | The CanSat must remain within the applicable mass limit. | Rulebook 2026 - General Rules | Mandatory | **500 g (+/-10%).** Exceeding by >10% is a disqualification | Weigh the completed CanSat on a documented scale. | **Requirement locked; hardware not built** | |
| MIS-001 | The CanSat must be lifted to the specified launch altitude. | Rulebook 2026 - Mission and section 2A | Mandatory | **100 ft (~30.5 m), released from a drone.** Both statements in the 2026 revision agree; the 150 ft figure and the 8-story rooftop are gone | Document the lift profile against a 100 ft release. | **Requirement locked** | |
| MIS-002 | The CanSat must be powered on before launch. | Rulebook - Mission | Mandatory | Manual power system - TBD | Observe and record power-on before launch. | Not Started | |
| MIS-003 | Initial ground-floor telemetry should report approximately zero altitude. | Rulebook - Mission | Mandatory | Altitude is relative to a pad reference: the startup calibrator averages barometer samples on the ground and every later altitude is computed against that pressure | Compare startup telemetry with the ground-floor baseline. | Implemented; **awaiting a lift test** | `test_startup_calibrator_stationary_and_moving`, `test_pressure_altitude` |
| MIS-004 | Telemetry must reflect the altitude change during lifting. | Rulebook - Mission | Mandatory | Continuous altitude in every packet, from the same compensated barometer reading the flight core uses for its state machine | Review logged packets during a lift test. | Implemented; **awaiting a lift test** | `test_altitude_varies_over_the_mission` runs the real controller through a scripted ascent and descent and checks the altitude the ground station reads back |
| MIS-005 | Parachute deployment must be demonstrated after release. | Rulebook - Mission and Descent | Mandatory | Parachute and deployment hardware - TBD | Demonstrate release and deployment in a controlled test. | Blocked | |
| PAY-001 | An egg payload must be carried and recovered intact. | Rulebook 2026 - General Rules; Evaluation A | Mandatory system; 20 points | **Not carried. Team decision, 2026-09-05, on personal grounds.** The 20 points for egg integrity are forgone deliberately. **This is not a disqualification condition** - the rulebook's list is closed and an absent egg is not on it | N/A - not attempted | **Declined** | [scoring-assessment.md](../project/scoring-assessment.md) |
| PAY-002 | A cushioned, secure egg chamber must be included. | Rulebook 2026 - Section 8 | Mandatory | **Still to be built despite PAY-001.** The chamber is a separately stated requirement, carries its own +7 cm dimensional allowance, and section D scores effective use of the volume the rules permit | Inspect the chamber and demonstrate cushioning. | Not Started | |
| MIS-006 | The CanSat must descend safely after release. | Rulebook - Mission | Mandatory | Descent system - TBD | Conduct a controlled descent test and inspect results. | Blocked | |
| MIS-007 | The egg must survive descent and landing. | Rulebook - Mission and Egg Payload | Mandatory | Egg chamber and cushioning - TBD | Perform documented impact and recovery tests. | Blocked | |
| MIS-008 | Telemetry must remain stable after release during flight. | Rulebook - Mission | Mandatory | Flight telemetry link - TBD | Analyze flight packets for continuity and validity. | Not Started | |
| MIS-009 | Payload recovery and final data logging must be checked after landing. | Rulebook - Mission | Mandatory | Recovery procedure and SD logging - TBD | Recover the CanSat and verify final records. | Not Started | |
| TEL-001 | Telemetry must transmit continuously from power-on at the ground floor through recovery. | Rulebook - Telemetry | Mandatory | Controller emits telemetry in every mission state, including FAULT; no manual trigger exists | End-to-end test from power-on through recovery. | Complete | `test_controller_sequence_and_degradation`, `test_state_machine_full_mission` |
| TEL-002 | Telemetry must continue during lift to launch altitude. | Rulebook - Telemetry | Mandatory | Telemetry scheduler is independent of mission state | Record packets throughout a lift test. | Complete | `test_controller_launch_detection` transmits across the READY→FLIGHT transition |
| TEL-003 | Telemetry must continue throughout flight. | Rulebook - Telemetry | Mandatory | Same scheduler; sensor loss suppresses a packet without stopping the loop | Review complete flight telemetry. | Complete | `test_controller_sensor_failure_suppresses_but_continues` |
| TEL-004 | Telemetry must continue after landing until recovery. | Rulebook - Telemetry | Mandatory | LANDED and RECOVERY continue transmitting; post-impact window enforced at >= 5 s | Observe telemetry from impact until recovery. | Complete | `validate_config` rejects < 5000 ms; `test_state_machine_full_mission` |
| TEL-005 | The minimum telemetry rate must be at least 1 packet per second. | Rulebook - Telemetry | Mandatory | 1 Hz default derived from measured packet size and LoRa airtime, not chosen | Timestamp received packets and calculate rate and loss. | Complete | [link-budget.md](../design/link-budget.md); `validate_config` refuses a period the radio cannot sustain; `test_config_radio_airtime_guard` |
| TEL-006 | Every packet must contain the correct team identifier. | Rulebook - Telemetry | Mandatory | Formatter refuses the `CAN-Team-XX` placeholder; `validate_config` refuses to start with it | Inspect generated and received packets. | Complete | `test_config_validation`, `test_parser_rejects_precision_and_order` |
| TEL-007 | Packet numbers must start at P-001. | Rulebook - Telemetry | Mandatory | Counter starts at 1; the formatter rejects packet number 0 | Cold-start the CanSat and inspect the first packet. | Complete | `test_packet_numbering_and_padding`, `flight_smoke_test` first packet is `P-001` |
| TEL-008 | Packet numbers must increment sequentially. | Rulebook - Telemetry | Mandatory | A suppressed packet does not consume its number, so transmitted packets stay sequential | Analyze a packet sequence for gaps and duplicates. | Complete | `test_controller_sensor_failure_suppresses_but_continues`; ground-station duplicate and gap detection |
| TEL-009 | Packets must use the exact required field order and delimiters. | Rulebook - Mandatory Packet Format | Mandatory | Byte-exact formatter; parser rejects any deviation in order or delimiters | Compare generated packets with the specified format. | Complete | `test_telemetry_format_exact`; [protocol-fixtures.tsv](../../test-data/protocol-fixtures.tsv) across three parsers |
| TEL-010 | The team identifier field must use `CAN-Team-XX`. | Rulebook - Mandatory Packet Format | Mandatory | Team identifier is the first field and is configuration, not a constant | Validate the field in recorded packets. | Complete | `test_telemetry_format_exact`; fixture `placeholder_team` is rejected |
| TEL-011 | The packet must include `Ti-HH:MM:SS:MS` timestamp data. | Rulebook - Mandatory Packet Format | Mandatory | `format_timestamp` produces HH:MM:SS:MS; hours wrap at 100 so the field never widens | Inspect timestamp format and monotonic behavior. | Complete | `test_formatter_and_parser_agree_at_the_edges` |
| TEL-012 | The packet must include altitude as `A-XXX.X` in metres with 1 decimal place. | Rulebook - Mandatory Packet Format | Mandatory | BMP280 altitude, 1 decimal place, relative to the pad baseline | Validate field syntax, units, and precision. | Complete | `test_pressure_altitude`, `test_bmp280_compensation_datasheet_vector` |
| TEL-013 | The packet must include pressure as `Pr-XXXX.XX` in Pa with 2 decimals. | Rulebook - Mandatory Packet Format | Mandatory | BMP280 pressure in Pa, 2 decimal places | Validate field syntax, units, and precision. | Complete | `test_bmp280_compensation_datasheet_vector` reproduces the datasheet vector |
| TEL-014 | The packet must include temperature as `T-XX.X` in degrees C with 1 decimal place. | Rulebook - Mandatory Packet Format | Mandatory | BMP280 temperature in degrees C, 1 decimal place | Validate field syntax, units, and precision. | Complete | `test_bmp280_compensation_datasheet_vector` |
| TEL-015 | The packet must include roll as `Ro-XX.X` in degrees with 1 decimal place. | Rulebook - Mandatory Packet Format | Mandatory | Mahony quaternion filter roll, 1 decimal place; no seam or singularity in the state | Validate field syntax, units, precision, and test orientation. | Complete | `test_orientation_levels_and_yaw`, `test_orientation_survives_the_wrap_and_the_poles` |
| TEL-016 | The packet must include pitch as `Pi-XX.X` in degrees with 1 decimal place. | Rulebook - Mandatory Packet Format | Mandatory | Mahony quaternion filter pitch, 1 decimal place | Validate field syntax, units, precision, and test orientation. | Complete | `test_orientation_levels_and_yaw`, `test_orientation_survives_the_wrap_and_the_poles` |
| TEL-017 | The packet must include yaw as `Ya-XX.X` in degrees with 1 decimal place. | Rulebook - Mandatory Packet Format | Mandatory | Gyro-propagated yaw, declared `YR-G`. The magnetometer-referenced path (`YR-M`) is implemented and tested but **cannot run on the delivered IMU, which has no magnetometer** (F-1). Field syntax is complete; whether a relative yaw is acceptable remains an organizer question, and is now a hardware question too | Obtain organizer clarification; test against known bearings on the assembled vehicle. | Implemented; acceptance TBD | `test_orientation_yaw_is_disciplined_by_the_magnetometer`, `test_telemetry_declares_the_yaw_reference` |
| TEL-018 | The packet must include X acceleration as `AX-XX.XX` in m/s2 with 2 decimals. | Rulebook - Mandatory Packet Format | Mandatory | MPU-9250 X acceleration in m/s2, 2 decimal places; range bits and scale come from one enum | Validate field syntax, units, precision, and calibrated readings. | Complete | `test_imu_scaling`, `test_imu_range_bits_match_their_sensitivities` |
| TEL-019 | The packet must include Y acceleration as `AY-XX.XX` in m/s2 with 2 decimals. | Rulebook - Mandatory Packet Format | Mandatory | MPU-9250 Y acceleration in m/s2, 2 decimal places | Validate field syntax, units, precision, and calibrated readings. | Complete | `test_imu_scaling`, `test_imu_range_bits_match_their_sensitivities` |
| TEL-020 | The packet must include Z acceleration as `AZ-XX.XX` in m/s2 with 2 decimals. | Rulebook - Mandatory Packet Format | Mandatory | MPU-9250 Z acceleration in m/s2, 2 decimal places | Validate field syntax, units, precision, and calibrated readings. | Complete | `test_imu_scaling`, `test_imu_range_bits_match_their_sensitivities` |
| TEL-021 | Missing or corrupted mandatory fields must not be accepted as telemetry points. | Rulebook - Telemetry | Mandatory | Per-field validity flags; an incomplete mandatory set produces no packet and no telemetry point | Inject malformed packets and confirm rejection. | Complete | `test_controller_sensor_plausibility`, `test_controller_sensor_failure_suppresses_but_continues` |
| TEL-022 | Optional sensor data may be appended only after mandatory data and must not displace it. | Rulebook - Telemetry | Recommended | Optional fields are appended after the mandatory block and shed first when the packet would exceed its budget | Test packets with and without optional fields. | Complete | `test_controller_drops_optional_fields_before_overrunning_the_budget` |
| TEL-023 | Official launch LoRa sync word must be `0xA5`. | Rulebook - LoRa Configuration | Mandatory | `0xA5` selected by `RadioMode::official`, defined once in the shared link profile | Inspect configuration and test against the official receiver. | Complete | `test_link_profile_is_shared_by_both_ends`, `test_sync_word_can_be_switched_for_the_official_launch` |
| TEL-024 | Pre-launch testing LoRa sync word must be `0xF3`. | Rulebook - LoRa Configuration | Mandatory | `0xF3` selected by `RadioMode::test`, the default | Test using `0xF3` and confirm isolation from launch mode. | Complete | `test_link_profile_is_shared_by_both_ends`; `flight_smoke_test` asserts the sync word |
| TEL-025 | The launch configuration must not use another team's sync word during its launch. | Rulebook - LoRa Configuration | Mandatory | Launch configuration procedure - TBD | Review procedure and inspect configured sync word. | Not Started | |
| TEL-026 | Other CanSats must remain powered off during another team's launch. | Rulebook - LoRa Configuration | Mandatory | Team operating procedure - TBD | Review and rehearse launch-day procedure. | Not Started | |
| SEN-001 | The CanSat must measure altitude. | Rulebook - Sensor Requirements | Mandatory | BMP280 pressure through the Bosch compensation, then the barometric formula against a pad reference taken at calibration | Sensor test, calibration, and altitude validation. | Complete | `test_bmp280_compensation_datasheet_vector`, `test_pressure_altitude` |
| SEN-002 | The CanSat must measure pressure. | Rulebook - Sensor Requirements | Mandatory | BMP280 over I2C0 at `0x76`, compensated by the datasheet's 64-bit integer path | Sensor test and reading validation. | Complete | `test_bmp280_compensation_datasheet_vector` reproduces the datasheet reference vector |
| SEN-003 | The CanSat must measure temperature. | Rulebook - Sensor Requirements | Mandatory | BMP280 temperature, from the same compensated burst read as pressure | Validate readings and required telemetry formatting. | Complete | `test_bmp280_compensation_datasheet_vector` |
| SEN-004 | The CanSat must measure angular motion with a gyroscope. | Rulebook - Sensor Requirements | Mandatory | IMU gyroscope at +/-2000 deg/s, datasheet sensitivities, bias estimated on the pad | Verify all axes and calibration in a sensor test. | Complete | `test_imu_scaling`, `test_imu_range_bits_match_their_sensitivities`, `test_startup_calibrator_stationary_and_moving`; bias and noise measured on hardware, bring-up rows 3.3 and 3.4 |
| SEN-004b | The CanSat measures the ambient acoustic level during flight. | Project addition | Supporting | Analogue microphone on `GP27` / ADC1. Each flight-loop tick samples a burst of conversions and reduces it to a **peak-to-peak envelope in millivolts** — a relative level, not a sound pressure level, because calibrating to dB SPL needs a reference instrument this project does not have and the module's gain trimpot is not recorded. Written to the SD log as `sound_mv_pp` and `sound_clipped`; **deliberately not transmitted** ([telemetry-protocol.md](../design/telemetry-protocol.md)) | Bring-up rows 3.11 and 3.12; confirm the log columns after a flight. | Implemented | `test_sound_level_reduces_a_window_to_its_envelope`, `test_the_sound_level_is_logged_and_never_transmitted` |
| SEN-004c | An additional sensor must never be able to degrade mandatory telemetry. | Project addition | Mandatory | The controller holds the microphone as a **pointer that may be null**; a failed `initialize()` does not fail the self-test, a failed read raises a warning only, and the fault cannot move the mission state | Fly with the sensor disconnected and confirm identical telemetry. | Complete | `test_a_vehicle_without_a_microphone_behaves_as_before`, `test_a_failed_microphone_costs_a_warning_and_nothing_else` |
| SEN-004a | The CanSat measures the magnetic field, for yaw reference. | Project addition | Supporting | Designed around the AK8963 inside an MPU-9250 — 16-bit, 100 Hz, axis-mapped into the body frame. **The delivered part is an MPU-6500 with no magnetometer** (`WHO_AM_I` `0x70`; `0x0C` never answers), so nothing implements this on the current hardware | Blocked on a nine-axis part. The driver, axis mapping and calibration are tested against a simulated device. | **Blocked — part absent** ([F-1](../hardware/receiving-inspection.md#findings)) | `test_magnetometer_conversions`, `test_magnetometer_axes_are_rotated_into_the_body_frame` |
| SEN-005 | The CanSat must measure X acceleration. | Rulebook - Sensor Requirements | Mandatory | IMU accelerometer over I2C0 at `0x68`, GP4/GP5 | Static and controlled-motion test. | Complete | `test_imu_scaling`, `test_imu_range_bits_match_their_sensitivities` |
| SEN-006 | The CanSat must measure Y acceleration. | Rulebook - Sensor Requirements | Mandatory | Same device and interface as SEN-005 | Static and controlled-motion test. | Complete | `test_imu_scaling` |
| SEN-007 | The CanSat must measure Z acceleration. | Rulebook - Sensor Requirements | Mandatory | Same device and interface as SEN-005 | Static and controlled-motion test. | Complete | `test_imu_scaling`; stationary magnitude measured at 1 g on hardware, bring-up row 3.2 |
| SEN-008 | Roll data must be generated and transmitted. | Rulebook - Mandatory Telemetry Fields | Mandatory | Mahony quaternion filter, roll seeded directly from the first accelerometer sample and corrected by gravity thereafter | Validate against known orientations. | Complete | `test_orientation_levels_and_yaw`, `test_orientation_survives_the_wrap_and_the_poles` |
| SEN-009 | Pitch data must be generated and transmitted. | Rulebook - Mandatory Telemetry Fields | Mandatory | Same filter as SEN-008; pitch is absolutely referenced by gravity | Validate against known orientations. | Complete | `test_orientation_levels_and_yaw`, `test_orientation_survives_the_wrap_and_the_poles` |
| SEN-010 | Yaw data must be generated and transmitted in an organizer-acceptable form. | Rulebook - Mandatory Telemetry Fields | Mandatory | Yaw is generated and transmitted, declared `YR-G`. Nine-axis fusion to an absolute magnetic yaw is implemented and would run on an MPU-9250; **the delivered MPU-6500 has no magnetometer**, so this vehicle transmits a relative yaw only | Obtain clarification; bring-up gates 8.11 to 8.14 test it on hardware. | Implemented; acceptance TBD | `test_magnetic_yaw_is_tilt_compensated` |
| SEN-011 | Additional working sensors may be used for scoring. | Rulebook - Sensor Requirements | Scoring | NEO-6M GPS, parsed from NMEA and transmitted as optional `GP-Lat`/`GP-Lon`/`GP-Alt` fields after every mandatory field | Demonstrate working GPS and document transmitted or logged data. | Implemented; **not yet demonstrated with a fix** | `test_gps_parser`, `test_gps_coordinate_validation`, `test_a_hemisphere_from_the_wrong_axis_is_rejected`; NMEA confirmed at 9600 baud on hardware (bring-up 4.1), but no fix has been acquired |
| PWR-001 | The CanSat must have a manual ON/OFF switch. | Rulebook - Power / Functional Requirements | Mandatory | Switch hardware - TBD | Inspect hardware and perform repeated power-cycle test. | Blocked | |
| PWR-002 | The CanSat must have a visible LED power indicator. | Rulebook - Power / Functional Requirements | Mandatory | LED hardware - TBD | Confirm visibility and measure immediate power-on behavior. | Blocked | |
| PWR-003 | The power LED must turn on immediately when the CanSat is powered. | Rulebook - Power / Functional Requirements | Mandatory | LED power path - TBD | Observe startup across repeated power cycles. | Blocked | |
| PWR-004 | Telemetry transmission must begin automatically when powered on. | Rulebook - Power / Functional Requirements | Mandatory | The controller transmits from `READY` onward with no arming step, trigger or operator action; transmission continues in every state including `FAULT` | Power on and observe the first packet without operator action. | Complete | `test_controller_sequence_and_degradation` initialises and polls with no trigger and gets packets; `flight_smoke_test` covers boot and the first three |
| PWR-005 | The 1S LiPo supply must be designed for approximately 4.2 V full charge and lower discharge voltage. | Rulebook - Confirmed Project Hardware and Power Architecture | Mandatory | Orange 3.7 V 1500 mAh 25C 1S LiPo; power architecture - TBD | Measure operating voltage range and review power design. | Not Started | |
| PWR-006 | A suitable regulated 3.3 V rail must be provided for applicable peripherals. | Project power architecture; rulebook sensor/radio requirements | Mandatory | 3.3 V regulated power supply - TBD | Electrical review, load test, and voltage measurement. | Blocked | |
| PWR-007 | The regulator model, current rating, efficiency, and circuit must be documented before use. | Project constraint | Mandatory | Regulator selection - TBD | Review schematic, datasheet, and measured behavior. | TBD | |
| PWR-008 | Battery charging and protection must be defined before LiPo operation. | Engineering safety requirement | Mandatory | Charging and protection hardware - TBD | Design review and controlled power test. | Blocked | |
| PWR-009 | Brownout, reset, grounding, decoupling, and power-load behavior must be tested. | Engineering safety requirement | Mandatory | Power test plan - TBD | Electrical test under representative loads and faults. | Not Started | |
| MEC-001 | The CanSat structure must fit the clarified size limit. | Rulebook - General Rules; conflicting pages | Mandatory | Mechanical structure - TBD | Measure completed structure after organizer clarification. | TBD | |
| MEC-002 | The completed CanSat must fit the clarified mass limit. | Rulebook - Page 4 extract | Mandatory | Mechanical and electronics mass budget - TBD | Weigh complete flight configuration. | TBD | |
| MEC-003 | Internal layout must protect the payload and use size and mass efficiently. | Rulebook - Structure / Material Scoring | Scoring | Layout and CAD - TBD | Design review and measured completed assembly. | Not Started | |
| MEC-004 | Wiring must be organized, labeled, and securely routed. | Rulebook - Structure / Material Scoring | Scoring | Wiring layout - TBD | Inspection against drawings and photographs. | Not Started | |
| MEC-005 | Materials and construction must support lightweight, durable, and safe operation. | Rulebook - Structure / Material Scoring | Scoring | Materials - TBD | Design review and structural/impact testing. | TBD | |
| MEC-006 | CAD documentation must represent the completed design. | Rulebook - Structure / Material Scoring | Scoring | CAD files - TBD | Compare CAD, drawings, and physical hardware. | Not Started | |
| REC-001 | A parachute or equivalent descent system must be provided. | Rulebook - Mandatory Systems | Mandatory | Parachute hardware - TBD | Inspect and deploy in controlled testing. | Blocked | |
| REC-002 | The parachute must deploy during descent. | Rulebook - Parachute / Descent | Mandatory | Deployment mechanism - TBD | Demonstrated release and deployment test. | Blocked | |
| REC-003 | Parachute placement must allow immediate deployment after release. | Rulebook - Parachute / Descent | Mandatory | External or semi-exposed placement - TBD | Deployment test from the stored configuration. | Blocked | |
| REC-004 | The parachute should not be tightly packed inside the structure. | Rulebook - Parachute / Descent | Recommended | Storage arrangement - TBD | Inspect packing and observe deployment. | Blocked | |
| REC-005 | Descent rate should be no more than 5 m/s. | Rulebook - Parachute / Descent | Mandatory | Parachute size and system - TBD | Measure descent rate in a controlled flight test. | Not Started | |
| REC-006 | Descent should be stable without tumbling or uncontrolled spinning. | Rulebook - Parachute / Descent | Scoring | Descent system and mechanical layout - TBD | Video and instrumented descent review. | Not Started | |
| REC-007 | The structure should remain intact after landing. | Rulebook - Post-Landing | Mandatory | Structure and recovery design - TBD | Post-impact inspection and documented test. | Not Started | |
| REC-008 | The CanSat must transmit for at least 5 seconds after impact. | Rulebook - Post-Landing | Mandatory | Post-impact firmware and power system - TBD | Impact simulation and packet timestamp analysis. | Not Started | |
| REC-009 | The egg must be cushioned and securely retained during impact. | Rulebook - Egg Payload | Mandatory | Egg chamber and cushioning - TBD | Impact test with inspection of egg and chamber. | Blocked | |
| GS-001 | A ground station must receive CanSat telemetry. | Rulebook - Ground Station / Telemetry | Mandatory | Second Pico and RA-02; software path complete, link untested end to end | End-to-end transmission and reception test. | Not Started | |
| GS-002 | **The CanSat must be compatible with one of the two official ground stations.** | Rulebook 2026 - General Rules | Mandatory | **Satisfied by design.** The 2026 revision names 433 MHz LoRa using the SX1278 RA-02, or nRF24L01. This vehicle carries the SX1278 RA-02, confirmed on the bench: version register `0x12`, and airtime measured within 1.8% of the model ([Gate 5](../testing/bring-up-record.md)) | Receive on an official ground station at the venue. | **Radio confirmed; interoperability untested** | Gate 5 rows 5.1-5.3 |
| GS-002 | Ground-station telemetry must be compatible with the official dual ground-station evaluation. | Rulebook - Telemetry Evaluation | Mandatory | Protocol implementation - TBD | Test with the official evaluation setup. | Not Started | |
| GS-003 | Ground station must preserve and identify missing or corrupted telemetry data. | Rulebook - Telemetry | Mandatory | Receiver validation and logging - TBD | Inject loss/corruption and inspect output. | Not Started | |
| GS-004 | Ground station must support data needed for the required post-flight analysis. | Rulebook - Data Analysis | Mandatory | Ground-station data export - TBD | Export a complete test dataset and load it into analysis tools. | Not Started | |
| GS-005 | Ground-station architecture and division between Pico and computer must be documented. | Project architecture | Recommended | Second Pico and ground software boundary - TBD | Review architecture document and interface test. | TBD | |
| PCB-001 | PCB work must be documented if used in the competition design. | Rulebook - PCB Design | Scoring | Two universal prototype PCBs are confirmed; documentation - TBD | Review PCB views, schematics, and assembly records. | Not Started | |
| PCB-002 | Original PCB design may receive bonus consideration. | Rulebook - PCB Design | Scoring | Custom PCB - TBD; prototype boards are not a custom PCB | Inspect completed custom PCB evidence if pursued. | TBD | |
| PCB-003 | PCB layout should minimize external wiring and use clear routing. | Rulebook - PCB Design | Scoring | PCB layout - TBD | Design review and physical inspection. | Not Started | |
| PCB-004 | Soldering and assembly quality should be suitable for competition operation. | Rulebook - PCB Design | Scoring | Electronics assembly - TBD | Inspection and functional test. | Not Started | |
| SW-001 | Flight code must acquire and process mandatory measurements. | Rulebook - Code | Mandatory | Flight firmware - TBD | Functional test with each sensor and recorded outputs. | Not Started | |
| SW-002 | Flight code must implement reliable mission-critical functions. | Rulebook - Code | Mandatory | Flight firmware with fault handling - TBD | Fault-injection and end-to-end tests. | Not Started | |
| SW-003 | Code should be self-written. | Rulebook - Code | Scoring | Team-developed firmware and software - TBD | Review repository history and source ownership. | Not Started | |
| SW-004 | Code should be well commented and efficient. | Rulebook - Code | Scoring | Source code quality process - TBD | Code review and resource measurements. | Not Started | |
| SW-005 | Open-source libraries, if used, must be appropriately modified and documented. | Rulebook - Code | Recommended | Library inventory and attribution - TBD | Review source, modifications, and documentation. | TBD | |
| DAT-001 | Data analysis must be completed within 4 hours after launch. | Rulebook - Data Analysis | Mandatory | Analysis workflow - TBD | Timed rehearsal using representative ground-station data. | Not Started | |
| DAT-002 | Analysis must include altitude versus time or packet number. | Rulebook - Data Analysis | Mandatory | Plot generation - TBD | Inspect generated graph from test data. | Not Started | |
| DAT-003 | Analysis must include temperature versus time or packet number. | Rulebook - Data Analysis | Mandatory | Plot generation - TBD | Inspect generated graph from test data. | Not Started | |
| DAT-004 | Analysis must include pressure versus time or packet number. | Rulebook - Data Analysis | Mandatory | Plot generation - TBD | Inspect generated graph from test data. | Not Started | |
| DAT-005 | Additional analysis may include acceleration, orientation, descent rate, correlations, and other derived metrics. | Rulebook - Data Analysis | Scoring | Analysis workflow - TBD | Review additional plots and calculations. | Not Started | |
| TST-001 | Sensor tests must verify operation, calibration, units, and invalid-data handling. | Project testing requirement | Mandatory | Sensor test procedures - TBD | Recorded sensor test results. | Not Started | |
| TST-002 | GPS tests must verify startup, fix acquisition, parsing, and loss-of-fix behavior. | Project testing requirement | Recommended | NEO-6M test procedure - TBD | Recorded GPS test results. | Not Started | |
| TST-003 | LoRa tests must verify range, antenna installation, sync words, rate, loss, and recovery. | Project testing requirement | Mandatory | Paired RA-02 test procedure - TBD | Communication test logs. | Not Started | |
| TST-004 | SD tests must verify initialization, sustained logging, corruption, and write failure behavior. | Project testing requirement | Mandatory | Micro SD test procedure - TBD | Logging test data and recovery results. | Not Started | |
| TST-005 | Power tests must verify voltage range, current behavior, switch, LED, regulator, and brownout response. | Project testing requirement | Mandatory | Power test procedure - TBD | Electrical measurements and test report. | Blocked | |
| TST-006 | Parachute and descent tests must verify deployment, rate, stability, and recovery. | Project testing requirement | Mandatory | Recovery test procedure - TBD | Videos, measurements, and test report. | Blocked | |
| TST-007 | Egg impact tests must verify payload and structure survival. | Project testing requirement | Mandatory | Impact test procedure - TBD | Test results and post-test inspection. | Blocked | |
| TST-008 | Post-impact testing must verify at least 5 seconds of valid telemetry. | Project testing requirement | Mandatory | Impact/telemetry test procedure - TBD | Packet log with impact reference. | Not Started | |
| TST-009 | Full-system integration testing must cover power-on through recovery. | Project testing requirement | Mandatory | Integrated CanSat and ground station - TBD | End-to-end mission test report. | Not Started | |
| DOC-001 | Preliminary report must exclude data analysis. | Rulebook - Final Report | Mandatory | Report process - TBD | Review submitted preliminary report. | Not Started | |
| DOC-002 | Final report must include design approach, architecture, mission procedure, results, and lessons learned. | Rulebook - Final Report | Mandatory | Final report - TBD | Checklist review before submission. | Not Started | |
| DOC-003 | Final report must include schematics, PCB files, CAD designs, wiring diagrams, references, and all analysis graphs. | Rulebook - Final Report | Mandatory | Design evidence and report - TBD | Evidence checklist and document review. | Not Started | |
| DOC-004 | Required photographs must include CanSat top, side, and bottom views. | Rulebook - Required Media | Mandatory | Photography plan - TBD | Inspect image set and submission records. | Not Started | |
| DOC-005 | Required photographs must include PCB top, bottom, and side views. | Rulebook - Required Media | Mandatory | PCB photography plan - TBD | Inspect image set and submission records. | Not Started | |
| DOC-006 | Required photographs must include a team photo with the CanSat. | Rulebook - Required Media | Mandatory | Photography plan - TBD | Inspect image set. | Not Started | |
| DOC-007 | Required photographs must include a group photo with mentors and the CanSat. | Rulebook - Required Media | Mandatory | Photography plan - TBD | Inspect image set. | Not Started | |
| SUB-001 | Reports must be submitted through the provided Google Form as Google Docs links. | Rulebook - Submission | Mandatory | Submission process - TBD | Review submitted links and form confirmation. | Not Started | |
| SUB-002 | Submitted files must meet the stated 30 MB file-size limit. | Rulebook - Submission | Mandatory | Report packaging - TBD | Check final file sizes before submission. | Not Started | |
| SUB-003 | Submitted Google Docs links must allow `Anyone with the link can view`. | Rulebook - Submission | Mandatory | Sharing permissions - TBD | Open links in a separate account or private test. | Not Started | |
| SUB-004 | A project video must be posted to a team member's YouTube channel. | Rulebook - Submission | Mandatory | Video production - TBD | Inspect public post and record URL. | Not Started | |
| SUB-005 | A project video must be posted to a team member's Instagram page as a post or reel. | Rulebook - Submission | Mandatory | Video production - TBD | Inspect post/reel and record URL. | Not Started | |
| SUB-006 | Physics Club, SVNIT must be tagged on both project video posts. | Rulebook - Submission | Mandatory | Social-media submission process - TBD | Inspect both posts. | Not Started | |
| SUB-007 | Both project video links must be submitted through the Google Form. | Rulebook - Submission | Mandatory | Submission process - TBD | Review form confirmation. | Not Started | |
| GEN-006 | The team must avoid disqualification for exceeding the applicable size or mass limit by more than 10%. | Rulebook 2026 - Disqualification | Mandatory | Limits now single-valued: **21 cm (+7) x 12 cm, 500 g**. The >10% threshold applies to both | Measure and weigh the completed CanSat, and retain the records. | **Limits locked; hardware not built** | |
| GEN-007 | The team must avoid unsafe deployment, projectile motion, uncontrolled crash, and other unsafe operation. | Rulebook - Disqualification | Mandatory | Safety and recovery procedures - TBD | Safety review and controlled tests. | Not Started | |
| GEN-008 | The CanSat must include an attempted communication system. | Rulebook - Scoring / Functional | Mandatory | Paired RA-02 SX1278 modules on one shared link profile, vehicle to ground bridge to PC, with CRC framing over USB | Demonstrate a working or attempted telemetry link. | Complete | `test_link_profile_is_shared_by_both_ends`, the SX1278 driver suite, and the radio answering `0x12` and transmitting on hardware with airtime within 1.8 % of the model (bring-up rows 5.1 to 5.3) |
| GEN-009 | The team must meet arrival and conduct requirements. | Rulebook - Disqualification | Mandatory | Team procedure - TBD | Confirm schedule, attendance, and conduct requirements. | TBD | |

## Hardware Gap Analysis

The following comparison is against the confirmed project BOM. A component is not considered compliant merely because its name appears suitable.

### Confirmed Hardware

- Raspberry Pi Pico x2: one intended for the CanSat and one for the ground station.
- SX1278 RA-02 433 MHz LoRa module x2: one intended for each node.
- 433 MHz LoRa antenna with SMA male connector x2.
- 10 cm IPEX-to-SMA female RG1.13 cable x2.
- IMU x1 — bought as an MPU-9250 with an AK8963. **Delivered an MPU-6500: 3-axis accelerometer and 3-axis gyroscope, no magnetometer** ([F-1](../hardware/receiving-inspection.md#findings)).
- NEO-6M GPS module with EEPROM x1.
- GY-BMP280-3.3 pressure/altitude sensor module x1.
- MicroSD card reader module x1.
- Orange 3.7 V 1500 mAh 25C 1S LiPo battery x1.
- 10 x 10 cm single-sided universal prototype PCB x2.

### Missing or Not Yet Confirmed Hardware

- Egg payload for testing and competition.
- Cushioned, secure egg chamber.
- Parachute or equivalent descent system.
- Parachute deployment and retention hardware.
- Manual ON/OFF switch.
- Visible power LED and its installation hardware.
- 3.3 V regulated power supply: model, current rating, efficiency, protection, and circuit are TBD.
- LiPo charging and protection hardware or an approved charging arrangement: TBD.
- Any additional hardware required after the power, connector, interface, mechanical, and safety reviews: TBD.
- Custom PCB is not present; it is a scoring opportunity, not an assumed mandatory requirement.

### Hardware Compatibility Decisions Required

- Exact Pico pin allocation: TBD.
- LoRa electrical interface, logic levels, configuration, and connector wiring: TBD.
- MPU-9250 interface, voltage, address/configuration, and wiring: TBD.
- BMP280 interface, voltage, module behavior, calibration, and wiring: TBD.
- NEO-6M interface, voltage, antenna arrangement, and wiring: TBD.
- MicroSD reader supply voltage, logic levels, interface, and wiring: TBD. Do not assume the module is safe for the Pico or the planned rail without verification.
- Complete battery-to-load architecture, grounding, decoupling, current budget, and brownout behavior: TBD.
- Whether a declared relative yaw is organizer-acceptable: TBD. This is no longer only a question of what is acceptable — the delivered IMU cannot produce an absolute magnetic yaw at all, so an answer of "magnetic required" is a procurement action.

## Open Questions for Organizers

**Questions 1 to 4 are closed by the 2026 revision of the guidelines**
([`updated CanSat Final Guidelines 2026.pdf`](updated%20CanSat%20Final%20Guidelines%202026.pdf)),
which states each figure identically wherever it appears. They are kept here, struck through,
because the answers are design inputs and the record of how they were obtained matters:

1. ~~Dimensions~~ **CLOSED: 21 cm (+7 cm maximum, for the egg chamber) x 12 cm.** Page 4 and page 10 now agree; the earlier 9 cm, 12.5 cm and +8 cm figures are gone.
2. ~~Mass~~ **CLOSED: 500 g (+/-10%).** Exceeding size or mass by more than 10% is a disqualification condition.
3. ~~Launch altitude~~ **CLOSED: 100 ft, released from a drone.** The mission profile and the launch guidelines now agree; the 150 ft figure is gone, as is the 8-story rooftop.
4. ~~Egg chamber inclusion~~ **CLOSED: added, not included.** The +7 cm is explicitly an allowance on top of the 21 cm body.
5. How is the <=5 m/s descent requirement enforced and scored?
6. What constitutes valid yaw data for the mandatory telemetry field?
7. Are any LoRa frequency, bandwidth, spreading factor, coding rate, power, bandwidth, preamble, CRC, or other radio settings prescribed beyond the stated sync words?
8. What scoring thresholds apply where the rulebook says higher performance is rewarded, including packet rate and stable descent? The 2026 revision rewards rates above 1 Hz and longer stable descents but names no thresholds for either.
9. What are the actual report, media, video, arrival, and other submission deadlines?
10. **Partly closed.** The 2026 revision names the two official ground stations by radio - 433 MHz LoRa SX1278 RA-02, and nRF24L01 - which settles that this vehicle's radio is compatible. The framing and host-side data format they expect are still unstated.

## Development Gates

A gate may be passed only when its conditions are met and evidence is recorded in the Evidence column or in linked engineering records.

### Gate 1 - Requirements Locked

- Organizer answers for dimensions, mass, launch altitude, egg-chamber allowance, yaw, radio settings, scoring thresholds, official ground-station interface, and deadlines are recorded.
- Team size and self-build constraints are confirmed.
- Each requirement has an owner, verification method, and acceptance condition.
- No unresolved requirement is silently treated as satisfied.

### Gate 2 - Electrical Architecture Approved

- Pico pin allocation is documented.
- Interfaces and voltage/logic requirements for MPU-9250, BMP280, NEO-6M, MicroSD reader, and RA-02 are verified from applicable documentation or measured hardware.
- Schematic, grounding, connector, antenna, decoupling, and wiring approach are reviewed.
- 3.3 V regulated power supply selection remains explicitly documented with its specifications before approval.

### Gate 3 - Power System Tested

- LiPo charging and protection arrangement is approved.
- Manual switch and visible power LED are installed.
- The LED turns on immediately at power-on.
- 3.3 V rail voltage remains within the confirmed peripheral requirements under representative loads.
- Current, startup, brownout, reset, and fault behavior are measured and recorded.

### Gate 4 - Sensors Individually Verified

- BMP280 pressure, altitude method, and temperature readings are working and calibrated.
- IMU acceleration and gyroscope axes are working, calibrated, and oriented in one consistent frame. The magnetometer axis check does not apply to the delivered six-axis part.
- Roll and pitch are validated.
- Yaw approach is clarified and tested against the organizer's acceptance interpretation.
- NEO-6M GPS is integrated and working if used as an additional sensor.
- The analogue microphone on `GP27` is integrated and producing a level, and its two columns are present in the recovered SD log. It is an additional sensor: nothing mandatory depends on it and its absence is not a fault of the flight.

### Gate 5 - Telemetry Verified

- Mandatory packet format and every mandatory field are generated correctly.
- Team identifier is correct.
- Numbering starts at P-001 and increments sequentially.
- Official and test sync-word modes are implemented correctly.
- Rate is at least 1 packet/second with packet loss measured.
- Corrupted and incomplete mandatory packets are rejected.
- Automatic startup, lift, flight, landing, and post-impact transmission are tested.

### Gate 6 - Ground Station Verified

- Ground-station Pico and RA-02 receive the flight packets.
- Receiver validates fields and identifies loss or corruption.
- Data is logged and exported without losing mandatory values.
- Compatibility with the official dual ground stations is demonstrated or formally accepted by organizers.

### Gate 7 - Mechanical and Recovery System Verified

- Clarified dimensions and mass limits are met.
- Egg chamber securely cushions and retains the egg.
- Parachute deploys immediately after release and during descent.
- Descent rate is no more than 5 m/s in the documented test.
- Descent is stable and the structure survives landing.
- At least 5 seconds of post-impact telemetry is verified.

### Gate 8 - Full System Integration Verified

- Power-on through recovery is tested as one system.
- Sensor, SD, telemetry, ground-station, mechanical, and recovery functions operate together.
- Required failure cases have documented responses.
- Flight-like lift, release, descent, landing, recovery, and data collection are completed in a controlled test.

### Gate 9 - Competition Readiness Verified

- All mandatory requirements are verified with evidence.
- Required analysis graphs can be produced within four hours.
- Preliminary and final reports are complete for their respective stages.
- Required photographs, PCB/CAD/wiring evidence, and video are complete.
- Submission links, permissions, file sizes, tags, and form entries are checked.
- Final safety, disqualification, arrival, and operating procedures are reviewed.

## Audit Summary Fields

These fields should be updated only after the checklist is reviewed and evidence is recorded:

- Total requirements: 121.
- Mandatory requirements: 103.
- Complete or Verified requirements: 0.
- Missing or blocked requirements: 16.
- TBD requirements: 15.
