#!/usr/bin/env python3
"""Watch a Pico's USB serial output and tee it to a file, across reboots.

Bench aid. `miniterm` does the same job interactively, but this one survives the thing you
are most often trying to catch: a vehicle that resets. A rebooting Pico drops off USB and
re-enumerates, which kills the open handle -- and on Windows a dead handle usually returns
empty reads rather than raising, so a naive watcher goes silent at exactly the moment the
evidence appears and looks like a vehicle that never rebooted at all.

So this reopens the port when it vanishes, and prints a marked line when it does. That line
IS the evidence: a Pico whose port drops and comes back has reset.

    python tools/watch_serial.py                 # list the ports and say which is which
    python tools/watch_serial.py COM6            # watch COM6, tee to serial-COM6.log
    python tools/watch_serial.py COM6 boot.log   # watch COM6, tee to boot.log

Ctrl+C stops it. The baud rate is ignored by USB CDC, so it is not a setting you can get
wrong.
"""
import sys
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("pyserial is not installed for THIS interpreter. Run:\n"
             "    python -m pip install pyserial\n"
             "(the -m form installs into the python you are actually running, which is the\n"
             "usual cause of 'I just installed it and it still says it is missing')")


def describe(port):
    """A guess at what is on the other end, so nobody has to count USB sockets."""
    name = (port.description or "") + " " + (port.manufacturer or "")
    if "Pico" in name or "RP2" in name or "Board CDC" in name:
        return "Raspberry Pi Pico"
    return port.description or "unknown device"


def list_them():
    ports = list(list_ports.comports())
    if not ports:
        print("No serial ports found. Is the Pico plugged in, and did it finish booting?")
        return
    print("Serial ports:\n")
    for p in ports:
        print("  %-8s  %s" % (p.device, describe(p)))
    print("\nBoth Picos look identical here. To tell them apart, watch one:")
    print("    python tools/watch_serial.py %s" % ports[0].device)
    print("\n  The VEHICLE prints a block starting '===== CanSat 2026 - flight firmware'.")
    print("  The BRIDGE prints lines starting '#state=RX' or '#bridge=online'.")


def emit(log, text):
    """One line to both destinations, so the log and the screen never disagree."""
    log.write(text.encode("utf-8", "replace"))
    sys.stdout.write(text)
    sys.stdout.flush()


def watch(device, log_path):
    print("Watching %s -- writing to %s." % (device, log_path))
    print("Reopens the port if the board resets. Ctrl+C to stop.\n")
    with open(log_path, "ab", 0) as log:
        emit(log, "\n===== watch_serial %s on %s =====\n"
             % (time.strftime("%Y-%m-%d %H:%M:%S"), device))
        port = None
        connected = False
        while True:
            if port is None:
                try:
                    port = serial.Serial(device, 115200, timeout=0.5)
                except (serial.SerialException, OSError):
                    # Not there yet, or still re-enumerating. Say so once, then wait quietly.
                    if connected:
                        emit(log, "\n[%s] --- PORT LOST: the board reset or was unplugged ---\n"
                             % time.strftime("%H:%M:%S"))
                        connected = False
                    time.sleep(0.2)
                    continue
                emit(log, "\n[%s] --- port open ---\n" % time.strftime("%H:%M:%S"))
                connected = True
            try:
                data = port.read(4096)
            except (serial.SerialException, OSError):
                # The handle died under us: the board went away mid-read.
                try:
                    port.close()
                except Exception:
                    pass
                port = None
                continue
            if data:
                log.write(data)
                sys.stdout.write(data.decode("utf-8", "replace"))
                sys.stdout.flush()
                continue
            # An empty read is normal (the timeout expired with nothing to say). But a
            # handle to a device that has gone away reads empty forever, which is exactly
            # how a reboot hides itself. Ask the OS whether the port still exists.
            if device not in [p.device for p in list_ports.comports()]:
                emit(log, "\n[%s] --- PORT LOST: the board reset or was unplugged ---\n"
                     % time.strftime("%H:%M:%S"))
                try:
                    port.close()
                except Exception:
                    pass
                port = None
                connected = False


def main():
    if len(sys.argv) < 2:
        list_them()
        return
    device = sys.argv[1]
    log_path = sys.argv[2] if len(sys.argv) > 2 else "serial-%s.log" % device
    try:
        watch(device, log_path)
    except KeyboardInterrupt:
        print("\nStopped. The log is in %s" % log_path)


if __name__ == "__main__":
    main()
