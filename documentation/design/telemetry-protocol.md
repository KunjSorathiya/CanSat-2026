# Telemetry Protocol

This document defines the CanSat-to-ground-station telemetry contract for software development. It is based on the official CanSat 2026 rulebook requirements recorded in `documentation/requirements/requirements.md`.

This is a protocol specification only. It does not implement firmware, ground-station software, radio configuration, or SD logging.

## Protocol Version

**Telemetry Protocol v0.1**

The protocol version identifies the packet contract and associated data model. A future incompatible change must increment the major version. A compatible addition, such as an optional field appended after all mandatory fields, may increment the minor version only after confirming that the official ground stations still accept the packet. Any change to mandatory field order, prefixes, units, precision, separators, or meaning requires a new protocol review and version.

The team identifier is a configuration item and is not a protocol version:

```text
CAN-Team-XX
```

The actual team number is **TBD - organizer/team number confirmation required**. It must not be invented or left ambiguous in launch configuration.

## Mandatory Packet

The rulebook packet format is:

```text
CAN-Team-XX; P-XXX; Ti-HH:MM:SS:MS; A-XXX.X; Pr-XXXX.XX; T-XX.X; Ro-XX.X; Pi-XX.X; Ya-XX.X; AX-XX.XX; AY-XX.XX; AZ-XX.XX;
```

The mandatory fields must appear in exactly this order. The separator shown by the rulebook is a semicolon followed by a space. The format includes a trailing semicolon.

| Order | Name | Prefix | Meaning | Unit | Precision | Source sensor | Data type | Valid range | Invalid-data behavior | Status |
|---:|---|---|---|---|---|---|---|---|---|---|
| 1 | Team identifier | `CAN-Team-XX` | Registered team identity | Not applicable | Exact configured text | Configuration | String | Team number not yet known | Configuration error; do not launch | Required; team ID TBD |
| 2 | Packet number | `P-XXX` | Sequential packet identity | Not applicable | Three displayed digits in rulebook example | Packet counter | Unsigned integer rendered as text | Starts at `P-001`; upper bound TBD | Counter/configuration error; packet is invalid | Required |
| 3 | Timestamp | `Ti-HH:MM:SS:MS` | Mission-relative or selected time reference | Time | Hours, minutes, seconds, milliseconds as shown | Recommended Pico monotonic timer | Time value rendered as text | Format bounds follow the clock; mission epoch TBD | Missing or malformed timestamp makes the mandatory packet invalid | Required |
| 4 | Altitude | `A-XXX.X` | Altitude | Metres | 1 decimal place | BMP280-derived altitude | Signed numeric value rendered as text | No rulebook numeric range established | Sensor-invalid; do not emit a valid mandatory packet | Required |
| 5 | Pressure | `Pr-XXXX.XX` | Atmospheric pressure | Pa | 2 decimal places | BMP280 | Signed/unsigned numeric representation TBD; rendered as text | No rulebook numeric range established | Sensor-invalid; do not emit a valid mandatory packet | Required |
| 6 | Temperature | `T-XX.X` | Temperature | Degrees C | 1 decimal place | BMP280 | Signed numeric value rendered as text | No rulebook numeric range established | Sensor-invalid; do not emit a valid mandatory packet | Required |
| 7 | Roll | `Ro-XX.X` | Rotation about the project-defined roll axis | Degrees | 1 decimal place | MPU-9250-derived orientation | Signed numeric value rendered as text | No rulebook numeric range established | Sensor-invalid or orientation-invalid; do not emit a valid mandatory packet | Required |
| 8 | Pitch | `Pi-XX.X` | Rotation about the project-defined pitch axis | Degrees | 1 decimal place | MPU-9250-derived orientation | Signed numeric value rendered as text | No rulebook numeric range established | Sensor-invalid or orientation-invalid; do not emit a valid mandatory packet | Required |
| 9 | Yaw | `Ya-XX.X` | Rotation about the project-defined yaw axis | Degrees | 1 decimal place | MPU-9250-derived orientation; absolute reference TBD | Signed numeric value rendered as text | No rulebook numeric range established | Yaw validity failure; do not silently claim an absolute heading | Required; engineering decision open |
| 10 | X acceleration | `AX-XX.XX` | Acceleration on project-defined X axis | m/s2 | 2 decimal places | MPU-9250 | Signed numeric value rendered as text | No rulebook numeric range established | Sensor-invalid; do not emit a valid mandatory packet | Required |
| 11 | Y acceleration | `AY-XX.XX` | Acceleration on project-defined Y axis | m/s2 | 2 decimal places | MPU-9250 | Signed numeric value rendered as text | No rulebook numeric range established | Sensor-invalid; do not emit a valid mandatory packet | Required |
| 12 | Z acceleration | `AZ-XX.XX` | Acceleration on project-defined Z axis | m/s2 | 2 decimal places | MPU-9250 | Signed numeric value rendered as text | No rulebook numeric range established | Sensor-invalid; do not emit a valid mandatory packet | Required |

