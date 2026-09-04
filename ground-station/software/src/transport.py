"""Byte-transport layer for the ground station.

A transport yields ``Frame`` objects. ``Frame.kind`` is:
  * ``"packet"``  -- a telemetry payload (CRC-valid when it came from a framed link)
  * ``"status"``  -- a bridge status line (payload starts with '#')
  * ``"crc"``     -- a frame whose CRC failed (payload kept for diagnostics)
  * ``"raw"``     -- an unframed line (file replay / plain serial)

Implementations:
  * ``SerialTransport``       -- ground-station Pico over USB serial (needs pyserial)
  * ``FileReplayTransport``   -- newline-delimited packets from a file, optional pacing
  * ``LoopbackTransport``     -- in-memory queue, for tests and the dashboard demo
"""

from __future__ import annotations

import queue
import time
from dataclasses import dataclass
from typing import Iterable, Iterator, Optional


# --------------------------------------------------------------------------- #
# Framing (mirror of firmware/ground-station/src/framing.cpp)
# --------------------------------------------------------------------------- #
def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def frame_encode(payload: bytes) -> bytes:
    return b"$%d,%04x,%b\n" % (len(payload), crc16_ccitt(payload), payload)


@dataclass
class Frame:
    kind: str          # "packet" | "status" | "crc" | "raw"
    payload: str
    crc_ok: bool = True


class FrameDecoder:
    """Incremental decoder for the '$<len>,<crc>,<payload>\\n' framing."""

    MAX_PAYLOAD = 512

    def __init__(self) -> None:
        self._state = "idle"
        self._len_text = b""
        self._crc_text = b""
        self._payload = bytearray()
        self._expected = 0
        self.frames_ok = 0
        self.crc_errors = 0
        self.resyncs = 0

    def feed(self, chunk: bytes) -> Iterator[Frame]:
        for byte in chunk:
            frame = self._feed_byte(byte)
            if frame is not None:
                yield frame

    def _reset(self) -> None:
        self._state = "idle"
        self._len_text = b""
        self._crc_text = b""
        self._payload = bytearray()
        self._expected = 0

    def _finish(self) -> Frame:
        want = int(self._crc_text, 16)
        payload = bytes(self._payload)
        crc_ok = crc16_ccitt(payload) == want
        text = payload.decode("utf-8", errors="replace")
        self._reset()
        if crc_ok:
            self.frames_ok += 1
            kind = "status" if text.startswith("#") else "packet"
            return Frame(kind, text, True)
        self.crc_errors += 1
        return Frame("crc", text, False)

    def _feed_byte(self, byte: int) -> Optional[Frame]:
        ch = bytes((byte,))
        if self._state == "idle":
            if ch == b"$":
                self._reset()
                self._state = "len"
            return None

        if self._state == "len":
            if b"0" <= ch <= b"9":
                self._len_text += ch
                if len(self._len_text) > 6:
                    self.resyncs += 1
                    self._reset()
            elif ch == b"," and self._len_text:
                self._expected = int(self._len_text)
                if self._expected > self.MAX_PAYLOAD:
                    self.resyncs += 1
                    self._reset()
                else:
                    self._state = "crc"
            elif ch == b"$":
                self._len_text = b""
            else:
                self.resyncs += 1
                self._reset()
            return None

        if self._state == "crc":
            if ch.isalnum() and ch.lower() in b"0123456789abcdef":
                self._crc_text += ch
                if len(self._crc_text) > 4:
                    self.resyncs += 1
                    self._reset()
            elif ch == b"," and len(self._crc_text) == 4:
                self._state = "payload"
                if self._expected == 0:
                    return self._finish()
            else:
                self.resyncs += 1
                self._reset()
            return None

        if self._state == "payload":
            self._payload += ch
            if len(self._payload) >= self._expected:
                return self._finish()
            return None
        return None


# --------------------------------------------------------------------------- #
# Transports
# --------------------------------------------------------------------------- #
class Transport:
    def frames(self) -> Iterator[Frame]:
        raise NotImplementedError

    def close(self) -> None:  # pragma: no cover - default no-op
        pass


