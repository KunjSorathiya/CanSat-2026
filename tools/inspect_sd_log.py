#!/usr/bin/env python3
"""Read a prepared card's FAT32 directly and report where the log file physically lives.

WHY THIS EXISTS
---------------
The firmware maps the log file through its cluster chain, so a fragmented file is handled
rather than refused - up to MAX_EXTENTS separate runs, past which it gives up. This reports
what the chain actually looks like, so a card can be checked at a desk instead of at the
bench with everything wired.

It is also the second implementation of that walk. It reads the same structures over a
different code path in a different language, exactly as the three telemetry parsers are held
to one fixture. If this and the firmware disagree about a card, one of them has a bug and the
disagreement says so out loud. If they agree, the answer is the card's.

It is also the pre-flight check `prepare_sd_card.py` cannot do: `fsutil file layout` works
only on NTFS, so there is no built-in Windows tool that will answer this for a FAT32 card.

WHAT IT NEEDS
-------------
Raw read access to the volume. On Windows a removable volume often opens without
elevation; a fixed one generally will not. If it reports access denied, re-run from an
Administrator prompt. It only ever reads.

USAGE
-----
    python tools/inspect_sd_log.py K:
    python tools/inspect_sd_log.py /dev/sdb1 --name "FLIGHT  CSV"
"""

import argparse
import os
import sys

SECTOR = 512
# Must match FatVolume::kMaxExtents in
# firmware/flight-computer/include/flight/fat_volume.hpp.
MAX_EXTENTS = 16
DEFAULT_NAME_83 = "FLIGHT  CSV"  # eight name bytes, three extension bytes, space padded


def u16(b, off):
    return b[off] | (b[off + 1] << 8)


def u32(b, off):
    return b[off] | (b[off + 1] << 8) | (b[off + 2] << 16) | (b[off + 3] << 24)


class Volume:
    """Sector reads against a raw volume handle, aligned as Windows insists."""

    def __init__(self, path):
        self.f = open(path, "rb", buffering=0)

    def sector(self, lba):
        self.f.seek(lba * SECTOR)
        data = self.f.read(SECTOR)
        if len(data) != SECTOR:
            raise IOError(f"short read at LBA {lba}")
        return data

    def close(self):
        self.f.close()


def parse_bpb(boot):
    if u16(boot, 11) != SECTOR:
        raise ValueError(f"sector size is {u16(boot, 11)}, not {SECTOR}")
    if u16(boot, 17) != 0 or u16(boot, 22) != 0:
        raise ValueError("not FAT32 (root entry count or 16-bit FAT size is non-zero)")
    spc = boot[13]
    reserved = u16(boot, 14)
    num_fats = boot[16]
    fat_sectors = u32(boot, 36)
    root_cluster = u32(boot, 44)
    if not (spc and reserved and num_fats and fat_sectors and root_cluster >= 2):
        raise ValueError("BPB fields are not sane")
    return {
        "spc": spc,
        "fat_lba": reserved,
        "data_lba": reserved + num_fats * fat_sectors,
        "root_cluster": root_cluster,
        "cluster_bytes": spc * SECTOR,
    }


def cluster_lba(bpb, cluster):
    return bpb["data_lba"] + (cluster - 2) * bpb["spc"]


