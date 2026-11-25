---
title: Controls
section: Window
---
## Amount

The hero. Its ring thins as you turn it up, from a near-solid disc at 0% to a hairline at 100%, with a notch that shows the turn. A klein dot rides the scale. Under the pointer the ticks darken and the value sits on an ink chip.

{{asset ui-states/amount-hover}}

{{asset ui-states/amount-drag}}

## Restore, Mix, Output

Small knobs: a paper disc with an ink outline and pointer, a klein value arc (Output's starts at 0 dB). Under the pointer the outline thickens and the value turns into a chip.

{{asset ui-states/knob-hover}}

{{asset ui-states/knob-drag}}

## Shape

A blend from Harmonics (overtones) to Body (weight). Each end shows its share.

{{asset ui-states/shape-drag}}

## Switches and buttons

Clean / Character and Live / HQ are pill switches: the chosen side is ink. Delta turns klein when it's on, because what you hear is then what you set. Auto gain and Bypass turn ink. Light / Dark, the small switch at the bottom right, picks the theme; it's a preference kept with the plugin, not a parameter.

{{asset ui-states/switch-hover}}

## Tooltips

After 600 ms over anything: an ink box with paper text, in plain words.

{{asset ui-states/tooltip}}

{{asset ui-states/tooltip-point}}

## Using it

- Drag knobs up or down; Shift for fine. Double-click resets. The wheel steps a knob once you've clicked in the window.
- No keyboard: the plugin takes no keys and never takes keyboard focus, so the host keeps all its shortcuts.
- Nothing eases: values and switches change on the next frame.
