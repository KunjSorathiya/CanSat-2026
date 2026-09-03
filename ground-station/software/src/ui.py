from __future__ import annotations

import tkinter as tk
from tkinter import ttk


class TelemetryUi:
    def __init__(self) -> None:
        self.root = tk.Tk()
        self.root.title("CanSat Ground Station")
        self.values: dict[str, tk.StringVar] = {}
        fields = ("connection", "received", "missing", "packet_number", "timestamp", "altitude", "pressure", "temperature", "roll", "pitch", "yaw", "ax", "ay", "az", "gps_status", "battery")
        for index, field in enumerate(fields):
            self.values[field] = tk.StringVar(value="--")
            ttk.Label(self.root, text=field.replace("_", " ").title()).grid(row=index, column=0, sticky="w", padx=8, pady=2)
            ttk.Label(self.root, textvariable=self.values[field]).grid(row=index, column=1, sticky="w", padx=8, pady=2)

    def update(self, values: dict[str, object]) -> None:
        for key, value in values.items():
            if key in self.values:
                self.values[key].set(str(value))

    def run(self) -> None:
        self.root.mainloop()
