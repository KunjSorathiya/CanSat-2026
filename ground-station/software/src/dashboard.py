"""Live Tkinter dashboard for the ground station.

The GUI never does I/O. A background ``GroundStation`` thread fills a queue; the UI
drains it on a periodic ``after`` callback and repaints. Matplotlib is used for the
time-series plots when it is installed; without it the dashboard still runs and shows
every numeric field.
"""

from __future__ import annotations

import tkinter as tk
from collections import deque
from tkinter import ttk
from typing import Optional

from app import GroundStation

try:  # optional
    from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
    from matplotlib.figure import Figure

    _HAVE_MPL = True
except Exception:  # pragma: no cover - env dependent
    _HAVE_MPL = False

_NUMERIC_FIELDS = [
    ("mission_time", "Mission time"),
    # Shown even though the operator supplies --team: without that flag any team is
    # accepted, and then this is the only place the identity on the air is visible.
    ("team_id", "Team"),
    ("packet_number", "Packet #"),
    ("altitude", "Altitude (m)"),
    ("pressure", "Pressure (Pa)"),
    ("temperature", "Temp (C)"),
    ("roll", "Roll (deg)"),
    ("pitch", "Pitch (deg)"),
    ("yaw", "Yaw (deg)"),
    # Yaw alone is ambiguous, so the reference is shown beside it rather than left for
    # the operator to infer from a number that looks the same either way.
    ("yaw_reference", "Yaw reference"),
    ("heading", "Heading (deg)"),
    ("ax", "Accel X (m/s2)"),
    ("ay", "Accel Y (m/s2)"),
    ("az", "Accel Z (m/s2)"),
    ("gps_lat", "GPS lat"),
    ("gps_lon", "GPS lon"),
    ("gps_alt", "GPS alt (m)"),
    ("gps_fix", "GPS fix"),
    ("mode", "Flight state"),
    ("fault_count", "Active faults"),
    # The two flags an operator is standing on a pad waiting for. The vehicle transmits
    # them in every packet as CAL and ARM, the web console has always shown them, and this
    # display -- the one the runbook opens for a live flight -- did not.
    ("calibrated", "Calibrated"),
    ("armed", "Armed"),
    # No battery row. The vehicle measures its pack voltage and raises a fault when it is
    # low, but the voltage itself is never transmitted -- the packet's optional fields are
    # MODE, FAULTS, CAL, ARM and YR, and nothing else. This row used to read the bridge's
    # status dictionary for a "battery" key that nothing has ever written, from a Pico that
    # has no battery sense at all, so it could only ever display "n/a". A permanently empty
    # field is worse than an absent one: it reads as a link that is not reporting rather
    # than a quantity that is not sent. A low pack still shows up here as an active fault.
]

_VALIDATION_FIELDS = [
    # The headline three first: how many packets arrived, how many were accepted, and how
    # many were not. The replay command has always printed them; the live display showed
    # every category of fault without ever showing the totals they are faults out of.
    ("received", "Received"),
    ("accepted", "Accepted"),
    ("rejected", "Rejected"),
    ("missing", "Missing"),
    ("duplicates", "Duplicates"),
    ("out_of_order", "Out of order"),
    ("wrong_team", "Wrong team"),
    ("restarts", "Vehicle restarts"),
    ("timestamp_regressions", "Clock regressions"),
    # An implausible fix is kept and flagged rather than dropped, so the count is the only
    # place it is visible at all.
    ("gps_rejected", "GPS rejected"),
]

_LINK_FIELDS = [
    ("connected", "Connected"),
    ("rate_hz", "Rate (Hz)"),
    # The rulebook's 1 Hz is a minimum, and reading a rate is not the same as judging it.
    # The vehicle refuses to build at or below 1 Hz; this is the receiving end saying
    # whether what arrived actually cleared it. `None` means "not yet enough link to
    # judge", which is why the value is rendered rather than compared here.
    ("rate_meets_rulebook", ">= 1 Hz rulebook"),
    ("packets_ok", "Packets OK"),
    ("packets_invalid", "Packets invalid"),
    ("missing", "Missing"),
    ("duplicates", "Duplicates"),
    ("crc_errors", "CRC errors"),
    ("loss_pct", "Loss %"),
    ("seconds_since_rx", "Since last RX (s)"),
    # A bridge that is alive but hearing nothing still sends status frames. Without this,
    # that case looks identical to a dead serial link.
    ("status_frames", "Status frames"),
]

