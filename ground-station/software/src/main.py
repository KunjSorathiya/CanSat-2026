from __future__ import annotations

import argparse
from pathlib import Path

from logger import PacketLog, detect_missing
from telemetry import parse_packet


def process_lines(lines: list[str], log: PacketLog, expected_team: str | None = None) -> tuple[int, int]:
    received = 0
    missing = 0
    previous: int | None = None
    for line in lines:
        raw = line.rstrip("\r\n")
        if not raw:
            continue
        result = parse_packet(raw, expected_team)
        log.append(raw, result.record, result.error)
        if result.record is None:
            continue
        received += 1
        missing += detect_missing(previous, result.record.packet_number)
        previous = result.record.packet_number
    return received, missing


def main() -> int:
    parser = argparse.ArgumentParser(description="CanSat telemetry replay and logger")
    parser.add_argument("input", type=Path, help="file containing raw telemetry packets")
    parser.add_argument("--output", type=Path, default=Path("logs"))
    parser.add_argument("--team", default=None)
    args = parser.parse_args()
    log = PacketLog(args.output / "raw_packets.tsv", args.output / "telemetry.csv")
    received, missing = process_lines(args.input.read_text(encoding="utf-8").splitlines(), log, args.team)
    print(f"received={received} missing={missing}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