class FileReplayTransport(Transport):
    """Replays newline-delimited packets from a file (unframed 'raw' lines)."""

    def __init__(self, path: str, rate_hz: float = 0.0, framed: bool = False) -> None:
        self.path = path
        self.period = (1.0 / rate_hz) if rate_hz > 0 else 0.0
        self.framed = framed
        self._decoder = FrameDecoder() if framed else None

    def frames(self) -> Iterator[Frame]:
        with open(self.path, "rb") as stream:
            for line in stream:
                if self._decoder is not None:
                    yield from self._decoder.feed(line)
                else:
                    text = line.decode("utf-8", errors="replace").strip()
                    if text:
                        kind = "status" if text.startswith("#") else "raw"
                        yield Frame(kind, text, True)
                if self.period:
                    time.sleep(self.period)


class LoopbackTransport(Transport):
    """In-memory transport; push frames/payloads from test code or a demo thread."""

    def __init__(self, framed: bool = False) -> None:
        self._q: "queue.Queue[Optional[Frame]]" = queue.Queue()
        self._decoder = FrameDecoder() if framed else None
        self.framed = framed

    def push_packet(self, packet: str) -> None:
        if self._decoder is not None:
            for frame in self._decoder.feed(frame_encode(packet.encode())):
                self._q.put(frame)
        else:
            kind = "status" if packet.startswith("#") else "raw"
            self._q.put(Frame(kind, packet, True))

    def push_bytes(self, data: bytes) -> None:
        assert self._decoder is not None, "push_bytes requires framed=True"
        for frame in self._decoder.feed(data):
            self._q.put(frame)

    def stop(self) -> None:
        self._q.put(None)

    def frames(self) -> Iterator[Frame]:
        while True:
            item = self._q.get()
            if item is None:
                return
            yield item


class SerialTransport(Transport):
    """Ground-station Pico over USB CDC serial. Requires pyserial."""

    # In unframed mode the reader accumulates bytes until a newline arrives. A link stuck
    # emitting noise with no newline would otherwise grow this buffer without bound for as
    # long as the station runs, so it is capped: past this the partial line is dropped and
    # counted as a resync rather than held forever.
    MAX_LINE = 4096

    def __init__(self, port: str, baud: int = 115200, framed: bool = True,
                 reconnect: bool = True) -> None:
        try:
            import serial  # type: ignore
        except ImportError as exc:  # pragma: no cover - env dependent
            raise RuntimeError(
                "pyserial is required for SerialTransport (pip install pyserial)"
            ) from exc
        self._serial_mod = serial
        self.port = port
        self.baud = baud
        self.framed = framed
        self.reconnect = reconnect
        self._decoder = FrameDecoder() if framed else None
        self._buf = b""
        self._handle = None
        self._closed = False
        self.resyncs = 0

    def _open(self):
        return self._serial_mod.Serial(self.port, self.baud, timeout=0.2)

    def frames(self) -> Iterator[Frame]:  # pragma: no cover - needs hardware
        while not self._closed:
            try:
                if self._handle is None:
                    self._handle = self._open()
                chunk = self._handle.read(256)
            except Exception:
                self._handle = None
                if not self.reconnect:
                    return
                time.sleep(1.0)
                continue
            if not chunk:
                continue
            if self._decoder is not None:
                yield from self._decoder.feed(chunk)
            else:
                self._buf += chunk
                while b"\n" in self._buf:
                    line, self._buf = self._buf.split(b"\n", 1)
                    text = line.decode("utf-8", errors="replace").strip()
                    if text:
                        kind = "status" if text.startswith("#") else "raw"
                        yield Frame(kind, text, True)
                if len(self._buf) > self.MAX_LINE:
                    self._buf = b""
                    self.resyncs += 1

    def close(self) -> None:  # pragma: no cover - needs hardware
        self._closed = True
        if self._handle is not None:
            try:
                self._handle.close()
            except Exception:
                pass


def iterate_packets(source: Iterable[str]) -> Iterator[Frame]:
    """Adapt a plain iterable of packet strings into Frames (used by CLI replay)."""
    for line in source:
        text = line.strip()
        if text:
            yield Frame("status" if text.startswith("#") else "raw", text, True)
