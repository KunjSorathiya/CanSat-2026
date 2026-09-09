#!/usr/bin/env python3
"""Emits the vehicle netlist as a machine-readable TSV, checked against the firmware.

    python tools/gen_netlist.py

The wiring exists in three places: `BoardPins` in the firmware, the wiring schedule
drawing, and whatever is actually soldered. The first two can be held together
mechanically, and this is what does it -- every GPIO net below carries the name of the
`BoardPins` constant it comes from, and the script refuses to write a netlist whose pin
numbers disagree with the header. The third is what the bring-up record is for.

A TSV rather than a schematic file because there is no schematic: the vehicle is a
point-to-point perfboard build. This is the artifact an EDA package would import, the one
a continuity check can be worked through row by row, and the one a diff can show a change
in. See electrical/schematics/README.md.
"""

from __future__ import annotations

import io
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
CONFIG_HPP = REPO_ROOT / "firmware/flight-computer/include/flight/config.hpp"
OUTPUT_PATH = REPO_ROOT / "electrical/schematics/vehicle-netlist.tsv"

# Every reference designator on the vehicle board. `ref` is what the netlist calls the
# part; `part` is what it is; `mounting` records how it attaches, from the 2026-09-05
# module-mounting decision in documentation/design/wiring.md#module-mounting.
PARTS: list[tuple[str, str, str]] = [
    ("U1", "Raspberry Pi Pico (RP2040)", "soldered down"),
    ("U2", "SX1278 RA-02 433 MHz LoRa module", "soldered down"),
    ("U3", "microSD card reader, 3.3 V, no level shifter", "soldered down"),
    ("U4", "MPU-6500 IMU breakout (sold as MPU-9250)", "male header, jumpered"),
    ("U5", "GY-BMP280 barometer breakout, 6-pin", "male header, jumpered"),
    ("U6", "NEO-6M GPS module with patch antenna", "male header, jumpered"),
    ("U7", "LM393 sound module, 4-pin AO/DO variant", "male header, jumpered"),
    ("BT1", "Orange 3.7 V 1500 mAh 1S LiPo", "JST-RCY pigtail"),
    ("SW1", "Manual ON/OFF switch", "not yet fitted"),
    ("D1", "Schottky diode, 1 A (1N5817 / SS14 / SS34)", "not yet fitted"),
    ("D2", "Power indicator LED — RED 5 mm (lowest Vf, so the brightest at a given resistor)", "not yet fitted"),
    ("D3", "Status LED — GREEN 5 mm, on GP14", "not yet fitted"),
    ("R1", "1 kOhm 5 % — status LED series", "not yet fitted"),
    ("R2", "1 kOhm 5 % — power LED series; see the note on outdoor visibility", "not yet fitted"),
    ("R3", "33 kOhm 1 % — battery divider, high side", "not yet fitted"),
    ("R4", "33 kOhm 1 % — battery divider, low side", "not yet fitted"),
    ("C1", "100 uF electrolytic — microSD bulk", "fitted"),
    ("C2", "100 uF electrolytic — microSD bulk", "fitted"),
    ("C3", "10 uF electrolytic — RA-02 bulk", "fitted"),
    ("C4", "0.1 uF ceramic 104 — microSD", "fitted"),
    ("C5", "0.1 uF ceramic 104 — RA-02", "fitted"),
    ("C6", "0.1 uF ceramic 104 — LM393", "fitted"),
    ("J1", "3.3 V test link, 2-pin header", "not yet fitted"),
]

