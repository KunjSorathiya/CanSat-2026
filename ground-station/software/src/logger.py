from __future__ import annotations

import csv
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable

from telemetry import TelemetryRecord

CSV_FIELDS = [
    "receipt_time", "team_id", "packet_number", "timestamp", "altitude",
    "pressure", "temperature", "roll", "pitch", "yaw", "ax", "ay", "az",
    "gps_lat", "gps_lon", "gps_alt", "valid", "error",
]


@dataclass
class PacketLog:
    raw_path: Path
    parsed_path: Path

    def __post_init__(self) -> None:
        self.raw_path.parent.mkdir(parents=True, exist_ok=True)
        self.parsed_path.parent.mkdir(parents=True, exist_ok=True)
        if not self.parsed_path.exists():
            with self.parsed_path.open("w", newline="", encoding="utf-8") as stream:
                writer = csv.DictWriter(stream, fieldnames=CSV_FIELDS)
                writer.writeheader()

    def append(self, raw_packet: str, record: TelemetryRecord | None, error: str | None = None) -> None:
        receipt_time = datetime.now(timezone.utc).isoformat()
        with self.raw_path.open("a", encoding="utf-8") as stream:
            stream.write(f"{receipt_time}\t{raw_packet.rstrip()}\n")
        if record is not None:
            with self.parsed_path.open("a", newline="", encoding="utf-8") as stream:
                writer = csv.DictWriter(stream, fieldnames=CSV_FIELDS)
                row = record.csv_row(receipt_time)
                row["error"] = ""
                writer.writerow(row)
        elif error:
            with self.parsed_path.open("a", newline="", encoding="utf-8") as stream:
                writer = csv.DictWriter(stream, fieldnames=CSV_FIELDS)
                writer.writerow({"receipt_time": receipt_time, "valid": False, "error": error})


def detect_missing(previous: int | None, current: int) -> int:
    if previous is None or current <= previous:
        return 0
    return max(0, current - previous - 1)
