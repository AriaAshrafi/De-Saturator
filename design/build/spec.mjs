// Writes the build spec from ui/core.js, so the numbers and the pictures can't drift apart:
//   spec/spec.json   everything, for code
//   spec/SPEC.md     the same, for people
//   spec/contrast.txt
//   tokens/de-saturator.tokens.json   DTCG, for Specimen
//   node spec.mjs
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { createRequire } from "node:module";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = join(HERE, "..");
const DS = createRequire(import.meta.url)("./ui/core.js");
const { L, S, T, COLOR, ROLE, STROKE, TYPE, PARAMS, CURVE, DRAG, TIPS, NOTES, ALPHA } = DS;
const r2 = (v) => Math.round(v * 100) / 100;
const OUTP = PARAMS.find((p) => p.id === "output");
const OUTSPAN = OUTP.max - OUTP.min;
const OUT0 = r2(-135 + 270 * (OUTP.origin - OUTP.min) / OUTSPAN);   // where 0 dB sits on the knob, 0° up, clockwise

/* ---------- contrast ---------- */
const lin = (c) => { c /= 255; return c <= 0.03928 ? c / 12.92 : ((c + 0.055) / 1.055) ** 2.4; };
const lum = (h) => { const [r, g, b] = [1, 3, 5].map((i) => parseInt(h.slice(i, i + 2), 16)); return 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b); };
const ratio = (a, b) => { const [x, y] = [lum(a), lum(b)].sort((p, q) => q - p); return (x + 0.05) / (y + 0.05); };
const PAIRS = [
  ["ink on paper", COLOR.ink, COLOR.paper, 4.5, "labels, values, the result line"],
  ["graphite deep on paper", COLOR.graphiteDeep, COLOR.paper, 4.5, "values, latency, frequency labels"],
  ["klein on paper", COLOR.klein, COLOR.paper, 3, "the curve, points, value arcs (graphics)"],
  ["paper on klein", COLOR.paper, COLOR.klein, 4.5, "Delta's label when on"],
  ["paper on ink", COLOR.paper, COLOR.ink, 4.5, "chips, tooltips, selected switches"],
  ["paper line on paper", COLOR.paperLine, COLOR.paper, 0, "grid and tracks: decorative, never carries meaning alone"],
  ["dark: paper on ink", COLOR.paper, COLOR.ink, 4.5, "labels, values, the result line"],
  ["dark: graphite on ink", COLOR.graphite, COLOR.ink, 4.5, "values, latency, frequency labels"],
  ["dark: klein light on ink", COLOR.kleinLight, COLOR.ink, 3, "the curve, points, value arcs (graphics)"],
  ["dark: ink on klein light", COLOR.ink, COLOR.kleinLight, 4.5, "Delta's label when on"],
  ["dark: ink on paper", COLOR.ink, COLOR.paper, 4.5, "chips, tooltips, selected switches"],
  ["dark: ink line on ink", COLOR.inkLine, COLOR.ink, 0, "grid and tracks: decorative"],
];
const contrast = PAIRS.map(([n, a, b, need, use]) => { const c = ratio(a, b); return { pair: n, ratio: r2(c), needs: need || null, pass: need ? c >= need : null, use }; });
const verdict = (c) => c.pass == null ? "decorative" : c.pass ? "pass" : "FAIL";

