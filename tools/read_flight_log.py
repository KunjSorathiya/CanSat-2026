#!/usr/bin/env python3
"""Extract the real records from a recovered FLIGHT.CSV, and split it into flights.

WHY THIS EXISTS
---------------
`FLIGHT.CSV` is pre-allocated at a fixed size and is never truncated, because changing a
file's length means writing FAT metadata and the vehicle refuses to do that in flight - a
brownout or a hard landing during a metadata update can cost the whole recording. The
consequence is that everything past the last record written is whatever was in those blocks
before: an earlier flight, or the bring-up diagnostic's test pattern. Opened in a
spreadsheet the file therefore reads as good data followed by garbage, and nothing on the
face of it says where one becomes the other.

The file already knows. `RawBlockLog` keeps two alternating header copies in the first two
blocks and rewrites them after every single record, so `next free lba` is always current.
This reads that header and stops exactly where the data stops.

It also splits the output by flight. `packet_number` restarts at 1 every boot, so several
runs accumulated in one file are separable without the firmware needing to know anything
about it.

It is the second implementation of this layout, like `inspect_sd_log.py` before it: if this
and the firmware ever disagree about a log, one of them has a bug and the disagreement says
so out loud.

WHAT IT NEEDS
-------------
A copy of FLIGHT.CSV. Copy it off the card first - this reads a file, not a raw volume, so
it needs no elevation and cannot touch the card. Use `inspect_sd_log.py` if what you want
is where the file physically lives.

USAGE
-----
    python tools/read_flight_log.py FLIGHT.CSV
    python tools/read_flight_log.py FLIGHT.CSV --out-dir logs
    python tools/read_flight_log.py FLIGHT.CSV --single      # one file, no splitting
"""

import argparse
import os
import sys

# Must match RawBlockLog in firmware/flight-computer/include/flight/raw_block_log.hpp
# and the header layout comment in src/raw_block_log.cpp.
BLOCK_SIZE = 512
MAGIC = 0x54415343  # 'CSAT' little-endian
VERSION = 2
HEADER_BLOCKS = 2

OFF_MAGIC = 0
OFF_VERSION = 4
OFF_BLOCK_SIZE = 6
OFF_NEXT_LBA = 8
OFF_BOOT_COUNT = 12
OFF_BLOCK_COUNT = 16
OFF_SEQUENCE = 20
OFF_CHECKSUM = 24
CHECKSUMMED_BYTES = 24


def u16(b, off):
    return b[off] | (b[off + 1] << 8)


def u32(b, off):
    return b[off] | (b[off + 1] << 8) | (b[off + 2] << 16) | (b[off + 3] << 24)


class BadHeader(Exception):
    pass


def parse_header(block):
    """Return (sequence, next_lba, boot_count, block_count) or raise BadHeader."""
    if len(block) < BLOCK_SIZE:
        raise BadHeader("short block")
    if u32(block, OFF_MAGIC) != MAGIC:
        raise BadHeader("magic is not CSAT")
    if u16(block, OFF_VERSION) != VERSION:
        raise BadHeader("version %d, expected %d" % (u16(block, OFF_VERSION), VERSION))
    if u16(block, OFF_BLOCK_SIZE) != BLOCK_SIZE:
        raise BadHeader("block size %d" % u16(block, OFF_BLOCK_SIZE))
    # The checksum covers bytes 0..23 only, so a torn write is detectable. This is the
    # check that makes "take the newer copy" safe: a half-written header has a higher
    # sequence number and is exactly the one you must not trust.
    stored = u32(block, OFF_CHECKSUM)
    actual = sum(block[:CHECKSUMMED_BYTES]) & 0xFFFFFFFF
    if stored != actual:
        raise BadHeader("checksum %d, computed %d - torn write" % (stored, actual))
    return (u32(block, OFF_SEQUENCE), u32(block, OFF_NEXT_LBA),
            u32(block, OFF_BOOT_COUNT), u32(block, OFF_BLOCK_COUNT))


