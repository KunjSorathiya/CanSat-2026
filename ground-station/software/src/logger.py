"""Raw + parsed telemetry logging.

* raw log  -- every received line, tab-prefixed with the receipt timestamp, nothing
              discarded (malformed packets included)
* parsed log -- one CSV row per packet with validation status and sequence notes

Two properties this module has to hold under a real mission:

*One record per line.* The raw log is the forensic record, so it keeps corrupted payloads
verbatim -- and a corrupted payload can contain a tab or a newline, which would silently
split one record into two and desynchronise every column after it. Control characters are
therefore escaped on the way in, reversibly, rather than written raw or dropped.

*A logging failure must not stop reception.* A full disk, a removed drive or a permission
error raises from the write, and an uncaught exception on the ground-station thread would
end the whole pipeline. Write errors are counted and surfaced instead, and the station
keeps receiving. Telemetry is worth more than its log.
"""

from __future__ import annotations

import csv
import shutil
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path

from telemetry import TelemetryRecord

CSV_FIELDS = [
    "receipt_time", "team_id", "packet_number", "timestamp", "altitude",
    "pressure", "temperature", "roll", "pitch", "yaw", "yaw_reference", "heading",
    "ax", "ay", "az",
    "gps_lat", "gps_lon", "gps_alt", "valid", "error", "seq_missing",
    "seq_note", "raw_packet",
]

# Backslash escapes for the raw log, chosen so the transformation is unambiguous and a
# reader that knows the same four rules can recover the original text exactly.
_ESCAPES = {
    "\\": "\\\\",
    "\t": "\\t",
    "\r": "\\r",
    "\n": "\\n",
}


def escape_raw(text: str) -> str:
    """Make ``text`` safe to store as one tab-separated field, reversibly."""
    out = []
    for char in text:
        if char in _ESCAPES:
            out.append(_ESCAPES[char])
        elif ord(char) < 0x20 or ord(char) == 0x7F:
            out.append("\\x%02x" % ord(char))
        else:
            out.append(char)
    return "".join(out)


def unescape_raw(text: str) -> str:
    """Inverse of :func:`escape_raw`, for post-flight analysis."""
    out = []
    index = 0
    simple = {"t": "\t", "r": "\r", "n": "\n", "\\": "\\"}
    while index < len(text):
        char = text[index]
        if char != "\\" or index + 1 >= len(text):
            out.append(char)
            index += 1
            continue
        marker = text[index + 1]
        if marker in simple:
            out.append(simple[marker])
            index += 2
        elif marker == "x" and index + 3 < len(text):
            try:
                out.append(chr(int(text[index + 2:index + 4], 16)))
                index += 4
            except ValueError:
                out.append(char)
                index += 1
        else:
            out.append(char)
            index += 1
    return "".join(out)


@dataclass
class PacketLog:
    raw_path: Path
    parsed_path: Path

    # Diagnostics. A silent logging failure is worse than a loud one: the operator needs
    # to know the flight is not being recorded while there is still time to fix it.
    write_errors: int = field(default=0, init=False)
    last_error: str = field(default="", init=False)

    def __post_init__(self) -> None:
        self.raw_path = Path(self.raw_path)
        self.parsed_path = Path(self.parsed_path)
        try:
            self.raw_path.parent.mkdir(parents=True, exist_ok=True)
            self.parsed_path.parent.mkdir(parents=True, exist_ok=True)
            if not self.parsed_path.exists():
                with self.parsed_path.open("w", newline="", encoding="utf-8") as stream:
                    csv.DictWriter(stream, fieldnames=CSV_FIELDS).writeheader()
        except OSError as exc:
            # Construction must not raise: a ground station with no writable log is still
            # a ground station, and the operator needs the live display either way.
            self._record_error(exc)

    def _record_error(self, exc: BaseException) -> None:
        self.write_errors += 1
        self.last_error = f"{type(exc).__name__}: {exc}"

    def append(self, raw_packet: str, record: TelemetryRecord | None,
               error: str | None = None, missing: int = 0, note: str = "") -> None:
        receipt_time = datetime.now(timezone.utc).isoformat()
        safe_raw = escape_raw(raw_packet.rstrip())

        try:
            with self.raw_path.open("a", encoding="utf-8") as stream:
                stream.write(f"{receipt_time}\t{safe_raw}\n")
        except OSError as exc:
            self._record_error(exc)

        try:
            with self.parsed_path.open("a", newline="", encoding="utf-8") as stream:
                writer = csv.DictWriter(stream, fieldnames=CSV_FIELDS)
                if record is not None:
                    row = record.csv_row(receipt_time)
                    row["error"] = ""
                    row["seq_missing"] = missing
                    row["seq_note"] = note
                    row["raw_packet"] = safe_raw
                    writer.writerow(row)
                else:
                    writer.writerow({
                        "receipt_time": receipt_time,
                        "valid": False,
                        "error": error or "unparsed",
                        "raw_packet": safe_raw,
                    })
        except OSError as exc:
            self._record_error(exc)

    def export_csv(self, destination: str | Path) -> Path:
        destination = Path(destination)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(self.parsed_path, destination)
        return destination


def detect_missing(previous: int | None, current: int) -> int:
    if previous is None or current <= previous:
        return 0
    return max(0, current - previous - 1)
