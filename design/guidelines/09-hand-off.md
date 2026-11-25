---
title: Hand-off
section: Window
---
Everything the plugin is built from. The numbers come from one file (`build/ui/core.js`), which also draws the prototype and every picture on these pages, so they agree.

- **The spec, written out:** {{asset ui-handoff/spec-md}}
- **The spec as JSON**, for code: {{asset ui-handoff/spec-json}}
- **Tokens (DTCG):** {{asset ui-handoff/tokens}} {{asset ui-handoff/tokens-light}} {{asset ui-handoff/tokens-dark}}
- **Release assets** are on the Release page.
- **The prototype:** open it in a browser and use the window. {{asset ui-handoff/prototype}}
- **Contrast:** {{asset ui-handoff/contrast}}

## Files to embed

{{asset fonts/anybody-medium}}

{{asset fonts/anybody-semibold}}

{{asset fonts/anybody-bold}}

{{asset fonts/departure-mono}}

{{asset ui-assets/wordmark-header}}

{{asset ui-assets/credit}}

For the dark theme: {{asset ui-assets/wordmark-header-dark}} {{asset ui-assets/credit-dark}}

Ship {{asset data/anybody-ofl}} and {{asset data/departure-mono-ofl}} with the plugin.

## What the window needs from the processor

- The input spectrum, and the result spectrum after thinning and Mix but before Output and Auto gain, in dB on the same bins. The gap between them is all the analyser shows, so a level change never looks like thinning.
- The effective thinning curve as values 0 to 2 on a log frequency axis, 20 Hz to 20 kHz, in both modes. In HQ it equals the drawn curve.
- The latency in ms, for the label beside Live / HQ.
- Display ballistics the stand-in uses: rise at once, fall 28 dB per second, a 1-2-1 smoothing across neighbouring bins, the 4.5 dB/oct tilt pivoting at 1 kHz, at 30 to 60 frames a second.

Every state is in the States collection at 1x and in States 2x at twice the size.
