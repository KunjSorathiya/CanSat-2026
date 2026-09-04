"""CanSat ground-station entry point.

Subcommands:
  replay <file>   parse + validate + log a file of packets (headless), print a summary
  live            run the live pipeline from a serial bridge or a paced file replay,
                  with the Tk dashboard unless --no-dashboard is given
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

from app import GroundStation
from transport import FileReplayTransport


def _run_replay(args: argparse.Namespace) -> int:
    transport = FileReplayTransport(str(args.input), framed=args.framed)
    station = GroundStation(transport, expected_team=args.team, log_dir=str(args.output))
    station.run_forever()  # synchronous: file transport ends on EOF

    link = station.health.snapshot()
    stats = vars(station.validator.stats)
    print(
        "received={received} accepted={accepted} rejected={rejected} "
        "missing={missing} duplicates={duplicates} out_of_order={out_of_order} "
        "crc_errors={crc}".format(crc=link["crc_errors"], **stats)
    )
    if args.export:
        dest = station.export_csv(args.export)
        print(f"exported {dest}")
    return 0


def _run_live(args: argparse.Namespace) -> int:
    if args.replay:
        transport = FileReplayTransport(str(args.replay), rate_hz=args.rate,
                                        framed=args.framed)
    else:
        from transport import SerialTransport

        transport = SerialTransport(args.port, baud=args.baud, framed=args.framed)

    station = GroundStation(transport, expected_team=args.team, log_dir=str(args.output))

    if args.no_dashboard:
        station.start()
        # Every print here is flushed. Python block-buffers stdout when it is not a
        # terminal, and the runbook's headless form of this command is the one an operator
        # pipes somewhere -- into `tee`, or a log file, or a second window. Buffered, a
        # status line meant to appear every two seconds appears every few thousand, which
        # makes a working link look like a dead one.
        print("live pipeline running; Ctrl-C to stop", flush=True)
        try:
            while True:
                time.sleep(2.0)
                snap = station.snapshot()
                print(snap["link"])
                validation = snap.get("validation", {})
                if validation.get("restarts"):
                    print("  VEHICLE RESTARTED {restarts} time(s) — packet numbering "
                          "began again".format(**validation))
                bridge = snap.get("bridge", {})
                if bridge:
                    print("  bridge: radio={radio} rssi={rssi} snr={snr} "
                          "dropped={dropped} sync={sync}"
                          .format(radio=bridge.get("radio", "?"),
                                  rssi=bridge.get("rssi", "?"),
                                  snr=bridge.get("snr", "?"),
                                  dropped=bridge.get("dropped", "0"),
                                  sync=bridge.get("sync", "?")))
                logging_state = snap.get("logging", {})
                if logging_state.get("write_errors"):
                    print("  LOGGING FAULT: {write_errors} error(s), last: {last_error}"
                          .format(**logging_state))
                sys.stdout.flush()
        except KeyboardInterrupt:
            pass
        finally:
            station.stop()
        return 0

    from dashboard import run_dashboard

    run_dashboard(station)
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="CanSat ground station")
    sub = parser.add_subparsers(dest="command", required=True)

    replay = sub.add_parser("replay", help="offline parse/validate/log of a packet file")
    replay.add_argument("input", type=Path)
    replay.add_argument("--output", type=Path, default=Path("logs"))
    replay.add_argument("--team", default=None)
    replay.add_argument("--framed", action="store_true",
                        help="input uses the '$len,crc,payload' bridge framing")
    replay.add_argument("--export", type=Path, default=None,
                        help="copy the parsed CSV to this path when done")
    replay.set_defaults(func=_run_replay)

    live = sub.add_parser("live", help="live pipeline from serial or a paced file")
    live.add_argument("--port", default=None, help="serial port of the ground-station Pico")
    live.add_argument("--baud", type=int, default=115200)
    live.add_argument("--replay", type=Path, default=None,
                      help="use a file instead of a serial port")
    live.add_argument("--rate", type=float, default=2.0, help="replay packets/second")
    live.add_argument("--output", type=Path, default=Path("logs"))
    live.add_argument("--team", default=None)
    live.add_argument("--framed", action="store_true")
    live.add_argument("--no-dashboard", action="store_true")
    live.set_defaults(func=_run_live)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    if args.command == "live" and not args.replay and not args.port:
        print("live: provide --port <serial> or --replay <file>")
        return 2
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
