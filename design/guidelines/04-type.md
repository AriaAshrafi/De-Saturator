---
title: Type
section: Brand
---
Two of the parent's three typefaces, both under the SIL Open Font License 1.1, so they ship inside the free plugin. Ship the licence files with it.

- **Anybody** is the voice: labels, switches, tooltips, the Amount title. The plugin embeds three static cuts at normal width, so it never has to instance a variable font.
- **Departure Mono** is the readout face: every value, the frequency scale, chips, the latency. It is a pixel font, drawn for 11 px and 22 px at 100%.
- The name is never set from a font: it's a drawing (see Logo).

| Role | File | Size | Use |
|---|---|---|---|
| title | `Anybody-Bold-700.ttf` | 15 px | The Amount title. |
| button | `Anybody-SemiBold-600.ttf` | 13 px | Switches and the Delta pill. |
| label | `Anybody-SemiBold-600.ttf` | 12 px | Control labels, Auto gain. |
| tooltip | `Anybody-Medium-500.ttf` | 12 px | Tooltips. |
| value | `DepartureMono-Regular.otf` | 11 px | Values, frequency and scale labels, chips, latency, version. |
| valueLarge | `DepartureMono-Regular.otf` | 22 px | The Amount value. |

Sentence case everywhere, no letter-spacing.

{{asset fonts/anybody-semibold}}

{{asset fonts/anybody-bold}}

{{asset fonts/anybody-medium}}

{{asset fonts/departure-mono}}
