---
title: Analyser
section: Window
---
A live spectrum, 20 Hz to 20 kHz, −90 to +6 dB, tilted 4.5 dB per octave around 1 kHz so the top end isn't empty. It shows only what thinning does: the result is measured after Mix but before Output and Auto gain, so a level change never looks like thinning.

It's a comb of 120 lines with two lines drawn across it:

- **Result**, the heavy ink line: what you hear. Under it, the comb in ink at 16%, quiet.
- **Original**, the fine ink line.
- **Removed**, the hatching between them: the comb's lines carry on as ink hairlines up to the original.

{{asset ui-states/in-use}}

**Delta** plays only what's removed, so the hatching becomes the heavy part, in klein, and the result drops to a fine grey line.

{{asset ui-states/delta-on}}

**Character** takes out more with the same settings, so the hatched band widens.

{{asset ui-states/character}}

**Bypassed**: nothing thins. One heavy line, no hatching; the curve and the value arcs fade.

{{asset ui-states/bypassed}}

With no signal the comb is empty and the curve stays.

{{asset ui-states/silent}}
