"""Stateful validation of a telemetry stream: team identity, packet sequencing
(missing / duplicate / out-of-order), timestamp monotonicity and GPS sanity.

Transport-level problems (no packet, CRC failure) are tracked separately in
``health.py`` -- this module only judges packets that already parsed.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional

from telemetry import TelemetryRecord


@dataclass
class SequenceReport:
    accepted: bool = True
    missing: int = 0          # packets skipped since the previous number
    duplicate: bool = False
    out_of_order: bool = False
    notes: list[str] = field(default_factory=list)


@dataclass
class ValidationStats:
    received: int = 0
    accepted: int = 0
    rejected: int = 0
    missing: int = 0
    duplicates: int = 0
    out_of_order: int = 0
    wrong_team: int = 0
    timestamp_regressions: int = 0
    gps_rejected: int = 0


class StreamValidator:
    def __init__(self, expected_team: Optional[str] = None,
                 gps_lat_range: tuple[float, float] = (-90.0, 90.0),
                 gps_lon_range: tuple[float, float] = (-180.0, 180.0)) -> None:
        self.expected_team = expected_team
        self.gps_lat_range = gps_lat_range
        self.gps_lon_range = gps_lon_range
        self.stats = ValidationStats()
        self._last_number: Optional[int] = None
        self._seen_numbers: set[int] = set()
        self._last_timestamp_ms: Optional[int] = None

    def reset(self) -> None:
        self.__init__(self.expected_team, self.gps_lat_range, self.gps_lon_range)

    def check(self, record: TelemetryRecord) -> SequenceReport:
        report = SequenceReport()
        self.stats.received += 1

        if self.expected_team and record.team_id != self.expected_team:
            report.accepted = False
            report.notes.append(f"wrong team {record.team_id!r}")
            self.stats.wrong_team += 1
            self.stats.rejected += 1
            return report

        number = record.packet_number
        if number in self._seen_numbers:
            report.duplicate = True
            report.notes.append(f"duplicate P-{number:03d}")
            self.stats.duplicates += 1
        elif self._last_number is not None:
            if number <= self._last_number:
                report.out_of_order = True
                report.notes.append(
                    f"out of order P-{number:03d} after P-{self._last_number:03d}"
                )
                self.stats.out_of_order += 1
            elif number > self._last_number + 1:
                report.missing = number - self._last_number - 1
                self.stats.missing += report.missing
                report.notes.append(f"{report.missing} missing before P-{number:03d}")

        # Timestamp monotonicity (informational; a reset restarts the mission clock).
        ts = record.timestamp_ms
        if self._last_timestamp_ms is not None and ts < self._last_timestamp_ms:
            report.notes.append("timestamp regression")
            self.stats.timestamp_regressions += 1

        # GPS sanity (optional field): reject an implausible fix but keep the packet.
        if record.has_gps:
            lat, lon = record.gps_lat, record.gps_lon
            lo_lat, hi_lat = self.gps_lat_range
            lo_lon, hi_lon = self.gps_lon_range
            if lat is None or lon is None or not (lo_lat <= lat <= hi_lat) or not (
                lo_lon <= lon <= hi_lon
            ):
                report.notes.append("implausible GPS fix ignored")
                self.stats.gps_rejected += 1

        self._seen_numbers.add(number)
        if self._last_number is None or number > self._last_number:
            self._last_number = number
        self._last_timestamp_ms = ts

        self.stats.accepted += 1
        return report