/* ---------- tokens (DTCG) ---------- */
const comps = (h) => [1, 3, 5].map((i) => Math.round(parseInt(h.slice(i, i + 2), 16) / 255 * 10000) / 10000);
const color = (h, d, alpha) => ({ $value: Object.assign({ colorSpace: "srgb", components: comps(h), hex: h.toLowerCase() }, alpha != null ? { alpha } : {}), ...(d ? { $description: d } : {}) });
const px = (v, d) => ({ $value: { value: v, unit: "px" }, ...(d ? { $description: d } : {}) });
const kebab = (k) => k.replace(/[A-Z]/g, (m) => "-" + m.toLowerCase());
const colourTokens = (theme) => ({
  $description: `De-Saturator's colours, ${theme} theme. Every one is the parent's.`,
  "ds-color": Object.assign({ $type: "color", $description: "Prefixed ds- so nothing inherits the parent's group types." },
    Object.fromEntries(Object.entries(ROLE).map(([k, [, d]]) => [kebab(k), color(COLOR[DS.THEMES[theme].roles[k]], d, k === "body" ? DS.THEMES[theme].bodyAlpha : undefined)]))),
});
const tokens = {
  $description: "De-Saturator: what it adds to aria-ashrafi, apart from colour. Window px at 100%; the window scales 75% to 200% at a fixed ratio.",
  "ds-stroke": Object.assign({ $type: "dimension" }, Object.fromEntries(Object.entries(STROKE).map(([k, [v, d]]) => [k.replace(/[A-Z]/g, (m) => "-" + m.toLowerCase()), px(v, d)]))),
  "ds-size": {
    $type: "dimension",
    "window-width": px(DS.W), "window-height": px(DS.H),
    "radius-square": px(0, "The window, the analyser, chips, tooltips."), "radius-pill": px(9999, "Switches, pills, the Shape thumb: half their height. Nothing in between."),
    "amount-radius": px(L.amount.r, "The Amount ring's outer edge."), "amount-track": px(L.amount.track, "Radius of the Amount track and value arc."),
    "knob-radius": px(L.small.r), "knob-track": px(L.small.track),
    "point-radius": px(L.point.r), "point-radius-hover": px(L.point.rHover), "point-radius-selected": px(L.point.rSelected), "point-halo": px(L.point.halo),
    "control-height": px(32, "Switches, Delta, Bypass."), "pill-small-height": px(24, "Auto gain."),
    "chip-height": px(19), "tooltip-max-width": px(L.tooltip.maxWidth),
  },
  "ds-type": {
    "family-voice": { $type: "fontFamily", $value: ["Anybody", "system-ui", "sans-serif"], $description: "Labels, switches, tooltips. Static cuts at width 100." },
    "family-machine": { $type: "fontFamily", $value: ["Departure Mono", "ui-monospace", "monospace"], $description: "Values. 11 or 22 px at 100%." },
    ...Object.fromEntries(Object.entries(TYPE).flatMap(([k, t]) => { const n = k.replace(/[A-Z]/g, (m) => "-" + m.toLowerCase()); return [[`${n}-size`, { $type: "dimension", ...px(t.size, t.use) }], [`${n}-weight`, { $type: "fontWeight", $value: t.weight }]]; })),
  },
  "ds-motion": {
    "tooltip-delay": { $type: "duration", $value: { value: L.tooltip.delayMs, unit: "ms" }, $description: "Rest the pointer this long for a tooltip." },
    "note-time": { $type: "duration", $value: { value: 2000, unit: "ms" }, $description: "How long a passing note (six points) stays." },
  },
};

