# Ground Station Software (PC)

Standard-library-only core. Optional extras (`pyserial`, `matplotlib`) enable the live
serial link and the dashboard plots.

## Modules

| Module | Responsibility |
|---|---|
| `telemetry.py` | packet parser + canonical `TelemetryRecord` (mandatory fields, precision, optional GPS / `MODE` / `FAULTS` tags) |
| `validator.py` | `StreamValidator`: team check, missing / duplicate / out-of-order, timestamp monotonicity, GPS sanity |
| `transport.py` | `FrameDecoder` (mirrors the Pico framing) + `SerialTransport` / `FileReplayTransport` / `LoopbackTransport` |
| `health.py` | `LinkHealth`: rate, loss %, CRC errors, staleness, connection state |
| `logger.py` | raw `.tsv` (nothing discarded) + parsed `.csv` with validation status; `export_csv` |
| `app.py` | `GroundStation`: background thread wiring transport -> parse -> validate -> log -> health; thread-safe `snapshot()` + event queue |
| `dashboard.py` | Tk UI; drains the event queue on `after()`, never blocks; live altitude/pressure/temperature plots when matplotlib is present |
| `main.py` | `replay` (offline) and `live` (serial or paced file, with/without dashboard) |

## Usage

```
# Offline: parse, validate and log a packet file, then export a CSV
python src/main.py replay packets.txt --team CAN-Team-01 --output logs --export logs/flight.csv

# Live from the ground-station Pico (needs pyserial)
python src/main.py live --port COM5 --team CAN-Team-25 --framed

# Live demo from a paced file (no hardware)
python src/main.py live --replay packets.txt --rate 2 --team CAN-Team-01

# Tests
python -m unittest discover -s tests -p "test_*.py"
```
