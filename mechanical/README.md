# Mechanical

The structure, the egg chamber, the parachute and the recovery system.

> [!IMPORTANT]
> **A design exists and is out for 3D printing in PETG. Nothing has come back yet.**
> `Cansat_D1` is modelled in Fusion 360, exported to STEP, and carries three static-stress
> studies. The electronics have been weighed for the first time. No part has been printed,
> the structure has never been on a scale, and no drop test has been done.

**Status: 2026-09-09.** Gate 7 (mechanical and recovery verified) has not been attempted.
It is still the largest single block of unclaimed points in the project — see
[scoring-assessment.md](../documentation/project/scoring-assessment.md).

**Every dimension on this page is read from
[`CAD/Cansat_D1.step`](CAD/Cansat_D1.step)** by
[`tools/cad_dimensions.py`](../tools/cad_dimensions.py), and `check_doc_claims.py` fails
the build if this page and the model disagree. Re-export the STEP after any model change
and run `bash tools/build_host.sh` — it will tell you which numbers moved.

---

## Contents

- [What the rulebook fixes](#what-the-rulebook-fixes)
- [The design](#the-design)
- [Material and manufacture](#material-and-manufacture)
- [Structural simulation](#structural-simulation)
- [The parachute](#the-parachute)
- [Mass budget](#mass-budget)
- [What has to be decided](#what-has-to-be-decided)
- [Directory contents](#directory-contents)

---

## What the rulebook fixes

Three numbers, and only three. The 2026 revision resolved the contradictions the original
guidelines carried, so these are now single-valued.

| Quantity | Value | Requirement |
|---|---|---|
| Body envelope | **21 cm × 12 cm** | [GEN-004](../documentation/requirements/requirements.md) |
| Egg chamber allowance | **+7 cm**, so 28 cm overall | GEN-004 |
| Mass | **500 g ± 10 %** — 450 g to 550 g | [GEN-005](../documentation/requirements/requirements.md) |
| Release altitude | **100 ft / 30.48 m**, from a drone | [MIS-001](../documentation/requirements/requirements.md) |
| Descent rate | **≤ 5 m/s** | [REC-005](../documentation/requirements/requirements.md) |

**Exceeding size or mass by more than 10 % is a disqualification.** It is not a scored
item; it is a closed list, and this is on it. That makes the mass budget below the single
most consequential table in this directory.

---

## The design

`Cansat_D1`, read straight out of the STEP:

| | |
|---|---:|
| Solids | one, `Body1`, 56 faces |
| **Bounding box** | **118.5 × 115.0 × 110.0 mm** |
| Cross-section diagonal | **159.1 mm** |
| Circular features | Ø5.1, Ø12.0, Ø16.0, Ø45.0 mm |
| Height against the 210 mm body allowance | **91.5 mm unused** |

It is **prismatic, not a cylinder** — the largest radius anywhere in the model is 22.5 mm,
nothing like the 60 mm a 120 mm circular section would need.

An open box frame: two solid side panels, two faces opened out with large arched cutouts, a
central vertical spine, harness slots top and bottom, and a square cutout with two small
holes beside it on one upper face. Renders in [photos/](photos/README.md).

![Envelope and the Cansat_D1 design](drawings/envelope-and-board-fit.svg)

### The board fits flat after all

**An earlier version of this page said the vehicle board could not be mounted flat and
should go in edge-on as a spine. That was reasoned against a 120 mm *circular* section,
before there was a design.** Against the actual one it is wrong:

| | |
|---|---:|
| Board, as built | 100 × 100 mm ([C.9](../documentation/hardware/receiving-inspection.md)) |
| Board diagonal | 141.4 mm |
| Design section | **115 × 110 mm** |

A 100 mm square fits a 115 × 110 mm opening flat, with 15 and 10 mm to spare for walls and
standoffs. The edge-on recommendation is withdrawn. *(It would still be the answer for a
circular section: a 120 mm bore caps a flat deck at 84.9 mm square.)*

### The envelope question — asked, and answered

**"12 cm across" did not say whether it meant a width or a diameter**, and for a prismatic
body those give different answers: 115 and 110 both pass as widths, while the 159.1 mm
corner-to-corner diagonal is 33 % over as a diameter — against a **10 % disqualification
threshold**. It was not a scored margin; it was the whole flight.

> [!NOTE]
> **Closed 2026-09-09. The organizers confirmed that a 12 cm sided box is acceptable.**
> The section limit is therefore a **120 mm square**, not a 120 mm bore, and this design
> fits it.
>
> **Keep the written confirmation with the submission.** This is a disqualification-class
> dimension rather than a scored one, and the answer is currently recorded only here.

| | |
|---|---:|
| Envelope section, confirmed | 120 × 120 mm |
| Design section | **115 × 110 mm** ✅ |
| **Clearance per side** | **2.5 and 5.0 mm** |
| Height | 118.5 mm against 210 mm ✅ |

**The clearance is the number to carry forward, and 2.5 mm per side is not much.** Anything
that protrudes past the modelled body — a switch boss, an LED bezel, a connector, a
parachute attachment, the antenna — has to live inside it, or the envelope grows past
120 mm and the question reopens as a real failure rather than an interpretive one. Model
those features before cutting anything.

---

## Material and manufacture

**PETG, 3D printed.** The body is out for printing as of 2026-09-09.

It is a sensible choice and worth being able to defend, because section D awards a **bonus
for material selection** and expects the reasoning:

- **Tougher than PLA and far easier than ABS.** PETG does not go brittle the way PLA does,
  which matters for a part whose job is to survive one impact.
- **Prints without an enclosure**, with little warping — ABS's toughness comes with
  warping and fumes that a school workshop usually cannot manage.
- **Impact energy goes into deformation rather than fracture**, which is the failure mode
  you want: a bent frame is a recovered vehicle, a shattered one is not.

**Two things about it that the simulation does not know**, both in
[simulation/README.md](simulation/README.md): the studies were run with Fusion's **PET**
material rather than PETG (21 % denser, somewhat stronger), and a **printed part is not
isotropic** — inter-layer strength is typically 40–70 % of in-plane, so the print
orientation decides which loads are the weak ones.

**Decide and write down the print orientation.** It is the single free variable that changes
the part's strength, it costs nothing at slicing time, and it cannot be changed afterwards.

---

## Structural simulation

Three static-stress studies, run 2026-09-09. Full write-up and caveats:
**[simulation/README.md](simulation/README.md)**.

| Study | Load | Mesh | **Max von Mises** | SF vs 54.40 MPa yield |
|---|---|---|---:|---:|
| 1 · Horizontal force | 30 N on +Z | 5838 nodes | **2.885 MPa** | ~19 |
| 2 · Tearing force | 30 N on −X | 5838 nodes | **1.330 MPa** | ~41 |
| 3 · Impact force | 100 N on −X | 7148 nodes | **2.345 MPa** | ~23 |

**The structure is nowhere near failing in any of the three**, which is the headline and it
is a good one. Four caveats, all in the simulation write-up and none of them small:

1. **The reports say the design "is expected to bend permanently or break."** That is
   contradicted by their own stress plots by a factor of 19 to 41, and appears in all three
   above a block containing *both* guided-result branches — it reads as template text.
   **Fusion's Result Summary table exported empty**, so the minimum safety factor has never
   actually been read. That is the one number to go back for.
2. **The material is PET, not PETG.** 21 % denser and somewhat stronger than what is being
   printed.
3. **A printed part is not isotropic**, and these studies assume it is.
4. **The 100 N impact load assumes a 25 ms arrest** for a 0.5 kg vehicle at 5 m/s. A rigid
   landing on concrete is several times worse — 10 ms is 250 N, 5 ms is 500 N.

**A simulation is not a drop test**, and section C scores the real descent. The model says
where to look; the drop test says whether it was right.

---

## The parachute

Sized by [`simulations/descent.py`](../simulations/descent.py), which is run by the host
test suite and pinned to closed-form limits — see [simulations/README.md](../simulations/README.md).

**The answer, and the reasoning is in the model rather than here:**

| Case | Flat diameter | Descent rate | Descent time |
|---|---:|---:|---:|
| 500 g, ISA 15 °C, vented flat circular | 73.7 cm | 5.00 m/s | 6.45 s |
| **550 g, 35 °C — size to this** | **80.0 cm** | **5.00 m/s** | **6.45 s** |

**Size at 550 g and a hot day, not at 500 g and ISA.** Canopy area is linear in mass, so
the ±10 % mass tolerance is ±10 % of area but only ~5 % of diameter. A canopy sized at the
nominal mass and flown at the top of the tolerance **breaks the 5 m/s cap**; one sized at
the top is compliant across the whole band and costs 6 cm of cloth.

**Three things the rulebook says about the parachute that are not the diameter:**

- It **must not be tightly packed** — external or semi-exposed, so it deploys immediately
  on release ([REC-003](../documentation/requirements/requirements.md), REC-004). A chute
  stuffed inside the body risks the 5 deployment points *and* a disqualification for
  unsafe deployment.
- Descent must be **stable, without tumbling or spinning** (REC-006), and it is scored
  *comparatively against other teams*. An unvented flat circular canopy oscillates. A
  central vent of roughly 10 % of the diameter costs a little drag and buys a great deal of
  stability; a cruciform is better still and is easy to sew.
- The structure must be **intact after landing** (REC-007), and telemetry must continue for
  **at least 5 s after impact** (REC-008) — which the firmware already enforces, and which
  means the antenna and the battery connection have to survive the arrival.

**The uncertainty that paper cannot close.** Drag coefficient is the dominant term and the
spread between canopy types is larger than everything else combined. Close it with a drop
test: known mass, known height, a stopwatch, and the measured rate fed back into the model.

---

## Mass budget

**Measured 2026-09-09**, on a scale, for the first time. These two numbers replace a table
of vendor figures:

| Item | Mass | How |
|---|---:|---|
| **Assembled vehicle PCB, no battery** | **110.573 g** | Weighed |
| **Battery** — Orange 1500 mAh 1S LiPo | **40.726 g** | Weighed |
| **Electronics, all-up** | **151.299 g** | Sum of the two |
| Structure, PETG print | **TBD** | Fusion knows it — see below |
| Egg chamber | TBD | Not designed |
| Parachute, lines, harness | TBD | Not built |
| **Budget** | **450–550 g** | [GEN-005](../documentation/requirements/requirements.md) |
| **Remaining for everything mechanical** | **348.701 g** | 500 − 151.299 |

> [!WARNING]
> **The earlier estimate on this page was wrong, and wrong in the dangerous direction.** It
> counted ~67 g of vendor figures and said the avionics were "unlikely to exceed ~120 g".
> **The measured figure is 151.3 g — 26 % past that ceiling**, and it makes the electronics
> **30 % of the entire mass budget** rather than the ~14 % the estimate implied.
>
> The lesson is the one this repository keeps relearning: a vendor figure is not a
> measurement, and a sum of vendor figures with three `TBD` rows in it is not a budget. The
> two lines that were `TBD` — wiring, headers, passives, solder, and the antenna — are most
> of the difference.

### The structure is now the number that decides this

**348.7 g is a comfortable allowance for a printed frame, but it is not unlimited**, and
PETG is not weightless:

| Solid volume | As PETG (1.27 g/cm³) | As printed, ~40 % infill |
|---:|---:|---:|
| 100 cm³ | 127 g | ~50–60 g |
| 150 cm³ | 190 g | ~75–90 g |
| 200 cm³ | 254 g | ~100–120 g |
| 250 cm³ | 318 g | ~125–150 g |

**Get the real number in one click**: Fusion reports the model's mass directly. Two
corrections to apply to what it says:

1. **The assigned material is PET, not PETG** — density 1.541 g/cm³ against ~1.27. **Fusion's
   figure will read about 21 % high**; multiply by 0.82.
2. **A printed part is not solid.** Perimeters and infill mean the real print is a fraction
   of the solid volume — the slicer's estimate is the one to trust, and it also tells you the
   filament cost.

**The slicer's own mass estimate is the best number available before anything is printed,
and it costs one minute.** Put it in this table when you have it.

### What is still unweighed

The antenna and its cable, the switch, the LEDs, the Schottky and the divider resistors are
all still to fit, and none of them is on the scale yet. They are small — grams, not tens of
grams — but the estimate above was wrong by 31 g, so **weigh the vehicle again once it is
complete** rather than adding figures to a measured base.

---

## What has to be decided

None of these is blocked on anybody outside the team. They are open because nobody has
started.

| Decision | Depends on | Note |
|---|---|---|
| ~~Board orientation~~ | — | **Settled by the design.** A 100 mm board fits the 115 × 110 mm section flat |
| ~~How "12 cm across" is measured~~ | — | **Answered 2026-09-09: a 12 cm sided box is acceptable.** The design fits with 2.5 and 5.0 mm per side |
| **Where the protruding features go** | Structure | Switch, LED, connector, chute attachment, antenna. **2.5 mm per side is all the clearance there is** |
| ~~Structure material~~ | — | **PETG, decided 2026-09-09**, and out for printing. The reasoning is [above](#material-and-manufacture) and section D rewards having it |
| **Print orientation** | Nothing | The one free variable that changes a printed part's strength, free at slicing time, unchangeable afterwards. **Not yet decided or recorded** |
| Whether to use the 91.5 mm of unused height | Nothing | The body allowance is 210 mm and the design is 118.5 mm. Section D scores *effective use of the volume the rules permit*, and half of it is currently empty |
| Egg chamber, built even though no egg flies | Nothing | PAY-002 is a separate requirement from PAY-001, carries the +7 cm allowance, and section D scores use of permitted volume. **Build it** — see [scoring-assessment.md](../documentation/project/scoring-assessment.md) |
| Canopy type and material | Sewing capability | Vented flat circular is the floor; cruciform scores better on stability |
| Deployment arrangement | Structure | REC-003/REC-004 forbid tight packing |
| Antenna routing and strain relief | Board orientation | The u.FL connector is the most fragile joint on the vehicle |
| Switch and LED placement | Structure | PWR-001 to PWR-003, and **5 of the cheapest points on the board** |

---

## Directory contents

| Path | Contents |
|---|---|
| [`CAD/`](CAD/README.md) | `Cansat_D1.f3d` (Fusion, native) and `Cansat_D1.step` (neutral export, and the one this repository can read) |
| [`simulation/`](simulation/README.md) | The three static-stress reports, and what they do and do not establish |
| [`photos/`](photos/README.md) | CAD renders now; photographs of the printed article when there is one |
| [`drawings/`](drawings/README.md) | Dimensioned drawings. Generated from the requirement **and from the CAD**, so they cannot drift from either |

Related: [simulations/descent.py](../simulations/descent.py) ·
[requirements.md](../documentation/requirements/requirements.md) ·
[scoring-assessment.md](../documentation/project/scoring-assessment.md) ·
[timeline.md](../documentation/project/timeline.md)
