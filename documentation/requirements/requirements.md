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

**Status as of 2026-09-04:** 23 of the requirements whose acceptance can be judged from
software are marked `Complete`, each with a named test in the Evidence column. None is
marked `Verified`: that word is reserved for evidence from hardware, and the vehicle has
never been powered. Every remaining row depends on procurement, the power design, the
mechanical build or a launch.


| ID | Requirement | Source | Priority | Implementation | Verification Method | Status | Evidence |
|---|---|---|---|---|---|---|---|
| GEN-001 | Team must consist of 3 to 5 students. | Rulebook - General Rules | Mandatory | Team membership - TBD | Confirm team roster against registration records. | TBD | |
| GEN-002 | The CanSat must be self-built during the build phase. | Rulebook - General Rules | Mandatory | Team fabrication and assembly - TBD | Review build records, photographs, and assembled hardware. | Not Started | |
| GEN-003 | Prefabricated kits are not permitted. | Rulebook - General Rules | Mandatory | Component-level BOM is confirmed; kit status - TBD | Review procurement records and fabrication evidence. | Not Started | |
| GEN-004 | The CanSat must remain within the applicable physical size limit. | Rulebook - General Rules; conflicting pages | Mandatory | Dimensions - TBD pending organizer clarification | Measure the completed CanSat against the clarified limit. | TBD | |
| GEN-005 | The CanSat must remain within the applicable mass limit. | Rulebook - Page 4 extract | Mandatory | Mass - TBD; one stated limit is 500 g | Weigh the completed CanSat using a documented scale and clarified limit. | TBD | |
| MIS-001 | The CanSat must be lifted to the specified launch altitude. | Rulebook - Mission and Launch Guidelines | Mandatory | Launch altitude - TBD | Confirm organizer clarification and document the lift profile. | TBD | |
| MIS-002 | The CanSat must be powered on before launch. | Rulebook - Mission | Mandatory | Manual power system - TBD | Observe and record power-on before launch. | Not Started | |
| MIS-003 | Initial ground-floor telemetry should report approximately zero altitude. | Rulebook - Mission | Mandatory | BMP280 baseline and altitude calculation - TBD | Compare startup telemetry with the ground-floor baseline. | Not Started | |
| MIS-004 | Telemetry must reflect the altitude change during lifting. | Rulebook - Mission | Mandatory | Continuous altitude telemetry - TBD | Review logged packets during a lift test. | Not Started | |
| MIS-005 | Parachute deployment must be demonstrated after release. | Rulebook - Mission and Descent | Mandatory | Parachute and deployment hardware - TBD | Demonstrate release and deployment in a controlled test. | Blocked | |
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
| TEL-015 | The packet must include roll as `Ro-XX.X` in degrees with 1 decimal place. | Rulebook - Mandatory Packet Format | Mandatory | Complementary filter roll, 1 decimal place; blending is correct across the +-180 deg seam | Validate field syntax, units, precision, and test orientation. | Complete | `test_orientation_levels_and_yaw`, `test_orientation_blends_across_the_wrap` |
| TEL-016 | The packet must include pitch as `Pi-XX.X` in degrees with 1 decimal place. | Rulebook - Mandatory Packet Format | Mandatory | Complementary filter pitch, 1 decimal place | Validate field syntax, units, precision, and test orientation. | Complete | `test_orientation_levels_and_yaw` |
| TEL-017 | The packet must include yaw as `Ya-XX.X` in degrees with 1 decimal place. | Rulebook - Mandatory Packet Format | Mandatory | Yaw source and algorithm - TBD | Obtain organizer clarification and test the selected implementation. | TBD | |
| TEL-018 | The packet must include X acceleration as `AX-XX.XX` in m/s2 with 2 decimals. | Rulebook - Mandatory Packet Format | Mandatory | MPU6050 X acceleration in m/s2, 2 decimal places; range bits and scale come from one enum | Validate field syntax, units, precision, and calibrated readings. | Complete | `test_mpu_scaling`, `test_imu_range_bits_match_their_sensitivities` |
| TEL-019 | The packet must include Y acceleration as `AY-XX.XX` in m/s2 with 2 decimals. | Rulebook - Mandatory Packet Format | Mandatory | MPU6050 Y acceleration in m/s2, 2 decimal places | Validate field syntax, units, precision, and calibrated readings. | Complete | `test_mpu_scaling`, `test_imu_range_bits_match_their_sensitivities` |
| TEL-020 | The packet must include Z acceleration as `AZ-XX.XX` in m/s2 with 2 decimals. | Rulebook - Mandatory Packet Format | Mandatory | MPU6050 Z acceleration in m/s2, 2 decimal places | Validate field syntax, units, precision, and calibrated readings. | Complete | `test_mpu_scaling`, `test_imu_range_bits_match_their_sensitivities` |
| TEL-021 | Missing or corrupted mandatory fields must not be accepted as telemetry points. | Rulebook - Telemetry | Mandatory | Per-field validity flags; an incomplete mandatory set produces no packet and no telemetry point | Inject malformed packets and confirm rejection. | Complete | `test_controller_sensor_plausibility`, `test_controller_sensor_failure_suppresses_but_continues` |
| TEL-022 | Optional sensor data may be appended only after mandatory data and must not displace it. | Rulebook - Telemetry | Recommended | Optional fields are appended after the mandatory block and shed first when the packet would exceed its budget | Test packets with and without optional fields. | Complete | `test_controller_drops_optional_fields_before_overrunning_the_budget` |
| TEL-023 | Official launch LoRa sync word must be `0xA5`. | Rulebook - LoRa Configuration | Mandatory | `0xA5` selected by `RadioMode::official`, defined once in the shared link profile | Inspect configuration and test against the official receiver. | Complete | `test_link_profile_is_shared_by_both_ends`, `test_sync_word_can_be_switched_for_the_official_launch` |
| TEL-024 | Pre-launch testing LoRa sync word must be `0xF3`. | Rulebook - LoRa Configuration | Mandatory | `0xF3` selected by `RadioMode::test`, the default | Test using `0xF3` and confirm isolation from launch mode. | Complete | `test_link_profile_is_shared_by_both_ends`; `flight_smoke_test` asserts the sync word |
| TEL-025 | The launch configuration must not use another team's sync word during its launch. | Rulebook - LoRa Configuration | Mandatory | Launch configuration procedure - TBD | Review procedure and inspect configured sync word. | Not Started | |
| TEL-026 | Other CanSats must remain powered off during another team's launch. | Rulebook - LoRa Configuration | Mandatory | Team operating procedure - TBD | Review and rehearse launch-day procedure. | Not Started | |
| SEN-001 | The CanSat must measure altitude. | Rulebook - Sensor Requirements | Mandatory | GY-BMP280-3.3; integration and altitude method - TBD | Sensor test, calibration, and altitude validation. | Not Started | |
| SEN-002 | The CanSat must measure pressure. | Rulebook - Sensor Requirements | Mandatory | GY-BMP280-3.3; interface - TBD | Compare readings against a controlled pressure test. | Not Started | |
| SEN-003 | The CanSat must measure temperature. | Rulebook - Sensor Requirements | Mandatory | BMP280 temperature reading intended; integration - TBD | Validate readings and required telemetry formatting. | Not Started | |
| SEN-004 | The CanSat must measure angular motion with a gyroscope. | Rulebook - Sensor Requirements | Mandatory | MPU6050; interface and calibration - TBD | Verify all axes and calibration in a sensor test. | Not Started | |
| SEN-005 | The CanSat must measure X acceleration. | Rulebook - Sensor Requirements | Mandatory | MPU6050; pin/interface - TBD | Static and controlled-motion test. | Not Started | |
| SEN-006 | The CanSat must measure Y acceleration. | Rulebook - Sensor Requirements | Mandatory | MPU6050; pin/interface - TBD | Static and controlled-motion test. | Not Started | |
| SEN-007 | The CanSat must measure Z acceleration. | Rulebook - Sensor Requirements | Mandatory | MPU6050; pin/interface - TBD | Static and controlled-motion test. | Not Started | |
| SEN-008 | Roll data must be generated and transmitted. | Rulebook - Mandatory Telemetry Fields | Mandatory | MPU6050 orientation processing - TBD | Validate against known orientations. | Not Started | |
| SEN-009 | Pitch data must be generated and transmitted. | Rulebook - Mandatory Telemetry Fields | Mandatory | MPU6050 orientation processing - TBD | Validate against known orientations. | Not Started | |
| SEN-010 | Yaw data must be generated and transmitted in an organizer-acceptable form. | Rulebook - Mandatory Telemetry Fields | Mandatory | Yaw implementation - TBD | Obtain clarification and conduct orientation tests. | TBD | |
| SEN-011 | Additional working sensors may be used for scoring. | Rulebook - Sensor Requirements | Scoring | NEO-6M GPS is available as an additional sensor; integration - TBD | Demonstrate working GPS and document transmitted or logged data. | Not Started | |
| PWR-001 | The CanSat must have a manual ON/OFF switch. | Rulebook - Power / Functional Requirements | Mandatory | Switch hardware - TBD | Inspect hardware and perform repeated power-cycle test. | Blocked | |
| PWR-002 | The CanSat must have a visible LED power indicator. | Rulebook - Power / Functional Requirements | Mandatory | LED hardware - TBD | Confirm visibility and measure immediate power-on behavior. | Blocked | |
| PWR-003 | The power LED must turn on immediately when the CanSat is powered. | Rulebook - Power / Functional Requirements | Mandatory | LED power path - TBD | Observe startup across repeated power cycles. | Blocked | |
| PWR-004 | Telemetry transmission must begin automatically when powered on. | Rulebook - Power / Functional Requirements | Mandatory | Flight firmware startup - TBD | Power-cycle test with no manual trigger. | Not Started | |
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
| GS-001 | A ground station must receive CanSat telemetry. | Rulebook - Ground Station / Telemetry | Mandatory | Second Pico and RA-02; software path - TBD | End-to-end transmission and reception test. | Not Started | |
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
| GEN-006 | The team must avoid disqualification for exceeding the applicable size or mass limit by more than 10%. | Rulebook - Disqualification | Mandatory | Compliance measurement - TBD | Measure against clarified limits and retain records. | TBD | |
| GEN-007 | The team must avoid unsafe deployment, projectile motion, uncontrolled crash, and other unsafe operation. | Rulebook - Disqualification | Mandatory | Safety and recovery procedures - TBD | Safety review and controlled tests. | Not Started | |
| GEN-008 | The CanSat must include an attempted communication system. | Rulebook - Disqualification | Mandatory | Paired RA-02 telemetry system - TBD | Demonstrate communication before competition. | Not Started | |
| GEN-009 | The team must meet arrival and conduct requirements. | Rulebook - Disqualification | Mandatory | Team procedure - TBD | Confirm schedule, attendance, and conduct requirements. | TBD | |

