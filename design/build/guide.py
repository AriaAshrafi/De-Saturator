"""Writes the Specimen guideline pages.

    python guide.py   ->  guidelines/*.md

Run after the other generators: every embed is checked against build/manifest/.
"""
import json

from ds import ROOT, write

VERSION = (ROOT / "VERSION").read_text().strip()
ITEMS = [a for f in sorted((ROOT / "build/manifest").glob("*.json")) for a in json.loads(f.read_text(encoding="utf-8"))]
SLUGS = {(a["collection"], a["slug"]) for a in ITEMS}
SPEC = json.loads((ROOT / "spec/spec.json").read_text(encoding="utf-8"))


def A(coll, slug):
    assert (coll, slug) in SLUGS, f"no asset {coll}/{slug}"
    return "{{asset " + coll + "/" + slug + "}}"


PAGES = []


def page(title, section, body):
    PAGES.append((title, section, body.strip() + "\n"))


page("De-Saturator", "Brand", f"""
The opposite of a saturator. It thins a sound: takes out harmonic density, body and squashed peaks. One knob makes it thinner; a curve drawn on the analyser says where. It goes from a transparent fix to an extreme effect.

{A("ui-states", "in-use")}

The brand is **Hairline**: thinner, drawn literally. What stays is heavy and what's removed is a hairline, in the analyser, in the Amount ring and in the name, which loses weight letter by letter.

De-Saturator is a sub-brand of Aria Ashrafi. It keeps the parent's colours, its flat fills, its corners (square or fully round, nothing between), its hard print offset and two of its three typefaces. Klein, one of the parent's track keys, is its one accent: the hand, meaning everything you set.

- **Product:** a free plugin, VST3 on Windows and macOS, AU on macOS. Built with JUCE 8.
- **Version:** {VERSION}. The plugin is built from the files on the Hand-off page.
- **The marks are reserved.** The name and the mark are Aria Ashrafi's; the fonts are open (SIL OFL).
""")

page("Logo", "Brand", f"""
## The name

**De-Saturator**, drawn in Anybody, losing weight from Black at the D to ExtraLight at the last r. Both a's open to width 150, the parent's rule: A gets the room. It's a drawing, not typed text: always use the files.

{A("logo-png", "wordmark-ink-png")}

{A("logo-png", "wordmark-paper-png")}

Under 24 px tall the thin end breaks up. Use the small cut: its weight stops at 420.

{A("logo", "wordmark-small-ink")}

## The mark

One period of the parent's sine wave, losing weight from left to right: a sound getting thinner. Below 32 px use the small cut, whose thin end stays a third as heavy as the thick one.

{A("symbol-png", "mark-ink-png")}

{A("symbol", "mark-small-klein")}

## Icon tiles

The mark on a square, hard corners. Klein is the main tile; ink and paper are for places where klein clashes. 16 and 32 px use the small cut.

{A("symbol-png", "tile-klein-512")}

{A("symbol-png", "tile-ink-256")}

{A("symbol-png", "tile-paper-256")}

## Lockups

The mark beside the name, as tall as the D. With the parent: the Aria Ashrafi credit under the name's end.

{A("logo-png", "lockup-ink-png")}

{A("logo-png", "endorsed-paper-png")}

## Clear space

Half the D's height on every side of the name, and the mark's thick end around the mark.

{A("logo", "clear-space")}

## Don't

- Don't set the name in a font or at one weight. The ramp is the name.
- Don't reverse the ramp or the mark: thinning runs left to right.
- Don't colour the name klein. It's ink or paper; klein is for what the user sets.
- Don't add effects: no gradients, glows or soft shadows.
""")

roles = "\n".join(f"| {k} | {v['color']}{' at ' + str(int(v['alpha'] * 100)) + '%' if 'alpha' in v else ''} | `{v['hex']}` | {v['use']} |" for k, v in SPEC["roles"].items())
contrast = "\n".join(f"| {c['pair']} | {c['ratio']}:1 | {'decorative' if c['pass'] is None else ('passes ' if c['pass'] else 'fails ') + str(c['needs']) + ':1'} | {c['use']} |" for c in SPEC["contrast"])
page("Colour", "Brand", f"""
Four of the parent's colours, and one rule: **weight means content, klein means you**. Ink in every weight draws the sound; klein draws what you set (the curve, its points, the value arcs, Delta when it's on). Nothing else is coloured.

| Role | Colour | Hex | Use |
|---|---|---|---|
{roles}

The body comb under the result line is the only colour drawn at an opacity: ink at 16%. Everything else is flat.

## Contrast

| Pair | Ratio | Needs | Where |
|---|---|---|---|
{contrast}

Paper line never carries meaning on its own: it's the grid and the tracks.
""")

page("Type", "Brand", f"""
Two of the parent's three typefaces, both under the SIL Open Font License 1.1, so they ship inside the free plugin. Ship the licence files with it.

- **Anybody** is the voice: labels, switches, tooltips, the Amount title. The plugin embeds three static cuts at normal width, so it never has to instance a variable font.
- **Departure Mono** is the readout face: every value, the frequency scale, chips, the latency. It is a pixel font, drawn for 11 px and 22 px at 100%.
- The name is never set from a font: it's a drawing (see Logo).

| Role | File | Size | Use |
|---|---|---|---|
""" + "\n".join(f"| {k} | `{t['file']}` | {t['size']} px | {t['use']} |" for k, t in SPEC["type"].items()) + f"""

Sentence case everywhere, no letter-spacing.

{A("fonts", "anybody-semibold")}

{A("fonts", "anybody-bold")}

{A("fonts", "anybody-medium")}

{A("fonts", "departure-mono")}
""")

