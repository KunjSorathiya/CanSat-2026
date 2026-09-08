# Mechanical drawings

Dimensioned drawings of the vehicle and its envelope.

**Generated where possible.** A drawing whose dimensions were typed by hand goes stale the
first time a requirement moves and nobody notices. The drawings here that can be computed
from the requirements are computed from them, by a script in `tools/`, and the script is
the thing to edit.

| Drawing | Source | What it shows |
|---|---|---|
| [`envelope-and-board-fit.svg`](envelope-and-board-fit.svg) | [`tools/gen_envelope_drawing.py`](../../tools/gen_envelope_drawing.py) | The 21 cm (+7 cm) × 12 cm rulebook envelope in elevation and section, with the 100 × 100 mm vehicle board in both candidate orientations, and the arithmetic that rules one of them out |

Regenerate after changing a dimension:

```bash
python tools/gen_envelope_drawing.py
```

---

## Still to draw

Each of these waits on a decision, not on a tool. They are listed so the gap is visible
rather than implied by an empty directory.

| Drawing | Blocked on |
|---|---|
| Structure general arrangement | Board orientation and material choice |
| Egg chamber section, with cushioning | Chamber design |
| Parachute cutting pattern — gores, vent, hem, line lengths | Canopy type; the **diameter is already computed** ([simulations](../../simulations/README.md)) |
| Harness and bridle arrangement | Structure |
| Switch and LED placement, with the panel cutouts | Structure |

---

## Conventions

- **Millimetres**, always. The rulebook states centimetres and the model states metres;
  the drawings convert once, here, so nothing downstream has to.
- **Scale is stated on the drawing**, in px/mm, and the drawing is generated at that scale
  rather than resized afterwards.
- **A dimension that comes from the rulebook is marked as such.** Anything else on a
  drawing of an unbuilt article is a proposal and says so on its face.
- **SVG**, not a raster. It stays legible when printed at any size, and a diff shows what
  changed.

Related: [mechanical/README.md](../README.md) ·
[requirements.md](../../documentation/requirements/requirements.md) ·
[board layout](../../documentation/hardware/diagrams/board-layout-to-scale.svg)