# The bridge reports the radio's own view of the link once a second. RSSI is what warns an
# operator that a link is running out of headroom while packet loss is still zero.
# The frame decoder's own view of the link, which is a different question from whether the
# packets inside those frames were any good. Absent on an unframed transport.
_FRAMING_FIELDS = [
    ("frames_ok", "Frames decoded"),
    ("crc_errors", "Frame CRC errors"),
    ("resyncs", "Resyncs"),
    ("overflows", "Oversized length"),
]

_BRIDGE_FIELDS = [
    ("radio", "Radio"),
    ("rssi", "RSSI (dBm)"),
    ("snr", "SNR (dB)"),
    ("frames", "Frames"),
    ("dropped", "Dropped (no host)"),
    # Reported by the bridge rather than assumed here: the rulebook's test and launch sync
    # words are one reflash apart, and an operator needs to see which one is actually in
    # use before the flight, not after it.
    ("sync", "Sync word"),
]

_MAX_POINTS = 600


class Dashboard:
    def __init__(self, station: GroundStation, poll_ms: int = 150,
                 title: str = "CanSat Ground Station") -> None:
        self.station = station
        self.poll_ms = poll_ms
        self.root = tk.Tk()
        self.root.title(title)
        self.root.minsize(720, 480)

        self._vars: dict[str, tk.StringVar] = {}
        self._t = deque(maxlen=_MAX_POINTS)
        self._series: dict[str, deque] = {
            k: deque(maxlen=_MAX_POINTS) for k in ("altitude", "pressure", "temperature")
        }
        self._build_layout()
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    # ------------------------------------------------------------------ layout
    def _build_layout(self) -> None:
        outer = ttk.Frame(self.root, padding=8)
        outer.pack(fill="both", expand=True)

        left = ttk.Frame(outer)
        left.pack(side="left", fill="y")

        link = ttk.LabelFrame(left, text="Link", padding=6)
        link.pack(fill="x", pady=(0, 6))
        for i, (key, label) in enumerate(_LINK_FIELDS):
            self._add_row(link, f"link.{key}", label, i)

        tele = ttk.LabelFrame(left, text="Telemetry", padding=6)
        tele.pack(fill="x")
        for i, (key, label) in enumerate(_NUMERIC_FIELDS):
            self._add_row(tele, f"tele.{key}", label, i)

        right = ttk.Frame(outer)
        right.pack(side="left", fill="both", expand=True, padx=(10, 0))

        validation = ttk.LabelFrame(left, text="Stream validation", padding=6)
        validation.pack(fill="x", pady=(6, 0))
        for i, (key, label) in enumerate(_VALIDATION_FIELDS):
            self._add_row(validation, f"val.{key}", label, i)

        bridge = ttk.LabelFrame(left, text="Bridge radio", padding=6)
        bridge.pack(fill="x", pady=(6, 0))
        for i, (key, label) in enumerate(_BRIDGE_FIELDS):
            self._add_row(bridge, f"bridge.{key}", label, i)

        framing = ttk.LabelFrame(left, text="Serial framing", padding=6)
        framing.pack(fill="x", pady=(6, 0))
        for i, (key, label) in enumerate(_FRAMING_FIELDS):
            self._add_row(framing, f"framing.{key}", label, i)

        # Logging health. A ground station that is receiving but not recording looks
        # perfectly healthy everywhere else, so it gets its own line.
        logs = ttk.LabelFrame(left, text="Logging", padding=6)
        logs.pack(fill="x", pady=(6, 0))
        self._add_row(logs, "log.status", "Status", 0)

        raw = ttk.LabelFrame(right, text="Latest packet", padding=6)
        raw.pack(fill="x")
        self._vars["latest_raw"] = tk.StringVar(value="--")
        ttk.Label(raw, textvariable=self._vars["latest_raw"], wraplength=520,
                  justify="left", font=("TkFixedFont", 9)).pack(anchor="w")

        self._plot = _PlotPanel(right) if _HAVE_MPL else _NoPlotPanel(right)

    def _add_row(self, parent: ttk.Frame, key: str, label: str, row: int) -> None:
        ttk.Label(parent, text=label).grid(row=row, column=0, sticky="w", padx=4, pady=1)
        var = tk.StringVar(value="--")
        self._vars[key] = var
        ttk.Label(parent, textvariable=var, font=("TkDefaultFont", 9, "bold")).grid(
            row=row, column=1, sticky="e", padx=4, pady=1
        )

    # -------------------------------------------------------------------- loop
    def run(self) -> None:
        self.station.start()
        self.root.after(self.poll_ms, self._tick)
        self.root.mainloop()

    def _tick(self) -> None:
        drained = 0
        newest_packet = None
        while drained < 1000:
            try:
                event = self.station.events.get_nowait()
            except Exception:
                break
            drained += 1
            if event.kind == "packet" and event.record is not None:
                newest_packet = event.record

        if newest_packet is not None:
            self._t.append(newest_packet.timestamp_ms / 1000.0)
            self._series["altitude"].append(newest_packet.altitude)
            self._series["pressure"].append(newest_packet.pressure)
            self._series["temperature"].append(newest_packet.temperature)

        self._repaint()
        self.root.after(self.poll_ms, self._tick)

    def _repaint(self) -> None:
        snap = self.station.snapshot()
        link = snap.get("link", {})
        for key, _ in _LINK_FIELDS:
            self._vars[f"link.{key}"].set(str(link.get(key, "--")))

        tele = snap.get("telemetry")
        if isinstance(tele, dict):
            self._vars["tele.mission_time"].set(tele.get("timestamp", "--"))
            self._vars["tele.packet_number"].set(str(tele.get("packet_number", "--")))
            for key in ("altitude", "pressure", "temperature", "roll", "pitch", "yaw",
                        "yaw_reference", "heading",
                        "ax", "ay", "az", "gps_lat", "gps_lon", "gps_alt", "gps_fix",
                        "mode", "fault_count"):
                value = tele.get(key)
                self._vars[f"tele.{key}"].set("--" if value is None else str(value))
        validation_stats = snap.get("validation", {})
        for key, _ in _VALIDATION_FIELDS:
            self._vars[f"val.{key}"].set(str(validation_stats.get(key, "--")))

        logging_state = snap.get("logging", {})
        if isinstance(logging_state, dict):
            errors = logging_state.get("write_errors", 0)
            if errors:
                self._vars["log.status"].set(
                    f"{errors} write error(s): {logging_state.get('last_error', '')}"[:60])
            else:
                self._vars["log.status"].set("recording")

        bridge_status = snap.get("bridge", {})
        for key, _ in _BRIDGE_FIELDS:
            self._vars[f"bridge.{key}"].set(str(bridge_status.get(key, "--")))

        framing_stats = snap.get("framing", {})
        for key, _ in _FRAMING_FIELDS:
            # "--" for an unframed transport: no decoder ran, so there is nothing to report
            # rather than a zero that would read as "nothing went wrong".
            self._vars[f"framing.{key}"].set(str(framing_stats.get(key, "--")))
        self._vars["latest_raw"].set(str(snap.get("latest_raw", "--")) or "--")

        self._plot.update(self._t, self._series)

    def _on_close(self) -> None:
        self.station.stop()
        self.root.destroy()


