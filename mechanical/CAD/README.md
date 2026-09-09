# CAD

The structure model, and what this repository can and cannot read of it.

| File | What it is | Readable here? |
|---|---|---|
| [`Cansat_D1.f3d`](Cansat_D1.f3d) | Fusion 360 archive — the **native, editable** model, and the three simulation studies | **No.** See below |
| [`Cansat_D1.step`](Cansat_D1.step) | STEP AP214 export of the same solid | **Yes**, and it is where every mechanical dimension in this repository comes from |

Exported 2026-09-09 by Autodesk Translation Framework v15.15. One solid, `Body1`, 56 faces,
millimetres.

---

## What the STEP says

```bash
python tools/cad_dimensions.py
```

```text
mechanical/CAD/Cansat_D1.step
  solids            Body1
  faces             56
  bounding box      115.0 x 118.5 x 110.0 mm
  largest first     118.5 x 115.0 x 110.0 mm
  cross-section     159.1 mm diagonal (the two smallest extents)
  circular features D5.1, D12.0, D16.0, D45.0 mm
```

Those numbers are not typed into any document. `check_doc_claims.py` reads them out of the
file and **fails the build if [mechanical/README.md](../README.md) disagrees**, and
[`tools/gen_envelope_drawing.py`](../../tools/gen_envelope_drawing.py) draws the envelope
from them. Edit the model, re-export the STEP, run `bash tools/build_host.sh`, and it will
name whichever numbers moved.

> **Re-export the STEP with every model change.** The `.f3d` is the source of truth for
> *editing*; the `.step` is the source of truth for everything this repository can check. A
> commit that updates one and not the other puts them out of step silently — the build only
> catches it once a documented figure actually changes.

---

## Why the `.f3d` cannot be read here

Two layers, and both are hard stops:

1. **The container.** It is a zip, but one using a compression method the standard tooling
   does not implement. Opening it raises `NotImplementedError` before any entry is read.
2. **The contents.** Even opened, the design and simulation data are Autodesk's own binary
   blobs — `SimMeshDB.BlobParts`, `Breps.BlobParts`, `ProteinAssets.BlobParts`. There is no
   documented format to parse.

What *is* visible is the archive's directory listing, which is how we know there are three
simulation studies: `Simulation Case`, `Simulation Case_2`, `Simulation Case_3`, plus a
`Simulation Studies` section.

**So the `.f3d` is committed for the team, not for the tooling.** It is the only way to
reopen and edit the design, and losing it would mean re-modelling from the STEP.

---

## Getting simulation results in

Nothing outside Fusion can read them, so they have to be exported. In order of usefulness:

**Three have been brought in already**, as Fusion HTML reports, and they live in
[`../simulation/`](../simulation/README.md) rather than here — a report is not a model.
That page carries the results and, more importantly, the four caveats that keep them
honest.

**What the export does not carry, and you will have to fetch by hand:** Fusion's *Result
Summary* table came out **completely empty** in all three reports, so the minimum safety
factor — the number the studies exist to produce — is not in them. Every figure on that
page was read off a colour-bar legend in a result plot.

**Say what was assumed, not just what came out.** A max von Mises figure without the
material, the constraint set and the load case is not a result anybody can check — and the
landing deceleration in particular is a modelling assumption, not a measured quantity.

---

## Conventions

- **Millimetres**, and the STEP reader refuses a file in anything else rather than
  silently reporting numbers a hundred times too small.
- **A STEP export beside every native file.** A native format nobody on the team can open
  in two years is an archive of nothing, and the rulebook's evaluation does not care which
  package it came from. [MEC-006](../../documentation/requirements/requirements.md) scores
  CAD that represents the built article.
- **No mesh exports of parts that are also here as solids.** An STL is a derivative; it
  belongs with a print job, not with the design.
- **`.f3d` is marked binary in `.gitattributes`.** Without that, git's text heuristic could
  apply line-ending normalisation and corrupt the archive silently.

---

## Constraints the model has to satisfy

| Constraint | Value | Design | Source |
|---|---|---|---|
| Body envelope, height | ≤ 210 mm | **118.5 mm** ✅ | [GEN-004](../../documentation/requirements/requirements.md) |
| Body envelope, across | ≤ 120 mm, **as a sided box** | **115 / 110 mm** ✅, with 2.5 and 5.0 mm per side | GEN-004 |
| Egg chamber allowance | +70 mm | Not designed | GEN-004 |
| Mass | 450–550 g all-up | **Electronics 151.299 g, weighed.** Structure not printed | [GEN-005](../../documentation/requirements/requirements.md) |
| Vehicle board | 100 × 100 × 1.6 mm, flat | Fits the 115 × 110 section | [assembly-procedure.md](../../documentation/hardware/assembly-procedure.md) |

**The second row was the open one, and the organizers closed it on 2026-09-09**: a 12 cm
sided box is acceptable, so the section limit is a 120 mm square rather than a bore — see
[the envelope question](../README.md#the-envelope-question--asked-and-answered). What is
left of it is the **clearance**: 2.5 mm per side on the wide axis is all there is for any
feature that protrudes past the modelled body.

Related: [mechanical/README.md](../README.md) · [drawings/](../drawings/) ·
[`tools/cad_dimensions.py`](../../tools/cad_dimensions.py)
