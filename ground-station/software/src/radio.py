"""Backwards-compatible shim.

The radio/serial handling now lives in ``transport.py``. This module keeps the old
``FilePacketSource`` API and re-exports the transport helpers.
"""

from __future__ import annotations

from collections.abc import Iterable

from transport import (  # noqa: F401  re-exported for convenience
    FileReplayTransport,
    Frame,
    FrameDecoder,
    LoopbackTransport,
    frame_encode,
    crc16_ccitt,
)


class FilePacketSource:
    def __init__(self, path: str) -> None:
        self.path = path

    def packets(self) -> Iterable[str]:
        with open(self.path, encoding="utf-8") as stream:
            yield from stream
