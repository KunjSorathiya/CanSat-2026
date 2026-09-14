# Post-flight analysis

The rulebook allows **four hours** after the launch to analyse the data. This directory is that
analysis, written and tested before there is a flight, so that on the day it only has to be
pointed at the real logs.

| File | What it is |
|---|---|
| [`flight_analysis.ipynb`](flight_analysis.ipynb) | **The notebook.** Configuration, data quality, phases, the three mandatory graphs, descent rate and drag coefficient, dynamics, sound, GPS, the SD-against-ground comparison, and export — one section per cell |
| [`flight_analysis.py`](flight_analysis.py) | Everything the notebook calls, and a command line that does the whole analysis in one step. numpy and matplotlib only |
| [`synthetic_flight.py`](synthetic_flight.py) | Generates the **synthetic** flight in [`test-data/synthetic-flight/`](../test-data/synthetic-flight/README.md) that everything here is tested against |
| [`requirements.txt`](requirements.txt) | numpy and matplotlib, pinned. `pip install -r analysis/requirements.txt` |
| [`tests/`](tests/) | 37 tests: the analysis recovers the synthetic flight's known answers, matches the firmware's columns and thresholds, and the notebook runs cell by cell |

---

## On launch day

**1 · Copy everything off first, and work on copies.**

```bash
python tools/read_flight_log.py FLIGHT.CSV --out-dir analysis-input
```

That extracts the SD card's log, splits it by power cycle and stops exactly where the data
stops. Copy the ground station's `logs/telemetry.csv` beside it.

**2 · Run it.** Either open the notebook, change the two paths and the mass in its
**Configuration** cell and *Run All* — or, with no Jupyter at all:

```bash
python analysis/flight_analysis.py analysis-input/flight-1.csv --compare logs/telemetry.csv --mass 0.50 --out analysis-output
```

`--mass` is the flight mass in kg and is only needed for the drag coefficient. **The submitted
mass was not recorded — weigh the vehicle.**

**3 · Read `analysis-output/summary.md` before any graph.** Its first table says whether the log
is what you think it is: how many packets, how many missing, whether it holds more than one
power cycle.

### What it writes

| File | Contents |
|---|---|
| `01-altitude.png`, `02-temperature.png`, `03-pressure.png` | **The three mandatory graphs**, against mission time with packet number on the top axis, cropped to the flight |
| `00-altitude-full-session.png` | Altitude over the whole power cycle, command window included |
| `04-descent.png` | Height and vertical rate through the descent, the regression line, the 5 m/s cap and the model |
| `05-acceleration.png`, `06-orientation.png` | Acceleration components and magnitude; roll, pitch and relative yaw |
| `07-sound.png` | Acoustic level against time, and against descent speed² |
| `08-gps-track.png` | Ground track coloured by time, with the drift |
| `09-correlations.png` | Pressure and temperature against altitude, against the standard atmosphere |
| `summary.md` | Every number, in tables ready for the report |
| `analysis.json` | The same numbers, machine-readable |

**Opening the notebook** needs Jupyter, which is not a project dependency: `pip install notebook`,
then `jupyter notebook analysis/flight_analysis.ipynb`. The notebook is committed without outputs.

---

## What it measures, and how

| Quantity | Method |
|---|---|
| **Launch, apex, release, landing** | From barometric altitude, using the firmware's own thresholds — a 15 m climb for launch, a descent faster than 2 m/s held for 1 s for release. Release is back-calculated from the first free-fall drop (`d = g t² / 2`); landing is where the steady-descent line meets the ground. The vehicle's own declared `FLIGHT` and `LANDED` are reported beside them, because neither is the physical event |
| **Descent rate** | Least-squares slope of height over the steady part of the fall — 1.5 s after release to 0.5 s before landing — with its standard error |
| **Drag coefficient** | `Cd = 2 m g / (ρ S v²)`, with ρ from the pressure and temperature the vehicle measured during the descent and S from the canopy diameter. The first measurement of the number the parachute was sized against |
| **Spin and pendulum** | Spin from the slope of unwrapped yaw; pendulum frequency from the spectrum of roll and pitch, reported with its resolution and Nyquist limit |
| **Radio loss** | Packet numbers in the SD log that are missing from the ground log — separated into during the flight and during the descent |
| **GPS drift** | Median fix in the 10 s before release against the median after landing |

### The altitude correction

**The descent rate is taken from temperature-corrected height, not from the vehicle's altitude.**

The firmware converts pressure to altitude with the ISA formula,
`44330 · (1 − (p / p₀)^(1/5.255))` (`sensors::pressure_altitude_m()`), which assumes the standard
atmosphere's temperature. Real height per pascal scales with the real absolute temperature, so
on a hot launch day the vehicle's altitude reads low by the ratio of 288 K to that temperature —
**about 5 % at 31 °C** — and a descent rate taken from it reads 5 % low too. That is the difference
between a 5.05 m/s descent and a compliant-looking 4.8.

The sealed flight image cannot be changed, so the analysis corrects for it with the hypsometric
equation, using the pad pressure and the measured temperature. It reports both rates, and the
scale factor it found against the one theory predicts. The synthetic flight is generated with
exactly this bias — pressure from the hypsometric equation, altitude through the firmware's
formula — and the tests hold the correction to it.

The BMP280 reads the board's temperature, which self-heating puts a little above the air's; a
3 K error in it moves the corrected height by about 1 %.

---

## What it cannot see

- **Everything is sampled once per packet** — about 3.1 Hz in flight. The onboard log appends a row
  in `Controller::emit_telemetry()`, not at the 30 Hz the sensors run at. The canopy snatch and the
  landing impact last tens of milliseconds, so **every peak acceleration is a lower bound**, and
  short acoustic transients are usually missed.
- **Pendulum modes above about 1.5 Hz alias.** The summary prints the Nyquist limit beside the
  frequency it found.
- **Yaw is relative.** The IMU has no magnetometer. Its *rate* — the spin — is real; its value is not
  a heading.
- **A temperature lapse rate over 30 m is not measurable** with this sensor: the standard atmosphere
  cools 0.2 K over that height, and the reading resolves 0.1 K with self-heating on top.
- **The sound level is relative millivolts**, not a sound pressure level. At terminal velocity the
  speed barely varies, so a weak correlation with speed is expected.

---

## Before a number goes in the report

- [ ] The input paths point at the real logs, not `test-data/synthetic-flight/`.
- [ ] The data-quality table was read, and any multi-session note is understood.
- [ ] Release and landing look right on `04-descent.png`.
- [ ] Acceleration peaks are called lower bounds; yaw is called relative.
- [ ] If the mass was not weighed, the drag coefficient says so.

Related: [runbook — post-flight analysis](../documentation/operations/runbook.md#post-flight-analysis) ·
[`tools/read_flight_log.py`](../tools/read_flight_log.py) ·
[`simulations/descent.py`](../simulations/descent.py)
