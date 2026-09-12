#!/usr/bin/env python3
"""Prepare a microSD card so the flight log lands inside a file a PC can open.

WHY THIS EXISTS
---------------
The vehicle writes its log as raw 512-byte blocks with no filesystem, because FAT metadata
updates are not atomic: a brownout or a hard landing during one can cost the whole
recording. That safety is worth keeping. But a card written that way does not mount, and
flight data nobody can read is not evidence.

So the filesystem work happens here, once, before the flight - and none of it during. This
script creates a fixed-size FLIGHT.CSV on an already-FAT32-formatted card. The firmware
finds where that file's blocks physically live and writes straight into them. The directory
entry, the FAT chain and the file size never change while the vehicle is in the air, so
there is no metadata to corrupt. Afterwards the card mounts normally and FLIGHT.CSV opens
in a spreadsheet.

WHAT IT DOES NOT DO
-------------------
It does not format the card. Formatting is destructive and platform-specific, and this
script deliberately will not do it for you - see the instructions it prints. It only
creates the file, then verifies the result.

USAGE
-----
    python tools/prepare_sd_card.py E:\\
    python tools/prepare_sd_card.py /media/you/CANSAT --size-mb 64

Run it again after every flight, once you have copied the data off. Re-running deletes and
recreates the file, which resets the log.

If the firmware reports the file as fragmented, that is usually nothing to act on: it tracks
up to FatVolume::kMaxExtents (16) separate runs and maps log block numbers through them, so
a file in a few pieces costs it nothing. Only a file broken into more runs than that is
refused. If it is, free space on the card is genuinely broken up - quick-format it FAT32 and
run this on the empty card: contiguity is then guaranteed, because the whole volume is one
free run. Use tools/inspect_sd_log.py to see the actual extent count.
"""

import argparse
import os
import shutil
import sys

FILENAME = "FLIGHT.CSV"
DEFAULT_SIZE_MB = 64
# Must match kMinLogBlocks in firmware/flight-computer/src/pico/sd_logger.cpp.
MIN_BLOCKS = 8192
BLOCK = 512
# Must match FatVolume::kMaxExtents in
# firmware/flight-computer/include/flight/fat_volume.hpp. A file in this many runs or
# fewer is mapped through the extent list and accepted; only more than this is refused.
MAX_EXTENTS = 16


def human(n: int) -> str:
    for unit in ("B", "KiB", "MiB", "GiB"):
        if n < 1024 or unit == "GiB":
            return f"{n:.1f} {unit}" if unit != "B" else f"{n} B"
        n /= 1024.0
    return str(n)


def count_extents(path: str):
    """Not checkable from here on a FAT32 card, and it is worth saying why.

    `fsutil file layout` is the obvious Windows tool and it refuses: "A local NTFS volume is
    required for this operation." There is no built-in way to ask a FAT32 volume how a file
    is laid out. Reading the FAT itself needs raw volume access, which needs elevation, and
    silently demanding that of anyone preparing a card is worse than admitting the gap.

    So this returns None, always, and `tools/inspect_sd_log.py` does the real job from an
    Administrator prompt when an answer is actually needed. The firmware checks regardless
    and names the reason when it refuses.
    """
    return None