/* ---------- the spec ---------- */
const seg = (g) => g.segs.reduce((a, s) => a + s[2], 0);
const PLOT = L.plot;
const components = {
  wordmark: { file: "ui/assets/wordmark-header.svg", fileDark: "ui/assets/wordmark-header-dark.svg", x: L.wordmark.x, baseline: L.wordmark.baseline, size: L.wordmark.size, note: "An outlined drawing, not text: the weight changes letter by letter. Draw the SVG with its origin at x 24 and its baseline (y 0 in the file) at y 38. Ink in the light theme, paper in the dark one (wordmark-header-dark.svg), in every state." },
  theme: { x: L.theme.x, y: L.theme.y, h: L.theme.h, segments: L.theme.segs.map(([v, l, w]) => ({ value: v, label: l, width: w })), note: "A small switch like Clean / Character, 24 high, label type. A window preference, not a parameter: keep it in the plugin's state, and start new instances with the theme last chosen." },
  legend: { baseline: 64, items: [{ x: 24, swatch: "18 px line, ink 2", label: "Result" }, { x: 100, swatch: "18 px line, ink 1", label: "Original" }, { x: 190, swatch: "four 10 px verticals 5 px apart, ink 1 (Delta: klein 2)", label: "Removed" }], labelDx: 24, type: "label", color: "textMuted", note: "Swatches follow the analyser's current style." },
  switch: { ids: ["character", "quality"], character: { x: L.character.x, y: L.character.y, h: L.character.h, segments: L.character.segs.map(([v, l, w]) => ({ value: v, label: l, width: w })) }, quality: { x: L.quality.x, y: L.quality.y, h: L.quality.h, segments: L.quality.segs.map(([v, l, w]) => ({ value: v, label: l, width: w })) }, track: "Pill outline, ink 1.5, radius half the height.", selected: "An inner pill inset 3 px, ink, with paper text.", hover: "On an unselected segment: a 1 px ink inner outline inset 3.5 px.", text: "button type, centred, baseline y + 20.5." },
  latency: { x: L.latency.x, baseline: L.latency.baseline, type: "value", color: "textMuted", text: "\"{ms} ms\" from the processor in HQ, \"0 ms\" in Live. Room for six characters (\"120 ms\")." },
  delta: { x: L.delta.x, y: L.delta.y, w: L.delta.w, h: L.delta.h, off: "Ink outline 1.5 (2.5 under the pointer), ink text.", on: "Klein fill, paper text.", text: "button type, centred." },
  bypass: { cx: L.bypass.cx, cy: L.bypass.cy, r: L.bypass.r, icon: "Arc radius 7 around (920, 29) from 38° to 322° (0° up, clockwise) and a stem from y 19 to 27, both 2 px with butt caps.", off: "Ink outline 1.5 (2.5 under the pointer), ink icon.", on: "Ink fill, paper icon." },
  analyser: {
    frame: { ...L.analyser, fill: "ground", stroke: "text", width: S.frame },
    plot: PLOT,
    strip: { y: PLOT.y + PLOT.h, h: L.strip.h, labelBaseline: L.strip.baseline, labels: ["50 Hz", "100", "200", "500", "1k", "2k", "5k", "10k"], type: "value", color: "textMuted" },
    frequency: { min: DS.FMIN, max: DS.FMAX, scale: "log", x: "x = 24 + 632 * log2(f / 20) / log2(1000)" },
    level: { min: DS.DBMIN, max: DS.DBMAX, tiltDbPerOctave: DS.TILT, tiltPivotHz: 1000, y: "y = 72 + 346 * (6 - dB) / 96" },
    grid: { frequencies: [50, 100, 200, 500, 1000, 2000, 5000, 10000], major: [100, 1000, 10000], minorOpacity: 0.55, dbLines: [-72, -48, -24, 0], dbDash: [2, 4], color: "grid", width: S.grid },
    comb: {
      lines: L.comb.lines, x: "x(j) = 24 + (j + 0.5) * 632 / 120, j = 0..119", base: DS.COMB_BASE,
      layers: [
        { name: "body", from: "plot bottom + 2", to: "result level", color: "ink", alpha: ALPHA.body, width: S.body },
        { name: "removed", from: "result level", to: "original level", color: "ink", width: S.hair, delta: { color: "hand", width: S.removedDelta }, bypass: "hidden" },
        { name: "original line", path: "through every analyser bin", color: "ink", width: S.original, bypass: "hidden (it equals the result)" },
        { name: "result line", path: "through every analyser bin", color: "ink", width: S.result, delta: { color: "textMuted", width: S.original } },
      ],
      note: "Clip all four to the plot. Sample each comb line at its own frequency from the spectra.",
    },
    scaleLabels: [{ text: "200%", baseline: PLOT.y + 16 }, { text: "100%", baseline: r2(DS.cyv(1) - 7) }, { text: "0%", baseline: PLOT.y + PLOT.h - 5 }].map((l) => ({ ...l, right: PLOT.x + PLOT.w - 10, chip: "paper at 85%, 15 high, 4 px past the text each side" })),
    stateLine: { x: PLOT.x + 15, baseline: PLOT.y + 21.5, chip: { x: PLOT.x + 8, y: PLOT.y + 8, h: 19, fill: "ground at 90%" }, type: "value", color: "text", priority: ["Bypassed", "Delta", "a passing note", "Live"], strings: NOTES },
  },
  curve: {
    model: "value(f) = clamp(1 + sum over points of (a - 1) * exp(-0.5 * (log2(f / f0) / (w / 2.3548))^2), 0, 2). w is the bell's width in octaves at half height.",
    y: "y = 72 + 346 * (1 - value / 2): 200% at the top, 100% (follows Amount) in the middle, 0% (untouched) at the bottom",
    hq: { under: { color: "ground", width: S.curveUnder }, line: { color: "hand", width: S.curve }, of: "the drawn curve" },
    live: { under: { color: "ground", width: S.curveUnder, of: "the applied curve" }, drawn: { color: "hand", width: S.curveDrawn, dash: [4, 4] }, applied: { color: "hand", width: S.curveApplied } },
    bypass: "Curve and points at 45% opacity.",
    points: { ...L.point, ring: S.pointRing, normal: "Paper fill, klein ring 2, radius 6.5.", hover: "Radius 8.5, plus the width bracket and the readout.", selected: "Klein fill radius 7 with a 2 px paper ring, a klein halo ring radius 12 (1.25), the width bracket and the readout. Stays after release.", drag: "As selected, plus a dashed klein drop line (1 px, 2/3) from 12 px under the point to the plot bottom.", bracket: "At the point's height, from f * 2^(-w/2) to f * 2^(w/2), klein 1.25, with 10 px end ticks.", position: "A point sits at (its frequency, its amount), not on the summed curve." },
    readout: { h: 19, padX: 7, baseline: 13.5, above: 39, belowWhenNearTop: 22, nearTop: 46, inset: 6, text: "\"{freq}  {amount}%  {width} oct\", two spaces apart. freq: \"140 Hz\", \"3.3 kHz\", \"12 kHz\". width to one decimal.", fill: "readout", textColor: "onReadout" },
    limits: CURVE,
  },
  amount: {
    ...L.amount,
    angle: "angle = -135° + 270° * amount / 100 (0° up, clockwise)",
    ticks: "11 ticks every 27° from radius 104 to 109; 113 at 0%, 50% and 100%. graphite deep 1.25; ink under the pointer.",
    track: "Arc radius 96, -135° to 135°, paper line 2.",
    valueArc: "Arc radius 96 from -135° to the angle, klein 2.",
    ring: { outer: L.amount.r, width: `w = ${S.ringMax} - ${S.ringMax - S.ringMin} * amount / 100`, centreRadius: "84 - w / 2", notch: "A 6 px gap (arc length at the centre radius) centred on the angle.", color: "text" },
    pointer: "A klein dot radius 5.5 (7.5 while dragging) with a 2 px paper ring, on radius 96 at the angle.",
    title: { text: "Amount", baseline: L.amount.title, type: "title" },
    value: { baseline: L.amount.readout, type: "valueLarge", hoverChip: L.amount.chip },
    bypass: "Ring, value arc and dot in graphite deep.",
  },
  knob: { ids: L.small.knobs.map(([id, label, cx]) => ({ id, label, cx })), cy: L.small.cy, r: L.small.r, body: "Paper fill, ink outline 1.5 (2.5 under the pointer or while dragging).", track: "Arc radius 29, -135° to 135°, paper line 3.5.", valueArc: `Klein 3.5 from -135° (Output: from 0 dB, at ${OUT0}°) to the angle. Graphite deep when bypassed.`, originTick: `Output only: a tick at 0 dB (${OUT0}°) from radius ${L.small.originIn} to ${L.small.originOut}, graphite deep 1.25, ink under the pointer or while dragging, so unity can be found on a ${OUTSPAN} dB knob.`, pointer: "Ink 3, square caps, from radius 7 to 18 at the angle.", label: { baseline: L.small.label, type: "label" }, value: { baseline: L.small.value, type: "value", color: "textMuted", hover: "On an ink chip 17 high, paper text." } },
  autoGain: { ...L.autoGain, on: "Ink fill, paper text.", off: "Ink outline 1.5 (2.5 under the pointer), ink text.", text: "label type, centred." },
  shape: { ...L.shape, track: "Paper line 3 from x 120 to 560, with a 12 px centre tick (1.5).", thumb: "Ink pill 16 × 30 centred on x = 120 + 440 * shape / 100, with a 14 px paper mark (2).", hover: "An ink outline 1.5 around the thumb, 4 px out.", ends: "\"Harmonics\" right-aligned at x 106, baseline 512, its share (100 - shape)% under it at 527; \"Body\" at x 574, its share shape% under it.", title: "\"Shape\" centred at x 340, baseline 492." },
  credit: { ...L.credit, file: "ui/assets/credit.svg", fileDark: "ui/assets/credit-dark.svg", version: { ...L.credit.version, text: "the plugin's version string, e.g. \"1.0.0\"", type: "value", color: "textMuted" } },
  tooltip: { ...L.tooltip, fill: "readout", text: "onReadout", type: "tooltip", placement: "8 px under the control (above when it would leave the window), centred, kept 8 px inside. On the analyser: 18 px under the pointer.", hides: "On press, on leaving the control, and while dragging." },
};
const params = PARAMS.map((p) => ({ ...p, format: p.kind === "knob" || p.kind === "slider" ? (p.id === "output" ? "one decimal, a sign, \"dB\": \"−1.5 dB\", \"0.0 dB\", \"+3.0 dB\"; the widest are \"−24.0 dB\" and \"+36.0 dB\", 8 characters" : "whole percent: \"30%\"") : undefined }));
const interaction = {
  drag: { vertical: true, fullRangeScreenPx: DRAG.fullRangePx, fineRangeScreenPx: DRAG.fineRangePx, fineKey: "Shift", note: "Knobs: up raises. Shape: horizontal, the thumb follows the pointer." },
  doubleClick: { knob: "reset to default", shape: "reset to default", emptyAnalyser: "add a point at the pointer (width 1 oct); with six already, show \"Six points is the most\" for 2 s", point: "remove it" },
  wheel: { knob: "one step per notch (1%, Output 0.5 dB)", point: "width ±0.1 oct per notch, 0.3 to 4", note: "Only once the window has been clicked, so a host's scroll isn't stolen." },
  pointer: { point: "Press selects and drags it: frequency follows x, amount follows y. Clicking empty analyser deselects.", hitRadius: L.point.hit, cursors: { knob: "up-down resize", shape: "left-right resize", point: "grab / grabbing", analyser: "crosshair (default when full)", buttons: "pointing hand" } },
  keyboard: "None. The window takes no keys and never takes keyboard focus, so the host keeps all its shortcuts (in JUCE: setWantsKeyboardFocus(false) and setMouseClickGrabsKeyboardFocus(false) on the editor and every control).",
  tooltips: { delayMs: L.tooltip.delayMs, strings: TIPS },
};
const processor = [
  "The input spectrum, and the result spectrum after thinning and Mix but before Output and Auto gain, in dB on the same bins. The gap between them is all the analyser shows, so a level change never looks like thinning.",
  "The effective thinning curve as values 0 to 2 on a log frequency axis, 20 Hz to 20 kHz, in both modes. In HQ it equals the drawn curve.",
  "The latency in ms, for the label beside Live / HQ.",
  "Display ballistics the stand-in uses: rise at once, fall 28 dB per second, a 1-2-1 smoothing across neighbouring bins, the 4.5 dB/oct tilt pivoting at 1 kHz, at 30 to 60 frames a second.",
];
const accessibility = [
  ...PARAMS.map((p) => ({ control: p.name, role: p.kind === "toggle" ? "toggle button" : p.kind === "choice" ? "radio group" : "slider", value: p.kind === "knob" || p.kind === "slider" ? "the readout text" : p.kind === "choice" ? p.labels.join(" or ") : "on / off", help: TIPS[p.id] ?? TIPS[`${p.id}:${p.options?.[0]}`] })),
  { control: "Thinning curve", role: "group", value: "\"{n} points\"", help: TIPS.display },
  { control: "Curve point", role: "slider (2D)", value: "the readout text", help: TIPS.point },
  { control: "Theme", role: "radio group", value: "Light or Dark", help: TIPS["theme:dark"] },
];