def fat_entry(vol, bpb, cluster):
    off = cluster * 4
    sec = vol.sector(bpb["fat_lba"] + off // SECTOR)
    return u32(sec, off % SECTOR) & 0x0FFFFFFF


def find_entry(vol, bpb, name_83):
    """Walk the root directory for an 8.3 name. Returns (first_cluster, size)."""
    cluster = bpb["root_cluster"]
    want = name_83.encode("ascii")
    seen = 0
    while 2 <= cluster < 0x0FFFFFF8 and seen < 4096:
        seen += 1
        for s in range(bpb["spc"]):
            sec = vol.sector(cluster_lba(bpb, cluster) + s)
            for off in range(0, SECTOR, 32):
                e = sec[off:off + 32]
                if e[0] == 0x00:
                    return None, None          # end of directory
                if e[0] == 0xE5:
                    continue                   # deleted
                if (e[11] & 0x0F) == 0x0F:
                    continue                   # long-name fragment
                if e[11] & 0x10:
                    continue                   # subdirectory
                if e[0:11] != want:
                    continue
                return (u16(e, 20) << 16) | u16(e, 26), u32(e, 28)
        cluster = fat_entry(vol, bpb, cluster)
    return None, None


def chain_extents(vol, bpb, first_cluster, size):
    """Walk the file's chain, returning a list of (start_cluster, count) runs."""
    expected = max(1, (size + bpb["cluster_bytes"] - 1) // bpb["cluster_bytes"])
    runs = [[first_cluster, 1]]
    current = first_cluster
    walked = 1
    while walked <= expected + 2:
        nxt = fat_entry(vol, bpb, current)
        if nxt >= 0x0FFFFFF8:
            break
        if nxt == current + 1:
            runs[-1][1] += 1
        else:
            runs.append([nxt, 1])
        current = nxt
        walked += 1
    return runs, expected, walked


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("volume", help="K: on Windows, or /dev/sdb1 on Linux")
    ap.add_argument("--name", default=DEFAULT_NAME_83,
                    help='on-disk 8.3 name, space padded (default "FLIGHT  CSV")')
    args = ap.parse_args()

    path = args.volume
    if os.name == "nt":
        path = r"\\.\%s" % path.rstrip("\\").rstrip(":") + ":"

    try:
        vol = Volume(path)
    except PermissionError:
        print("error: raw volume access denied.\n"
              "On Windows this needs an Administrator prompt. It only reads.",
              file=sys.stderr)
        return 2
    except OSError as e:
        print(f"error: cannot open {path}: {e}", file=sys.stderr)
        return 2

    try:
        bpb = parse_bpb(vol.sector(0))
    except (ValueError, IOError) as e:
        print(f"error: {e}", file=sys.stderr)
        print("A card prepared for this project is FAT32 with 512-byte sectors.",
              file=sys.stderr)
        vol.close()
        return 2

    print(f"FAT32 volume: {bpb['spc']} sectors/cluster "
          f"({bpb['cluster_bytes'] // 1024} KiB clusters), "
          f"FAT at LBA {bpb['fat_lba']}, data at LBA {bpb['data_lba']}")

    first, size = find_entry(vol, bpb, args.name)
    if first is None:
        print(f'"{args.name.strip()}" is not in the root directory.')
        vol.close()
        return 1

    runs, expected, walked = chain_extents(vol, bpb, first, size)
    lba = cluster_lba(bpb, first)
    print(f'"{args.name.strip()}": {size} bytes, first cluster {first}, first LBA {lba}')
    print(f"  clusters expected from the size: {expected}")
    print(f"  clusters walked in the chain   : {walked}")
    print(f"  extents (runs of consecutive clusters): {len(runs)}")

    if len(runs) == 1:
        print("\nCONTIGUOUS. The firmware will accept this file.")
        print(f"  It should report first LBA {lba} and {walked * bpb['spc']} blocks.")
        vol.close()
        return 0

    print("\nFRAGMENTED. The firmware will refuse it, correctly.")
    for i, (start, count) in enumerate(runs[:10]):
        print(f"  run {i}: cluster {start} .. {start + count - 1}  "
              f"({count} clusters, LBA {cluster_lba(bpb, start)})")
    if len(runs) > 10:
        print(f"  ... and {len(runs) - 10} more")
    print("\nFree space on this card is broken up. Quick-format it FAT32 and run\n"
          "prepare_sd_card.py on the empty volume: contiguity is then guaranteed,\n"
          "because the whole volume is one free run.")
    vol.close()
    return 1


if __name__ == "__main__":
    sys.exit(main())
