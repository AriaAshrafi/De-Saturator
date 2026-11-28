# Changes

## 1.2.0

- **Output goes up to +36 dB** (Aria, after testing 0.9.0): the range is now −24 to +36 dB. The default stays 0 dB, the wheel step 0.5 dB and the format "+36.0 dB". The widest values, "−24.0 dB" and "+36.0 dB", are both 8 characters, and the value label and its chip hold them. The value arc still starts at 0 dB, which is now at −27°, 40% of the way round.
- **A 0 dB tick on Output**: a short mark outside the track at 0 dB, radius 33 to 38, graphite deep 1.25 (ink under the pointer), so unity is easy to find on a 60 dB knob. Restore and Mix have no origin, so they get no tick.
- The drag doesn't change: 240 px for the whole range (now 0.25 dB a pixel, a 0.5 dB step every 2 px), 1200 px with Shift (0.05 dB a pixel). Double-click still resets to 0 dB.
- A new state, `states/output-max.png`: Output dragged to +36 dB.

## 1.1.0

- **No keyboard.** The keyboard shortcuts are gone from the spec and the prototype (Aria). The window takes no keys and never takes keyboard focus, so the host keeps its shortcuts. Mouse and wheel are unchanged; Shift still fines a drag.
- **Dark theme.** The same window in ink, switched by Light / Dark at the bottom right (x 848, y 562, 88 × 24). It's a window preference kept with the plugin's state, not a parameter. Every colour role now has a light and a dark value (`spec.json` → `roles`, `tokens/de-saturator.{light,dark}.tokens.json`); the dark header name and credit are `ui/assets/*-dark.svg`. Eight dark states in `states/*-dark.png`.
- **Release assets** in `installers/` and `favicons/`: ICON_BIG, the Windows .ico, the VST3 bundle's PlugIn.ico, desktop.ini and snapshots, Inno Setup wizard images, macOS .icns, .pkg and disk image backgrounds. `installers/README.md` says where each goes.
- **Store and social pictures** in `store/`: a hero, a square and a GitHub preview, light and dark, at 1x and 2x.
- The credit shows the plugin's own version string, not the design's. The sample latency in the pictures is 85 ms.
- The tokens are now three files: sizes, strokes and type in `de-saturator.tokens.json`, colours per theme.

## 1.0.0

- First hand-off: direction B, Hairline.