const spec = {
  name: "De-Saturator", version: DS.VERSION, direction: "Hairline",
  idea: "Thinner, drawn literally. What stays is heavy, what's removed is a hairline: in the analyser, in the Amount ring, in the name.",
  units: "Window pixels at 100%. Every number scales with the window; the 1 px hairlines never go under one physical pixel.",
  window: { width: DS.W, height: DS.H, aspect: DS.W / DS.H, scale: { min: 0.75, max: 2, default: 1 }, background: "ground", corners: 0 },
  colors: COLOR,
  themes: { default: "light", note: "Every role has a light and a dark colour. Descriptions in this spec name the light theme's colours; swap by role for dark." },
  roles: Object.fromEntries(Object.entries(ROLE).map(([k, [c, d]]) => { const dc = DS.THEMES.dark.roles[k]; return [k, { color: c, hex: COLOR[c], dark: { color: dc, hex: COLOR[dc] }, ...(k === "body" ? { alpha: DS.THEMES.light.bodyAlpha, darkAlpha: DS.THEMES.dark.bodyAlpha } : {}), use: d }]; })),
  settings: DS.SETTINGS,
  strokes: Object.fromEntries(Object.entries(STROKE).map(([k, [v, d]]) => [k, { px: v, use: d }])),
  type: TYPE, fonts: [
    { file: "fonts/Anybody-Medium-500.ttf", family: "Anybody", weight: 500, licence: "OFL-1.1" },
    { file: "fonts/Anybody-SemiBold-600.ttf", family: "Anybody", weight: 600, licence: "OFL-1.1" },
    { file: "fonts/Anybody-Bold-700.ttf", family: "Anybody", weight: 700, licence: "OFL-1.1" },
    { file: "fonts/DepartureMono-Regular.otf", family: "Departure Mono", weight: 400, licence: "OFL-1.1" },
  ],
  drawOrder: ["ground", "analyser frame and grid", "frequency labels", "comb body, removed, original line, result line (clipped to the plot)", "curve (underlay, then line)", "scale labels", "state line", "points, brackets, readout", "header, legend, Amount, knobs, Auto gain, Shape, credit", "tooltip"],
  components, parameters: params, curve: CURVE, interaction, processor, accessibility, contrast,
  motion: "Nothing eases. Values, hovers and switches change on the next frame; the analyser runs at 30 to 60 fps. The only timers are the tooltip delay (600 ms) and the six-points note (2 s).",
};

