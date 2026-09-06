# Hardware Photographs

Photographs of the delivered boards. These are evidence, not illustration: every board-level
fact promoted from `TBD` in [hardware.md](../hardware.md) to a real value should be traceable
to a photograph here.

## Index

Taken 2026-09-04, on delivery. Transcribed in
[receiving-inspection.md](../receiving-inspection.md), Part C.

| File | Subject | What it establishes |
|---|---|---|
| `894292-pico-front.jpg` · `-back.jpg` | Raspberry Pi Pico | `© 2020` silkscreen, `RP2-B2` marking, full pin legend, debug pads, **no headers fitted** |
| `1150780-ra02-front.jpg` · `-back.jpg` | SX1278 RA-02 | Shield markings, `J1`/`J2` header order, u.FL socket, `3.3V` supply pin, no regulator or translator |
| `1150780-ra02-antenna-mated.jpg` | RF chain assembled | **Antenna → SMA joint → pigtail → u.FL → module, mated with no adapter** |
| `1121334-antenna-front.jpg` | 433 MHz antenna | Female shell with knurled coupling nut. **Mating face not shot straight on** |
| `1674982-ipex-sma-cable.jpg` | IPEX-to-SMA pigtail | Male bulkhead shell, nut and star washer, u.FL plug. **Centre contact not resolvable** |
| `2846-mpu9250-front.jpg` · `-back.jpg` | IMU | `MPU-9250/6500` silkscreen, **die marked `MP92`**, 10-pin header, regulator fitted, five 10 kΩ pull-ups, printed axes |
| `835813-bmp280-front.jpg` · `-back.jpg` | Barometer | 6-pin `VCC GND SCL SDA CSB SDO`, no regulator, four 10 kΩ pull-ups. **`GY-BM ☐E/☐P 280` box unreadable** |
| `11782-neo6m-front.jpg` · `-back.jpg` | GPS | `GY-NEO6MV2`, `u-blox NEO-6M-0-001`, `24C32A` EEPROM, backup cell, patch antenna fitted, 4-pin `VCC RX TX GND` |
| `11566-sd-reader-front.jpg` · `-back.jpg` | microSD reader | **No regulator, no level shifter**, supply pin `3V3`, four 10 kΩ pull-ups, header `GND MISO CLK MOSI CS 3V3` |
| `1125094-lipo-front.jpg` · `-back.jpg` | 1S LiPo | **Pro-Range, not Orange.** `1 Cell 3.7V 25C`, 1500 mAh, JST-RCY and JST-XH leads, no charge parameters printed |
| `1031002-protoboard-front.jpg` · `-back.jpg` | Prototype PCB | `10*10CM 2.54MM`, single-sided, isolated pads, edge rails, `A`–`K`/`01`–`35` grid |
| `lm393-sound-front.jpg` | LM393 sound module | **Four-pin header `AO DO GND VCC`** — the fact that decides whether the analogue channel exists at all. `LM393` `49M` `BWQ64` SOIC, cermet gain trimpot, electret capsule with `+`/`−` pad marks, `PWR-LED` and `DO-LED` indicators, SMD resistors marked `102` and `201`. Transcribed in [receiving-inspection.md](../receiving-inspection.md) D.5 |
| `passives-2026-09-06.jpg` | Second-batch passives, as delivered | The capacitors (`10µF 50V`, `100µF 50V`, `100µF 25V`, ceramic `104` discs) and all three resistor groups in one frame. Quantities and layout; the values come from the three close-ups below |
| `capacitors-2026-09-06.jpg` | The capacitors | **`10µF 50V`, `100µF 50V`, `100µF 25V`** read off the sleeves, and one ceramic disc whose print is worn illegible. Quantities as delivered — see [D.7](../receiving-inspection.md#d7--the-capacitors) |
| `resistors-100k-5pct.jpg` | Resistor group 1 | **100 kΩ ±5 %.** Carbon film, beige body, four bands: brown‑black‑yellow‑gold |
| `resistors-33k-1pct.jpg` | Resistor group 2 | **33 kΩ ±1 %.** Metal film, blue body, five bands: orange‑orange‑black‑red‑brown |
| `resistors-1k-5pct.jpg` | Resistor group 3 | **1 kΩ ±5 %.** Mint body, four bands: brown‑black‑red‑gold |

Outstanding re-shoots, all macro:

- ~~The **resistor colour bands**.~~ Settled 2026-09-06 by the three close-ups above. **They should still be metered before fitting** — a photograph proves what is printed on a part, never what the part does.

- Both RF **mating faces**, straight on — the only way to settle SMA against RP-SMA.
- ~~The **BMP280 die**, to settle BMP280 against BME280.~~ Settled by register instead on 2026-09-05: chip ID `0xD0` returned `0x58` ([F-4](../receiving-inspection.md#findings)). A photograph is no longer needed for this.
- The **MPU-9250, NEO-6M and microSD regulator markings** — all SOT-23 parts whose text is
  below these photographs' resolution.

## Naming

```text
<sku>-<short-name>-<view>.jpg
```

Examples:

```text
894292-pico-front.jpg
1150780-ra02-header-labels.jpg
11566-sd-reader-back.jpg
11566-sd-reader-regulator.jpg
1121334-antenna-connector.jpg
1125094-lipo-label.jpg
```

Views: `front`, `back`, plus a descriptive word for close-ups — `regulator`,
`level-shifter`, `header-labels`, `connector`, `label`, `strap`.

## What makes a photograph useful

- Part markings legible at full zoom. If the regulator's marking cannot be read, the photo
  has recorded nothing.
- Even light, no flash glare across the silkscreen.
- Something for scale in at least one shot per board.
- Straight-on for connectors — the centre contact is what distinguishes SMA from RP-SMA.

## Related

- [Receiving Inspection Record](../receiving-inspection.md) — Part B lists the shots required
- [Hardware Reference](../hardware.md) — what these photographs are evidence for
