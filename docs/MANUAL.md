# De-Saturator: manual

De-Saturator thins a sound. A saturator adds harmonics, fills the gaps in the spectrum and flattens peaks, which makes a sound thicker. De-Saturator does the opposite of each of those: it takes out overtones, clears the fill between the strong partials, lightens the low mids and gives peaks back.

It is free, and an early release: it works, and its sound may still be tuned between versions.

## Quick start

1. Put it on a track that feels too thick or too dense.
2. Turn **Amount** up until it's thin enough.
3. Press **Delta** to hear only what is being taken away. If it is taking something you want to keep, move **Shape**, or draw a point on the analyser and pull it down over that range.
4. Leave **Auto gain** on, so you compare thin against thick and not quiet against loud.

## The analyser

The display shows the sound from 20 Hz to 20 kHz.

- The **heavy line** is the result: what you hear.
- The **fine line** above it is the original.
- The **hatched band** between them is what was removed.

The result is measured before Output and Auto gain, so the gap only ever shows thinning, never a level change.

## Controls

**Amount** (0 to 100%). How much thinner. Everything else shapes what Amount does. The first third of its travel already makes a clear difference; Clean can take up to 30 dB out of a part of the sound, Character up to 60 dB.

**The thinning curve.** Drawn on the analyser, like an EQ, but its height sets *how much thinning* happens at each frequency, not volume.

- The middle (100%) follows Amount.
- The top (200%) thins twice as much there.
- The bottom (0%) leaves that range completely untouched.
- Double-click empty space to add a point (up to six). Drag a point to move it. Scroll over it to change its width (0.3 to 4 octaves). Double-click it to remove it.

**Shape** (Harmonics to Body). What kind of thinning.

- Towards **Harmonics**: takes out overtones, the upper partials of pitched sounds. The sound gets purer and less rich.
- Towards **Body**: takes out weight in the low mids. The sound gets lighter.
- In the middle you get both in full.
- At every setting it also clears the fill between partials, and pulls down the parts of the spectrum that already sit lower than their surroundings, so the sound is left standing on its strongest regions.

**Restore** (0 to 100%). Undoes the squashing itself. A soft clipper bends the top of the waveform over; Restore applies the exact reverse of that curve, which cancels the harmonics and the grit the clipping added, and it lets hits and attacks stand out again.

- In **Clean**, Restore *measures* how hard the sound was driven. It listens to the incoming audio all the time, tries a range of reverse curves and keeps the one that leaves the cleanest spectrum, settling within a couple of seconds. The knob is how much of that measured correction to apply: at 100% it undoes what it found. If the sound was never saturated, or it can't tell (on noise, or very light saturation), it does nothing, so it can't make clean audio worse.
- In **Character**, Restore doesn't ask: it forces a strong reverse curve and lifts every attack, which opens a squashed sound right up and gives clean audio a peaky, exaggerated edge.

Restore works on the sound before it is thinned.

**Mix** (0 to 100%). The thinned sound against the original.

**Output** (-24 to +36 dB). The level after everything else. The top of its range is a lot of gain: turn it up with care.

**Auto gain.** Keeps the loudness steady as you thin, by up to 24 dB, so thinner doesn't just mean quieter. It measures loudness the way the ear weighs it (the K-weighting used for broadcast loudness), and follows the sound over about a second, so it does not pump.

**Delta.** Plays only what is being removed. Use it to check that you are removing what you meant to.

**Clean / Character.**

- **Clean** is the transparent range, for fixing a sound that is too thick. It is limited in how deep it can cut.
- **Character** goes much further with the same settings, for effect: hollow, glassy, skeletal.

**Live / HQ.**

- **HQ** is the full engine. It splits the spectrum finely enough to tell single partials from what lies between them, so it can clear the fill and leave the notes. It adds latency, which the plugin reports to the host so tracks stay in time: 4096 samples at 44.1 and 48 kHz (93 and 85 ms), and the same length of time at higher rates.
- **Live** has no latency at all, for playing and recording through. It works with ten bands and a pitch tracker instead of a fine spectrum, so it is broader: the thinning curve is applied in a smoothed form (the window draws the curve you drew as a dashed line and the curve that is applied as a solid one), and overtone removal only engages on a single clear pitch, such as a bass or a voice, fading out on chords.

Switching between Live and HQ changes the plugin's latency, and the audio restarts with a short fade.

**Bypass.** The untouched sound, kept in time.

**Light | Dark.** The window's theme. New instances open with the one you chose last.

## Where each mode fits

| | Good for | Notes |
|---|---|---|
| HQ, Clean | Mixing: buses, dense synths, distorted guitars, over-saturated stems | The most exact |
| HQ, Character | Sound design, breakdowns, making something skeletal | Can go as far as 60 dB down |
| Live, Clean | Tracking and playing through | Broader strokes, no delay |
| Live, Character | Performance effects | |

## Details

- Formats: VST3 on Windows and macOS, Audio Unit on macOS. Mono and stereo.
- Sample rates: 44.1 kHz to 192 kHz.
- Stereo: both channels always get the same thinning, so the image doesn't move.
- With Amount and Restore at 0 the audio passes through bit for bit.
- Onsets (drum hits, consonants) are detected and let through, so thinning doesn't soften attacks.
- The curve, the theme and the window's size are saved with the project. The curve is not automatable; every other control is.
- No keyboard shortcuts: the window never takes keys from the host.

## Installing

**Windows.** Run the installer. The plugin goes to `C:\Program Files\Common Files\VST3`.

**macOS.** Open the `.pkg`. This release is not signed with an Apple developer certificate, so macOS will refuse to open it by double-click. Right-click the package and choose **Open**, then **Open** again; or try to open it once, then go to **System Settings > Privacy & Security** and press **Open Anyway**. The plugins go to `/Library/Audio/Plug-Ins/VST3` and `/Library/Audio/Plug-Ins/Components`.

## Licence

GPL-3.0. Source: https://github.com/AriaAshrafi/De-Saturator. The fonts in the window (Anybody, Departure Mono) are under the SIL Open Font License 1.1.
