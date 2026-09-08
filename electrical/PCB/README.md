# PCB

## The vehicle flies on perfboard, and that costs points

The vehicle board is a **100 × 100 mm single-sided universal perfboard**, hand-wired
point to point. It is built, every device on it works, and the whole
[bring-up record](../../documentation/testing/bring-up-record.md) was taken on it.

It is also, by the rulebook's own scoring, the cheapest large improvement left in the
project. Section E scores technical design and assembly quality, and
[scoring-assessment.md](../../documentation/project/scoring-assessment.md) puts a
fabricated PCB among the recommendations that recover more points than the egg payload
forgoes — for the price of a board order and a week of lead time.

**This directory is empty of layout files. That is the current state, not an oversight.**

---

## The layout that exists

The perfboard floorplan is designed, drawn to scale, and documented hole by hole:

| Artifact | Where |
|---|---|
| To-scale board layout | [`board-layout-to-scale.svg`](../../documentation/hardware/diagrams/board-layout-to-scale.svg) |
| Its generator | [`tools/gen_board_layout.py`](../../tools/gen_board_layout.py) |
| Floorplan reasoning — why each module sits where it does | [assembly-procedure.md](../../documentation/hardware/assembly-procedure.md#floorplan) |
| Every net, machine-readable | [`../schematics/vehicle-netlist.tsv`](../schematics/vehicle-netlist.tsv) |

A fabricated board would not start from nothing. The floorplan has already absorbed the
constraints that matter: SPI runs of 20–30 mm and near-vertical, the analogue ground tied
once beside the digital ground pin, the microphone's `AO` on a short paired run, the power
zone kept hard against pins 36/38/39, and supply capacitors at each module's own pins
rather than at a distant bulk point.

---

## What has to be decided before ordering one

| Decision | Note |
|---|---|
| **Board outline** | **Not 100 × 100 mm.** A 100 mm square does not fit the 120 mm rulebook section — see [mechanical/README.md](../../mechanical/README.md). Either 84.9 mm square for a horizontal deck, or a 100 mm board mounted edge-on as a spine |
| Layer count | Two layers is enough and is what a cheap fabricator quotes. A ground pour on the bottom layer does more for this design than any routing change |
| Module mounting | Soldered down or socketed. The [2026-09-05 decision](../../documentation/design/wiring.md#module-mounting) — Pico, RA-02 and microSD down, the other four on headers — was made for a perfboard and should be revisited for a PCB |
| Antenna | The u.FL connector is the most fragile joint on the vehicle. A PCB is the opportunity to give it a strain relief that perfboard cannot |
| The parts not yet fitted | Switch, Schottky, LEDs, divider resistors and the 3.3 V test link are all in the netlist. **Lay them out before ordering**, not after |
| Fabrication evidence | [GEN-002 and GEN-003](../../documentation/requirements/requirements.md) require the CanSat to be self-built and forbid prefabricated kits. An ordered PCB from the team's own layout is self-built; keep the Gerbers and the order record as the evidence |

---

## What would go here

| File | Contents |
|---|---|
| `cansat-vehicle.*` | The native layout project |
| `gerbers/` | Fabrication output, zipped as sent to the fabricator |
| `bom.csv` | Bill of materials as ordered, with the supplier's part numbers |
| `pick-and-place.csv` | Only if assembly is ordered; this design is hand-assembled |

**Commit the Gerbers actually sent, not a regenerated set.** They are the record of what was
manufactured, and they are also the fabrication evidence GEN-002 asks for.

Related: [electrical/README.md](../README.md) · [schematics/](../schematics/) ·
[mechanical/README.md](../../mechanical/README.md) ·
[scoring-assessment.md](../../documentation/project/scoring-assessment.md)