# One row per connection point. `pin_constant` names the BoardPins member the Pico end
# comes from, and is verified against config.hpp before anything is written; a row with no
# GPIO behind it leaves it empty. `net` groups the rows -- every row sharing a net name is
# one electrical node.
#
# Pico pin numbers are physical, USB to the left, component side up, matching
# documentation/hardware/diagrams/wiring-schedule.svg.
Row = tuple[str, str, str, str, str, str, str]
CONNECTIONS: list[Row] = [
    # net, class, ref, pin, signal, pin_constant, note
    ("VBAT", "power", "BT1", "+", "battery +", "", "JST-RCY, red"),
    ("VBAT", "power", "SW1", "1", "switch in", "", "manual ON/OFF, PWR-001"),
    ("VSW", "power", "SW1", "2", "switch out", "", "switched battery node"),
    ("VSW", "power", "D1", "A", "anode", "", "Schottky, stops USB back-powering the pack"),
    ("VSW", "power", "R3", "1", "divider high", "", "33 kOhm 1 %"),
    # The power LED hangs off the REGULATED rail, not the switched battery node. Decided
    # 2026-09-09; documentation/design/electrical-architecture.md#the-indicator-leds has the
    # trade. Short version: on +3V3 the LED means "the system is actually powered", the
    # current is deterministic across the whole discharge curve, and it tracks "is this
    # thing transmitting" -- which is the question the radio-silence procedure turns on.
    # On VSW it would mean only "the battery is connected" and would dim as the cell drains,
    # which is a poor property for an indicator the rulebook requires to be *visible*.
    ("VSYS", "power", "D1", "K", "cathode", "", "band toward the Pico"),
    ("VSYS", "power", "U1", "39", "VSYS", "", "Pico second supply, datasheet 4.5"),
    ("VBUS", "power", "U1", "40", "VBUS", "", "LIVE 5 V — never wired, nothing here tolerates 5 V"),

    ("+3V3", "power", "U1", "36", "3V3(OUT)", "", "the only 3.3 V source on the vehicle"),
    ("+3V3", "power", "J1", "1", "test link", "", "opens the rail for a series-current measurement"),
    ("+3V3", "power", "R2", "1", "power LED series", "", "lit whenever the 3.3 V rail is up"),
    ("+3V3", "power", "U2", "J2.3", "3.3V", "", "star point, C3 and C5 here"),
    ("+3V3", "power", "U3", "6", "3V3", "", "star point, C1/C2 and C4 here"),
    ("+3V3", "power", "U4", "1", "VCC", "", ""),
    ("+3V3", "power", "U5", "1", "VCC", "", ""),
    ("+3V3", "power", "U6", "1", "VCC", "", ""),
    ("+3V3", "power", "U7", "4", "VCC", "", "C6 here"),
    ("+3V3", "power", "C1", "+", "bulk", "", "stripe is the NEGATIVE leg"),
    ("+3V3", "power", "C2", "+", "bulk", "", ""),
    ("+3V3", "power", "C3", "+", "bulk", "", ""),
    ("+3V3", "power", "C4", "1", "decoupling", "", "ceramic, no polarity"),
    ("+3V3", "power", "C5", "1", "decoupling", "", ""),
    ("+3V3", "power", "C6", "1", "decoupling", "", ""),

    ("GND", "power", "U1", "38", "GND", "", "GND node; the ring starts here"),
    ("GND", "power", "U1", "33", "AGND", "", "separate plane — ONE tie, beside pin 38"),
    ("GND", "power", "BT1", "-", "battery -", "", "JST-RCY, black"),
    ("GND", "power", "U2", "J2.1", "GND", "", ""),
    ("GND", "power", "U2", "J2.2", "GND", "", "star point"),
    ("GND", "power", "U3", "1", "GND", "", "star point"),
    ("GND", "power", "U4", "2", "GND", "", ""),
    ("GND", "power", "U5", "2", "GND", "", ""),
    ("GND", "power", "U6", "4", "GND", "", ""),
    ("GND", "power", "U7", "3", "GND", "", "near the AGND tie"),
    ("GND", "power", "R4", "2", "divider low", "", ""),
    ("GND", "power", "D2", "K", "cathode", "", ""),
    ("GND", "power", "D3", "K", "cathode", "", ""),
    ("GND", "power", "C1", "-", "bulk", "", ""),
    ("GND", "power", "C2", "-", "bulk", "", ""),
    ("GND", "power", "C3", "-", "bulk", "", ""),
    ("GND", "power", "C4", "2", "decoupling", "", ""),
    ("GND", "power", "C5", "2", "decoupling", "", ""),
    ("GND", "power", "C6", "2", "decoupling", "", ""),
    ("GND", "power", "J1", "2", "test link return", "", ""),

    ("I2C0_SDA", "i2c", "U1", "6", "GP4", "i2c_sda", "shared bus"),
    ("I2C0_SDA", "i2c", "U4", "4", "SDA", "i2c_sda", "MPU-6500 at 0x68"),
    ("I2C0_SDA", "i2c", "U5", "4", "SDA", "i2c_sda", "BMP280 at 0x76"),
    ("I2C0_SCL", "i2c", "U1", "7", "GP5", "i2c_scl", "shared bus"),
    ("I2C0_SCL", "i2c", "U4", "3", "SCL", "i2c_scl", ""),
    ("I2C0_SCL", "i2c", "U5", "3", "SCL", "i2c_scl", ""),
    ("IMU_INT", "i2c", "U1", "10", "GP7", "imu_int", "wired; firmware does not enable it"),
    ("IMU_INT", "i2c", "U4", "8", "INT", "imu_int", ""),

    ("SPI0_SCK", "spi", "U1", "24", "GP18", "spi_sck", "4 MHz, shared"),
    ("SPI0_SCK", "spi", "U2", "J1.5", "SCK", "spi_sck", ""),
    ("SPI0_SCK", "spi", "U3", "3", "CLK", "spi_sck", "silkscreen says CLK, not SCK"),
    ("SPI0_MOSI", "spi", "U1", "25", "GP19", "spi_mosi", ""),
    ("SPI0_MOSI", "spi", "U2", "J1.3", "MOSI", "spi_mosi", ""),
    ("SPI0_MOSI", "spi", "U3", "4", "MOSI", "spi_mosi", ""),
    ("SPI0_MISO", "spi", "U1", "21", "GP16", "spi_miso", "two devices must tri-state here"),
    ("SPI0_MISO", "spi", "U2", "J1.4", "MISO", "spi_miso", ""),
    ("SPI0_MISO", "spi", "U3", "2", "MISO", "spi_miso", ""),
    ("LORA_CS", "spi", "U1", "22", "GP17", "lora_cs", ""),
    ("LORA_CS", "spi", "U2", "J1.2", "NSS", "lora_cs", ""),
    ("SD_CS", "spi", "U1", "9", "GP6", "sd_cs", ""),
    ("SD_CS", "spi", "U3", "5", "CS", "sd_cs", ""),

    ("LORA_RST", "radio", "U1", "26", "GP20", "lora_reset", ""),
    ("LORA_RST", "radio", "U2", "J2.4", "RST", "lora_reset", ""),
    ("LORA_DIO0", "radio", "U1", "27", "GP21", "lora_dio0", "TX done / RX done"),
    ("LORA_DIO0", "radio", "U2", "J2.5", "DIO0", "lora_dio0", ""),
    ("LORA_DIO1", "radio", "U1", "29", "GP22", "lora_dio1", "wired, never read"),
    ("LORA_DIO1", "radio", "U2", "J2.6", "DIO1", "lora_dio1", ""),

    ("GPS_TX", "uart", "U1", "16", "GP12", "gps_tx", "Pico transmits"),
    ("GPS_TX", "uart", "U6", "2", "RX", "gps_tx", ""),
    ("GPS_RX", "uart", "U1", "17", "GP13", "gps_rx", "Pico receives, 9600 8N1"),
    ("GPS_RX", "uart", "U6", "3", "TX", "gps_rx", ""),

    ("BATT_SENSE", "analogue", "U1", "31", "GP26 / ADC0", "battery_adc", "divider midpoint"),
    ("BATT_SENSE", "analogue", "R3", "2", "divider high", "battery_adc", ""),
    ("BATT_SENSE", "analogue", "R4", "1", "divider low", "battery_adc", "no capacitor — D-7"),
    ("SOUND_AO", "analogue", "U1", "32", "GP27 / ADC1", "sound_adc", "short run, paired return to AGND"),
    ("SOUND_AO", "analogue", "U7", "1", "AO", "sound_adc", ""),
    ("SOUND_DO", "digital", "U1", "20", "GP15", "sound_gate", "comparator output"),
    ("SOUND_DO", "digital", "U7", "2", "DO", "sound_gate", ""),

    ("STATUS_LED", "digital", "U1", "19", "GP14", "status_led", "blink code says the mission state"),
    ("STATUS_LED", "digital", "R1", "1", "series", "status_led", ""),
    ("LED_A", "digital", "R1", "2", "series", "", ""),
    ("LED_A", "digital", "D3", "A", "anode", "", ""),
    ("PWR_LED_A", "digital", "R2", "2", "series", "", ""),
    ("PWR_LED_A", "digital", "D2", "A", "anode", "", "PWR-002/PWR-003: lit as soon as the rail is up"),
]

