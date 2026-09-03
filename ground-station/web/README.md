# Web Telemetry Console

`index.html` is a self-contained (single file, no build, no dependencies) browser console
for CanSat-2026 telemetry. Open it directly, or serve the folder.

## Sources

| Source | Use |
|---|---|
| **Demo** | replays a generated drone-lift mission (READY → FLIGHT → LANDED → RECOVERY) at 2 Hz; auto-starts, and deliberately injects one dropped packet and one duplicate so link health is exercised |
| **File…** | replays a packet file — plain newline packets, a `raw_packets.tsv` from the Python logger, or a framed `.bin` |
| **Web Serial** | connects to the ground-station bridge Pico over USB (Chrome/Edge, HTTPS or `localhost`), decoding the `$len,crc,payload` framing live |

## What it shows

- **Stat tiles** — altitude (m AGL), vertical speed, pressure, temperature and
  acceleration magnitude, each with a sparkline and a 5-second delta
- **Mission** — state, mission clock, state ladder, team, packet number, calibration,
  armed status, active faults, sync word
- **Link health** — rate, packets OK, missing, duplicates, CRC errors, loss percentage,
  time since last packet, and a rate sparkline
- **Flight view · 3D** — orbit / chase / top / profile views of the trajectory, with
  ascent and descent tracks and a filtered ground track
- **Flight profile** — altitude, vertical speed, pressure and temperature against time,
  with phase bands and apogee, touchdown and peak-g markers, plus a data table view
- **Attitude** — roll, pitch and a yaw heading card with spin rate
- **GPS** — fix status, position, satellites
- **Raw packet monitor** — every line received, accepted or rejected, with the reason

**Copy CSV** puts the accepted samples on the clipboard. **Pause** freezes the display
while packets keep arriving and buffering. The theme toggle switches light and dark.

## Ports

The parser, `StreamValidator`, `LinkHealth` and the CRC-16/CCITT framing are ports of
`ground-station/software/src/{telemetry,validator,health}.py` and
`firmware/ground-station/src/framing.cpp`, so the console accepts exactly what the Pico
bridge emits.

The packet rate uses the same 5-second sliding window as `health.py` — deliberately not an
EWMA of instantaneous intervals, because a duplicate or a buffered burst arriving in the
same millisecond would otherwise report thousands of Hz and take about ten packets to
settle.

> [!NOTE]
> These ports have **no automated tests**. If the packet format, the validation rules or
> the framing change, this file must be updated by hand and checked in a browser.

`index.legacy.html` is the previous single-panel console, kept for reference only.
