# Ground-Station Pico Bridge

Physical LoRa bridge: `RA-02 -> ground-station Pico -> USB serial -> PC`.

- `include/ground/radio_bridge.hpp` - radio-side boundary contract.
- `include/ground/framing.hpp`, `src/framing.cpp` - USB-serial framing shared with the
  PC application: `'$' <len> ',' <crc16-ccitt-hex> ',' <payload> '\n'`. Host-buildable and
  unit-tested (`tests/framing_test.cpp`); the Python side (`ground-station/software/src/transport.py`)
  mirrors it byte for byte.
- `src/pico/main.cpp` - the bridge firmware: brings up the SX1278 in continuous RX,
  frames every received payload and every `#status` line to USB, and re-initialises the
  radio after repeated failures. Reuses `firmware/common/src/sx1278.cpp`. Sync word
  `0xA5` (official), for testing as well as the launch: the organizers' ground station listens
  on nothing else, and the vehicle flies it too.

The Pico SDK is required only for the `cansat_ground_bridge_firmware` target. The framing
library and its tests build with the host toolchain (`tools/build_host.sh`).
