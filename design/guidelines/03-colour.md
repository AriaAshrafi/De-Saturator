---
title: Colour
section: Brand
---
Four of the parent's colours, and one rule: **weight means content, klein means you**. Ink in every weight draws the sound; klein draws what you set (the curve, its points, the value arcs, Delta when it's on). Nothing else is coloured.

| Role | Colour | Hex | Use |
|---|---|---|---|
| ground | paper | `#ECEBE6` | The window. |
| text | ink | `#0C0C0B` | Labels, the name, button outlines, the analyser's frame. |
| textMuted | graphiteDeep | `#5F5E59` | Values, the latency, frequency labels, the credit. |
| grid | paperLine | `#D6D4CB` | The analyser's grid, knob tracks, the Shape track. |
| body | ink at 16% | `#0C0C0B` | Analyser: the comb under the result line, at 16% opacity. What stays, kept quiet. |
| result | ink | `#0C0C0B` | Analyser: the result line, heavy. What you hear. |
| original | ink | `#0C0C0B` | Analyser: the original's line, a hairline. |
| removed | ink | `#0C0C0B` | Analyser: hairlines between the two lines. What's removed. |
| hand | klein | `#2E40FF` | What you set: the thinning curve, its points, value arcs, Delta when on. |
| selected | ink | `#0C0C0B` | The chosen segment of a switch; Auto gain and Bypass when on. |
| onSelected | paper | `#ECEBE6` | Text and icons on ink. |
| onHand | paper | `#ECEBE6` | Text on klein. |
| readout | ink | `#0C0C0B` | Readout chips and tooltips. |
| onReadout | paper | `#ECEBE6` | Text in readout chips and tooltips. |

The body comb under the result line is the only colour drawn at an opacity: ink at 16%. Everything else is flat.

## Contrast

| Pair | Ratio | Needs | Where |
|---|---|---|---|
| ink on paper | 16.39:1 | passes 4.5:1 | labels, values, the result line |
| graphite deep on paper | 5.44:1 | passes 4.5:1 | values, latency, frequency labels |
| klein on paper | 5.34:1 | passes 3:1 | the curve, points, value arcs (graphics) |
| paper on klein | 5.34:1 | passes 4.5:1 | Delta's label when on |
| paper on ink | 16.39:1 | passes 4.5:1 | chips, tooltips, selected switches |
| paper line on paper | 1.24:1 | decorative | grid and tracks: decorative, never carries meaning alone |

Paper line never carries meaning on its own: it's the grid and the tracks.