def read_header(data):
    """Take whichever header copy is valid; if both are, the one with the higher sequence."""
    best = None
    problems = []
    for i in range(HEADER_BLOCKS):
        block = data[i * BLOCK_SIZE:(i + 1) * BLOCK_SIZE]
        try:
            parsed = parse_header(block)
        except BadHeader as exc:
            problems.append("  copy %d: %s" % (i, exc))
            continue
        if best is None or parsed[0] > best[0]:
            best = parsed
    if best is None:
        raise BadHeader("neither header copy is usable:\n" + "\n".join(problems))
    return best, problems


def record_text(block):
    """One record per block: text, a newline, then space padding."""
    end = block.find(b"\n")
    if end < 0:
        end = len(block)
    return block[:end].rstrip(b" ").decode("utf-8", errors="replace")


def packet_number(line):
    """Field 2 of the telemetry packet. None if this is not a numbered record."""
    parts = line.split(",")
    if len(parts) < 2:
        return None
    try:
        return int(parts[1])
    except ValueError:
        return None


def split_flights(records):
    """Split on packet_number restarting. Returns a list of lists."""
    flights = []
    current = []
    previous = None
    for line in records:
        number = packet_number(line)
        if number is not None:
            if previous is not None and number <= previous:
                flights.append(current)
                current = []
            previous = number
        current.append(line)
    if current:
        flights.append(current)
    return flights


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("path", help="a copy of FLIGHT.CSV")
    parser.add_argument("--out-dir", default=".", help="where to write the split files")
    parser.add_argument("--single", action="store_true",
                        help="write one file instead of splitting by flight")
    args = parser.parse_args()

    with open(args.path, "rb") as handle:
        data = handle.read()

    file_blocks = len(data) // BLOCK_SIZE
    print("file            %s" % args.path)
    print("size            %d bytes (%d blocks)" % (len(data), file_blocks))

    try:
        (sequence, next_lba, boots, block_count), problems = read_header(data)
    except BadHeader as exc:
        print("\nHEADER UNREADABLE: %s" % exc)
        print("\nA log whose header cannot be read has no trustworthy end. The bring-up")
        print("diagnostic's storage rows overwrite these blocks deliberately, so this is")
        print("the expected state for a card that has only ever run bring-up.")
        return 2

    for line in problems:
        print("note%s" % line[1:])

    print("header seq      %d" % sequence)
    print("boot count      %d" % boots)
    print("region blocks   %d" % block_count)
    print("next free block %d" % next_lba)

    if next_lba < HEADER_BLOCKS or next_lba > file_blocks:
        print("\nnext free block is outside the file - refusing to guess.")
        return 2

    records = []
    for lba in range(HEADER_BLOCKS, next_lba):
        text = record_text(data[lba * BLOCK_SIZE:(lba + 1) * BLOCK_SIZE])
        if text:
            records.append(text)

    print("records         %d" % len(records))
    if not records:
        print("\nThe header is valid and the log is empty: nothing has been written yet.")
        return 0

    # The firmware writes a column header as the first record of a fresh log, so it can be
    # carried onto every split file rather than left only on the first.
    column_header = records[0] if records[0].startswith("team_id,") else None
    body = records[1:] if column_header else records

    os.makedirs(args.out_dir, exist_ok=True)
    groups = [body] if args.single else split_flights(body)
    print("flights         %d" % len(groups))

    written = []
    for index, group in enumerate(groups, start=1):
        name = "flight.csv" if args.single else "flight-%d.csv" % index
        out_path = os.path.join(args.out_dir, name)
        with open(out_path, "w", encoding="utf-8", newline="\n") as handle:
            if column_header:
                handle.write(column_header + "\n")
            for line in group:
                handle.write(line + "\n")
        written.append((out_path, len(group)))

    print("")
    for out_path, count in written:
        print("wrote %-28s %d records" % (out_path, count))
    if column_header is None:
        print("\nNo column-header record was found, so the output has no header line.")
        print("That happens when the log resumed rather than starting fresh.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