page("The window", "Window", f"""
One view, 960 × 600 at 100%, resizable from 75% to 200% at a fixed 16:10. Everything is shapes and strokes, so it scales without bitmaps.

{A("ui-states", "default")}

- **Header:** the name; Clean / Character; Live / HQ with the latency beside it; Delta; Bypass. Under the name, a small key to the analyser.
- **Left:** the analyser with the thinning curve over it, and Shape under it.
- **Right:** Amount, the hero, with Restore, Mix and Output under it, and Auto gain under Output.
- **Bottom left:** the Aria Ashrafi credit and the version.

Every position and size, in window pixels at 100%:

{A("ui-states", "redlines")}

At the extremes:

{A("ui-states", "scale-75")}

{A("ui-states-2x", "scale-200-2x")}
""")

page("Analyser", "Window", f"""
A live spectrum, 20 Hz to 20 kHz, −90 to +6 dB, tilted 4.5 dB per octave around 1 kHz so the top end isn't empty. It shows only what thinning does: the result is measured after Mix but before Output and Auto gain, so a level change never looks like thinning.

It's a comb of 120 lines with two lines drawn across it:

- **Result**, the heavy ink line: what you hear. Under it, the comb in ink at 16%, quiet.
- **Original**, the fine ink line.
- **Removed**, the hatching between them: the comb's lines carry on as ink hairlines up to the original.

{A("ui-states", "in-use")}

**Delta** plays only what's removed, so the hatching becomes the heavy part, in klein, and the result drops to a fine grey line.

{A("ui-states", "delta-on")}

**Character** takes out more with the same settings, so the hatched band widens.

{A("ui-states", "character")}

**Bypassed**: nothing thins. One heavy line, no hatching; the curve and the value arcs fade.

{A("ui-states", "bypassed")}

With no signal the comb is empty and the curve stays.

{A("ui-states", "silent")}
""")

page("Thinning curve", "Window", f"""
Drawn on the analyser in klein. Its height is how much thinning happens at that frequency, not volume: the middle line (100%) follows Amount, the top is twice Amount (200%), the bottom leaves that range untouched (0%). It starts flat at 100%.

Up to six points, each a bell with a frequency, an amount and a width (0.3 to 4 octaves). Double-click empty space to add one, drag it, scroll on it to change its width, double-click it to remove it.

| State | |
|---|---|
| Hover | {A("ui-states", "point-hover")} |
| Selected | {A("ui-states", "point-selected")} |
| Dragging | {A("ui-states", "point-drag")} |
| Six points | {A("ui-states", "points-full")} |

The readout says frequency, amount and width: `3.3 kHz  175%  0.7 oct`.

**Live** has no latency and applies a smoothed version of the curve. Both are shown: your curve dashed, the applied one solid. In HQ they're the same line.

{A("ui-states", "live")}

{A("ui-states", "hq")}
""")

page("Controls", "Window", f"""
## Amount

The hero. Its ring thins as you turn it up, from a near-solid disc at 0% to a hairline at 100%, with a notch that shows the turn. A klein dot rides the scale. Under the pointer the ticks darken and the value sits on an ink chip.

{A("ui-states", "amount-hover")}

{A("ui-states", "amount-drag")}

## Restore, Mix, Output

Small knobs: a paper disc with an ink outline and pointer, a klein value arc (Output's starts at 0 dB). Under the pointer the outline thickens and the value turns into a chip.

{A("ui-states", "knob-hover")}

{A("ui-states", "knob-drag")}

## Shape

A blend from Harmonics (overtones) to Body (weight). Each end shows its share.

{A("ui-states", "shape-drag")}

## Switches and buttons

Clean / Character and Live / HQ are pill switches: the chosen side is ink. Delta turns klein when it's on, because what you hear is then what you set. Auto gain and Bypass turn ink.

{A("ui-states", "switch-hover")}

## Tooltips

After 600 ms over anything: an ink box with paper text, in plain words.

{A("ui-states", "tooltip")}

{A("ui-states", "tooltip-point")}

## Using it

- Drag knobs up or down; Shift for fine. Double-click resets. The wheel steps a knob once you've clicked in the window.
- Keys, with the window focused: arrows move the selected point (or Amount, with none selected), Delete removes it, D is Delta, B is Bypass.
- Nothing eases: values and switches change on the next frame.
""")

page("Hand-off", "Window", f"""
Everything the plugin is built from. The numbers come from one file (`build/ui/core.js`), which also draws the prototype and every picture on these pages, so they agree.

- **The spec, written out:** {A("ui-handoff", "spec-md")}
- **The spec as JSON**, for code: {A("ui-handoff", "spec-json")}
- **Tokens (DTCG):** {A("ui-handoff", "tokens")}
- **The prototype:** open it in a browser and use the window. {A("ui-handoff", "prototype")}
- **Contrast:** {A("ui-handoff", "contrast")}

## Files to embed

{A("fonts", "anybody-medium")}

{A("fonts", "anybody-semibold")}

{A("fonts", "anybody-bold")}

{A("fonts", "departure-mono")}

{A("ui-assets", "wordmark-header")}

{A("ui-assets", "credit")}

Ship {A("data", "anybody-ofl")} and {A("data", "departure-mono-ofl")} with the plugin.

## What the window needs from the processor

""" + "\n".join(f"- {p}" for p in SPEC["processor"]) + """

Every state is in the States collection at 1x and in States 2x at twice the size.
""")

out = ROOT / "guidelines"
out.mkdir(exist_ok=True)
for old in out.glob("*.md"):
    old.unlink()
for i, (title, section, body) in enumerate(PAGES, 1):
    write(f"guidelines/{i:02d}-{title.lower().replace(' ', '-')}.md", f"---\ntitle: {title}\nsection: {section}\n---\n{body}")
print(f"guidelines: {len(PAGES)} pages")
