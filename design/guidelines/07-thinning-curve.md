---
title: Thinning curve
section: Window
---
Drawn on the analyser in klein. Its height is how much thinning happens at that frequency, not volume: the middle line (100%) follows Amount, the top is twice Amount (200%), the bottom leaves that range untouched (0%). It starts flat at 100%.

Up to six points, each a bell with a frequency, an amount and a width (0.3 to 4 octaves). Double-click empty space to add one, drag it, scroll on it to change its width, double-click it to remove it.

| State | |
|---|---|
| Hover | {{asset ui-states/point-hover}} |
| Selected | {{asset ui-states/point-selected}} |
| Dragging | {{asset ui-states/point-drag}} |
| Six points | {{asset ui-states/points-full}} |

The readout says frequency, amount and width: `3.3 kHz  175%  0.7 oct`.

**Live** has no latency and applies a smoothed version of the curve. Both are shown: your curve dashed, the applied one solid. In HQ they're the same line.

{{asset ui-states/live}}

{{asset ui-states/hq}}
