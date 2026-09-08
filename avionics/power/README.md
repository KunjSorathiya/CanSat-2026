# Power

One battery, one rail, and three parts that are not fitted yet.

**Status: 2026-09-08.** The rail is measured and holds under every load tried. The battery
end of the path is incomplete.

---

## The architecture, after it got simpler twice

```text
LiPo 1S  →  SW1  →  D1 (Schottky)  →  Pico VSYS  →  Pico 3V3(OUT)  →  everything
 3.7 V     switch    stops USB          pin 39         pin 36          one rail
1500 mAh  NOT FITTED  back-powering    2.6–5.5 V      3.28–3.30 V      no second rail
                       NOT FITTED                       measured
```

**There is no external regulator, and none is needed.** That was not the plan. The design
spent weeks blocked on selecting one, because:

- the AMS1117-3.3 was assessed and **rejected** — a full cell at ≈4.2 V does not clear its
  high-load dropout, and its output sits below what the microSD reader's *listing* claimed
  to need;
- the microSD reader's supply was believed to be 4.5–5.5 V, which would have forced a boost
  stage.

**Both dissolved on inspection and measurement.** The delivered microSD board has no
regulator and no level shifter, its supply pin is printed `3V3`, and its entire parts list
is four 10 kΩ pull-ups and two capacitors — the listing described a different product
([D.1](../../documentation/hardware/receiving-inspection.md), [sd-module-analysis.md](../../documentation/hardware/sd-module-analysis.md)).
Then the Pico's own regulator was measured carrying every load without strain. One rail,
no boost, no external part.

---

## What has been measured

From [bring-up-record.md](../../documentation/testing/bring-up-record.md), gates 2, 5 and 6.

| Load | Rail voltage | Droop | Note |
|---|---|---|---|
| 45 back-to-back radio transmits, 15 s | **3.28–3.29 V** | 0.3 % | 45 sent, 0 failed |
| 100 % microSD write duty, 10 s | **3.28–3.30 V** | 0.6 % | 297–367 writes/s, far beyond anything the mission asks |

**The two together have never been run.** The radio draws ~120 mA transmitting and the GPS
~67 mA, both from this same rail. Gate 7 is where that gets settled, and until it does this
gate stays amber rather than green: the individual questions are closed, the total is not.

---

## What is not fitted

Four items, all held or cheap, and two of them are mandatory requirements.

| Item | Requirement | Why it matters | State |
|---|---|---|---|
| **Manual ON/OFF switch** | [PWR-001](../../documentation/requirements/requirements.md), mandatory | There is no way to turn the vehicle on and off | Held, not fitted |
| **Power LED**, immediate | PWR-002, PWR-003, mandatory | Must light **the instant the switch closes**, so it goes on the rail and not on a GPIO — a firmware-driven LED is not immediate | Held, not fitted |
| **Schottky diode**, 1 A | Pico datasheet §4.5 | Without it **USB back-powers the LiPo**. Until it is in, the battery switch must be OFF whenever a USB cable is connected — which is most of bring-up | **Not bought.** ~₹10, the only outstanding purchase |
| **Battery divider**, 33 kΩ / 33 kΩ → GP26 | — | `battery_divider_ratio` stays 0 and the firmware reports the raw pin voltage rather than inventing a scale | Resistors held, not fitted |

**Not a 1N4001.** A silicon diode drops 0.7 V for nothing here; the part is a `1N5817`,
`SS14` or `SS34`.

---

## The decoupling is not decoration

A long supply jumper made **every** microSD write fail while every read passed, and the same
fault on the RA-02's supply made a healthy radio fail eight transmits in a row
([F-10](../../documentation/testing/bring-up-record.md#findings)).

| Capacitor | Where | Why |
|---|---|---|
| 2 × 100 µF electrolytic | microSD `3V3`/`GND`, at the module's own pins | 200 µF against a ~50 µF requirement |
| 10 µF electrolytic | RA-02 `3.3V`/`GND` | PA key-up is 1.5 mA to 87 mA in microseconds |
| 3 × 0.1 µF ceramic (`104`) | microSD, RA-02, LM393 | Every bulk capacitor of every type is too slow for those edges |

The IMU, barometer and GPS are deliberately without a `104`
([D-8](../../documentation/hardware/assembly-procedure.md)). Electrolytics are polarised —
the stripe is the **negative** leg.

**A clean bench test does not mean the fault is absent.** F-10 was intermittent and
load-dependent: it appeared and vanished across five bench runs. The flight symptom is an
empty log or a radio that stops.

---

## Open items

| Item | Note |
|---|---|
| **Series current draw** | Four bench sessions have skipped it. The 3.3 V test link (`J1`) exists to make it a one-minute measurement |
| **Radio and card drawing simultaneously** | Gate 7. Each holds alone; the sum is untested |
| **Battery life** | Needs the series current above. No estimate should be quoted until then |
| **Write-transient current** | The rail *voltage* is measured under write load; the *current* is not |
| **Charging and protection** | [PWR-008](../../documentation/requirements/requirements.md). A balance charger is held; the procedure is not written |
| **Brownout and reset behaviour under load** | PWR-009 |

---

## Safety

- **Pico pin 40 is `VBUS`, live 5 V. Nothing on this board tolerates 5 V.** It is wired to
  nothing and must stay that way.
- **`AGND` (pin 33) is a separate plane.** Tie it to ground exactly once, beside pin 38.
- **Battery switch OFF whenever USB is connected**, until the Schottky is fitted.
- Charge the LiPo on a non-flammable surface, attended.

Related: [electrical-architecture.md](../../documentation/design/electrical-architecture.md) ·
[power-path diagram](../../documentation/hardware/diagrams/power-path-battery-to-pico.svg) ·
[bring-up-record.md](../../documentation/testing/bring-up-record.md) ·
[avionics/README.md](../README.md)