## Hardware Gap Analysis

The following comparison is against the confirmed project BOM. A component is not considered compliant merely because its name appears suitable.

### Confirmed Hardware

- Raspberry Pi Pico x2: one intended for the CanSat and one for the ground station.
- SX1278 RA-02 433 MHz LoRa module x2: one intended for each node.
- 433 MHz LoRa antenna with SMA male connector x2.
- 10 cm IPEX-to-SMA female RG1.13 cable x2.
- MPU6050 3-axis accelerometer and gyroscope x1.
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
- MPU6050 interface, voltage, address/configuration, and wiring: TBD.
- BMP280 interface, voltage, module behavior, calibration, and wiring: TBD.
- NEO-6M interface, voltage, antenna arrangement, and wiring: TBD.
- MicroSD reader supply voltage, logic levels, interface, and wiring: TBD. Do not assume the module is safe for the Pico or the planned rail without verification.
- Complete battery-to-load architecture, grounding, decoupling, current budget, and brownout behavior: TBD.
- Whether the MPU6050-based design can provide organizer-acceptable yaw data: TBD.

## Open Questions for Organizers

1. **Dimensions:** Page 1 states 12 cm width with an egg chamber; page 4 states <=21 cm x 9 cm; page 10 states 21 cm (+8 cm maximum for the egg chamber) x 12.5 cm. Which dimensional limit applies?
2. **Mass:** Is the <=500 g value the applicable mass limit, and how does the stated more-than-10% disqualification threshold apply?
3. **Launch altitude:** The mission section states 100 ft, while the launch guidelines state 150 ft from a drone. Which altitude applies?
4. Are egg chamber dimensions included in or added to the main CanSat dimensions?
5. How is the <=5 m/s descent requirement enforced and scored?
6. What constitutes valid yaw data for the mandatory telemetry field?
7. Are any LoRa frequency, bandwidth, spreading factor, coding rate, power, bandwidth, preamble, CRC, or other radio settings prescribed beyond the stated sync words?
8. What scoring thresholds apply where the rulebook says higher performance is rewarded, including packet rate and stable descent?
9. What are the actual report, media, video, arrival, and other submission deadlines?
10. What exact interface and data format will be used by the official dual ground stations?

## Development Gates

A gate may be passed only when its conditions are met and evidence is recorded in the Evidence column or in linked engineering records.

### Gate 1 - Requirements Locked

- Organizer answers for dimensions, mass, launch altitude, egg-chamber allowance, yaw, radio settings, scoring thresholds, official ground-station interface, and deadlines are recorded.
- Team size and self-build constraints are confirmed.
- Each requirement has an owner, verification method, and acceptance condition.
- No unresolved requirement is silently treated as satisfied.

### Gate 2 - Electrical Architecture Approved

- Pico pin allocation is documented.
- Interfaces and voltage/logic requirements for MPU6050, BMP280, NEO-6M, MicroSD reader, and RA-02 are verified from applicable documentation or measured hardware.
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
- MPU6050 acceleration and gyroscope axes are working, calibrated, and oriented.
- Roll and pitch are validated.
- Yaw approach is clarified and tested against the organizer's acceptance interpretation.
- NEO-6M GPS is integrated and working if used as an additional sensor.

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
