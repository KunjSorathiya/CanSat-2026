# Flight Computer Firmware

The flight core is implemented as a hardware-independent C++ library with interfaces for the IMU, barometer, GPS, RA-02 radio, SD logger, and board I/O. `tests/` contains deterministic mock hardware and a host smoke test.

The current code uses the provisional board configuration in `include/flight/config.hpp`:

- I2C0: GPIO4/GPIO5
- SPI0: GPIO18/GPIO19/GPIO16
- RA-02: GPIO17 CS, GPIO20 RESET, GPIO21 DIO0, GPIO22 DIO1
- GPS UART0: GPIO12/GPIO13
- MPU6050 INT: GPIO7
- Status LED: GPIO14
- Battery ADC: GPIO26
- SD CS: GPIO6

The Pico SDK adapter layer is intentionally not included yet because the SDK is not installed in this workspace and breakout-board behavior remains provisional. The interfaces isolate that work from the tested core.

Host build with MinGW:

```text
g++ -std=c++17 -Wall -Wextra -Wpedantic -Ifirmware/common/include -Ifirmware/flight-computer/include -Ifirmware/flight-computer/tests firmware/common/src/telemetry.cpp firmware/flight-computer/src/state_machine.cpp firmware/flight-computer/src/controller.cpp firmware/flight-computer/tests/flight_smoke_test.cpp -o build/flight_smoke_test
build/flight_smoke_test.exe
```
