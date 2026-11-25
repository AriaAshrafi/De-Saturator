# Changes

## 1.1.0

- **No keyboard.** The keyboard shortcuts are gone from the spec and the prototype (Aria). The window takes no keys and never takes keyboard focus, so the host keeps its shortcuts. Mouse and wheel are unchanged; Shift still fines a drag.
- **Dark theme.** The same window in ink, switched by Light / Dark at the bottom right (x 848, y 562, 88 × 24). It's a window preference kept with the plugin's state, not a parameter. Every colour role now has a light and a dark value (`spec.json` → `roles`, `tokens/de-saturator.{light,dark}.tokens.json`); the dark header name and credit are `ui/assets/*-dark.svg`. Eight dark states in `states/*-dark.png`.
- **Release assets** in `installers/` and `favicons/`: ICON_BIG, the Windows .ico, the VST3 bundle's PlugIn.ico, desktop.ini and snapshots, Inno Setup wizard images, macOS .icns, .pkg and disk image backgrounds. `installers/README.md` says where each goes.
- **Store and social pictures** in `store/`: a hero, a square and a GitHub preview, light and dark, at 1x and 2x.
- The credit shows the plugin's own version string, not the design's. The sample latency in the pictures is 85 ms.
- The tokens are now three files: sizes, strokes and type in `de-saturator.tokens.json`, colours per theme.

## 1.0.0

- First hand-off: direction B, Hairline.
