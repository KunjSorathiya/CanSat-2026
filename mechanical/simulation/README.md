# Structural simulation

Three static-stress studies on `Cansat_D1`, run in Fusion 360 on 2026-09-09 by Kunj
Sorathiya.

| Report | Study | Load | Mesh | **Max von Mises** |
|---|---|---|---|---:|
| [study-1-horizontal-force.html](study-1-horizontal-force.html) | Horizontal force | **30 N** on +Z | 5838 nodes / 2619 elements | **2.885 MPa** |
| [study-2-tearing-force.html](study-2-tearing-force.html) | Tearing force | **30 N** on −X | 5838 nodes / 2619 elements | **1.330 MPa** |
| [study-3-impact-force.html](study-3-impact-force.html) | Impact force | **100 N** on −X | 7148 nodes / 3299 elements | **2.345 MPa** |

All three: static stress, one fixed constraint, parabolic elements, no adaptive refinement.

**Against the 54.40 MPa yield strength of the assigned material, those are safety factors of
roughly 19, 41 and 23.** The structure is not close to failing in any of the three cases.

---

## Four things to know before quoting these numbers

### 1 · The report says the design breaks. It does not.

All three exports contain the sentence:

> *"With the analysis criteria in this study setup, the design is expected to bend
> permanently or break."*

**That is contradicted by the studies' own stress plots by a factor of 19 to 41.** The
sentence sits directly above a block that contains *both* the "Below Safety Factor Target"
and the "Above Safety Factor Limit" advice lists, which is what a template emits when it
renders every branch rather than the one that applied.

**It has not been confirmed either way.** Fusion's *Result Summary* table — the one place the
numeric minimum safety factor would appear — **exported completely empty** in all three
reports. Every number on this page was read off the colour-bar legends in the result plots
instead.

> **Open: read the minimum safety factor off each study in Fusion and record it here.** It is
> the single number these studies exist to produce, and it is the one the export dropped.

### 2 · The simulation material is PET. The part is being printed in PETG.

The studies assign Fusion's **"PET Plastic"**:

| Property | Value in the study | Typical bulk PETG |
|---|---:|---:|
| Density | **1.541 g/cm³** | ~1.27 g/cm³ |
| Young's modulus | 2757.9 MPa | ~2000–2100 MPa |
| Poisson's ratio | 0.417 | ~0.40 |
| Yield strength | 54.40 MPa | ~50 MPa |
| Ultimate tensile | 55.10 MPa | ~50 MPa |

PET is stiffer, denser and slightly stronger than PETG, so the studies are **mildly
optimistic on strength and materially wrong on mass** — the density is **21 % high**, which
matters directly to the mass budget.

### 3 · A printed part is not a bulk part, and this is the bigger caveat

These are isotropic bulk-material studies. **An FDM part is not isotropic.** Inter-layer
adhesion in the Z direction is typically **40–70 % of the in-plane strength**, and printed
parts carry voids the model does not have. A load that pulls layers apart sees far less
material strength than the study assumes.

**Which loads are the weak ones depends on the print orientation, and the orientation is not
in the model.** Two consequences:

- **Decide and record the print orientation**, then check it against where these studies put
  their peak stress. The arched cutouts and the corner where study 1 peaks are the places to
  look.
- **The safety factors above are the optimistic bound.** At 19–41 there is plenty of room to
  absorb an anisotropy derating; at 2 there would not have been. This is why the margin is
  worth having rather than designing to the edge.

### 4 · The 100 N impact load is an assumption, and it is worth stating

The vehicle lands at **5 m/s** under its 500 g budget, which is 2.5 N·s of momentum to
absorb. The force depends entirely on how long the arrest takes:

| Arrest time | Force |
|---:|---:|
| 25 ms | **100 N** ← what study 3 assumes |
| 10 ms | 250 N |
| 5 ms | 500 N |

**100 N therefore models a landing that takes 25 ms to stop** — a compliant arrival on grass
or soil, with the structure and the parachute doing some of the work. A rigid landing on
concrete is several times worse. Neither is wrong; the assumption just has to be written
down beside the result, which is what this section is for.

**A drop test settles it**, and it is also what section C actually scores.

---

## The reports

Fusion's own HTML exports, committed unmodified. They are ~4.5 MB each because every result
plot is embedded as a base64 image — that is also why they are worth keeping, since the
plots are where the numbers on this page came from.

Open them in a browser. They carry the full study setup: contact tolerance, mesh parameters,
element order, material properties, constraints and loads.

---

## What is still missing

| Item | Why it matters |
|---|---|
| **Minimum safety factor, per study** | The number the studies exist to produce, and the export dropped it |
| **Total displacement, per study** | The displacement legend is cropped out of the exported plot in study 1 |
| **A PETG material definition** | See above — the current one is 21 % too dense and somewhat too strong |
| **Print orientation** | Decides which of these loads is the weak one on a real part |
| **The structure's mass** | Fusion knows it. At the assigned PET density it will read **21 % high** for a PETG print, before infill |
| **A drop test** | The only thing that answers what a real landing does, and the only thing section C scores |

Related: [mechanical/README.md](../README.md) · [CAD/](../CAD/README.md) ·
[photos/](../photos/README.md)