The rulebook does not establish numeric valid ranges for these fields. Firmware must not invent arbitrary acceptance limits. Engineering plausibility checks may be added later only when based on sensor documentation, calibration, physical constraints, or a documented project requirement.

## Packet Rules

- Packet numbering starts at `P-001`.
- Packet numbers increment sequentially for transmitted packets.
- Mandatory fields always have priority over optional fields.
- Optional fields may be appended only after all mandatory fields and only if bandwidth allows.
- Missing or corrupted mandatory data must be detected.
- Missing or corrupted mandatory fields result in no telemetry points.
- The hard minimum rate is 1 packet per second.
- Higher rates are allowed only if stable and interference-free.
- Telemetry begins automatically at power-on.
- Transmission continues during ground-floor startup, lift/ascent, descent, and after landing.
- Transmission continues for at least 5 seconds after impact.
- The actual team identifier must be present in every packet.
- The official launch sync word is `0xA5`; the pre-launch testing sync word is `0xF3`.

### Counter and Invalid Data Policy

A malformed packet must never be transmitted as a valid telemetry packet. The exact policy for assigning a packet number to a sensor-invalid packet candidate is open because the rulebook requires both sequential numbering and rejection of missing mandatory data. The implementation must choose and test one policy that preserves sequential numbering for transmitted packets and does not create false telemetry points.

## Timestamp Design

### Candidate Sources

| Source | Startup behavior | Accuracy/availability | GPS-lock dependency | Complexity | Assessment |
|---|---|---|---|---|---|
| Pico monotonic timer | Available immediately after Pico startup | Stable for elapsed mission time; absolute time accuracy and long-term drift are TBD | None | Low | Strong primary candidate |
| GPS time | Available only after valid GPS data and fix/configuration behavior | Potentially tied to GPS time; startup availability is not guaranteed | Yes or potentially dependent on valid GPS data | Higher | Useful optional/reference time, not primary startup clock |
| Another external source | Not present in confirmed BOM | Unknown | Unknown | Unknown | No source selected |

### Recommendation

Use the **Pico monotonic timer** as the primary `Ti` source for v0.1 because it is available immediately at power-on and does not delay telemetry while waiting for GPS lock. The timestamp should represent elapsed mission time from a documented startup epoch, with the display format `HH:MM:SS:MS`.

GPS time may be logged internally or appended as an optional field after GPS integration, but it must not be required for mandatory telemetry startup. Timer resolution, rollover, epoch definition, and behavior across reset remain implementation details to document before firmware freeze.

## Sensor Sources

| Required field/group | Source | Processing | Status |
|---|---|---|---|
| Altitude | BMP280 | Project-defined pressure-to-altitude calculation and ground baseline | Required; algorithm and calibration TBD |
| Pressure | BMP280 | Read and convert to Pa at required precision | Required; driver and validity rules TBD |
| Temperature | BMP280 | Read and convert to degrees C at required precision | Required; integration TBD |
| Roll | MPU-9250 | Orientation processing using project frame | Required; algorithm TBD |
| Pitch | MPU-9250 | Orientation processing using project frame | Required; algorithm TBD |
| Yaw | MPU-9250-derived orientation | Method and absolute-reference validity are open | Required; high-risk engineering decision |
| AX/AY/AZ | MPU-9250 | Axis mapping and unit conversion to m/s2 | Required; calibration and frame TBD |
| GPS extensions | NEO-6M | Parse position/time when valid | Optional; no scoring claim until working |

## Orientation Definitions

The project will define a right-handed body reference frame fixed to the CanSat structure:

- **X axis:** forward direction of the CanSat body, exact physical direction TBD.
- **Y axis:** lateral direction completing the project body frame, exact physical direction TBD.
- **Z axis:** vertical/body direction completing the project body frame, exact physical direction TBD.
- **Roll:** rotation about the project X axis.
- **Pitch:** rotation about the project Y axis.
- **Yaw:** rotation about the project Z axis.