class _PlotPanel:  # pragma: no cover - requires matplotlib + display
    def __init__(self, parent: tk.Widget) -> None:
        self.fig = Figure(figsize=(5.2, 4.0), dpi=96)
        self.axes = {
            "altitude": self.fig.add_subplot(311),
            "pressure": self.fig.add_subplot(312),
            "temperature": self.fig.add_subplot(313),
        }
        for name, ax in self.axes.items():
            ax.set_ylabel(name[:4])
            ax.grid(True, alpha=0.3)
        self.fig.tight_layout()
        self.canvas = FigureCanvasTkAgg(self.fig, master=parent)
        self.canvas.get_tk_widget().pack(fill="both", expand=True, pady=(8, 0))
        self._lines = {name: ax.plot([], [])[0] for name, ax in self.axes.items()}

    def update(self, t, series) -> None:
        if not t:
            return
        xs = list(t)
        for name, ax in self.axes.items():
            ys = list(series[name])
            self._lines[name].set_data(xs[-len(ys):], ys)
            ax.relim()
            ax.autoscale_view()
        self.canvas.draw_idle()


class _NoPlotPanel:
    def __init__(self, parent: tk.Widget) -> None:
        ttk.Label(
            parent,
            text="(install matplotlib for live altitude / pressure / temperature plots)",
            foreground="#888",
        ).pack(anchor="w", pady=(8, 0))

    def update(self, t, series) -> None:  # noqa: D401 - no-op
        return


def run_dashboard(station: GroundStation, poll_ms: int = 150) -> None:
    Dashboard(station, poll_ms=poll_ms).run()