mkdirSync(join(ROOT, "spec"), { recursive: true });
mkdirSync(join(ROOT, "tokens"), { recursive: true });
writeFileSync(join(ROOT, "spec/spec.json"), JSON.stringify(spec, null, 1) + "\n");
writeFileSync(join(ROOT, "tokens/de-saturator.tokens.json"), JSON.stringify(tokens, null, 1) + "\n");
for (const th of ["light", "dark"]) writeFileSync(join(ROOT, `tokens/de-saturator.${th}.tokens.json`), JSON.stringify(colourTokens(th), null, 1) + "\n");
writeFileSync(join(ROOT, "spec/contrast.txt"), contrast.map((c) => `${verdict(c)}  ${c.ratio.toFixed(2)}:1  ${c.needs ? `(needs ${c.needs})  ` : ""}${c.pair}: ${c.use}`).join("\n") + "\n");

/* ---------- SPEC.md ---------- */
const row = (...c) => `| ${c.join(" | ")} |`;
const md = [];
md.push(`# De-Saturator: window spec ${DS.VERSION}`, "");
md.push(`Direction B, Hairline. ${spec.idea}`, "");
md.push("Everything here comes from `build/ui/core.js`, the same file that draws the prototype and the state pictures, so the two agree; if they ever don't, the picture is right. Machine-readable: `spec/spec.json`. Pictures: `states/`. Try it: `prototype/prototype.html`.", "");
md.push("## Window", "");
md.push(`- ${DS.W} × ${DS.H} at 100%, aspect ${DS.W}:${DS.H} (1.6), resizable from 75% (720 × 450) to 200% (1920 × 1200) at that ratio. Draw at 100% coordinates under one scale transform.`);
md.push("- Square corners everywhere except pills (switches, Delta, Auto gain, the Shape thumb) and circles. Nothing in between.");
md.push("- Flat: no gradients, no shadows, no glows. One colour is drawn at an opacity: the analyser's body comb, ink at 16%.");
md.push("- Hairlines (1 px) never go under one physical pixel at 75%.");
md.push("- Two themes, light (paper, the default) and dark (ink), switched by Light / Dark at the bottom right. Every colour below is a role with a light and a dark value; the descriptions name the light ones.");
md.push(`- No keyboard. ${interaction.keyboard}`, "");
md.push("Draw order: " + spec.drawOrder.join(" → ") + ".", "");
md.push("![Redlines](../states/redlines.png)", "");
md.push("## Colour", "", row("Role", "Light", "Dark", "Use"), row("---", "---", "---", "---"));
for (const [k, v] of Object.entries(spec.roles)) md.push(row(k, `${v.color}${v.alpha != null ? ` at ${Math.round(v.alpha * 100)}%` : ""} \`${v.hex}\``, `${v.dark.color}${v.darkAlpha != null ? ` at ${Math.round(v.darkAlpha * 100)}%` : ""} \`${v.dark.hex}\``, v.use));
md.push("", "Contrast (WCAG):", "", "```", ...contrast.map((c) => `${verdict(c)}  ${c.ratio.toFixed(2)}:1  ${c.pair}`), "```", "");
md.push("## Type", "", row("Role", "Font file", "Size", "Use"), row("---", "---", "---", "---"));
for (const [k, t] of Object.entries(TYPE)) md.push(row(k, "`fonts/" + t.file + "`", t.size + " px" + (t.lineHeight ? ` / ${t.lineHeight}` : ""), t.use));
md.push("", "Sentence case everywhere. Departure Mono is a pixel face: it is drawn for 11 and 22 px at 100%. Both families are under the SIL Open Font License 1.1: embed the files and ship `fonts/*-OFL.txt` with the plugin. The name is never set from the font; it is `ui/assets/wordmark-header.svg`.", "");
md.push("## Strokes", "", row("Stroke", "px", "Use"), row("---", "---", "---"));
for (const [k, [v, d]] of Object.entries(STROKE)) md.push(row(k, v, d));
md.push("", "## Header", "");
md.push(`- **Name**: ${components.wordmark.note}`);
md.push(`- **Key** (baseline 64): a short heavy line \"Result\" at x 24, a hairline \"Original\" at x 100, four hairlines \"Removed\" at x 190, labels 24 px right of each swatch, in the label type, graphite deep. In Delta the Removed swatch turns klein.`);
md.push(`- **Clean / Character**: x ${L.character.x}, y ${L.character.y}, ${seg(L.character)} × ${L.character.h} (segments ${L.character.segs.map((s) => s[2]).join(" + ")}). **Live / HQ**: x ${L.quality.x}, ${seg(L.quality)} × ${L.quality.h} (${L.quality.segs.map((s) => s[2]).join(" + ")}). ${components.switch.track} ${components.switch.selected} ${components.switch.hover}`);
md.push(`- **Latency**: ${components.latency.text} Value type, graphite deep, x ${L.latency.x}, baseline ${L.latency.baseline}.`);
md.push(`- **Delta**: x ${L.delta.x}, y ${L.delta.y}, ${L.delta.w} × ${L.delta.h}. ${components.delta.off} ${components.delta.on}`);
md.push(`- **Bypass**: a circle radius ${L.bypass.r} at (${L.bypass.cx}, ${L.bypass.cy}). ${components.bypass.icon} ${components.bypass.on}`, "");
md.push("## Analyser", "");
md.push(`Frame x ${L.analyser.x}, y ${L.analyser.y}, ${L.analyser.w} × ${L.analyser.h}: paper, ink outline ${S.frame}. The plot is its top ${PLOT.h} px; the ${L.strip.h} px strip under it carries the frequency labels (value type, graphite deep, centred, baseline ${L.strip.baseline}).`, "");
md.push(`- Frequency: ${DS.FMIN} Hz to ${DS.FMAX / 1000} kHz, log. ${components.analyser.frequency.x}.`);
md.push(`- Level: ${DS.DBMIN} to +${DS.DBMAX} dB with a ${DS.TILT} dB/oct tilt pivoting at 1 kHz. ${components.analyser.level.y}.`);
md.push(`- Grid: verticals at 50, 100, 200, 500, 1k, 2k, 5k, 10k (100, 1k, 10k full, the rest at 55%); dashed (2/4) level lines at −72, −48, −24 and 0 dB; a line along the plot's bottom. Paper line, 1 px.`, "");
md.push("**The comb.** 120 vertical lines, " + components.analyser.comb.x + ". Each line is sampled at its own frequency and drawn in two parts, with two lines across all the bins over them:", "");
md.push(row("Layer", "From → to", "Normal", "Delta", "Bypassed"), row("---", "---", "---", "---", "---"));
md.push(row("Body", "plot bottom + 2 → result", "ink at 16%, 2.6", "same", "same"));
md.push(row("Removed", "result → original", "ink 1", "klein 2.6", "hidden"));
md.push(row("Original line", "every bin", "ink 1", "ink 1", "hidden (equals the result)"));
md.push(row("Result line", "every bin", "ink 2", "graphite deep 1", "ink 2"));
md.push("", "So the result reads as one heavy line over a quiet grey comb, the original as a fine line above it, and what's removed as the hatched band between them. In Delta the hatching is what you hear, so it turns heavy and klein.", "");
md.push(`**Scale labels**, right-aligned at x ${PLOT.x + PLOT.w - 10}: \"200%\" (baseline ${PLOT.y + 16}), \"100%\" (${r2(DS.cyv(1) - 7)}), \"0%\" (${PLOT.y + PLOT.h - 5}), each on paper at 85%.`, "");
md.push(`**State line**, top left of the plot (text x ${PLOT.x + 15}, baseline ${PLOT.y + 21.5}, on paper at 90%): one of ${Object.values(NOTES).map((n) => `\"${n}\"`).join(", ")}. Priority: Bypassed, then Delta, then a passing note, then Live.`, "");
md.push("## Thinning curve", "");
md.push(`- ${components.curve.model}`, `- ${components.curve.y}.`, `- Up to ${CURVE.maxPoints} points. Frequency ${CURVE.f[0]} Hz to ${CURVE.f[1] / 1000} kHz, amount 0 to 200%, width ${CURVE.width[0]} to ${CURVE.width[1]} octaves (new points ${CURVE.widthDefault}).`);
md.push(`- **HQ**: one line, klein ${S.curve}, over a paper underlay ${S.curveUnder}.`);
md.push(`- **Live**: the curve you drew dashed (klein ${S.curveDrawn}, 4 on 4 off) and the applied curve from the processor solid (klein ${S.curveApplied}, paper underlay ${S.curveUnder}).`);
md.push(`- **Bypassed**: curve and points at 45%.`);
md.push(`- **Points**: ${components.curve.points.position} ${components.curve.points.normal} Hover: ${components.curve.points.hover} Selected: ${components.curve.points.selected} Dragging: ${components.curve.points.drag} Bracket: ${components.curve.points.bracket}`);
md.push(`- **Readout**: an ink chip 19 high, paper text, ${components.curve.readout.text} 39 px above the point (22 below when it's within 46 px of the top), kept 6 px inside the plot.`, "");
md.push("## Amount", "");
md.push(`Centre (${L.amount.cx}, ${L.amount.cy}). ${components.amount.angle}.`, "");
md.push(`- **The ring** thins as Amount rises: outer radius ${L.amount.r}, width ${components.amount.ring.width} (a near-solid disc at 0%, a hairline at 100%), centred on radius ${components.amount.ring.centreRadius}. ${components.amount.ring.notch} Ink.`);
md.push(`- ${components.amount.track} ${components.amount.valueArc}`);
md.push(`- ${components.amount.pointer}`);
md.push(`- ${components.amount.ticks}`);
md.push(`- \"Amount\" in the title type, centred, baseline ${L.amount.title}. The value (22 px mono) centred at baseline ${L.amount.readout}; under the pointer or while dragging it sits on an ink chip ${L.amount.chip.w} × ${L.amount.chip.h} at y ${L.amount.chip.y}, paper text.`);
md.push(`- Bypassed: ${components.amount.bypass}`, "");
md.push("## Restore, Mix, Output", "");
md.push(`Centres at x ${L.small.knobs.map((k) => k[2]).join(", ")}, y ${L.small.cy}. ${components.knob.body} ${components.knob.track} ${components.knob.valueArc} ${components.knob.originTick} ${components.knob.pointer} Label baseline ${L.small.label}; value baseline ${L.small.value}, graphite deep. ${components.knob.value.hover}`, "");
md.push(`**Auto gain** under Output: a pill at x ${L.autoGain.x}, y ${L.autoGain.y}, ${L.autoGain.w} × ${L.autoGain.h}. ${components.autoGain.on} ${components.autoGain.off}`, "");
md.push("## Shape", "", `${components.shape.track} ${components.shape.thumb} ${components.shape.hover} ${components.shape.ends} ${components.shape.title}`, "");
md.push("## Footer", "", `**Credit**: \`ui/assets/credit.svg\` (dark: \`credit-dark.svg\`) drawn ${L.credit.w} × ${L.credit.h} at (${L.credit.x}, ${L.credit.y}); the plugin's own version string (\"1.0.0\" in the pictures) in the value type, graphite deep, at x ${L.credit.version.x}, baseline ${L.credit.version.baseline}.`, "");
md.push(`**Theme**: Light / Dark at x ${L.theme.x}, y ${L.theme.y}, ${seg(L.theme)} × ${L.theme.h} (${L.theme.segs.map((s) => s[2]).join(" + ")}). ${components.theme.note}`, "");
md.push("## Interaction", "");
md.push(`- **Drag**: knobs vertically, ${DRAG.fullRangePx} screen px for the whole range, ${DRAG.fineRangePx} with Shift. Shape horizontally, the thumb follows the pointer.`);
for (const [k, v] of Object.entries(interaction.doubleClick)) md.push(`- **Double-click ${k.replace(/([A-Z])/g, " $1").toLowerCase()}**: ${v}.`);
md.push(`- **Wheel**: on a knob ${interaction.wheel.knob}; on a point ${interaction.wheel.point}. ${interaction.wheel.note}`);
md.push(`- **Points**: ${interaction.pointer.point} Hit radius ${L.point.hit} px.`);
md.push(`- **Keyboard**: ${interaction.keyboard}`);
md.push(`- **Tooltips** after ${L.tooltip.delayMs} ms: ${components.tooltip.placement} ${components.tooltip.hides} Ink box, paper text, Anybody Medium 12/16, padding ${L.tooltip.padX} × ${L.tooltip.padY}, at most ${L.tooltip.maxWidth} wide.`, "");
md.push(row("Over", "Tooltip"), row("---", "---"));
for (const [k, v] of Object.entries(TIPS)) md.push(row(k, v));
md.push("", `- **Motion**: ${spec.motion}`, "");
md.push("## Parameters", "", row("ID", "Name", "Range", "Default", "Shown as"), row("---", "---", "---", "---", "---"));
for (const p of params) md.push(row("`" + p.id + "`", p.name, p.kind === "choice" ? p.options.join(" / ") : p.kind === "toggle" ? "on / off" : `${p.min} to ${p.max} ${p.unit}`, String(p.def), p.format ?? (p.kind === "choice" ? p.labels.join(" / ") : "pill or button state")));
md.push("", "## What the window needs from the processor", "", ...processor.map((p) => `- ${p}`), "");
md.push("## Accessibility", "", row("Control", "Role", "Value", "Help"), row("---", "---", "---", "---"));
for (const a of accessibility) md.push(row(a.control, a.role, a.value, a.help ?? ""));
md.push("", "## States", "", "Every state is in `states/` at 1x and 2x (`@2x`).", "");
const statesFile = join(ROOT, "states/states.json");
if (existsSync(statesFile)) {
  md.push(row("State", "What changes", "Picture"), row("---", "---", "---"));
  for (const s of JSON.parse(readFileSync(statesFile, "utf8"))) md.push(row(s.name, s.description, `[1x](../states/${s.file}), [2x](../states/${s.file2x})`));
  md.push("");
} else md.push("Run `node build/states.mjs` to render them.", "");
md.push("## Files", "", "- `fonts/`: the four font files and their licences.", "- `ui/assets/`: the name for the header and the Aria Ashrafi credit, as SVG, for each theme.", "- `logo/`: the name, the mark, lockups and icon tiles, SVG and PNG.", "- `tokens/`: the same values as DTCG tokens: `de-saturator.tokens.json` (sizes, strokes, type, timings), `de-saturator.light.tokens.json` and `de-saturator.dark.tokens.json` (colours).", "- `states/`: every state as PNG, and `redlines.png`.", "- `installers/`: release art and icons (see `installers/README.md`).", "- `store/`: store and social pictures.", "- `prototype/prototype.html`: the window, working.", "");
writeFileSync(join(ROOT, "spec/SPEC.md"), md.join("\n"));

mkdirSync(join(HERE, "manifest"), { recursive: true });
writeFileSync(join(HERE, "manifest", "spec.json"), JSON.stringify([
  { collection: "ui-handoff", file: "spec/spec.json", slug: "spec-json", name: "Window spec (JSON)", description: "Everything the window is made of, for code: layout, colours, strokes, type, parameters, states, strings.", tags: ["spec"] },
  { collection: "ui-handoff", file: "spec/SPEC.md", slug: "spec-md", name: "Window spec", description: "The same spec, written out.", tags: ["spec"] },
  { collection: "ui-handoff", file: "tokens/de-saturator.tokens.json", slug: "tokens", name: "Tokens (DTCG): sizes, strokes, type", description: "Strokes, sizes, type and timings.", tags: ["tokens"] },
  { collection: "ui-handoff", file: "tokens/de-saturator.light.tokens.json", slug: "tokens-light", name: "Tokens (DTCG): light colours", description: "Every colour role, light theme.", tags: ["tokens", "light"] },
  { collection: "ui-handoff", file: "tokens/de-saturator.dark.tokens.json", slug: "tokens-dark", name: "Tokens (DTCG): dark colours", description: "Every colour role, dark theme.", tags: ["tokens", "dark"] },
  { collection: "ui-handoff", file: "spec/contrast.txt", slug: "contrast", name: "Contrast check", description: "WCAG ratios for every pairing the window uses.", tags: ["accessibility"] },
], null, 1));
console.log("spec/spec.json, spec/SPEC.md, spec/contrast.txt, tokens/de-saturator.tokens.json");
console.log(contrast.map((c) => `  ${verdict(c)} ${c.ratio}:1 ${c.pair}`).join("\n"));
