# Flight Computer Firmware

## Layout

```
include/flight/            hardware-independent flight core (host + Pico)
  config.hpp               centralised configuration + provisional thresholds
  interfaces.hpp           Imu / Barometer / Gps / Radio / SdLogger / BoardIo
  controller.hpp           flight loop orchestrator
  state_machine.hpp        INIT -> SELF_TEST -> READY -> FLIGHT -> LANDED -> RECOVERY (+ FAULT)
  scheduler.hpp            non-blocking periodic-task timers
  orientation.hpp          nine-axis Mahony quaternion attitude (accel + gyro + magnetometer);
                           runs six-axis on the delivered MPU-6500, which has no magnetometer
  sensor_math.hpp          MPU-9250 and AK8963 scaling, BMP280 Bosch compensation, pressure altitude
  telemetry_builder.hpp    canonical record + rulebook packet string
  fault_manager.hpp        fixed-size fault store, no allocation
  raw_block_log.hpp        append-only 512-byte-block log (no filesystem)
  health.hpp               health snapshot
  pico/                    Pico HAL: mpu9250, bmp280, neo6m, sd_card, sx1278 glue
src/                       core sources; src/pico/ is Pico-only
tests/                     host mocks + smoke test + comprehensive suite
```

The flight core has no hardware dependency. The concrete Pico HAL (`src/pico/*`,
`firmware/common/src/sx1278.cpp`) is compiled only into `cansat_pico_firmware` when the
Pico SDK is present. Nothing here uses Arduino or a large framework.

## GPIO map

See `documentation/hardware/pico-gpio-map.md` (`BoardPins` in `config.hpp` mirrors it).

## Building & testing on the host (no Pico SDK required)

```
bash tools/build_host.sh        # compiles + runs flight_smoke_test, flight_tests, framing test, python tests
bash tools/check_pico_syntax.sh # -fsyntax-only of the PICO_BUILD branches against minimal SDK stubs
```

## Building the Pico image

Requires the standard Raspberry Pi Pico SDK setup (`PICO_SDK_PATH`, `pico_sdk_import.cmake`,
`pico_sdk_init()` in a top-level CMake). Then the `cansat_pico_firmware` target builds a
UF2. Hardware has not been integrated; treat the image as unverified until bench-tested.

## Startup calibration & safeguards

On the pad (`INIT`/`SELF_TEST`/`READY`), `StartupCalibrator` collects stationary IMU +
barometer samples and, once a stillness gate passes (per-axis gyro std-dev and
|accel| ≈ 1 g), captures:

- **gyro bias** — subtracted from every rate before orientation integration (kills yaw
  drift);
- **accelerometer offset** — residual after removing gravity, subtracted from AX/AY/AZ;
- **barometric ground reference** — the `A-` field then reads AGL, ~0 on the pad.

If the vehicle is not still, calibration retries until `calib_timeout_ms`, then resolves
best-effort (gyro/accel bias **not** applied, baro reference still used) and raises a
`calibration` warning fault. It never blocks the mission.

Safeguards in the flight loop:

- **Launch lockout** — `READY → FLIGHT` is refused until `arming_delay_ms` has elapsed
  and calibration has settled (`require_calibration_to_arm`), plus the launch condition
  sustained for `launch_confirm_ms`. A startup glitch cannot trigger a false launch.
- **Sensor plausibility** — readings outside datasheet-derived bounds (`baro_min/max_pa`,
  `accel_clip_mps2`, `gyro_clip_dps`) are rejected, the prior value is dropped, and a
  `sensor_implausible` fault is raised; a persistently bad sensor suppresses telemetry
  rather than transmitting a wrong value.
- **Watchdogs** — 2 s on the flight computer, 3 s on the ground bridge; a watchdog reboot
  is flagged (`watchdog_reboot` fault) and telemetry restarts automatically.
- **Graceful degradation** — one dead sensor / GPS / SD / radio never stops the loop;
  telemetry continues in every state including `FAULT`; radio TX failure uses a bounded
  re-init + back-off; SD disables itself after repeated write failures.
- Diagnostic tags `MODE-<state>`, `FAULTS-<n>`, `CAL-<0|1>`, `ARM-<0|1>` are appended
  after the mandatory (and GPS) fields when `append_diagnostic_fields` is set.

## Provisional / hardware-dependent values

`config.hpp` marks every value the rulebook or hardware has not fixed: launch/landing
thresholds, LoRa RF parameters (only the sync words 0xF3/0xA5 are fixed), battery divider
ratio (0 = report raw pin voltage), barometric reference pressure.
