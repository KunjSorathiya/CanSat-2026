# Mechanical photos and renders

| File | What it is |
|---|---|
| [`cansat-d1-render-1.png`](cansat-d1-render-1.png) | `Cansat_D1` render, three-quarter view from above |
| [`cansat-d1-render-2.png`](cansat-d1-render-2.png) | `Cansat_D1` render, second view |

**These are renders of the CAD, not photographs of hardware. Nothing has been printed.**
Photographs of the printed article belong here too, when there is one — and they are worth
taking, because section D awards **15 of its 30 points for aesthetics and build quality**,
which is judged from what the vehicle looks like rather than from what it is.

---

## What the render shows

An open box frame, 118.5 × 115.0 × 110.0 mm, to be printed in **PETG**:

- **Two solid side panels** and two faces opened out with large arched cutouts — the
  material is where the load path is, which is what the [simulation](../simulation/README.md)
  studies were checking.
- **A central vertical spine** with a through-hole, dividing the volume.
- **Rectangular slots** top and bottom, in pairs — harness or strap routing.
- **Round holes** in the side panels and the spine.
- **A rectangular cutout with two small round holes beside it** on one upper face — the
  switch and the two indicator LEDs.

> **The rectangular cutout is the switch** — confirmed 2026-09-09 — and the two small round
> holes beside it are the LED positions. That accounts for three of the four penetrations the
> vehicle needs; **USB access for the Pico is the one left to confirm**, and there is
> [only 2.5 mm of clearance per side](../README.md#the-envelope-question--asked-and-answered)
> for anything that protrudes.

---

## Conventions

- **PNG for renders**, which have flat colour and hard edges. JPEG for photographs of real
  hardware, which do not — the same split the
  [hardware photos](../../documentation/hardware/photos/README.md) use.
- **Name the file after what it shows**, not after the export counter. `Cansat_CAD_Photo_1`
  tells the next reader nothing.
- **A render is not evidence.** It shows the design, not the article. Anything claimed about
  the built vehicle needs a photograph of the built vehicle.

Related: [mechanical/README.md](../README.md) · [CAD/](../CAD/README.md) ·
[simulation/](../simulation/README.md)