def format_instructions(target: str) -> str:
    return f"""
The card must be FAT32 with 512-byte sectors before this script can prepare it.
This script will not format it for you: formatting destroys data, and doing that
silently to whatever drive letter was typed is not a risk worth taking.

  Windows   Right-click the drive, Format..., File system FAT32,
            Allocation unit size 32 kilobytes, Quick Format on.
            Cards over 32 GB: Windows hides FAT32 - use the SD Association's
            official SD Memory Card Formatter, then reformat as FAT32.

  Linux     sudo mkfs.vfat -F 32 -s 64 /dev/sdX1      (the PARTITION, not the disk)

  macOS     diskutil eraseVolume FAT32 CANSAT /dev/diskNsN

Then run this script again against {target}
"""


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("target", help="the mounted card, e.g. E:\\ or /media/you/CANSAT")
    ap.add_argument("--size-mb", type=int, default=DEFAULT_SIZE_MB,
                    help=f"log file size in MiB (default {DEFAULT_SIZE_MB})")
    ap.add_argument("--force", action="store_true",
                    help="recreate the file even if one already exists")
    args = ap.parse_args()

    target = args.target
    if not os.path.isdir(target):
        print(f"error: {target} is not a mounted directory", file=sys.stderr)
        return 2

    size_bytes = args.size_mb * 1024 * 1024
    blocks = size_bytes // BLOCK
    if blocks < MIN_BLOCKS:
        print(f"error: --size-mb {args.size_mb} is {blocks} blocks; the firmware needs at "
              f"least {MIN_BLOCKS} ({MIN_BLOCKS * BLOCK // (1024*1024)} MiB)",
              file=sys.stderr)
        return 2

    path = os.path.join(target, FILENAME)
    if os.path.exists(path) and not args.force:
        existing = os.path.getsize(path)
        print(f"{FILENAME} already exists ({human(existing)}).")
        print("Copy any flight data off it first, then re-run with --force to recreate it.")
        return 1

    free = shutil.disk_usage(target).free
    if os.path.exists(path):
        free += os.path.getsize(path)
    if free < size_bytes:
        print(f"error: {human(free)} free, {human(size_bytes)} needed.", file=sys.stderr)
        print(format_instructions(target), file=sys.stderr)
        return 2

    # Delete before recreating rather than truncating in place. Truncating frees the old
    # clusters and then immediately asks for them back, and the filesystem is under no
    # obligation to return the same run -- which is exactly how a file that was contiguous
    # on its first creation came back fragmented after a --force. Deleting first gives the
    # allocator a clean look at the largest free extent.
    if os.path.exists(path):
        os.remove(path)

    # Write in one continuous pass and never seek. A sparse file created by seeking would
    # have no allocated clusters at all, and the firmware would refuse it - correctly,
    # since there would be nothing there to write into.
    print(f"Creating {path} ({human(size_bytes)}, {blocks} blocks)...")
    chunk = b" " * (1024 * 1024)
    written = 0
    try:
        with open(path, "wb") as f:
            while written < size_bytes:
                n = min(len(chunk), size_bytes - written)
                f.write(chunk[:n])
                written += n
            f.flush()
            os.fsync(f.fileno())
    except OSError as e:
        print(f"error: {e}", file=sys.stderr)
        print(format_instructions(target), file=sys.stderr)
        return 2

    actual = os.path.getsize(path)
    if actual != size_bytes:
        print(f"error: wrote {human(actual)}, expected {human(size_bytes)}", file=sys.stderr)
        return 2

    extents = count_extents(path)
    if extents == 1:
        piece = "in one piece - verified"
    elif extents is None:
        piece = ("size verified. Contiguity is not checkable from here on FAT32 - "
                 "run tools/inspect_sd_log.py to confirm it, or let the "
                 "firmware report it")
    elif extents <= MAX_EXTENTS:
        piece = f"in {extents} pieces - fine, the firmware maps up to {MAX_EXTENTS}"
    else:
        piece = f"in {extents} pieces - TOO FRAGMENTED"

    # Only more runs than the firmware can track is an actual failure. A file in a few
    # pieces is mapped through the extent list and costs nothing, so it is not reported
    # as a problem here: telling an operator to reformat a flight-ready card is the way
    # the pre-flight step gets skipped for real.
    if extents is not None and extents > MAX_EXTENTS:
        print(f"""
PROBLEM: {FILENAME} is {human(actual)} but lands in {extents} separate runs, more
than the {MAX_EXTENTS} the firmware tracks. It will refuse the file.

Free space on this card is broken up. Quick-format it FAT32 again - which empties it
completely - and run this script on the clean card. Contiguity is then guaranteed,
because the whole volume is one free run.
""")
        return 2

    print(f"""
Done. {FILENAME} is {human(actual)}, {piece}.

  1. Eject the card properly, then put it in the vehicle.
  2. The firmware locates this file and writes records into its blocks. It refuses to
     start if the file is missing, too small, or in more than {MAX_EXTENTS} separate runs -
     it never falls back to a guessed address, because that would destroy the filesystem.
     A file in a few runs is mapped through the extent list and is fine.
  3. After the flight, mount the card and open {FILENAME}.

     The first two lines are the log's own header blocks and are not data. Skip them.
     The third line is the column header. Records follow, one per line, space-padded.
     Trailing lines that are entirely blank are unused space, not lost records.

  4. Copy the file off, then run this script again with --force before the next flight.

If the firmware reports the file as fragmented, check the count before acting: up to
{MAX_EXTENTS} runs is fine and needs nothing done. Only past that is the card's free space
broken up enough to matter - reformat it and re-run this script on the empty card.
""")
    return 0


if __name__ == "__main__":
    sys.exit(main())
