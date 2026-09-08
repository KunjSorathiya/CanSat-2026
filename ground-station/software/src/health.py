"""Communication-link health: receive rate, packet loss, corruption and staleness.

Kept separate from telemetry validation so a transport fault (no carrier, CRC
failure) is never mistaken for a sensor fault.
"""

from __future__ import annotations

import time
from collections import deque
from dataclasses import dataclass, field

# Packet arrivals inside this window define the reported rate.
RATE_WINDOW_S = 5.0

# The rulebook requires at least one packet per second. It is a MINIMUM the vehicle must
# never be found below -- and the vehicle firmware refuses to build at or below it, with
# 50 ms of jitter margin (cansat::link::kMaxTelemetryPeriodMs). This is the receiving end
# of the same rule: the ground station says out loud whether what actually arrived meets
# it, rather than leaving an operator to read a number and do the comparison themselves.
RULEBOOK_MIN_RATE_HZ = 1.0

# How long the link has to have been up before the rate is worth judging. The rate window
# is five seconds and a fresh link has one or two packets in it, so a station that judged
# immediately would report non-compliance for the first few seconds of every session and
# teach its operator to ignore the indicator.
RATE_JUDGEMENT_AFTER_S = RATE_WINDOW_S


@dataclass
class LinkHealth:
    started_at: float = field(default_factory=time.monotonic)
    last_rx_at: float | None = None

    frames_total: int = 0
    packets_ok: int = 0
    packets_invalid: int = 0
    status_frames: int = 0
    crc_errors: int = 0

    missing_packets: int = 0
    duplicate_packets: int = 0

    # Arrival times of recent packets. A sliding window is used instead of an EWMA of
    # instantaneous 1/dt intervals: two frames arriving in the same millisecond -- a
    # duplicate, or a serial buffer flushing a burst -- push an EWMA to thousands of Hz
    # and it then needs ~10 packets to decay, which misreads the link during a mission.
    _arrivals: deque = field(default_factory=lambda: deque(maxlen=512))

    # ---- event hooks ----
    def on_frame(self, kind: str) -> None:
        self.frames_total += 1
        now = time.monotonic()
        if kind in ("packet", "raw"):
            self._arrivals.append(now)
            self.last_rx_at = now
        elif kind == "status":
            self.status_frames += 1
        elif kind == "crc":
            self.crc_errors += 1

    def on_packet_result(self, ok: bool, missing: int = 0, duplicate: bool = False) -> None:
        if ok:
            self.packets_ok += 1
        else:
            self.packets_invalid += 1
        self.missing_packets += max(0, missing)
        if duplicate:
            self.duplicate_packets += 1

    # ---- derived ----
    @property
    def connected(self) -> bool:
        return self.last_rx_at is not None and (time.monotonic() - self.last_rx_at) < 3.0

    @property
    def rate_hz(self) -> float:
        """Packets per second over the last RATE_WINDOW_S seconds.

        Returns 0.0 once the window empties, so a dropped link reads zero instead of
        freezing at the last observed rate.
        """
        cutoff = time.monotonic() - RATE_WINDOW_S
        while self._arrivals and self._arrivals[0] < cutoff:
            self._arrivals.popleft()
        count = len(self._arrivals)
        if count < 2:
            return 0.0
        span = self._arrivals[-1] - self._arrivals[0]
        if span <= 0.0:
            # Every arrival landed in the same instant; report a lower bound rather
            # than dividing by zero.
            return round(count / RATE_WINDOW_S, 2)
        return round((count - 1) / span, 2)

    @property
    def rate_meets_rulebook(self) -> bool | None:
        """Is the received rate at or above the rulebook's 1 Hz minimum?

        ``None`` while there is not yet enough of a link to judge -- no connection, or
        fewer than ``RATE_JUDGEMENT_AFTER_S`` seconds of it. That is deliberately distinct
        from ``False``: "not measured yet" and "measured, and too slow" call for very
        different reactions from an operator, and a boolean cannot say the first.
        """
        if not self.connected or self.last_rx_at is None:
            return None
        if not self._arrivals:
            return None
        if (self._arrivals[-1] - self._arrivals[0]) < RATE_JUDGEMENT_AFTER_S:
            return None
        return self.rate_hz >= RULEBOOK_MIN_RATE_HZ

    @property
    def seconds_since_rx(self) -> float | None:
        if self.last_rx_at is None:
            return None
        return round(time.monotonic() - self.last_rx_at, 1)

    @property
    def loss_pct(self) -> float:
        expected = self.packets_ok + self.missing_packets
        if expected <= 0:
            return 0.0
        return round(100.0 * self.missing_packets / expected, 1)

    def snapshot(self) -> dict[str, object]:
        return {
            "connected": self.connected,
            "rate_hz": self.rate_hz,
            "rate_meets_rulebook": self.rate_meets_rulebook,
            "packets_ok": self.packets_ok,
            "packets_invalid": self.packets_invalid,
            "missing": self.missing_packets,
            "duplicates": self.duplicate_packets,
            "crc_errors": self.crc_errors,
            "loss_pct": self.loss_pct,
            "seconds_since_rx": self.seconds_since_rx,
            "status_frames": self.status_frames,
        }
