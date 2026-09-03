# Ground Station Software

The PC ground-station prototype uses Python's standard library only. It provides a telemetry parser, mandatory-field validation, raw packet preservation, parsed CSV logging, missing-packet detection, replay CLI, and a simple Tk UI.

Replay packets from a file:

```text
python ground-station/software/src/main.py packets.txt --output logs --team CAN-Team-XX
```

Run software tests:

```text
python -m unittest discover -s ground-station/software/tests -p "test_*.py"
```

No hardware radio or serial dependency is assumed yet. The ground-station Pico radio boundary is defined in `firmware/ground-station/include/ground/radio_bridge.hpp`.
