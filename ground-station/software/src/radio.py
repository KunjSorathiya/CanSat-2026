from __future__ import annotations

from collections.abc import Iterable
from typing import Protocol


class PacketSource(Protocol):
    def packets(self) -> Iterable[str]:
        ...


class FilePacketSource:
    def __init__(self, path: str) -> None:
        self.path = path

    def packets(self) -> Iterable[str]:
        with open(self.path, encoding="utf-8") as stream:
            yield from stream
