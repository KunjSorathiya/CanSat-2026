"""Ground-station orchestrator.

Wires transport -> parser -> validator -> logger -> health together on a background
thread and exposes a thread-safe snapshot plus an event queue for the dashboard. No
part of this blocks a GUI event loop.
"""

from __future__ import annotations

import queue
import threading
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

from health import LinkHealth
from logger import PacketLog
from telemetry import TelemetryRecord, parse_packet
from transport import Transport
from validator import SequenceReport, StreamValidator


@dataclass
class Event:
    kind: str  # "packet" | "invalid" | "crc" | "status"
    record: Optional[TelemetryRecord] = None
    report: Optional[SequenceReport] = None
    raw: str = ""
    error: str = ""
    status: dict = field(default_factory=dict)


class GroundStation:
    def __init__(self, transport: Transport, expected_team: Optional[str] = None,
                 log_dir: str | Path = "logs", event_queue_size: int = 4000) -> None:
        self.transport = transport
        self.expected_team = expected_team
        self.validator = StreamValidator(expected_team)
        self.health = LinkHealth()
        log_dir = Path(log_dir)
        self.log = PacketLog(log_dir / "raw_packets.tsv", log_dir / "telemetry.csv")
        self.events: "queue.Queue[Event]" = queue.Queue(maxsize=event_queue_size)

        self._lock = threading.Lock()
        self._latest_record: Optional[TelemetryRecord] = None
        self._latest_raw: str = ""
        self._bridge_status: dict[str, str] = {}
        self._thread: Optional[threading.Thread] = None
        self._running = False

    # ---------------------------------------------------------------- lifecycle
    def start(self) -> None:
        if self._thread is not None:
            return
        self._running = True
        self._thread = threading.Thread(target=self.run_forever, name="ground-station",
                                        daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._running = False
        try:
            self.transport.close()
        except Exception:
            pass
        stop = getattr(self.transport, "stop", None)
        if callable(stop):
            stop()
        if self._thread is not None:
            self._thread.join(timeout=2.0)
            self._thread = None

    def run_forever(self) -> None:
        for frame in self.transport.frames():
            if not self._running and self._thread is not None:
                break
            self._handle_frame(frame.kind, frame.payload)

    # ------------------------------------------------------------------ pipeline
    def _handle_frame(self, kind: str, payload: str) -> None:
        self.health.on_frame(kind)

        if kind == "status":
            self._ingest_status(payload)
            self._push(Event("status", status=dict(self._bridge_status), raw=payload))
            return

        if kind == "crc":
            self.log.append(payload, None, "transport crc error")
            self._push(Event("crc", raw=payload, error="transport crc error"))
            return

        # kind in ("packet", "raw")
        result = parse_packet(payload, self.expected_team)
        if result.record is None:
            self.health.on_packet_result(False)
            self.log.append(payload, None, result.error)
            self._push(Event("invalid", raw=payload, error=result.error or "unparsed"))
            return

        report = self.validator.check(result.record)
        self.health.on_packet_result(report.accepted, report.missing, report.duplicate)
        note = "; ".join(report.notes)
        self.log.append(
            payload,
            result.record if report.accepted else None,
            None if report.accepted else note or "rejected",
            report.missing,
            note,
        )
        if report.accepted:
            with self._lock:
                self._latest_record = result.record
                self._latest_raw = payload
        self._push(Event("packet", record=result.record, report=report, raw=payload))

    def _ingest_status(self, payload: str) -> None:
        body = payload[1:] if payload.startswith("#") else payload
        for token in body.split():
            if "=" in token:
                key, value = token.split("=", 1)
                self._bridge_status[key] = value

    def _push(self, event: Event) -> None:
        try:
            self.events.put_nowait(event)
        except queue.Full:
            try:
                self.events.get_nowait()
                self.events.put_nowait(event)
            except queue.Empty:
                pass

    # -------------------------------------------------------------------- views
    def snapshot(self) -> dict[str, object]:
        with self._lock:
            record = self._latest_record
            raw = self._latest_raw
        data: dict[str, object] = {
            "link": self.health.snapshot(),
            "validation": vars(self.validator.stats).copy(),
            "bridge": dict(self._bridge_status),
            # What the frame decoder saw on the way in. A resync is noise on the wire; an
            # overflow is a length field no frame on this link can have, which means a
            # corrupted header or a sender configured for frames this receiver will never
            # accept. Empty for an unframed transport, which has no decoder.
            "framing": self.transport.framing_stats(),
            "latest_raw": raw,
            # Logging failures never stop reception, so they must be visible somewhere:
            # a flight that is not being recorded should be discovered on the pad, not
            # afterwards.
            "logging": {
                "write_errors": self.log.write_errors,
                "last_error": self.log.last_error,
            },
        }
        if record is not None:
            data["telemetry"] = {
                "team_id": record.team_id,
                "packet_number": record.packet_number,
                "timestamp": record.timestamp,
                "altitude": record.altitude,
                "pressure": record.pressure,
                "temperature": record.temperature,
                "roll": record.roll,
                "pitch": record.pitch,
                "yaw": record.yaw,
                "yaw_reference": record.yaw_reference,
                "heading": record.heading,
                "ax": record.ax,
                "ay": record.ay,
                "az": record.az,
                "gps_lat": record.gps_lat,
                "gps_lon": record.gps_lon,
                "gps_alt": record.gps_alt,
                "gps_fix": record.has_gps,
                "mode": record.mode,
                "fault_count": record.fault_count,
                "calibrated": record.calibrated,
                "armed": record.armed,
            }
        return data

    def export_csv(self, destination: str | Path) -> Path:
        return self.log.export_csv(destination)
