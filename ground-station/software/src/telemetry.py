"""CanSat telemetry packet parser and canonical record model.

The wire format (rulebook):

    CAN-Team-XX; P-XXX; Ti-HH:MM:SS:MS; A-XXX.X; Pr-XXXX.XX; T-XX.X; Ro-XX.X;
    Pi-XX.X; Ya-XX.X; AX-XX.XX; AY-XX.XX; AZ-XX.XX;

Optional fields (GP-Lat / GP-Lon / GP-Alt / ...) may follow, always after every
mandatory field. A missing or malformed mandatory field yields no record.

One optional field changes how a mandatory one is read: ``YR-M`` or ``YR-G`` says whether
``Ya-`` is an absolute magnetic yaw or a relative gyro integration. See
``TelemetryRecord.yaw_reference``.
"""

from __future__ import annotations

import math
import re
from dataclasses import dataclass, field
from typing import Optional

TEAM_PATTERN = re.compile(r"CAN-Team-[A-Za-z0-9]+")
TIMESTAMP_PATTERN = re.compile(r"\d{2}:\d{2}:\d{2}:\d{3}")

# (prefix, required decimal places) in mandatory order.
MANDATORY_FIELDS = (
    ("A-", 1), ("Pr-", 2), ("T-", 1), ("Ro-", 1), ("Pi-", 1),
    ("Ya-", 1), ("AX-", 2), ("AY-", 2), ("AZ-", 2),
)
_FIELD_NAMES = ("altitude", "pressure", "temperature", "roll", "pitch", "yaw",
                "ax", "ay", "az")


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
    # Legacy view kept for backwards compatibility: {"GP-Lat": "GP-Lat-18.5", ...}
    optional: dict[str, str] = field(default_factory=dict)
    # Preferred view: the key maps to the value after the separating '-'.
    #   "GP-Lat-18.5" -> {"GP-Lat": "18.5"}, "MODE-READY" -> {"MODE": "READY"},
    #   "GP-Lat--18.5" -> {"GP-Lat": "-18.5"} -- the value keeps its sign.
    tags: dict[str, str] = field(default_factory=dict)

    # ---- derived / typed optional data ----
    @property
    def timestamp_ms(self) -> int:
        hh, mm, ss, ms = (int(part) for part in self.timestamp.split(":"))
        return ((hh * 60 + mm) * 60 + ss) * 1000 + ms

    def _opt_float(self, key: str) -> Optional[float]:
        value = self.tags.get(key)
        if value is None:
            return None
        try:
            return float(value)
        except ValueError:
            return None

    @property
    def gps_lat(self) -> Optional[float]:
        return self._opt_float("GP-Lat")

    @property
    def gps_lon(self) -> Optional[float]:
        return self._opt_float("GP-Lon")

    @property
    def gps_alt(self) -> Optional[float]:
        return self._opt_float("GP-Alt")

    @property
    def has_gps(self) -> bool:
        return self.gps_lat is not None and self.gps_lon is not None

    @property
    def sound_mv(self) -> Optional[float]:
        """The microphone's peak-to-peak level in millivolts, from the optional ``SN-`` field.

        A relative loudness, not a sound pressure level: it tracks the module's gain trimpot as
        much as the room. None when the vehicle did not send it -- a lean max-rate packet, or a
        microphone that was not producing valid windows.
        """
        return self._opt_float("SN")

    @property
    def mode(self) -> Optional[str]:
        return self.tags.get("MODE")

    @property
    def fault_count(self) -> Optional[int]:
        value = self.tags.get("FAULTS")
        try:
            return int(value) if value is not None else None
        except ValueError:
            return None

    @property
    def calibrated(self) -> Optional[bool]:
        value = self.tags.get("CAL")
        return None if value is None else value == "1"

    @property
    def armed(self) -> Optional[bool]:
        value = self.tags.get("ARM")
        return None if value is None else value == "1"

    @property
    def yaw_reference(self) -> Optional[str]:
        """What the ``yaw`` field means in this packet.

        ``"magnetic"``  yaw is referenced to magnetic north through a calibrated
                        magnetometer, so it is an absolute angle.
        ``"gyro"``      yaw is a free-running gyro integration whose zero is wherever the
                        vehicle happened to be pointing when the estimator last reset. It
                        is a relative angle and must not be read as a heading.
        ``None``        the vehicle did not say, which is the case for any packet older
                        than the nine-axis upgrade.

        The distinction is not cosmetic. A relative yaw plotted on a compass rose looks
        exactly like an absolute one and is wrong by an unknown constant.
        """
        value = self.tags.get("YR")
        if value == "M":
            return "magnetic"
        if value == "G":
            return "gyro"
        return None

    @property
    def yaw_is_magnetic(self) -> bool:
        return self.yaw_reference == "magnetic"

    @property
    def heading(self) -> Optional[float]:
        """Magnetic heading in degrees clockwise from north, or ``None``.

        The vehicle reports yaw as a right-handed Z-Y-X Euler angle about its up axis,
        which runs anticlockwise; a compass bearing runs clockwise. They are the same
        angle with opposite signs, and this is the only place that conversion is done.

        ``None`` unless the vehicle declared the yaw magnetic, because a bearing derived
        from a relative yaw would be a bearing to nowhere.
        """
        if not self.yaw_is_magnetic:
            return None
        return (-self.yaw) % 360.0

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
            "gps_lat": self.gps_lat if self.gps_lat is not None else "",
            "gps_lon": self.gps_lon if self.gps_lon is not None else "",
            "gps_alt": self.gps_alt if self.gps_alt is not None else "",
            "sound_mv": self.sound_mv if self.sound_mv is not None else "",
            # Recorded alongside yaw so a log analysed months later still says which of
            # the two kinds of yaw its numbers are.
            "yaw_reference": self.yaw_reference or "",
            "heading": self.heading if self.heading is not None else "",
            "valid": True,
        }


