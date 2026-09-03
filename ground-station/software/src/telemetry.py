from __future__ import annotations

from dataclasses import dataclass, field
import re
from typing import Optional

TEAM_PATTERN = re.compile(r"CAN-Team-[A-Za-z0-9]+")
MANDATORY_FIELDS = (
    ("A-", 1), ("Pr-", 2), ("T-", 1), ("Ro-", 1), ("Pi-", 1),
    ("Ya-", 1), ("AX-", 2), ("AY-", 2), ("AZ-", 2),
)


@dataclass
class TelemetryRecord:
    team_id: str
    packet_number: int
    timestamp: str
    altitude: float
    pressure: float
    temperature: float
    roll: float
    pitch: float
    yaw: float
    ax: float
    ay: float
    az: float
    optional: dict[str, str] = field(default_factory=dict)

    def csv_row(self, receipt_time: str = "") -> dict[str, object]:
        return {
            "receipt_time": receipt_time,
            "team_id": self.team_id,
            "packet_number": self.packet_number,
            "timestamp": self.timestamp,
            "altitude": self.altitude,
            "pressure": self.pressure,
            "temperature": self.temperature,
            "roll": self.roll,
            "pitch": self.pitch,
            "yaw": self.yaw,
            "ax": self.ax,
            "ay": self.ay,
            "az": self.az,
            "gps_lat": self.optional.get("GP-Lat", ""),
            "gps_lon": self.optional.get("GP-Lon", ""),
            "gps_alt": self.optional.get("GP-Alt", ""),
            "valid": True,
        }


@dataclass
class ParseResult:
    record: Optional[TelemetryRecord] = None
    error: Optional[str] = None


def parse_packet(packet: str, expected_team: Optional[str] = None) -> ParseResult:
    fields = [part.strip() for part in packet.split(";") if part.strip()]
    if len(fields) < 12:
        return ParseResult(error="missing mandatory fields")
    team_id = fields[0]
    if not TEAM_PATTERN.fullmatch(team_id) or team_id == "CAN-Team-XX":
        return ParseResult(error="invalid team identifier")
    if expected_team and team_id != expected_team:
        return ParseResult(error="unexpected team identifier")
    if not fields[1].startswith("P-"):
        return ParseResult(error="invalid packet prefix")
    try:
        packet_number = int(fields[1][2:])
    except ValueError:
        return ParseResult(error="invalid packet number")
    if packet_number < 1:
        return ParseResult(error="packet number must be positive")
    if not fields[2].startswith("Ti-"):
        return ParseResult(error="invalid timestamp prefix")
    timestamp = fields[2][3:]
    if not re.fullmatch(r"\d{2}:\d{2}:\d{2}:\d{3}", timestamp):
        return ParseResult(error="invalid timestamp")

    values: list[float] = []
    for field_value, (prefix, precision) in zip(fields[3:12], MANDATORY_FIELDS):
        if not field_value.startswith(prefix):
            return ParseResult(error=f"expected {prefix} field")
        numeric_text = field_value[len(prefix):]
        if not re.fullmatch(rf"-?\d+\.\d{{{precision}}}", numeric_text):
            return ParseResult(error=f"invalid {prefix} precision")
        try:
            value = float(numeric_text)
        except ValueError:
            return ParseResult(error=f"invalid {prefix} value")
        if value != value or value in (float("inf"), float("-inf")):
            return ParseResult(error=f"invalid {prefix} value")
        values.append(value)

    optional: dict[str, str] = {}
    for optional_field in fields[12:]:
        if "-" in optional_field:
            key, value = optional_field.split("-", 1)
            optional[f"{key}-{value.split('-', 1)[0]}"] = optional_field

    return ParseResult(TelemetryRecord(team_id, packet_number, timestamp, *values, optional=optional))
