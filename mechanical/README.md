# Mechanical

The structure, the egg chamber, the parachute and the recovery system.

> [!IMPORTANT]
> **Built and submitted.** `Cansat_D1` was printed in **white PETG** and assembled on
> 2026-09-12 — electronics mounted, egg chamber fitted, **280 g without a parachute** on the
> scale. Before submission on 2026-09-14 the **80 cm canopy was sewn and fitted** and the
> vehicle was **ballasted into the 450–550 g band** (both reported by the team).
>
> **What was never done: a drop test.** No descent rate has been measured, the structure has
> never been dropped, and the canopy has never opened. The launch will be the first time any
> of it happens.

**Status: 2026-09-14 — submitted, launch pending.** Gate 7 (mechanical and recovery
verified) is **built but not verified**: structure, egg chamber and canopy all exist, and
nothing about recovery has been tested. See
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
- [What was decided](#what-was-decided)
- [What was never done](#what-was-never-done)
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

**White PETG, 3D printed. Printed and assembled as of 2026-09-12.** The body came back in
white, the electronics are mounted, and the egg chamber is fitted.

**White was a finish decision, and it is worth one line in front of a judge**, because
section D puts 15 of its 30 points on aesthetics and build quality. White shows a clean print
— layer lines, stringing and blemishes have nowhere to hide on it, which is a reason to
choose it only if the print is good — and it is the easiest colour to photograph against any
background for the mandatory top, side and bottom views. It also runs cooler in sunlight on
a launch pad than a dark part would, which is a small real benefit for a sealed box holding a
LiPo.

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

**The print orientation is decided: as modelled, sitting on its base**, 2026-09-09. It was
the single free variable that changes the part's strength, it costs nothing at slicing time,
and it cannot be changed afterwards. Layers stack vertically, so the weak directions are
tension normal to the layers and interlayer shear — which turns the anisotropy caveat from a
warning into arithmetic, and leaves an effective safety factor of **6 to 13**.

---

## Structural simulation

Three static-stress studies, run 2026-09-09. Full write-up and caveats:
**[simulation/README.md](simulation/README.md)**.

| Study | Load | Mesh | **Max von Mises** | **Min safety factor** |
|---|---|---|---:|---:|
| 1 · Horizontal force | 30 N on +Z | 5838 nodes | **2.885 MPa** | **≥ 15** |
| 2 · Tearing force | 30 N on −X | 5838 nodes | **1.330 MPa** | **≥ 15** |
| 3 · Impact force | 100 N on −X | 7148 nodes | **2.345 MPa** | **≥ 15** |

**The structure is nowhere near failing in any of the three**, which is the headline and it
is a good one. Four caveats, all in the simulation write-up and none of them small — and
**caveat 2 is now measured rather than argued**: the printed part weighs ≈ 128.7 g against
the 193 g the PET-density model implied, so the studies were wrong about mass by more than
the density correction alone predicted. Stress and safety factor are unaffected by that;
they scale with geometry and yield, not with density.

1. ~~The reports say the design "is expected to bend permanently or break."~~ **Settled: it
   is template text.** The reported 15 is Fusion's display cap, not a result — three load
   cases with peak stresses differing by 2× cannot all minimise at exactly 15. So the real
   statement is **≥ 15 everywhere**, which matches the yield-derived 19 / 41 / 23.
2. **The material is PET, not PETG.** 21 % denser and somewhat stronger than what is being
   printed.
3. **A printed part is not isotropic**, and these studies assume it is. **The orientation is
   now decided** — printed on its base, as modelled — so the weak direction is vertical.
   Derating the margin by the usual 40–70 % still leaves an effective safety factor of
   **6 to 13**.
4. **The 100 N impact load assumes a 25 ms arrest** for a 0.5 kg vehicle at 5 m/s. A rigid
   landing on concrete is several times worse — 10 ms is 250 N, 5 ms is 500 N. **And all
   three studies load horizontally**, while a vehicle under a parachute lands base-first.

**A simulation is not a drop test**, and section C scores the real descent. The model says
where to look; the drop test says whether it was right.

---

## The parachute

**Sewn and fitted before submission** (reported by the team, 2026-09-14), to the 80.0 cm
flat-diameter specification below. **Not recorded here, and worth recording:** the canopy
type actually sewn (vented, cruciform or plain flat), its measured diameter, and where it is
stowed on the vehicle — the last two decide REC-003, REC-004 and REC-006.

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
spread between canopy types is larger than everything else combined. **No drop test was made
before submission**, so the launch itself is the measurement: time the descent from the
release to the landing transient in the SD log, and feed the rate back into the model.

---

## Mass budget

> [!NOTE]
> **Closed before submission: the vehicle was ballasted into the 450–550 g band** (reported
> by the team, 2026-09-14). **The final all-up mass was not recorded in this repository** —
> the last scale reading here is the 280 g below. If the submitted mass is known, add it to
> the table as its own row rather than editing the history beneath it.
>
> Everything below this note is the record of how the vehicle got there, and it is kept
> because the lesson in it — two estimates wrong by tens of grams in opposite directions — is
> the useful part.

**The structure is printed and the vehicle is assembled. This table is a measurement, not an
estimate — and the estimate it replaced was wrong by 64 g in the direction that matters.**

| Item | Mass | How |
|---|---:|---|
| **Assembled vehicle PCB, no battery** | **110.573 g** | Weighed 2026-09-09 |
| **Battery** — Orange 1500 mAh 1S LiPo | **40.726 g** | Weighed 2026-09-09 |
| **Electronics, all-up** | **151.299 g** | Sum |
| **Printed structure + egg chamber** | **≈ 128.7 g** | **By difference**, see below |
| **As-built vehicle, no parachute** | **280 g** | **Weighed 2026-09-12** |
| Parachute, lines, harness | 30–55 g | Sewn and fitted after this weighing |
| Switch, LEDs, Schottky, divider | 5–10 g | Switch, power LED and divider fitted; Schottky not |
| Ballast | to the band | **Added before submission** — amount not recorded |
| **Submitted vehicle** | **450–550 g** | **Reported by the team, 2026-09-14** — no scale reading on record |
| **Budget** | **450–550 g** | [GEN-005](../documentation/requirements/requirements.md) — 500 g ± 10 % |

**What was measured and what was derived.** The scale figure is the **280 g assembled
vehicle**, printed in white PETG with the egg chamber fitted. The electronics inside it were
weighed separately at 151.299 g on 2026-09-09. The structure line is therefore **arithmetic
on two measurements, not a third measurement**: 280 − 151.299 = 128.701 g. It is quoted as
≈ 128.7 g rather than to three decimals because the 280 g came off a scale reading whole
grams, so the difference carries ±1 g at best.

### The solid-volume estimate was a 33 % overestimate

The 193 g this page carried was solid volume × density, and it was stated as an upper bound.
It was one — by a wider margin than expected:

| | |
|---|---:|
| Predicted, solid volume × 1.27 g/cm³ | 193.0 g |
| **Printed, and that includes an egg chamber the estimate did not** | **≈ 128.7 g** |
| Error | **−64 g, −33 %** |

**The infill is the whole of it.** 193 g assumed no infill saving at all; a real slice of an
open frame with thin walls is mostly perimeter and air. The estimate was labelled an upper
bound and behaved like one, which is the correct outcome — but the size of the gap is the
useful part, because it is now **mass the design can spend on purpose**.

### The risk did not just materialise, it grew

**280 g is on the scale. The budget floor is 450 g.**

| | |
|---|---:|
| As-built, no parachute | **280 g** |
| Projected all-up, with chute and the last four parts | **315–345 g** |
| To reach the **450 g floor** | **+105 to +135 g** needed |
| To stay under the **550 g cap** | +205 to +235 g available |

**This is the same finding this page carried three days ago, and it has roughly doubled.**
The projection then was 414–479 g and straddled the floor; the measured article projects to
**315–345 g, entirely below it** — because the print came in 64 g under its upper bound and
the egg chamber came free inside that. Nothing here is close to the 550 g cap. **Every
remaining mass question on this vehicle is about adding, not removing.**

> [!NOTE]
> ~~**Whether 450 g is a floor is still unanswered.**~~ **Made moot by ballasting into the
> band.** GEN-005 reads *"500 g (±10%)"* as a band while
> [GEN-006](../documentation/requirements/requirements.md)'s disqualification names only
> *exceeding*. A vehicle inside 450–550 g satisfies both readings, so the answer stopped
> mattering the moment the ballast went in.

### If 450 g binds, there are two routes and one of them is free

*Written 2026-09-12, before the decision. The team took route 2 — ballast — before
submission; whether a re-print was also made is not recorded.*

**Route 1 — re-print heavier, and prefer it.** The printed part is ≈ 128.7 g against a
152.0 cm³ solid volume that would weigh 193 g. Raising infill and wall count moves the part
up that range at **no cost but print time**, and it adds material exactly where the
[anisotropy caveat](simulation/README.md) says a printed part is weakest — so it buys
strength margin and mass in the same change. It is a slicer setting, not a redesign.

**Route 2 — ballast.** Cruder, earns nothing, and section D scores effective use of volume.
It is the fallback if there is no time to re-print.

**Route 1 alone may not close a 105–135 g gap**, since the whole body has only ~64 g of
headroom to solid. The likely answer is both: re-print at high infill for the strength, and
ballast the remainder — ideally as something that earns its place, low and central, which
also helps descent stability under REC-006.

### What the new mass does to the descent

**Nothing that needs a new canopy.** The 80.0 cm flat diameter was sized at 550 g on a hot
day and is compliant across the whole range this vehicle could occupy.

**The vehicle flies in the band, so the first row below no longer applies to the flight** —
it is kept as the unballasted comparison. Inside the band the canopy gives **4.37 m/s over
7.28 s at 450 g** (ISA) up to **5.00 m/s over 6.45 s at 550 g on a 35 °C day**: 9 or 10
packets of descent at 1.43 Hz, or 20 to 22 at the 3.11 Hz the vehicle will actually be
transmitting after its command window closes.

| Flight mass | Rate under the 80 cm canopy | Descent time | Packets at 1.43 Hz |
|---|---:|---:|---:|
| **315 g** — as-built, unballasted | **3.66 m/s** | **8.59 s** | **12** |
| 500 g — ballasted to nominal | 4.61 m/s | 6.94 s | 9 |
| 550 g, 35 °C — the sizing case | 5.00 m/s | 6.45 s | 9 |

All three clear the 5 m/s cap, so **the canopy does not have to be re-sized whatever the
ballast decision turns out to be.** Two things follow that are worth having:

- **Flying light descends more slowly, and section C scores descent time comparatively.**
  8.59 s against 6.45 s is a third longer under canopy, and 12 packets instead of 9 is a
  third more descent telemetry — the dataset this mission is thinnest on.
- **So the mass question was not only compliance.** Ballasting to the bottom of the band
  rather than the middle keeps some of that longer descent — which is a reason to record the
  submitted mass, since it decides which row the flight should be compared against.

Figures from [`simulations/descent.py`](../simulations/descent.py); reproduce with
`python simulations/descent.py --mass 0.315 --diameter 0.80`.

### What is still unweighed

**The submitted vehicle.** The canopy, the switch, the LED and the ballast all went in after
the 280 g reading, and the team reports the result inside the band without a number on
record.

**Weigh it before the launch if a scale is available**, rather than adding figures to a
measured base. This page has now been wrong twice by tens of grams in the same direction — the
electronics estimate by 31 g high, the structure estimate by 64 g high — and both times the
scale settled it in one minute.

---

## What was decided

Everything on this list was settled before submission. Rows that closed without evidence in
the repository say so.

| Decision | Outcome |
|---|---|
| ~~Board orientation~~ | **Settled by the design.** A 100 mm board fits the 115 × 110 mm section flat |
| ~~How "12 cm across" is measured~~ | **Answered 2026-09-09: a 12 cm sided box is acceptable.** The design fits with 2.5 and 5.0 mm per side |
| ~~Structure material~~ | **PETG, decided 2026-09-09; printed in white and assembled 2026-09-12.** The reasoning is [above](#material-and-manufacture) |
| ~~Print orientation~~ | **As modelled, printed on its base.** Layers stack vertically, so the weak direction is vertical tension and interlayer shear |
| ~~Egg chamber~~ | **Built and fitted 2026-09-12**, inside the 280 g. No egg flies — PAY-001, team decision |
| ~~Mass~~ | **Ballasted into the 450–550 g band** before submission (reported). Final mass not recorded |
| ~~Canopy~~ | **80 cm canopy sewn and fitted** (reported). Type and stowage not recorded |
| ~~Switch and LED placement~~ | **Fitted** (reported), in the rectangular cutout and the two holes beside it |

## What was never done

Not decisions — tests. None of them happened before submission, and each is now answered, if
at all, by the launch.

| Item | Why it matters |
|---|---|
| **A drop test** | The only measurement of the drag coefficient the canopy is sized against, and the only check that the structure survives an arrival |
| **A vertical impact case** | All three stress studies load horizontally; a vehicle under a canopy lands base-first, along the print's weak axis |
| **Calipers on the printed envelope** | Every dimension here is read from the STEP. A printed part is not its model, and the clearance is 2.5 mm per side |
| **Photographs of the built vehicle** | Mandatory media, and section D's 15 build-quality points are judged from them. [photos/](photos/README.md) still holds only CAD renders |

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