@dataclass
class ParseResult:
    record: Optional[TelemetryRecord] = None
    error: Optional[str] = None

    def __bool__(self) -> bool:
        return self.record is not None


def _finite(value: float) -> bool:
    return not (math.isnan(value) or math.isinf(value))


# The compact status field, ST-<state><armed><calibrated><faults> -- "ST-R110" is READY,
# armed, calibrated, no active faults. The vehicle sends it on every rich packet it fits,
# because the five diagnostic tags are off the air. It is expanded into the MODE, ARM, CAL and
# FAULTS tags everything downstream already reads; a tag the vehicle sent explicitly wins.
# Mirrors expandStatusTag() in the web console; test-data/status-tag-cases.tsv holds both, and
# the firmware's encoder, to one definition.
_STATUS_STATES = {"I": "INIT", "T": "SELF_TEST", "R": "READY", "F": "FLIGHT",
                  "L": "LANDED", "V": "RECOVERY", "X": "FAULT"}
_STATUS_PATTERN = re.compile(r"([ITRFLVX])([01])([01])([0-9])")


def _expand_status(tags: dict[str, str]) -> None:
    match = _STATUS_PATTERN.fullmatch(tags.get("ST", ""))
    if not match:
        return
    state, armed, calibrated, faults = match.groups()
    tags.setdefault("MODE", _STATUS_STATES[state])
    tags.setdefault("ARM", armed)
    tags.setdefault("CAL", calibrated)
    tags.setdefault("FAULTS", faults)


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
    # Digits only, no sign, no interior whitespace, and inside the transmitter's 32-bit
    # counter. int() would accept " 7" and an arbitrarily large value; a parser that
    # disagrees with the transmitter about what a packet number is cannot count loss
    # correctly. The C++ and JavaScript parsers apply the identical rule.
    digits = fields[1][2:]
    if not digits.isdigit() or len(digits) > 10:
        return ParseResult(error="invalid packet number")
    packet_number = int(digits)
    if packet_number < 1 or packet_number > 0xFFFFFFFF:
        return ParseResult(error="invalid packet number")

    if not fields[2].startswith("Ti-"):
        return ParseResult(error="invalid timestamp prefix")
    timestamp = fields[2][3:]
    if not TIMESTAMP_PATTERN.fullmatch(timestamp):
        return ParseResult(error="invalid timestamp")
    hh, mm, ss, _ = (int(part) for part in timestamp.split(":"))
    if mm > 59 or ss > 59 or hh > 99:
        return ParseResult(error="invalid timestamp range")

    values: list[float] = []
    for field_value, (prefix, precision) in zip(fields[3:12], MANDATORY_FIELDS):
        if not field_value.startswith(prefix):
            return ParseResult(error=f"expected {prefix} field")
        numeric_text = field_value[len(prefix):]
        if not re.fullmatch(rf"-?\d+\.\d{{{precision}}}", numeric_text):
            return ParseResult(error=f"invalid {prefix} precision")
        value = float(numeric_text)
        if not _finite(value):
            return ParseResult(error=f"invalid {prefix} value")
        values.append(value)

    optional: dict[str, str] = {}
    tags: dict[str, str] = {}
    for optional_field in fields[12:]:
        if "-" in optional_field:
            head, tail = optional_field.split("-", 1)
            optional[f"{head}-{tail.split('-', 1)[0]}"] = optional_field
            key, _, value = optional_field.rpartition("-")
            # A negative value carries its own '-', and rpartition takes that one as the
            # separator: "GP-Lat--18.5" split to the key "GP-Lat-" and the value "18.5" --
            # the sign eaten and the key unrecognisable, so gps_lat read back as None and
            # a southern-hemisphere fix vanished with no error and no rejection counter.
            # A key never ends in '-', so a key that does means the separator was the
            # value's sign.
            if key.endswith("-"):
                key, value = key[:-1], "-" + value
            tags[key] = value
    _expand_status(tags)

    return ParseResult(
        TelemetryRecord(team_id, packet_number, timestamp, *values,
                        optional=optional, tags=tags)
    )
