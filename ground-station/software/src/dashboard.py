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
    ("packet_number", "Packet #"),
    ("altitude", "Altitude (m)"),
    ("pressure", "Pressure (Pa)"),
    ("temperature", "Temp (C)"),
    ("roll", "Roll (deg)"),
    ("pitch", "Pitch (deg)"),
    ("yaw", "Yaw (deg)"),
    ("ax", "Accel X (m/s2)"),
    ("ay", "Accel Y (m/s2)"),
    ("az", "Accel Z (m/s2)"),
    ("gps_lat", "GPS lat"),
    ("gps_lon", "GPS lon"),
    ("gps_alt", "GPS alt (m)"),
    ("gps_fix", "GPS fix"),
    ("mode", "Flight state"),
    ("fault_count", "Active faults"),
    ("battery", "Battery (V)"),
]

_LINK_FIELDS = [
    ("connected", "Connected"),
    ("rate_hz", "Rate (Hz)"),
    ("packets_ok", "Packets OK"),
    ("packets_invalid", "Packets invalid"),
    ("missing", "Missing"),
    ("duplicates", "Duplicates"),
    ("crc_errors", "CRC errors"),
    ("loss_pct", "Loss %"),
    ("seconds_since_rx", "Since last RX (s)"),
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
                        "ax", "ay", "az", "gps_lat", "gps_lon", "gps_alt", "gps_fix",
                        "mode", "fault_count"):
                value = tele.get(key)
                self._vars[f"tele.{key}"].set("--" if value is None else str(value))
        bridge = snap.get("bridge", {})
        self._vars["tele.battery"].set(str(bridge.get("battery", "n/a")))
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
