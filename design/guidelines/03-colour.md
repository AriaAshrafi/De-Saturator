---
title: Colour
section: Brand
---
The parent's colours, and one rule: **weight means content, klein means you**. Ink in every weight draws the sound; klein draws what you set (the curve, its points, the value arcs, Delta when it's on). Nothing else is coloured. The dark theme swaps each role: paper draws the sound on ink, and klein turns klein light, the parent's klein for ink grounds.

| Role | Light | Dark | Use |
|---|---|---|---|
| ground | paper `#ECEBE6` | ink `#0C0C0B` | The window. |
| text | ink `#0C0C0B` | paper `#ECEBE6` | Labels, the name, button outlines, the analyser's frame. |
| textMuted | graphiteDeep `#5F5E59` | graphite `#8A8983` | Values, the latency, frequency labels, the credit. |
| grid | paperLine `#D6D4CB` | inkLine `#2A2A27` | The analyser's grid, knob tracks, the Shape track. |
| body | ink at 16% `#0C0C0B` | paper at 20% `#ECEBE6` | Analyser: the comb under the result line, at 16% opacity. What stays, kept quiet. |
| result | ink `#0C0C0B` | paper `#ECEBE6` | Analyser: the result line, heavy. What you hear. |
| original | ink `#0C0C0B` | paper `#ECEBE6` | Analyser: the original's line, a hairline. |
| removed | ink `#0C0C0B` | paper `#ECEBE6` | Analyser: hairlines between the two lines. What's removed. |
| hand | klein `#2E40FF` | kleinLight `#7C88FF` | What you set: the thinning curve, its points, value arcs, Delta when on. |
| selected | ink `#0C0C0B` | paper `#ECEBE6` | The chosen segment of a switch; Auto gain and Bypass when on. |
| onSelected | paper `#ECEBE6` | ink `#0C0C0B` | Text and icons on ink. |
| onHand | paper `#ECEBE6` | ink `#0C0C0B` | Text on klein. |
| readout | ink `#0C0C0B` | paper `#ECEBE6` | Readout chips and tooltips. |
| onReadout | paper `#ECEBE6` | ink `#0C0C0B` | Text in readout chips and tooltips. |

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
| dark: paper on ink | 16.39:1 | passes 4.5:1 | labels, values, the result line |
| dark: graphite on ink | 5.58:1 | passes 4.5:1 | values, latency, frequency labels |
| dark: klein light on ink | 6.36:1 | passes 3:1 | the curve, points, value arcs (graphics) |
| dark: ink on klein light | 6.36:1 | passes 4.5:1 | Delta's label when on |
| dark: ink on paper | 16.39:1 | passes 4.5:1 | chips, tooltips, selected switches |
| dark: ink line on ink | 1.36:1 | decorative | grid and tracks: decorative |

Paper line never carries meaning on its own: it's the grid and the tracks.