The exact sign convention, zero orientation, angle wrapping, and relationship between body axes and MPU-9250 axes must be documented with the mechanical mounting orientation before firmware implementation.

The telemetry protocol does not prescribe a sensor-fusion algorithm. Roll, pitch, and yaw
are derived values, not direct raw MPU-9250 fields.

An absolute magnetic yaw needs a magnetometer, and a calibration of it for the assembled
airframe. Because the mandatory `Ya-` field is a single number that looks identical either
way, the vehicle appends an optional tag saying which quantity it is transmitting:

| Tag | Meaning |
|---|---|
| `YR-M` | Yaw is referenced to magnetic north through a calibrated magnetometer: an absolute angle. |
| `YR-G` | Yaw is a free-running gyro integration whose zero is wherever the vehicle was pointing at reset: a relative angle. |
| *(absent)* | The transmitter did not say. Treat the yaw as relative. |

`YR-` is an optional field like any other: it follows every mandatory field, and a
conforming parser that does not know it ignores it. It is four characters plus the
separator, which is what the airtime budget could afford.

> [!IMPORTANT]
> **On the delivered hardware this vehicle transmits `YR-G` and nothing else.** The IMU is
> an MPU-6500 — six axes, no magnetometer ([F-1](../hardware/receiving-inspection.md#findings)).
> `YR-M` remains part of the protocol, is implemented in the firmware and exercised by
> `test_magnetic_yaw_is_tilt_compensated`, and would be emitted by a vehicle carrying a real
> MPU-9250. The protocol is written for both; this airframe can only produce one of them.

Whether a relative yaw satisfies the mandatory field is a question for the organizers, not
one this document can answer. What the protocol guarantees is that the receiver is never
left guessing which of the two it has.

## Data Validation

Validation has three distinct failure classes.

### Sensor-Invalid

A measurement is sensor-invalid when the source is unavailable, has not initialized, is stale, reports a documented device error, cannot be converted to the required unit, or fails a documented physical plausibility check. The project must not invent numeric limits solely for this protocol.

Examples:

- Sensor initialization failure
- Sensor disconnected or communication error
- GPS unavailable when an optional GPS field is requested
- Stale sample beyond the later-defined freshness policy
- NaN, infinity, malformed numeric value, or failed conversion
- Orientation unavailable because required inputs are invalid

Sensor-invalid mandatory data must not be formatted into a valid mandatory packet.

### Packet-Invalid

A packet is packet-invalid when the serialized message is malformed, including:

- Missing mandatory field
- Wrong prefix or field order
- Wrong separator or precision
- Missing team identifier
- Invalid packet number
- Malformed timestamp
- Corrupted bytes or failed receive integrity check
- Mandatory data replaced with an unapproved placeholder

A packet-invalid message produces no telemetry points.

### Communication Failure

Communication failure is a transport condition, including no packet received, packet loss, radio link loss, wrong sync word, or a packet that cannot be recovered from transport corruption. The ground station must record the missing sequence or link condition separately from sensor-invalid and packet-invalid data.

The protocol does not silently convert communication failure into a sensor value.

## Packet Generation Contract

The future flight firmware shall implement the following process, without this document implementing it:

1. Acquire the current sensor samples and their validity state.
2. Produce derived altitude and orientation values using documented project algorithms.
3. Select the timestamp from the Pico monotonic timer.
4. Confirm all mandatory values are valid.
5. Assign the next transmitted packet number, beginning with `P-001`.
6. Format mandatory fields in the exact rulebook order and precision.
7. Append optional fields only after mandatory fields and only when the packet budget allows.
8. Apply the selected line-termination policy.
9. Transmit using the active launch or test sync-word configuration.
10. Record the same logical record for onboard logging, subject to the SD representation decision.

Required formatting:

- Field separator: semicolon followed by one space, as shown by the rulebook.
- Mandatory field order: exactly the twelve fields listed above.
- Decimal precision: exactly as listed in the mandatory-field table.
- Trailing separator: the rulebook example ends with a semicolon.
- Line termination: **TBD** because the supplied rulebook format does not specify a line terminator. The implementation must use one documented choice consistently and ensure it does not alter the required field format.
- Character encoding: **TBD**; confirm the official receiver expectation before implementation.

## Optional Sensor Extensions

Optional data may be appended after the complete mandatory packet using short prefixes consistent with the rulebook. The currently planned examples are:

```text
GP-Lat-...; GP-Lon-...; GP-Alt-...;
```

The exact value formatting for optional fields is TBD because the supplied rulebook extract gives prefixes but not complete precision, units, or field syntax for each optional value.

Rules:

- Optional fields never replace mandatory fields.
- Optional fields never move ahead of mandatory fields.
- Optional fields are omitted when they would threaten the 1 Hz minimum, packet stability, or mandatory data priority.
- GPS remains an additional sensor; no scoring result is claimed until it works.
- Do not add optional fields without a documented consumer and bandwidth assessment.
- **Not every sensor belongs in the packet.** The onboard log is a wider record than the link, and an additional sensor whose questions are all post-flight belongs in the first and not the second. See the acoustic level below.

### Decided: the acoustic level is logged and not transmitted

The vehicle carries an analogue microphone on `GP27` as an additional sensor. **Its level
is written to the SD log and is never put in a packet.** That is a decision, taken against
this document's own rule that an optional field needs a documented consumer and a bandwidth
assessment, and both are recorded here.

**The rulebook does not require it.** `TEL-022` says optional sensor data *may* be appended
after mandatory data and must not displace it. May, not must. Nothing in the mandatory
packet format has a place for an acoustic level, and the scoring for additional sensors
rewards integrating the sensor, not transmitting it.

**The bandwidth assessment.** A `SND-xxx.x` field is about nine bytes against a 255-byte
budget whose measured worst case is 206. It is affordable. But affordable is not free: those
nine bytes are airtime, the packet already sheds optional fields when it grows, and buying
them for a field no rule asks for and no ground display reads is a poor trade.

**The consumer is the analysis, not the link.** Every question this sensor exists to answer
is a post-flight one — when did the canopy inflate, when did it land, how did the acoustic
level track descent rate. All of them are asked of the log, at leisure, against the altitude
and acceleration columns beside them. None of them needs an answer during the flight, and an
operator watching a live link can do nothing with a millivolt figure.

**What that costs.** The link is the only telemetry that survives a lost vehicle. If the
CanSat is never recovered the acoustic record is gone, where a transmitted field would have
been on the ground already. That is accepted: this is an additional sensor, and the mandatory
data — which does go over the link — is what a lost vehicle must still have delivered.

Reversing the decision is a one-line change in the controller, and it must come with a
bandwidth re-check and a ground-station consumer, exactly as this document requires of any
new field.

### Open: the pack voltage is measured and not transmitted

The vehicle samples its battery through the divider on `GP26`, keeps the result in its
health snapshot, and raises a fault when it falls below `battery_low_voltage`. **The
voltage itself never leaves the vehicle.** The transmitted optional fields are `MODE`,
`FAULTS`, `CAL`, `ARM` and `YR`, and nothing else, so an operator watching the link sees a
fault count increment and cannot see how much margin is left before it does.

The ground station used to imply otherwise: its dashboard carried a **Battery (V)** row
reading a `battery` key out of the *bridge's* status dictionary — a key nothing has ever
written, from a Pico with no battery sense. It could only ever display `n/a`, which reads
as a link that is not reporting rather than a quantity that is not sent. The row is gone.

Adding `BAT-x.xx` would cost roughly nine bytes of a 255-byte budget whose measured
worst case is 206, and the controller already drops optional fields before overrunning it.
That makes it affordable, not decided: it is a change to the packet contract, and this
document requires a documented consumer and a bandwidth assessment before one is made.
**The decision is open, and it is the team's, not the firmware's.**

## Radio Configuration

### Required Sync Words

| Mode | Sync word | Use |
|---|---|---|
| Official launch | `0xA5` | Required competition configuration |
| Pre-launch testing | `0xF3` | Required test configuration |

Wrong sync-word use during another team's launch is penalised, and **the 2026 revision made
the penalty five times harsher: -1 point per 2 stray packets received, where the earlier
rulebook said per 10.** At this vehicle's 1 Hz rate that is half a point per second of
accidental transmission, so a CanSat left powered on through another team's launch can
burn more points than the entire telemetry section is worth in under a minute.

The operating procedure must therefore ensure that CanSats remain powered off during
another team's launch. This is a switch discipline problem, not a firmware one: the
firmware transmits automatically on power-up by rulebook requirement, so the only
control is the manual switch.

### Parameters Still TBD

The following parameters are not specified in the supplied rulebook extract and must not be invented here:

- Spreading factor
- Bandwidth
- Coding rate
- Frequency configuration
- Transmit power
- Preamble
- CRC
- Retry behavior
- Acknowledgement behavior
- Payload encoding
- Character encoding
- Maximum payload policy

These parameters require competition confirmation, exact RA-02 documentation, and engineering testing.

Because the rulebook does not fix them, the project selects them itself, in one place, and
documents the reasoning: [`cansat/link_profile.hpp`](../../firmware/common/include/cansat/link_profile.hpp)
holds the single definition both the flight computer and the ground-station bridge read, and
[link-budget.md](link-budget.md) shows the airtime arithmetic behind each value. Current
selection: 433 MHz, SF7, 125 kHz, CR 4/5, 8-symbol preamble, CRC on, 17 dBm. All remain
provisional until confirmed against the physical RA-02 and any organiser guidance.

## Packet-Rate Design

The hard requirement is at least 1 packet per second. The achievable rate is set by LoRa
airtime, which is computed in [link-budget.md](link-budget.md) rather than chosen. For the
real ~190-byte packet on the selected SF7 / 125 kHz modem, one transmission occupies
**400 ms** of channel time in the worst case, and about 327 ms for a typical
in-flight packet.

| Candidate | Airtime cost at SF7/125 kHz | Assessment | Decision |
|---:|---|---|---|
| 1 Hz | ~32 % channel occupancy | Meets the rulebook minimum with room for radio recovery and retries | **Selected default** |
| 2 Hz | ~64 % at 125 kHz; ~32 % at 250 kHz | Only viable by widening the bandwidth (costs ~3 dB sensitivity) or shortening the packet | Upgrade path, after a hardware range test |
| 5 Hz | Exceeds the channel at any 125/250 kHz setting | Not achievable with this packet | Rejected |
| 10 Hz and above | Exceeds the channel by a wide margin | Not achievable with this packet | Rejected |

**Selected rate:** 1 packet per second. This is the rulebook minimum and, at the chosen
modem settings, the fastest rate the radio sustains with meaningful margin. The firmware
enforces the choice: `validate_config()` recomputes the packet airtime at startup and
refuses to run with a telemetry period the radio cannot deliver, so an over-optimistic
setting fails on the pad instead of silently under-transmitting in flight.

A higher rate is not automatically better. Stability, low packet loss, mandatory-field
priority, interference behavior, power consumption, and official-ground-station
compatibility take precedence.

## Ground Station Contract

For each received packet, the future ground-station system shall:

1. Receive the radio frame.
2. Apply the active sync-word configuration.
3. Validate transport integrity when the final radio configuration provides it.
4. Identify and check the team identifier.
5. Parse and validate the packet number.
6. Detect missing, duplicate, or out-of-order packets.
7. Parse all mandatory fields in their required order.
8. Parse optional fields only after mandatory fields.
9. Record receive time and packet time separately if both are available.
10. Log valid data and explicit invalid/lost-packet status.
11. Display live mission telemetry and connection status.
12. Export data for the mandatory post-launch analysis.

This is a software contract only. No ground-station implementation is created here.

## Common Data Model

The flight firmware, radio packet, onboard SD log, ground-station log, and analysis tools should use one canonical logical record:

| Data group | Canonical content |
|---|---|
| Identity | Team identifier, protocol version, packet number |
| Time | Mission timestamp; optional receive timestamp at ground station |
| Mandatory measurements | Altitude, pressure, temperature, roll, pitch, yaw, AX, AY, AZ |
| Optional measurements | GPS latitude, longitude, altitude, and other approved extensions |
| Quality state | Per-field validity and packet validity, stored internally/logically; serialization TBD |
| Communication state | Missing, duplicate, out-of-order, corrupted, or received status |

The mandatory radio string remains exactly the rulebook format. Quality and communication metadata should be represented in logs and analysis records without adding unapproved mandatory packet fields. The exact SD serialization is open.

## Data Logging

The common logical record should be used for:

- Mandatory radio packets
- Ground-station logs
- Onboard SD logs
- Post-flight analysis input

The following representation decisions remain open:

- SD file format
- Column names and ordering
- Whether invalid sensor samples are logged as records or as separate fault events
- How missing radio packets are represented at the ground station
- Whether optional GPS values use separate columns or serialized extension fields
- Flush and recovery behavior after reset or impact

The SD logger must not create a representation that loses packet number, timestamp, mandatory measurements, or validity state.

## Test Requirements

Each test record must contain a requirement reference, method, expected result, and evidence location.

| Test | Requirement | Method | Expected result | Evidence |
|---|---|---|---|---|
| Packet formatting | Exact mandatory packet format | Generate known records and compare byte/character fields | Correct order, prefixes, separators, precision, and trailing semicolon | Test vectors and comparison output |
| P-001 initialization | Packet numbering starts at `P-001` | Cold-start and inspect first valid transmitted packet | First valid packet is `P-001` | Packet log |
| Sequential numbering | Numbers increment sequentially | Analyze long packet sequence | No unexpected gaps, duplicates, or resets | Packet analysis report |
| 1 Hz minimum | At least 1 packet/s | Measure timestamps and receive intervals under representative load | Rate never violates the documented test acceptance condition | Rate and loss log |
| Higher-rate stability | **1.43 Hz default** (700 ms, 199-byte packet, GPS logged rather than transmitted); 1.18 Hz with GPS on the air; higher still only over a 250 kHz modem, which costs 3 dB of sensitivity | Range-test each profile and compare measured loss | Chosen rate is stable, low-loss, and compatible with power/airtime | Comparison report |
| Corrupted packets | Mandatory corruption is detected | Inject malformed fields and corrupted frames | No telemetry point is produced; fault is classified | Parser test results |
| Missing packets | Loss is detected | Drop packets in a controlled stream | Missing sequence is recorded separately from sensor faults | Ground-station log |
| Invalid sensor fields | Sensor-invalid data is rejected | Simulate unavailable, stale, NaN, and initialization-failure states | No malformed mandatory packet is accepted | Fault-injection results |
| Sync-word modes | `0xA5` launch and `0xF3` test modes | Exercise both configurations | Correct mode receives; wrong launch mode is not used | Radio test log |
| Automatic startup | Telemetry starts at power-on | Power-cycle with no manual trigger | Telemetry begins automatically | Startup capture |
| Post-impact transmission | At least 5 seconds after impact | Simulate or test impact and log packets | Valid telemetry continues for at least 5 seconds | Timestamped packet log/video |
| Ground-station compatibility | Parser handles required packets | Replay generated and recorded packets | Mandatory fields parse identically | Parser test output |
| Data-model consistency | Common record is preserved | Compare SD, radio, ground log, and analysis import | No mandatory value or status is silently lost | Cross-format comparison |

## Open Decisions

- Team number for `CAN-Team-XX`
- Whether the Pico timer timestamp is accepted as the final mission timestamp and the exact epoch/rollover policy
- Roll, pitch, and yaw sign convention and body-frame mounting definition
- Yaw method and whether the current MPU-9250-only hardware provides an acceptable field
- Final packet rate; 1 Hz is the selected default and the airtime-supported choice
- Spreading factor, bandwidth, coding rate, frequency, transmit power, preamble, CRC, and retry behavior
- Packet counter policy when sensor data is invalid
- Line termination and character encoding
- SD logging representation and validity-event format
- Optional GPS field precision and syntax

## Implementation Dependencies

Future flight firmware will need to implement:

- Sensor acquisition and initialization
- Sensor freshness and validity state
- BMP280 pressure, temperature, and altitude processing
- MPU-9250 axis conversion and orientation processing
- Timestamp generation from the selected source
- Team-ID configuration
- Packet counter and startup behavior
- Exact mandatory formatter
- Optional-field priority and bandwidth policy
- 1 Hz minimum and target-rate scheduler
- Launch/test sync-word configuration
- Radio transmission and post-impact behavior
- SD logging using the common data model
- Reset, brownout, and communication-failure handling

Future ground-station software will need to implement:

- Radio reception and frame validation
- Team identification
- Packet parsing and mandatory-field validation
- Sequence tracking and loss classification
- Optional-field parsing
- Receive logging and common-record export
- Live display of telemetry and faults
- Data analysis input and required graph generation

No implementation is claimed by this document.

## Protocol Consistency Review

- **Internally consistent:** Yes, subject to the explicitly open packet-counter policy for invalid sensor data and the final line-termination/encoding decisions.
- **All mandatory fields covered:** Yes. All twelve rulebook fields are specified, including units, precision, sources, and invalid-data behavior.
- **Formatting rules covered:** Yes for field order, prefixes, separators, precision, trailing semicolon, numbering, and priority. Line termination and encoding remain open because they are not supplied by the rulebook extract.
- **Decisions still open:** Team number, timestamp epoch, orientation convention, yaw method, packet rate, radio parameters, invalid-data counter policy, optional-field syntax, and SD representation.
- **Implementable once hardware arrives:** Sensor drivers, timestamping, packet formatting, parser test vectors, rate scheduler, validity model, radio mode handling, SD/common-record implementation, and ground-station parser can be implemented once the hardware interfaces and electrical constraints are available.
