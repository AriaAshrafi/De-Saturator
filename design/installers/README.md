# Release assets

Made by `build/installers.py` (then `node build/render.mjs build/jobs/installers.json` and `python build/installers.py pack`). Paths are relative to `design/`.

## The plugin icon

- **`installers/icon-big.png`**: 1024 × 1024, klein, in Apple's rounded template with its transparent margin. Give it to JUCE as `ICON_BIG` (no `ICON_SMALL`: JUCE scales this one down).
- Klein is the main tile, not ink: the logo guidelines keep ink and paper for places where klein clashes. Full-bleed squares, if you need them: `logo/png/de-saturator-tile-klein-1024.png` and its smaller sizes.

## VST3 bundle

| File | Goes to |
|---|---|
| `installers/vst3/PlugIn.ico` | `De-Saturator.vst3/PlugIn.ico` (Windows) |
| `installers/vst3/desktop.ini` | `De-Saturator.vst3/desktop.ini` (Windows). Give the `.vst3` folder the read-only or system attribute so Explorer reads it. |
| `installers/vst3/Resources/Snapshots/PROCESSOR-UID_snapshot.png` | `De-Saturator.vst3/Contents/Resources/Snapshots/`, renamed with the processor's class ID (32 hex digits) |
| `installers/vst3/Resources/Snapshots/PROCESSOR-UID_snapshot_2.0x.png` | Same folder, same rename |

The snapshots are the default window in the light theme, 960 × 600 and 1920 × 1200.

## Windows installer (Inno Setup)

```
SetupIconFile=installers\windows\de-saturator.ico
UninstallDisplayIcon={app}\de-saturator.ico
WizardImageFile=installers\windows\wizard\WizardImage-164x314.bmp,installers\windows\wizard\WizardImage-192x386.bmp,installers\windows\wizard\WizardImage-246x459.bmp,installers\windows\wizard\WizardImage-273x556.bmp,installers\windows\wizard\WizardImage-328x604.bmp,installers\windows\wizard\WizardImage-355x700.bmp,installers\windows\wizard\WizardImage-410x797.bmp
WizardSmallImageFile=installers\windows\wizard\WizardSmallImage-55x58.bmp,installers\windows\wizard\WizardSmallImage-64x68.bmp,installers\windows\wizard\WizardSmallImage-83x80.bmp,installers\windows\wizard\WizardSmallImage-92x97.bmp,installers\windows\wizard\WizardSmallImage-110x106.bmp,installers\windows\wizard\WizardSmallImage-119x123.bmp,installers\windows\wizard\WizardSmallImage-138x140.bmp
WizardStyle=modern
```

The paths are relative to `design/`; adjust them to where the `.iss` file lives. `installers/windows/wizard/preview.png` is the 200% image as a PNG, to look at.

## macOS installer

- **`.pkg` background**: `installers/macos/pkg/background-light.png` and `background-dark.png`, each with an `@2x`. In `distribution.xml`:

  ```xml
  <background file="background-light.png" alignment="bottomleft" scaling="none" mime-type="image/png"/>
  <background-darkAqua file="background-dark.png" alignment="bottomleft" scaling="none" mime-type="image/png"/>
  ```

  Only the bottom 70 px carry art, so the installer's own text stays clear.
- **Disk image** (if you ship one): `installers/macos/dmg/background.png` (+ `@2x`) for a 660 × 400 window; put the `.pkg` at (200, 190) and a read-me at (460, 190). `installers/macos/dmg/VolumeIcon.icns` is the mounted volume's icon.
- **Icon**: `installers/macos/de-saturator.icns` (16 to 1024 px).

## The product page

`favicons/favicon.ico` (16, 32, 48) and `favicons/apple-touch-icon.png` (180). Store and social pictures are in `store/`.
