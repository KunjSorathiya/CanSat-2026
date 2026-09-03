# Ground Station Pico Firmware Boundary

The ground-station Pico owns the RA-02 radio-side boundary. `include/ground/radio_bridge.hpp` defines the minimal receive interface expected by the PC ground-station application.

The exact Pico SDK adapter is intentionally deferred until the RA-02 carrier pinout and electrical behavior are verified. The provisional onboard radio resources remain the project reference; no new GPIO assignments are introduced here.