HEADER = ("net", "class", "ref", "pin", "signal", "pin_constant", "note")


def board_pins() -> dict[str, int]:
    """Read the `BoardPins` GPIO assignments straight out of the firmware header."""
    source = CONFIG_HPP.read_text(encoding="utf-8")
    match = re.search(r"struct BoardPins \{(.*?)\n\};", source, re.DOTALL)
    if not match:
        raise SystemExit(f"could not find struct BoardPins in {CONFIG_HPP}")
    return {name: int(value) for name, value in
            re.findall(r"static constexpr int (\w+)\s*=\s*(\d+)", match.group(1))}


# Pico physical pin -> GPIO number, for the pins this design uses. From the RP2040
# datasheet's pinout; only the GPIO pins are listed, since power and ground pins have no
# GPIO to check against.
PHYSICAL_TO_GPIO = {
    "6": 4, "7": 5, "9": 6, "10": 7, "16": 12, "17": 13, "19": 14, "20": 15,
    "21": 16, "22": 17, "24": 18, "25": 19, "26": 20, "27": 21, "29": 22,
    "31": 26, "32": 27,
}


def verify(pins: dict[str, int]) -> list[str]:
    """Every GPIO row must agree with `BoardPins`, and with the Pico's own pinout."""
    problems: list[str] = []
    for net, _cls, ref, pin, signal, constant, _note in CONNECTIONS:
        if not constant:
            continue
        if constant not in pins:
            problems.append(f"{net}: BoardPins has no member {constant!r}")
            continue
        if ref != "U1":
            continue  # only the Pico end carries a physical pin to check
        gpio = PHYSICAL_TO_GPIO.get(pin)
        if gpio is None:
            problems.append(f"{net}: Pico physical pin {pin} is not a GPIO in this map")
        elif gpio != pins[constant]:
            problems.append(
                f"{net}: physical pin {pin} is GP{gpio}, but BoardPins::{constant} "
                f"is GP{pins[constant]} (signal column says {signal!r})")
    # Every GPIO the firmware assigns should appear somewhere, or the netlist is missing a
    # net rather than merely disagreeing about one.
    used = {row[5] for row in CONNECTIONS if row[5]}
    for name in pins:
        if name not in used:
            problems.append(f"BoardPins::{name} is assigned in firmware but has no net")
    return problems


def build() -> str:
    lines = ["# CanSat 2026 vehicle netlist. Generated by tools/gen_netlist.py -- "
             "do not hand-edit.",
             "# Rows sharing a net name are one electrical node. Pico pins are physical, "
             "USB to the left.",
             "# pin_constant names the flight::BoardPins member the assignment comes "
             "from; the generator refuses to run if they disagree.",
             "#",
             "# Reference designators:"]
    lines += [f"#   {ref:<4} {part}  [{mounting}]" for ref, part, mounting in PARTS]
    lines.append("\t".join(HEADER))
    lines += ["\t".join(row) for row in CONNECTIONS]
    return "\n".join(lines) + "\n"


def main() -> int:
    problems = verify(board_pins())
    if problems:
        print("netlist disagrees with the firmware:", file=sys.stderr)
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        return 1
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    io.open(OUTPUT_PATH, "w", encoding="utf-8", newline="\n").write(build())
    nets = len({row[0] for row in CONNECTIONS})
    print(f"written {OUTPUT_PATH.relative_to(REPO_ROOT)} "
          f"({nets} nets, {len(CONNECTIONS)} connections, {len(PARTS)} parts)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
