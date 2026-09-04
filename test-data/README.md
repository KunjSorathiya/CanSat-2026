# test-data

Files here are read by code, not by people, and each one is read by more than one
implementation on purpose.

| File | Read by | Purpose |
|---|---|---|
| [`protocol-fixtures.tsv`](protocol-fixtures.tsv) | the C++, Python and JavaScript **parsers** | Every packet with a recorded accept/reject verdict. A parser that disagrees fails the build |
| [`validator-scenarios.tsv`](validator-scenarios.tsv) | the Python and JavaScript **validators** | Packet sequences with the verdict the validator must reach: gaps, duplicates, reboots, wrong team, clock regressions, implausible fixes |
| [`raw-log-escapes.tsv`](raw-log-escapes.tsv) | the Python **logger** and the JavaScript **console** | Escaped/plain pairs for the raw log's four escape rules, plain text stored as hex because it contains tabs and newlines |
| [`sample-mission.txt`](sample-mission.txt) | anyone following the documentation | A replayable mission, so every documented `replay` command runs against a file that exists |

## `sample-mission.txt`

Thirty packets, and not hand-written: they are the output of `build/host/emit_mission`,
which runs the **real flight controller** through a scripted ascent and descent. So the file
carries what the vehicle actually transmits — the mode advancing `READY` to `FLIGHT`, the
arming and calibration flags changing, GPS and diagnostic tags present, and `YR-G` on every
packet because the delivered IMU has no magnetometer
([F-1](../documentation/hardware/receiving-inspection.md#findings)).

```bash
cd ground-station/software
python src/main.py replay ../../test-data/sample-mission.txt --team CAN-Team-01 --export logs/flight.csv
```

To regenerate it after a change to the controller or the packet format:

```bash
bash tools/build_host.sh && ./build/host/emit_mission > test-data/sample-mission.txt
```

Every line is held to the parser by `check_doc_claims.py`, so a format change that leaves
this file behind fails the build rather than shipping a sample the documentation cannot
replay.
