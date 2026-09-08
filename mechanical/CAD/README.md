# CAD

3D models of the structure, egg chamber and mounting.

**This directory is empty of models, and that is an accurate report of the project state:
nothing mechanical has been designed.** It is not empty for lack of a place to put things.

---

## Why it matters more than it looks

[MEC-006](../../documentation/requirements/requirements.md) is a **scored** requirement, not
a documentation nicety: *"CAD documentation must represent the completed design"*, verified
by comparing CAD, drawings and physical hardware. Section D of the rulebook — 30 points —
scores structural and material innovation, and a model is how that gets shown to a judge who
is not holding the vehicle.

The failure mode to avoid is the common one: build the article, then draw a model of what
you wish you had built. A model that disagrees with the hardware scores worse than no model,
because the discrepancy is what gets noticed.

---

## What goes here, when there is something

| File | Contents |
|---|---|
| `structure.*` | The body: envelope, board mounting, panel cutouts for USB, the switch and the LED |
| `egg-chamber.*` | The +7 cm chamber and its cushioning |
| `assembly.*` | Everything together, for interference checking and for the mass properties |
| `*.step` | A neutral export alongside every native file |

**Commit a STEP export next to every native file.** A native format nobody on the team can
open in two years is an archive of nothing, and the rulebook's evaluation does not care
which package it came from.

**Do not commit mesh exports of parts that are also here as solids.** An STL is a
derivative; it belongs with the print job, not with the design.

---

## Constraints the model has to satisfy

All four are already fixed, so the model can be started before anything else is decided:

| Constraint | Value | Source |
|---|---|---|
| Body envelope | 210 mm × 120 mm | [GEN-004](../../documentation/requirements/requirements.md) |
| Egg chamber allowance | +70 mm, 280 mm overall | GEN-004 |
| Mass | 450–550 g all-up | [GEN-005](../../documentation/requirements/requirements.md) |
| Vehicle board | 100 × 100 × 1.6 mm, edge-mounted | [mechanical/README.md](../README.md) |

The board orientation is the one that is easy to get wrong: **a 100 mm square does not fit
flat inside a 120 mm section.** The drawing that shows why is
[`../drawings/envelope-and-board-fit.svg`](../drawings/envelope-and-board-fit.svg).

Related: [mechanical/README.md](../README.md) · [drawings/](../drawings/) ·
[assembly-procedure.md](../../documentation/hardware/assembly-procedure.md)
