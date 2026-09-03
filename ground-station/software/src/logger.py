"""Raw + parsed telemetry logging.

* raw log  -- every received line, tab-prefixed with the receipt timestamp, nothing
              discarded (malformed packets included)
* parsed log -- one CSV row per packet with validation status and sequence notes
"""

from __future__ import annotations

import csv
import shutil
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path

from telemetry import TelemetryRecord

CSV_FIELDS = [
    "receipt_time", "team_id", "packet_number", "timestamp", "altitude",
    "pressure", "temperature", "roll", "pitch", "yaw", "ax", "ay", "az",
    "gps_lat", "gps_lon", "gps_alt", "valid", "error", "seq_missing",
    "seq_note", "raw_packet",
]


@dataclass
class PacketLog:
    raw_path: Path
    parsed_path: Path

    def __post_init__(self) -> None:
        self.raw_path = Path(self.raw_path)
        self.parsed_path = Path(self.parsed_path)
        self.raw_path.parent.mkdir(parents=True, exist_ok=True)
        self.parsed_path.parent.mkdir(parents=True, exist_ok=True)
        if not self.parsed_path.exists():
            with self.parsed_path.open("w", newline="", encoding="utf-8") as stream:
                csv.DictWriter(stream, fieldnames=CSV_FIELDS).writeheader()

    def append(self, raw_packet: str, record: TelemetryRecord | None,
               error: str | None = None, missing: int = 0, note: str = "") -> None:
        receipt_time = datetime.now(timezone.utc).isoformat()
        with self.raw_path.open("a", encoding="utf-8") as stream:
            stream.write(f"{receipt_time}\t{raw_packet.rstrip()}\n")

        with self.parsed_path.open("a", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=CSV_FIELDS)
            if record is not None:
                row = record.csv_row(receipt_time)
                row["error"] = ""
                row["seq_missing"] = missing
                row["seq_note"] = note
                row["raw_packet"] = raw_packet.rstrip()
                writer.writerow(row)
            else:
                writer.writerow({
                    "receipt_time": receipt_time,
                    "valid": False,
                    "error": error or "unparsed",
                    "raw_packet": raw_packet.rstrip(),
                })

    def export_csv(self, destination: str | Path) -> Path:
        destination = Path(destination)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(self.parsed_path, destination)
        return destination


def detect_missing(previous: int | None, current: int) -> int:
    if previous is None or current <= previous:
        return 0
    return max(0, current - previous - 1)
