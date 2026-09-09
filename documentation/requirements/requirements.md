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

**Status as of 2026-09-09:** 37 of the 127 requirement rows are marked
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

**Updated 2026-09-08.** Ten more rows had gone stale, this time because work overtook them
rather than because they predated it:

- `PWR-006` and `PWR-007` asked for a regulator to be selected and documented. **There is no
  regulator**, and none is needed — the Pico's own rail was measured carrying every load.
  Both are now complete by elimination, with the measurement as evidence.
- `MEC-001`, `MEC-002` and `MEC-006` waited on an organizer clarification that the 2026
  revision has since given. The envelope and the mass limit are locked; what is missing is a
  structure, and that is a different kind of missing.
- `REC-001`, `REC-005` and `REC-006` said `Blocked` and `TBD` for a parachute that is now
  **sized**: an 80 cm canopy, computed and pinned by tests, though not built or dropped.
- `TEL-005` said "1 Hz default" for a link that transmits at 1.43 Hz.
- `REC-008` gained a caveat rather than a status: [F-20](../testing/bring-up-record.md#findings)
  could spend the post-impact window in the air. **Closed the same day by the descent gate**,
  which refuses a landing until a real descent has been observed.

**And one row was not stale but wrong:** two different requirements were both numbered
`GS-002`. The one added when the 2026 revision named the official radios is now `GS-006`;
the original keeps its number, because that is the one the changelog records as new.

**Updated 2026-09-09.** `TEL-025` — the launch configuration must not use another team's
sync word — moves from `Not Started` to `Complete`. It was the last row whose
implementation column read *"Launch configuration procedure - TBD"* for something that is
purely procedural, and the reason it stayed there is that a procedure alone is a promise. It
now has a **guard**: the sync word is a compile-time constant in two separate images, and
`check_doc_claims.py` parses both and fails the build if they disagree, so reflashing one
Pico and not the other is a red build rather than silence at a launch. The nine-step switch
and the post-launch revert are in the [runbook](../operations/runbook.md#launch-configuration--switching-the-sync-word).
It is **not** `Verified`, and cannot be until `0xA5` has carried a real link — only `0xF3`
ever has.

`TEL-026` — other CanSats powered off during another team's launch — closes the same
way, and the useful part of writing it was discovering what it depends on. The firmware
cannot help and is not meant to: `PWR-004` requires telemetry to begin automatically at
power-on, so a powered vehicle is a transmitting vehicle and the only control is physical.
What makes it verifiable rather than assumed is the team's **own bridge**: with the vehicle
supposedly off, `frames=` in the status line must not move, and if it does, the packet's team
identifier says whose it is.

**And it exposed three rows that were wrong rather than stale.** `PWR-001`, `PWR-002` and
`PWR-003` — the switch and the power LED — all read `Blocked`, whose definition is
*"requires missing hardware, an external decision, or another prerequisite"*. **Nothing is
missing.** Both parts have been held since 2026-09-06; they are simply not fitted, which is
`Not Started`. Calling unfitted parts blocked hides work that could be done this week behind
a word that means somebody else's problem — and these two are the only control behind
`TEL-026`, so their absence costs considerably more than their own five points.


| ID | Requirement | Source | Priority | Implementation | Verification Method | Status | Evidence |
|---|---|---|---|---|---|---|---|
| GEN-001 | Team must consist of 3 to 5 students. | Rulebook - General Rules | Mandatory | Team membership - TBD | Confirm team roster against registration records. | TBD | |
| GEN-002 | The CanSat must be self-built during the build phase. | Rulebook - General Rules | Mandatory | Team fabrication and assembly - TBD | Review build records, photographs, and assembled hardware. | Not Started | |
| GEN-003 | Prefabricated kits are not permitted. | Rulebook - General Rules | Mandatory | Component-level BOM is confirmed; kit status - TBD | Review procurement records and fabrication evidence. | Not Started | |
| GEN-004 | The CanSat must remain within the applicable physical size limit. | Rulebook 2026 - General Rules and section 8 | Mandatory | **21 cm (+7 cm maximum, egg chamber) x 12 cm.** Both pages of the 2026 revision agree; the earlier three-way contradiction is gone. Exceeding by >10% is a disqualification | Measure the completed CanSat against 21/28 cm x 12 cm. | **Requirement locked; avionics built, structure not** — the vehicle board is complete and working as of 2026-09-07; nothing in `mechanical/` exists yet, so neither dimension nor mass can be measured | |
| GEN-005 | The CanSat must remain within the applicable mass limit. | Rulebook 2026 - General Rules | Mandatory | **500 g (+/-10%).** Exceeding by >10% is a disqualification | Weigh the completed CanSat on a documented scale. | **Requirement locked; avionics built, structure not** — the vehicle board is complete and working as of 2026-09-07; nothing in `mechanical/` exists yet, so neither dimension nor mass can be measured | |
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
| TEL-005 | The minimum telemetry rate must be at least 1 packet per second. | Rulebook - Telemetry | Mandatory | **1.43 Hz (700 ms)**, sized from the *measured* airtime rather than the model, which reads 1.8 % low on this hardware. The rulebook's 1 Hz is a minimum and the 2026 revision scores rates above it; 700 ms also carries 300 ms of margin against jitter that would otherwise put an interval over a second | Timestamp received packets and calculate rate and loss. | Complete | [link-budget.md](../design/link-budget.md); three `static_assert`s in `link_profile.hpp` refuse to compile a profile that cannot meet 1 Hz; `validate_config` refuses a period the radio cannot sustain; `test_config_radio_airtime_guard`. **1.0000 Hz demonstrated on a closed link** at the configuration then carried (bring-up row 5.4); the row needs re-taking at 700 ms |
| TEL-006 | Every packet must contain the correct team identifier. | Rulebook - Telemetry | Mandatory | Formatter refuses the `CAN-Team-XX` placeholder; `validate_config` refuses to start with it. **The flight build carries `CAN-Team-25`, confirmed as the registered identifier on 2026-09-07** — which the guard could not have told you, since it rejects only the rulebook's example | Inspect generated and received packets. | Complete | `test_config_validation`, `test_parser_rejects_precision_and_order` |
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
| TEL-025 | The launch configuration must not use another team's sync word during its launch. | Rulebook - LoRa Configuration | Mandatory | **Procedure written 2026-09-09**, and the half of it that can be mechanised is: `check_doc_claims.py` parses the radio mode out of the vehicle image and the sync word out of the bridge image and **fails the build if they disagree**, naming both sides. A consistent tree reports which configuration it would fly. The nine-step switch, the two confirmations on the images themselves, the end-to-end check and the post-launch revert are in the runbook | Read the build's `vehicle and bridge agree:` line, then the vehicle's startup summary and the bridge's `sync=` status field, then confirm packets actually arrive. | Complete | [runbook.md](../operations/runbook.md#launch-configuration--switching-the-sync-word); `vehicle and bridge agree` in `tools/check_doc_claims.py`. **`0xA5` has never been on the air** — only `0xF3` has ever linked (bring-up row 5.13), so this cannot be `Verified` until the launch configuration has carried a real link |
| TEL-026 | Other CanSats must remain powered off during another team's launch. | Rulebook - LoRa Configuration | Mandatory | **Procedure written 2026-09-09.** Named ownership of the power state, battery disconnected before another team's launch, and — the part that makes it verifiable rather than assumed — **the team's own bridge used as the detector**: with the vehicle supposedly off, `frames=` in the bridge status line must not move over 30 s, and if it does the packet's team identifier says whose it is. **The firmware cannot help and is not meant to**: PWR-004 requires telemetry to begin automatically at power-on, so a powered vehicle is a transmitting vehicle and the only control is physical | Rehearse the procedure; confirm the bridge's frame counter is static with the vehicle off. | Complete | [runbook.md](../operations/runbook.md#radio-silence--when-your-vehicle-must-be-off). **Depends on `PWR-001` and `PWR-002`/`PWR-003`, none of them fitted**: until the switch and the power LED exist, "off" means the battery lead is physically out and nothing indicates it from outside the structure. Not `Verified` — no launch has been attended |
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
| SEN-011 | Additional working sensors may be used for scoring. | Rulebook - Sensor Requirements | Scoring | NEO-6M GPS, parsed from NMEA and **logged** as `gps_lat`/`gps_lon`/`gps_alt`/`gps_satellites`/`gps_hdop` columns in every SD row. The requirement is satisfied by data *transmitted or logged*, and the three `GP-` packet fields are 56 bytes — the difference between a 199- and a 255-byte worst case, and so between 1.43 Hz and 1.18 Hz on a rate the rulebook separately scores. `Configuration::transmit_gps` puts them back on the air when a flight needs live position for recovery | Demonstrate working GPS and document transmitted or logged data. | Implemented; **not yet demonstrated with a fix** | `test_gps_parser`, `test_gps_coordinate_validation`, `test_a_hemisphere_from_the_wrong_axis_is_rejected`; NMEA confirmed at 9600 baud on hardware (bring-up 4.1), but no fix has been acquired |
| PWR-001 | The CanSat must have a manual ON/OFF switch. | Rulebook - Power / Functional Requirements | Mandatory | **Part held since 2026-09-06** — an I/O switch, confirmed in the [purchase list](../hardware/purchase-list.md). **Not fitted.** Its place in the power path is drawn: battery → switch → Schottky → Pico `VSYS` ([netlist](../../electrical/schematics/vehicle-netlist.tsv), nets `VBAT`/`VSW`) | Inspect hardware and perform repeated power-cycle test. | Not Started | Nothing blocks fitting it. It is also the only control behind [TEL-026](../operations/runbook.md#radio-silence--when-your-vehicle-must-be-off), so its absence costs more than its own five points |
| PWR-002 | The CanSat must have a visible LED power indicator. | Rulebook - Power / Functional Requirements | Mandatory | **Part held since 2026-09-06** — LEDs in two colours and 1 kΩ series resistors. **Not fitted.** Wired from the switched rail, not from a GPIO | Confirm visibility and measure immediate power-on behavior. | Not Started | It must be on the rail rather than firmware-driven, or it cannot satisfy PWR-003. Nets `VSW`/`PWR_LED_A` in the [netlist](../../electrical/schematics/vehicle-netlist.tsv) |
| PWR-003 | The power LED must turn on immediately when the CanSat is powered. | Rulebook - Power / Functional Requirements | Mandatory | **Satisfied by construction once PWR-002 is fitted**, because the LED hangs off the switched rail through a series resistor: it lights the instant the switch closes, with no firmware involved. A GPIO-driven LED could not meet this — it would wait for boot | Observe startup across repeated power cycles. | Not Started | Design decision recorded in [avionics/power](../../avionics/power/README.md); the separate GP14 status LED is firmware-driven and is **not** this indicator |
| PWR-004 | Telemetry transmission must begin automatically when powered on. | Rulebook - Power / Functional Requirements | Mandatory | The controller transmits from `READY` onward with no arming step, trigger or operator action; transmission continues in every state including `FAULT` | Power on and observe the first packet without operator action. | Complete | `test_controller_sequence_and_degradation` initialises and polls with no trigger and gets packets; `flight_smoke_test` covers boot and the first three |
| PWR-005 | The 1S LiPo supply must be designed for approximately 4.2 V full charge and lower discharge voltage. | Rulebook - Confirmed Project Hardware and Power Architecture | Mandatory | Orange 3.7 V 1500 mAh 25C 1S LiPo; power architecture - TBD | Measure operating voltage range and review power design. | Not Started | |
| PWR-006 | A suitable regulated 3.3 V rail must be provided for applicable peripherals. | Project power architecture; rulebook sensor/radio requirements | Mandatory | **Satisfied without an external regulator.** Every load runs from the Pico's own `3V3(OUT)`. The AMS1117-3.3 was assessed and rejected; the microSD reader turned out to be a 3.3 V board, so no boost stage is needed either | Electrical review, load test, and voltage measurement. | **Complete** — measured, not asserted | Rail measured at **3.28–3.29 V through 45 back-to-back transmits** and 3.28–3.30 V at 100 % microSD write duty ([bring-up gates 2, 5 and 6](../testing/bring-up-record.md)). **Open:** the radio and card drawing simultaneously, which is gate 7 |
| PWR-007 | The regulator model, current rating, efficiency, and circuit must be documented before use. | Project constraint | Mandatory | **No regulator to document.** There is no external regulator; the rail is the RP2040 board's own, whose specification is in the Pico datasheet. The decision and its reasoning are recorded rather than the part | Review schematic, datasheet, and measured behavior. | **Complete by elimination** | [electrical-architecture.md](../design/electrical-architecture.md); [avionics/power](../../avionics/power/README.md) |
| PWR-008 | Battery charging and protection must be defined before LiPo operation. | Engineering safety requirement | Mandatory | Charging and protection hardware - TBD | Design review and controlled power test. | Blocked | |
| PWR-009 | Brownout, reset, grounding, decoupling, and power-load behavior must be tested. | Engineering safety requirement | Mandatory | Power test plan - TBD | Electrical test under representative loads and faults. | Not Started | |
| MEC-001 | The CanSat structure must fit the clarified size limit. | Rulebook - General Rules; conflicting pages | Mandatory | **A design exists and it fits: `Cansat_D1`, 118.5 × 115.0 × 110.0 mm.** Height is inside the 210 mm body allowance with 91.5 mm to spare, and the section is inside the 120 mm limit on both faces with **2.5 and 5.0 mm of clearance per side**. **The limit's meaning was the open part and is now closed** — the organizers confirmed on 2026-09-09 that a 12 cm sided box is acceptable, so the 159.1 mm diagonal does not decide anything. Nothing is fabricated | Measure the completed CanSat against 21/28 cm x 12 cm, as a sided box. | **Designed and compliant; nothing built** | [mechanical/README.md](../../mechanical/README.md#the-envelope-question--asked-and-answered); dimensions read from the STEP by `tools/cad_dimensions.py` and held to that document by `check_doc_claims.py`. **The clearance is now the live constraint**: 2.5 mm per side is all any protruding feature has |
| MEC-002 | The completed CanSat must fit the clarified mass limit. | Rulebook - Page 4 extract | Mandatory | **Electronics weighed 2026-09-09: 151.299 g** — assembled PCB 110.573 g plus a 40.726 g battery. That is **30 % of the 500 g budget**, leaving **348.701 g** for the structure, the egg chamber and the parachute. The PETG structure has not been printed or weighed | Weigh the complete flight configuration. | In Progress | [mechanical/README.md](../../mechanical/README.md#mass-budget). **The previous estimate was wrong in the dangerous direction** — it counted vendor figures to ~67 g and predicted the avionics would not exceed 120 g, against a measured 151.3 g |
| MEC-003 | Internal layout must protect the payload and use size and mass efficiently. | Rulebook - Structure / Material Scoring | Scoring | **Layout is modelled and the frame is out for printing.** An open box frame: two solid side panels, two faces opened out with arched cutouts, a central spine, harness slots. The 115 × 110 mm section takes the 100 × 100 mm board flat. **91.5 mm of the 210 mm height allowance is unused**, and section D scores effective use of permitted volume — a deliberate decision to make rather than an oversight to leave | Design review and measured completed assembly. | In Progress | [`CAD/Cansat_D1.step`](../../mechanical/CAD/Cansat_D1.step); [renders](../../mechanical/photos/README.md); [envelope drawing](../../mechanical/drawings/envelope-and-board-fit.svg) |
| MEC-004 | Wiring must be organized, labeled, and securely routed. | Rulebook - Structure / Material Scoring | Scoring | Wiring layout - TBD | Inspection against drawings and photographs. | Not Started | |
| MEC-005 | Materials and construction must support lightweight, durable, and safe operation. | Rulebook - Structure / Material Scoring | Scoring | **PETG, 3D printed — decided 2026-09-09** and out for printing. Chosen for impact behaviour: tougher than PLA and not brittle, printable without an enclosure unlike ABS, and it deforms rather than shatters, which is the failure mode that leaves a recoverable vehicle. Three static-stress studies back it | Design review and structural/impact testing. | In Progress | [Material and manufacture](../../mechanical/README.md#material-and-manufacture); [simulation/](../../mechanical/simulation/README.md). **The studies were run with Fusion's PET rather than PETG** (21 % denser, somewhat stronger) and assume an isotropic part, which a printed one is not — both recorded. Drop testing is what closes this |
| MEC-006 | CAD documentation must represent the completed design. | Rulebook - Structure / Material Scoring | Scoring | **CAD exists**: `Cansat_D1.f3d` (native, editable, carrying three simulation studies) with a `Cansat_D1.step` neutral export beside it, both committed. The STEP is the copy this repository can actually read, and every mechanical dimension it states is extracted from it rather than typed | Compare CAD, drawings, and physical hardware. | In Progress | [mechanical/CAD/](../../mechanical/CAD/README.md). **Cannot be `Complete` until there is a built article for the model to represent** — MEC-006 asks for CAD of the *completed design*, and nothing is fabricated |
| REC-001 | A parachute or equivalent descent system must be provided. | Rulebook - Mandatory Systems | Mandatory | **Sized, not built.** An **80.0 cm** flat circular canopy brings 550 g down at 5.00 m/s in 35 °C air. Sized at the top of the mass tolerance deliberately: a canopy sized at the nominal 500 g exceeds the cap when flown at 550 g | Inspect and deploy in controlled testing. | **Designed; not built** | [`simulations/descent.py`](../../simulations/descent.py), run by the host suite and pinned to closed-form limits; `test_sizing_at_the_top_of_the_tolerance_covers_the_whole_band` |
| REC-002 | The parachute must deploy during descent. | Rulebook - Parachute / Descent | Mandatory | Deployment mechanism - TBD | Demonstrated release and deployment test. | Blocked | |
| REC-003 | Parachute placement must allow immediate deployment after release. | Rulebook - Parachute / Descent | Mandatory | External or semi-exposed placement - TBD | Deployment test from the stored configuration. | Blocked | |
| REC-004 | The parachute should not be tightly packed inside the structure. | Rulebook - Parachute / Descent | Recommended | Storage arrangement - TBD | Inspect packing and observe deployment. | Blocked | |
| REC-005 | Descent rate should be no more than 5 m/s. | Rulebook - Parachute / Descent | Mandatory | **Computed: 5.00 m/s by construction**, since the canopy is sized to that rate. Descent from 30.48 m takes **6.45 s**, from the closed-form solution of the equation of motion rather than height ÷ rate. **The drag coefficient is the dominant uncertainty and is unmeasured** | Measure descent rate in a controlled flight test. | **Computed; unverified** | [`simulations/descent.py`](../../simulations/descent.py); [simulations/README.md](../../simulations/README.md). Closed only by a drop test with a known mass and a stopwatch |
| REC-006 | Descent should be stable without tumbling or uncontrolled spinning. | Rulebook - Parachute / Descent | Scoring | Not built. **The canopy choice is a stability choice:** an unvented flat circular oscillates; a ~10 % central vent costs a little drag and buys a great deal of stability; a cruciform is better still and is easy to sew. Scored *comparatively across teams*, so no absolute target exists | Video and instrumented descent review. | Not Started | [mechanical/README.md](../../mechanical/README.md#the-parachute) |
| REC-007 | The structure should remain intact after landing. | Rulebook - Post-Landing | Mandatory | Structure and recovery design - TBD | Post-impact inspection and documented test. | Not Started | |
| REC-008 | The CanSat must transmit for at least 5 seconds after impact. | Rulebook - Post-Landing | Mandatory | `post_impact_transmission_ms` is 5000 and `validate_config()` refuses to build with less. **[F-20](../testing/bring-up-record.md#findings) closed 2026-09-08:** a hover before release used to spend the `LANDED` window in the air. The descent gate refuses a landing until a real descent has been observed, so the window is now spent on the ground where the requirement wants it | Impact simulation and packet timestamp analysis. | Implemented; **[F-20](../testing/bring-up-record.md#findings) open** | `test_state_machine_full_mission`; `validate_config` rejects < 5000 ms; [concept-of-operations.md](../mission/concept-of-operations.md#the-hover-problem-f-20) |
| REC-009 | The egg must be cushioned and securely retained during impact. | Rulebook - Egg Payload | Mandatory | Egg chamber and cushioning - TBD | Impact test with inspection of egg and chamber. | Blocked | |
| GS-001 | A ground station must receive CanSat telemetry. | Rulebook - Ground Station / Telemetry | Mandatory | Second Pico and RA-02; software path complete, link untested end to end | End-to-end transmission and reception test. | Not Started | |
| GS-006 | **The CanSat must be compatible with one of the two official ground stations.** | Rulebook 2026 - General Rules | Mandatory | **Satisfied by design.** The 2026 revision names 433 MHz LoRa using the SX1278 RA-02, or nRF24L01. This vehicle carries the SX1278 RA-02, confirmed on the bench: version register `0x12`, and airtime measured within 1.8% of the model ([Gate 5](../testing/bring-up-record.md)) | Receive on an official ground station at the venue. | **Radio confirmed; interoperability untested** | Gate 5 rows 5.1-5.3 |
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
11. ~~How is the 12 cm "across" limit measured on a non-cylindrical CanSat?~~ **CLOSED
    2026-09-09: a 12 cm sided box is acceptable.** Confirmed with the organizers. The
    section limit is a 120 mm square, not a 120 mm bore, so the prismatic 115 x 110 mm
    design fits with 2.5 and 5.0 mm of clearance per side. The question was worth asking:
    read as a diameter, the 159.1 mm corner-to-corner diagonal would have been 33% over,
    and exceeding a dimensional limit by more than 10% is a disqualification rather than a
    deduction. **Keep the written confirmation with the submission** - the answer is
    currently recorded only in this repository, and it is a disqualification-class
    dimension. See [mechanical/README.md](../../mechanical/README.md#the-envelope-question--asked-and-answered).

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
