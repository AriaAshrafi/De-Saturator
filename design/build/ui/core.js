/* De-Saturator, the window's single source.
 *
 * Direction B, Hairline: thinner, drawn literally. The analyser is a comb of lines: what stays is
 * drawn heavy, what's removed carries on as a hairline up to where the original reached. The Amount
 * knob is a ring that thins as you turn it, and the name loses weight letter by letter.
 *
 * Everything the window is made of lives here: layout (window px at 100%), colours, strokes, type,
 * parameters, strings, the drawing (SVG strings) and the hit areas. The prototype (browser), the
 * state renders and spec.mjs (Node) all read this file, so the spec and the pictures agree.
 * The signal and the thinning below are stand-ins for the look only; the real ones are the processor's.
 */
(function (root) {
"use strict";

const VERSION = "1.2.1";                    // the design's version; the window shows the plugin's own
const W = 960, H = 600;
const FMIN = 20, FMAX = 20000, OCT = Math.log2(FMAX / FMIN);
const DBMIN = -90, DBMAX = 6, TILT = 4.5;
const NB = 192;
const BINF = new Float64Array(NB);
for (let i = 0; i < NB; i++) BINF[i] = FMIN * Math.pow(2, OCT * i / (NB - 1));
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
const lerp = (a, b, t) => a + (b - a) * t;
const n2 = v => Math.round(v * 100) / 100;

/* ---------- colour: all the parent's, nothing mixed on the spot ---------- */
const COLOR = {
  paper: "#ECEBE6", paperRaised: "#E2E0D8", paperLine: "#D6D4CB",
  ink: "#0C0C0B", inkLine: "#2A2A27", graphite: "#8A8983", graphiteDeep: "#5F5E59", klein: "#2E40FF", kleinLight: "#7C88FF",
};
const ROLE = {
  ground: ["paper", "The window."],
  text: ["ink", "Labels, the name, button outlines, the analyser's frame."],
  textMuted: ["graphiteDeep", "Values, the latency, frequency labels, the credit."],
  grid: ["paperLine", "The analyser's grid, knob tracks, the Shape track."],
  body: ["ink", "Analyser: the comb under the result line, at 16% opacity. What stays, kept quiet."],
  result: ["ink", "Analyser: the result line, heavy. What you hear."],
  original: ["ink", "Analyser: the original's line, a hairline."],
  removed: ["ink", "Analyser: hairlines between the two lines. What's removed."],
  hand: ["klein", "What you set: the thinning curve, its points, value arcs, Delta when on."],
  selected: ["ink", "The chosen segment of a switch; Auto gain and Bypass when on."],
  onSelected: ["paper", "Text and icons on ink."],
  onHand: ["paper", "Text on klein."],
  readout: ["ink", "Readout chips and tooltips."],
  onReadout: ["paper", "Text in readout chips and tooltips."],
};
const T = Object.fromEntries(Object.entries(ROLE).map(([k, [c]]) => [k, COLOR[c]]));
const ALPHA = { body: 0.16 };                // the only colour drawn at an opacity
// Two themes. Light is paper, the default. Dark is the same window in ink: the parent's rule is
// klein-light on ink, and text on it is ink.
const THEMES = {
  light: { roles: Object.fromEntries(Object.entries(ROLE).map(([k, [c]]) => [k, c])), bodyAlpha: 0.16 },
  dark: {
    roles: { ground: "ink", text: "paper", textMuted: "graphite", grid: "inkLine", body: "paper", result: "paper", original: "paper", removed: "paper",
      hand: "kleinLight", selected: "paper", onSelected: "ink", onHand: "ink", readout: "paper", onReadout: "ink" },
    bodyAlpha: 0.2,
  },
};
function useTheme(name) {
  const th = THEMES[name] || THEMES.light;
  for (const [k, c] of Object.entries(th.roles)) T[k] = COLOR[c];
  ALPHA.body = th.bodyAlpha;
  return th;
}

/* ---------- strokes, px at 100% ---------- */
const STROKE = {
  frame: [1.5, "The analyser's frame."],
  grid: [1, "Grid lines."],
  body: [2.6, "Analyser comb, the part under the result line."],
  result: [2, "The result line."],
  original: [1, "The original's line. Never under 1 physical pixel."],
  hair: [1, "Analyser comb, the part between the two lines (what's removed). Never under 1 physical pixel."],
  removedDelta: [2.6, "Delta: the removed part of the comb, heavy."],
  outline: [1.5, "Switches, pills, Bypass, small knob bodies."],
  outlineHover: [2.5, "The same outline under the pointer."],
  curve: [2, "The thinning curve."],
  curveApplied: [2.25, "Live: the applied (smoothed) curve."],
  curveDrawn: [1.25, "Live: the curve you drew, dashed 4 on, 4 off."],
  curveUnder: [5, "A paper line under the curve so it reads over the analyser."],
  knobTrack: [3.5, "Small knob track and value arc."],
  amountArc: [2, "Amount track and value arc."],
  tick: [1.25, "Amount scale ticks."],
  pointRing: [2, "Curve point ring."],
  bracket: [1.25, "A point's width bracket."],
  ringMax: [44, "Amount ring at 0%: nearly a solid disc."],
  ringMin: [1.5, "Amount ring at 100%: a hairline."],
};
const S = Object.fromEntries(Object.entries(STROKE).map(([k, [v]]) => [k, v]));

/* ---------- type ---------- */
const TYPE = {
  title: { family: "Anybody", weight: 700, width: 100, size: 15, file: "Anybody-Bold-700.ttf", use: "The Amount title." },
  button: { family: "Anybody", weight: 600, width: 100, size: 13, file: "Anybody-SemiBold-600.ttf", use: "Switches and the Delta pill." },
  label: { family: "Anybody", weight: 600, width: 100, size: 12, file: "Anybody-SemiBold-600.ttf", use: "Control labels, Auto gain." },
  tooltip: { family: "Anybody", weight: 500, width: 100, size: 12, lineHeight: 16, file: "Anybody-Medium-500.ttf", use: "Tooltips." },
  value: { family: "Departure Mono", weight: 400, size: 11, file: "DepartureMono-Regular.otf", use: "Values, frequency and scale labels, chips, latency, version." },
  valueLarge: { family: "Departure Mono", weight: 400, size: 22, file: "DepartureMono-Regular.otf", use: "The Amount value." },
};
const MONO_ADV = 0.6;                        // Departure Mono advance, em
const FONTSTACK = { "Anybody": "Anybody, system-ui, sans-serif", "Departure Mono": "Departure Mono, ui-monospace, monospace" };

/* ---------- layout, window px at 100% ---------- */
const L = {
  header: { h: 56 },
  wordmark: { x: 24, baseline: 38, size: 27 },
  character: { x: 472, y: 12, h: 32, segs: [["clean", "Clean", 64], ["character", "Character", 96]] },
  quality: { x: 648, y: 12, h: 32, segs: [["live", "Live", 52], ["hq", "HQ", 52]] },
  latency: { x: 760, baseline: 32, maxChars: 6 },
  delta: { x: 820, y: 12, w: 72, h: 32 },
  bypass: { cx: 920, cy: 28, r: 16 },
  analyser: { x: 24, y: 72, w: 632, h: 368 },
  plot: { x: 24, y: 72, w: 632, h: 346 },
  strip: { h: 22, baseline: 433 },
  amount: { cx: 808, cy: 222, r: 84, track: 96, tickIn: 104, tickOut: 109, tickMajor: 113, notch: 6, title: 100, readout: 358, chip: { w: 80, h: 32, y: 334 }, hit: 106 },
  small: { cy: 428, r: 22, track: 29, pointerIn: 7, pointerOut: 18, originIn: 33, originOut: 38, label: 474, value: 493.5, hit: 32, knobs: [["restore", "Restore", 724], ["mix", "Mix", 808], ["output", "Output", 892]] },
  autoGain: { x: 852, y: 504, w: 80, h: 24 },
  shape: { x1: 120, x2: 560, y: 514, title: 492, thumb: { w: 16, h: 30 }, endGap: 14 },
  credit: { x: 24, y: 566, w: 54, h: 15, version: { x: 86, baseline: 578 } },
  theme: { x: 848, y: 562, h: 24, segs: [["light", "Light", 44], ["dark", "Dark", 44]] },
  comb: { lines: 120 },
  point: { r: 6.5, rHover: 8.5, rSelected: 7, halo: 12, hit: 13 },
  tooltip: { maxWidth: 220, padX: 10, padY: 8, gap: 8, delayMs: 600 },
};
const P = L.plot;
const fx = f => P.x + P.w * Math.log2(f / FMIN) / OCT;
const xf = x => FMIN * Math.pow(2, OCT * (x - P.x) / P.w);
const dy = db => P.y + P.h * (DBMAX - db) / (DBMAX - DBMIN);
const cyv = v => P.y + P.h * (1 - v / 2);
const yv = y => 2 * (1 - (y - P.y) / P.h);
const XS = Array.from(BINF, fx);

/* ---------- parameters (the processor's IDs) ---------- */
const PARAMS = [
  { id: "amount", name: "Amount", kind: "knob", min: 0, max: 100, def: 30, unit: "%", step: 1 },
  { id: "shape", name: "Shape", kind: "slider", min: 0, max: 100, def: 50, unit: "%", step: 1, ends: ["Harmonics", "Body"] },
  { id: "restore", name: "Restore", kind: "knob", min: 0, max: 100, def: 0, unit: "%", step: 1 },
  { id: "mix", name: "Mix", kind: "knob", min: 0, max: 100, def: 100, unit: "%", step: 1 },
  { id: "output", name: "Output", kind: "knob", min: -24, max: 36, def: 0, unit: "dB", step: 0.5, origin: 0 },
  { id: "autoGain", name: "Auto gain", kind: "toggle", def: true },
  { id: "delta", name: "Delta", kind: "toggle", def: false },
  { id: "character", name: "Character", kind: "choice", options: ["clean", "character"], labels: ["Clean", "Character"], def: "clean" },
  { id: "quality", name: "Quality", kind: "choice", options: ["live", "hq"], labels: ["Live", "HQ"], def: "hq" },
  { id: "bypass", name: "Bypass", kind: "toggle", def: false },
];
const PM = Object.fromEntries(PARAMS.map(p => [p.id, p]));
// a window preference, not a processor parameter: kept with the plugin's state, not automatable
const SETTINGS = [{ id: "theme", name: "Theme", kind: "choice", options: ["light", "dark"], labels: ["Light", "Dark"], def: "light" }];
const CURVE = { maxPoints: 6, f: [FMIN, FMAX], amount: [0, 2], width: [0.3, 4], widthDefault: 1, widthStep: 0.1, baseline: 1 };
const DRAG = { fullRangePx: 240, fineRangePx: 1200 };
// latencyMs and versionText come from the processor and the build; these are sample values for the pictures
const defaults = () => Object.assign(Object.fromEntries(PARAMS.concat(SETTINGS).map(p => [p.id, p.def])), { points: [], latencyMs: 85, versionText: "1.0.0" });

const pct = v => Math.round(v) + "%";
function fmt(id, v) {
  if (id === "output") { const r = Math.round(v * 10) / 10; return (r > 0 ? "+" : r < 0 ? "−" : "") + Math.abs(r).toFixed(1) + " dB"; }
  return pct(v);
}
const fmtF = f => f < 1000 ? Math.round(f) + " Hz" : (f < 9950 ? (f / 1000).toFixed(1) : String(Math.round(f / 1000))) + " kHz";

/* ---------- words ---------- */
const TIPS = {
  amount: "How much thinner. The curve scales it at each frequency.",
  shape: "Harmonics takes out overtones. Body takes out weight.",
  restore: "Undoes soft clipping. Clean only undoes what it finds. Character goes further, lifting attacks.",
  mix: "How much of the thinned sound you hear against the original.",
  output: "The level after everything else.",
  autoGain: "Keeps the level steady, so thinner doesn't just mean quieter.",
  delta: "Hear only what's being removed.",
  "character:clean": "Clean: transparent, for fixing.",
  "character:character": "Character: goes further, for effect.",
  "quality:live": "Live: no latency. The curve is applied smoothed.",
  "quality:hq": "HQ: full resolution. Adds latency.",
  bypass: "Hear the track untouched.",
  "theme:light": "A paper window.",
  "theme:dark": "An ink window, for a dark room.",
  display: "Double-click to add a point, up to six.",
  point: "Drag to move. Scroll to change its width. Double-click to remove.",
};
const NOTES = {
  bypass: "Bypassed",
  delta: "Delta: you hear only what's removed",
  live: "Live: the applied curve is smoothed",
  full: "Six points is the most",
};

/* ---------- stand-in signal: a bass line, a pad, kick, snare and hats at 120 BPM ---------- */
function rng(seed) {
  let a = seed >>> 0;
  return () => { a = (a + 0x6D2B79F5) >>> 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
}
function Signal(seed) {
  this.t = 0; this.rand = rng(seed || 7);
  this.lv = new Float64Array(NB).fill(DBMIN); this.tg = new Float64Array(NB); this.nz = new Float64Array(NB);
}
Signal.prototype.target = function () {
  const t = this.t, bar = Math.floor(t / 2) % 4, f0 = 55 * Math.pow(2, [0, 5, 3, 7][bar] / 12);
  const k = Math.exp(-(t % 0.5) / 0.09), hh = Math.exp(-(t % 0.25) / 0.04), sn = Math.exp(-((t + 0.5) % 1) / 0.12);
  const swell = 0.5 + 0.5 * Math.sin(t * 1.3), r0 = f0 * 4 * 1.2599;
  for (let i = 0; i < NB; i++) {
    const f = BINF[i], o = Math.log2(f / 1000);
    let p = Math.pow(10, (-48 - 3 * o) / 10);
    for (let h = 1; h <= 64; h++) {
      const fh = f0 * h; if (fh > 20000) break;
      const d = Math.log2(f / fh) / 0.04; if (d > 5 || d < -5) continue;
      p += Math.pow(10, (-8 - 8 * Math.log2(h) - (h % 2 ? 0 : 3)) / 10) * Math.exp(-0.5 * d * d);
    }
    for (let h = 1; h <= 24; h++) {
      const fh = r0 * h; if (fh > 20000) break;
      const d = Math.log2(f / fh) / 0.03; if (d > 5 || d < -5) continue;
      p += Math.pow(10, (-18 - 6 * Math.log2(h) - 6 * (1 - swell)) / 10) * Math.exp(-0.5 * d * d);
    }
    const dk = Math.log2(f / 58) / 0.55; p += Math.pow(10, -0.4) * k * Math.exp(-0.5 * dk * dk);
    const ds = Math.log2(f / 2500) / 1.6; p += Math.pow(10, -2.2) * sn * Math.exp(-0.5 * ds * ds);
    if (f > 3000) { const dh = Math.log2(f / 11000) / 0.8; p += Math.pow(10, -2.4) * hh * Math.exp(-0.5 * dh * dh); }
    this.nz[i] = this.nz[i] * 0.8 + (this.rand() - 0.5) * 1.2;
    this.tg[i] = 10 * Math.log10(p) + TILT * o + this.nz[i];
  }
};
Signal.prototype.step = function (dt) {
  this.t += dt; this.target();
  for (let i = 0; i < NB; i++) {
    const v = clamp(0.25 * this.tg[Math.max(0, i - 1)] + 0.5 * this.tg[i] + 0.25 * this.tg[Math.min(NB - 1, i + 1)], DBMIN, DBMAX);
    this.lv[i] = v > this.lv[i] ? lerp(this.lv[i], v, 0.7) : Math.max(v, this.lv[i] - 28 * dt);
  }
};
function snapshot() { const s = new Signal(7); for (let i = 0; i < 200; i++) s.step(1 / 60); return Float64Array.from(s.lv); }
const SILENCE = new Float64Array(NB).fill(DBMIN);

/* ---------- the thinning curve ---------- */
function curveAt(points, f) {
  let v = CURVE.baseline;
  for (const p of points) { const d = Math.log2(f / p.f) / (p.w / 2.3548); v += (p.a - CURVE.baseline) * Math.exp(-0.5 * d * d); }
  return clamp(v, 0, 2);
}
function curves(st) {
  const drawn = new Float64Array(NB);
  for (let i = 0; i < NB; i++) drawn[i] = curveAt(st.points, BINF[i]);
  if (st.quality === "hq") return { drawn, applied: drawn };
  // Live: stand-in for the processor's smoothed curve (a half-octave blur)
  const sg = 0.5 * (NB - 1) / OCT, k = Math.ceil(sg * 3), applied = new Float64Array(NB);
  for (let i = 0; i < NB; i++) {
    let s = 0, ws = 0;
    for (let j = -k; j <= k; j++) { const wt = Math.exp(-0.5 * (j / sg) ** 2); s += wt * drawn[clamp(i + j, 0, NB - 1)]; ws += wt; }
    applied[i] = s / ws;
  }
  return { drawn, applied };
}
const HW = Float64Array.from(BINF, f => 1 / (1 + Math.exp(-Math.log2(f / 1400) / 0.5)));
const BW = Float64Array.from(BINF, f => { const b = Math.log2(f / 170) / 1.1; return Math.exp(-0.5 * b * b); });
// stand-in thinning, in dB removed per bin, after Mix and before Output / Auto gain
function reduce(st, applied, orig, out) {
  out = out || new Float64Array(NB);
  if (st.bypass) { out.fill(0); return out; }
  const s = st.shape / 100, amt = st.amount / 100, mx = st.character === "character" ? 36 : 18, mix = st.mix / 100, rs = st.restore / 100;
  for (let i = 0; i < NB; i++) {
    let sm = 0, n = 0;
    for (let j = i - 4; j <= i + 4; j++) if (j >= 0 && j < NB) { sm += orig[j]; n++; }
    const pk = clamp((orig[i] - sm / n) / 6 + 0.5, 0, 1);
    const wgt = (1 - s) * HW[i] * (0.45 + 0.55 * pk) + s * BW[i] * (0.8 + 0.2 * pk);
    out[i] = Math.min(amt * applied[i] * wgt * mx * (1 - 0.55 * rs * pk) * mix, orig[i] - DBMIN);
  }
  return out;
}

/* ---------- the comb ---------- */
const COMB_X = [], COMB_I = [];
for (let j = 0; j < L.comb.lines; j++) {
  const x = P.x + (j + 0.5) * P.w / L.comb.lines;
  COMB_X.push(x); COMB_I.push(Math.log2(xf(x) / FMIN) / OCT * (NB - 1));
}
const COMB_BASE = P.y + P.h + 2;
function comb(orig, red) {
  const keep = [], top = [];
  for (let j = 0; j < COMB_X.length; j++) {
    const fi = COMB_I[j], i0 = Math.floor(fi), i1 = Math.min(NB - 1, i0 + 1), t = fi - i0;
    const o = lerp(orig[i0], orig[i1], t), r = lerp(red[i0], red[i1], t);
    keep.push(dy(o - r)); top.push(dy(o));
  }
  return { keep, top };
}
// [colour, stroke] for each layer; null hides the layer
function combStyle(st) {
  if (st.bypass) return { keep: [T.body, S.body], removed: null, result: [T.result, S.result], original: null };
  if (st.delta) return { keep: [T.body, S.body], removed: [T.hand, S.removedDelta], result: [T.textMuted, S.original], original: [T.original, S.original] };
  return { keep: [T.body, S.body], removed: [T.removed, S.hair], result: [T.result, S.result], original: [T.original, S.original] };
}
function contour(orig, red) {
  let s = "";
  for (let i = 0; i < NB; i++) s += (i ? "L" : "M") + XS[i].toFixed(1) + " " + dy(red ? orig[i] - red[i] : orig[i]).toFixed(1);
  return s;
}
// the key above the analyser: what the three marks mean
function legend(st) {
  const sty = combStyle(st), y = 64, mid = y - 4;
  const item = (x, sw, label) => sw + text(x + 24, y, label, "label", T.textMuted);
  const ln = (x, c) => c ? `<path d="M${x} ${mid}H${x + 18}" stroke="${c[0]}" stroke-width="${c[1]}"/>` : "";
  const r = sty.removed || [T.removed, S.hair];
  let hatch = "";
  for (let k = 0; k < 4; k++) hatch += `<path d="M${190 + 1 + k * 5} ${mid - 5}V${mid + 5}" stroke="${r[0]}" stroke-width="${Math.min(r[1], 2)}"/>`;
  return item(24, ln(24, sty.result), "Result") + item(100, ln(100, sty.original || [T.textMuted, S.original]), "Original") + item(190, hatch, "Removed");
}

/* ---------- drawing helpers ---------- */
const esc = s => String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
function text(x, y, s, role, fill, anchor, extra) {
  const t = TYPE[role];
  return `<text x="${n2(x)}" y="${n2(y)}" font-family="${FONTSTACK[t.family]}" font-size="${t.size}" font-weight="${t.weight}"${anchor && anchor !== "start" ? ` text-anchor="${anchor}"` : ""} fill="${fill}"${extra || ""}>${esc(s)}</text>`;
}
function polar(cx, cy, r, deg) { const a = (deg - 90) * Math.PI / 180; return [cx + r * Math.cos(a), cy + r * Math.sin(a)]; }
function arc(cx, cy, r, a0, a1) {
  if (Math.abs(a1 - a0) < 0.05) return "";
  if (a1 < a0) [a0, a1] = [a1, a0];
  const [x0, y0] = polar(cx, cy, r, a0), [x1, y1] = polar(cx, cy, r, a1);
  return `M${n2(x0)} ${n2(y0)}A${n2(r)} ${n2(r)} 0 ${a1 - a0 > 180 ? 1 : 0} 1 ${n2(x1)} ${n2(y1)}`;
}
const ang = n => -135 + 270 * n;
const normOf = (id, v) => (v - PM[id].min) / (PM[id].max - PM[id].min);
const isOn = (ui, key) => ui.hover === key || (ui.drag && ui.drag.id === key);
const chipW = (s, size) => s.length * size * MONO_ADV + 12;

function hit(ctx, h) { ctx.hits.push(h); return h; }
function rectHit(ctx, id, x, y, w, h, extra) { return hit(ctx, Object.assign({ id, k: "rect", x, y, w, h, box: [x, y, w, h] }, extra)); }
function circleHit(ctx, id, cx, cy, r, extra) { return hit(ctx, Object.assign({ id, k: "circle", cx, cy, r, box: [cx - r, cy - r, 2 * r, 2 * r] }, extra)); }

/* ---------- parts ---------- */
const assets = { wordmark: null, credit: null };   // filled by the build: outlined paths

function wordmark() {
  const m = L.wordmark;
  if (assets.wordmark) return `<path transform="translate(${m.x} ${m.baseline}) scale(${m.size / assets.wordmark.size})" d="${assets.wordmark.d}" fill="${T.text}"/>`;
  const sp = "De-Saturator".split("").map((c, i) => `<tspan font-weight="${Math.round(900 - 700 * i / 11)}"${c === "a" ? ' style="font-stretch:150%"' : ""}>${c}</tspan>`).join("");
  return `<text x="${m.x}" y="${m.baseline}" font-family="${FONTSTACK.Anybody}" font-size="${m.size}" fill="${T.text}">${sp}</text>`;
}
function segmented(ctx, id) {
  const { st, ui } = ctx, g = L[id];
  const total = g.segs.reduce((a, s) => a + s[2], 0);
  let s = `<rect x="${g.x}" y="${g.y}" width="${total}" height="${g.h}" rx="${g.h / 2}" fill="none" stroke="${T.text}" stroke-width="${S.outline}"/>`;
  let x = g.x;
  for (const [v, label, w] of g.segs) {
    const key = id + ":" + v, on = st[id] === v;
    if (on) s += `<rect x="${x + 3}" y="${g.y + 3}" width="${w - 6}" height="${g.h - 6}" rx="${(g.h - 6) / 2}" fill="${T.selected}"/>`;
    else if (ui.hover === key) s += `<rect x="${x + 3.5}" y="${g.y + 3.5}" width="${w - 7}" height="${g.h - 7}" rx="${(g.h - 7) / 2}" fill="none" stroke="${T.text}" stroke-width="1"/>`;
    s += text(x + w / 2, g.y + g.h / 2 + 4.5, label, g.h > 28 ? "button" : "label", on ? T.onSelected : T.text, "middle");
    rectHit(ctx, id, x, g.y, w, g.h, { v, type: "choice", key });
    x += w;
  }
  return s;
}
function pill(ctx, id, g, label, onFill, onText, role) {
  const on = !!ctx.st[id], hv = ctx.ui.hover === id;
  rectHit(ctx, id, g.x, g.y, g.w, g.h, { type: "toggle", key: id });
  return `<rect x="${g.x}" y="${g.y}" width="${g.w}" height="${g.h}" rx="${g.h / 2}" fill="${on ? onFill : "none"}" stroke="${on ? onFill : T.text}" stroke-width="${hv ? S.outlineHover : S.outline}"/>` +
    text(g.x + g.w / 2, g.y + g.h / 2 + 4.5, label, role, on ? onText : T.text, "middle");
}
function bypass(ctx) {
  const on = ctx.st.bypass, hv = ctx.ui.hover === "bypass", { cx, cy, r } = L.bypass, ic = on ? T.onSelected : T.text;
  circleHit(ctx, "bypass", cx, cy, r + 2, { type: "toggle", key: "bypass" });
  return `<circle cx="${cx}" cy="${cy}" r="${r}" fill="${on ? T.selected : "none"}" stroke="${T.text}" stroke-width="${hv ? S.outlineHover : S.outline}"/>` +
    `<path d="${arc(cx, cy + 1, 7, 38, 322)}" fill="none" stroke="${ic}" stroke-width="2"/><path d="M${cx} ${cy - 9}V${cy - 1}" stroke="${ic}" stroke-width="2"/>`;
}
function header(ctx) {
  const { st } = ctx;
  return wordmark() + segmented(ctx, "character") + segmented(ctx, "quality") +
    text(L.latency.x, L.latency.baseline, `${st.quality === "hq" ? st.latencyMs : 0} ms`, "value", T.textMuted) +
    pill(ctx, "delta", L.delta, "Delta", T.hand, T.onHand, "button") + bypass(ctx);
}
function amount(ctx) {
  const { st, ui } = ctx, g = L.amount, { cx, cy } = g, n = st.amount / 100, a = ang(n);
  const hv = isOn(ui, "amount"), dragging = ui.drag && ui.drag.id === "amount";
  const valueCol = st.bypass ? T.textMuted : T.hand, ringCol = st.bypass ? T.textMuted : T.text;
  let s = text(cx, g.title, "Amount", "title", T.text, "middle");
  for (let t = 0; t <= 10; t++) {
    const ta = ang(t / 10), [x0, y0] = polar(cx, cy, g.tickIn, ta), [x1, y1] = polar(cx, cy, t % 5 ? g.tickOut : g.tickMajor, ta);
    s += `<path d="M${n2(x0)} ${n2(y0)}L${n2(x1)} ${n2(y1)}" stroke="${hv ? T.text : T.textMuted}" stroke-width="${S.tick}"/>`;
  }
  s += `<path d="${arc(cx, cy, g.track, -135, 135)}" fill="none" stroke="${T.grid}" stroke-width="${S.amountArc}"/>`;
  s += `<path d="${arc(cx, cy, g.track, -135, a)}" fill="none" stroke="${valueCol}" stroke-width="${S.amountArc}"/>`;
  // the ring thins as Amount rises; a notch at the pointer shows its turn
  const sw = lerp(S.ringMax, S.ringMin, n), rm = g.r - sw / 2, gap = (g.notch / 2) / rm * 180 / Math.PI;
  s += `<path d="${arc(cx, cy, rm, a + gap, a + 360 - gap)}" fill="none" stroke="${ringCol}" stroke-width="${n2(sw)}"/>`;
  const [px, py] = polar(cx, cy, g.track, a);
  s += `<circle cx="${n2(px)}" cy="${n2(py)}" r="${dragging ? 7.5 : 5.5}" fill="${valueCol}" stroke="${T.ground}" stroke-width="2"/>`;
  const val = pct(st.amount);
  if (hv) s += `<rect x="${cx - g.chip.w / 2}" y="${g.chip.y}" width="${g.chip.w}" height="${g.chip.h}" fill="${T.readout}"/>`;
  s += text(cx, g.readout, val, "valueLarge", hv ? T.onReadout : T.text, "middle");
  circleHit(ctx, "amount", cx, cy, g.hit, { type: "knob", key: "amount", tipBox: [cx - g.r, cy - g.r, 2 * g.r, g.chip.y + g.chip.h - (cy - g.r)] });
  return s;
}
function smallKnob(ctx, id, label, cx) {
  const { st, ui } = ctx, g = L.small, cy = g.cy, n = normOf(id, st[id]), a = ang(n), hv = isOn(ui, id);
  const a0 = PM[id].origin != null ? ang(normOf(id, PM[id].origin)) : -135;
  const [x0, y0] = polar(cx, cy, g.pointerIn, a), [x1, y1] = polar(cx, cy, g.pointerOut, a);
  let s = `<path d="${arc(cx, cy, g.track, -135, 135)}" fill="none" stroke="${T.grid}" stroke-width="${S.knobTrack}"/>`;
  s += `<path d="${arc(cx, cy, g.track, a0, a)}" fill="none" stroke="${st.bypass ? T.textMuted : T.hand}" stroke-width="${S.knobTrack}"/>`;
  // a knob with an origin inside its range (Output, 0 dB) marks it outside the track, like the Amount ticks
  if (PM[id].origin != null) { const [ox0, oy0] = polar(cx, cy, g.originIn, a0), [ox1, oy1] = polar(cx, cy, g.originOut, a0); s += `<path d="M${n2(ox0)} ${n2(oy0)}L${n2(ox1)} ${n2(oy1)}" stroke="${hv ? T.text : T.textMuted}" stroke-width="${S.tick}"/>`; }
  s += `<circle cx="${cx}" cy="${cy}" r="${g.r}" fill="${T.ground}" stroke="${T.text}" stroke-width="${hv ? S.outlineHover : S.outline}"/>`;
  s += `<path d="M${n2(x0)} ${n2(y0)}L${n2(x1)} ${n2(y1)}" stroke="${T.text}" stroke-width="3" stroke-linecap="square"/>`;
  s += text(cx, g.label, label, "label", T.text, "middle");
  const val = fmt(id, st[id]);
  if (hv) { const w = chipW(val, 11); s += `<rect x="${n2(cx - w / 2)}" y="${g.value - 12.5}" width="${n2(w)}" height="17" fill="${T.readout}"/>`; }
  s += text(cx, g.value, val, "value", hv ? T.onReadout : T.textMuted, "middle");
  circleHit(ctx, id, cx, cy, g.hit, { type: "knob", key: id, tipBox: [cx - g.hit, cy - g.hit, 2 * g.hit, g.value + 4.5 - (cy - g.hit)] });
  return s;
}
function shape(ctx) {
  const { st, ui } = ctx, g = L.shape, x = lerp(g.x1, g.x2, st.shape / 100), y = g.y, hv = isOn(ui, "shape"), th = g.thumb;
  let s = text((g.x1 + g.x2) / 2, g.title, "Shape", "label", T.text, "middle");
  s += `<path d="M${g.x1} ${y}H${g.x2}" stroke="${T.grid}" stroke-width="3"/><path d="M${(g.x1 + g.x2) / 2} ${y - 6}V${y + 6}" stroke="${T.grid}" stroke-width="1.5"/>`;
  s += text(g.x1 - g.endGap, y - 2, "Harmonics", "label", T.text, "end") + text(g.x1 - g.endGap, y + 13, (100 - Math.round(st.shape)) + "%", "value", T.textMuted, "end");
  s += text(g.x2 + g.endGap, y - 2, "Body", "label", T.text) + text(g.x2 + g.endGap, y + 13, Math.round(st.shape) + "%", "value", T.textMuted);
  s += `<rect x="${n2(x - th.w / 2)}" y="${y - th.h / 2}" width="${th.w}" height="${th.h}" rx="${th.w / 2}" fill="${T.selected}"/>`;
  s += `<path d="M${n2(x)} ${y - 7}V${y + 7}" stroke="${T.onSelected}" stroke-width="2"/>`;
  if (hv) s += `<rect x="${n2(x - th.w / 2 - 4)}" y="${y - th.h / 2 - 4}" width="${th.w + 8}" height="${th.h + 8}" rx="${th.w / 2 + 4}" fill="none" stroke="${T.text}" stroke-width="${S.outline}"/>`;
  rectHit(ctx, "shape", g.x1 - 12, y - 18, g.x2 - g.x1 + 24, 36, { type: "slider", key: "shape" });
  return s;
}
function credit(ctx) {
  const c = L.credit;
  const mark = assets.credit
    ? `<svg x="${c.x}" y="${c.y}" width="${c.w}" height="${c.h}" viewBox="${assets.credit.vb}" preserveAspectRatio="xMinYMid meet"><path d="${assets.credit.d}" fill="${T.textMuted}"/></svg>`
    : text(c.x, c.version.baseline, "Aria Ashrafi", "value", T.textMuted);
  return mark + text(c.version.x, c.version.baseline, ctx.st.versionText || "1.0.0", "value", T.textMuted);
}
function background() {
  const A = L.analyser;
  let s = `<rect width="${W}" height="${H}" fill="${T.ground}"/>`;
  s += `<rect x="${A.x}" y="${A.y}" width="${A.w}" height="${A.h}" fill="${T.ground}" stroke="${T.text}" stroke-width="${S.frame}"/>`;
  for (const db of [-72, -48, -24, 0]) s += `<path d="M${P.x} ${n2(dy(db))}H${P.x + P.w}" stroke="${T.grid}" stroke-width="${S.grid}" stroke-dasharray="2 4"/>`;
  for (const f of [50, 100, 200, 500, 1000, 2000, 5000, 10000]) {
    const x = n2(fx(f)), major = f === 100 || f === 1000 || f === 10000;
    s += `<path d="M${x} ${P.y}V${P.y + P.h}" stroke="${T.grid}" stroke-width="${S.grid}"${major ? "" : ' opacity="0.55"'}/>`;
    s += text(x, L.strip.baseline, f === 50 ? "50 Hz" : f >= 1000 ? f / 1000 + "k" : String(f), "value", T.textMuted, "middle");
  }
  s += `<path d="M${P.x} ${P.y + P.h}H${P.x + P.w}" stroke="${T.grid}" stroke-width="${S.grid}"/>`;
  return s;
}
function line(arr) { let s = ""; for (let i = 0; i < arr.length; i++) s += (i ? "L" : "M") + XS[i].toFixed(1) + " " + cyv(arr[i]).toFixed(1); return s; }
function overlay(ctx) {
  const { st, ui, drawn, applied } = ctx;
  let s = "";
  const dim = st.bypass ? ' opacity="0.45"' : "";
  if (st.quality === "live") {
    s += `<path d="${line(applied)}" fill="none" stroke="${T.ground}" stroke-width="${S.curveUnder}" stroke-linejoin="round"${dim}/>`;
    s += `<path d="${line(drawn)}" fill="none" stroke="${T.hand}" stroke-width="${S.curveDrawn}" stroke-dasharray="4 4"${dim}/>`;
    s += `<path d="${line(applied)}" fill="none" stroke="${T.hand}" stroke-width="${S.curveApplied}" stroke-linejoin="round"${dim}/>`;
  } else {
    const d = line(drawn);
    s += `<path d="${d}" fill="none" stroke="${T.ground}" stroke-width="${S.curveUnder}" stroke-linejoin="round"${dim}/>`;
    s += `<path d="${d}" fill="none" stroke="${T.hand}" stroke-width="${S.curve}" stroke-linejoin="round"${dim}/>`;
  }
  // the curve's scale, right edge
  const lab = (y, t) => { const w = chipW(t, 11) - 4; return `<rect x="${n2(P.x + P.w - 6 - w)}" y="${n2(y - 11)}" width="${n2(w)}" height="15" fill="${T.ground}" opacity="0.85"/>` + text(P.x + P.w - 10, y, t, "value", T.textMuted, "end"); };
  s += lab(P.y + 16, "200%") + lab(cyv(1) - 7, "100%") + lab(P.y + P.h - 5, "0%");
  // the state line, top left
  const note = st.bypass ? NOTES.bypass : st.delta ? NOTES.delta : ui.note ? ui.note : st.quality === "live" ? NOTES.live : "";
  if (note) s += `<rect x="${P.x + 8}" y="${P.y + 8}" width="${n2(chipW(note, 11) + 2)}" height="19" fill="${T.ground}" opacity="0.9"/>` + text(P.x + 15, P.y + 21.5, note, "value", T.text);
  // points
  let chip = "", ps = "";
  st.points.forEach((p, i) => {
    let s = "";
    const x = fx(p.f), y = cyv(p.a), key = "point:" + i, sel = ui.sel === i, hv = ui.hover === key, dr = !!(ui.drag && ui.drag.id === "point" && ui.drag.i === i);
    const g = L.point;
    if (sel || hv || dr) {
      const xa = fx(clamp(p.f * Math.pow(2, -p.w / 2), FMIN, FMAX)), xb = fx(clamp(p.f * Math.pow(2, p.w / 2), FMIN, FMAX));
      s += `<path d="M${n2(xa)} ${n2(y)}H${n2(xb)}M${n2(xa)} ${n2(y - 5)}v10M${n2(xb)} ${n2(y - 5)}v10" fill="none" stroke="${T.hand}" stroke-width="${S.bracket}"/>`;
    }
    if (dr) s += `<path d="M${n2(x)} ${n2(y + g.halo)}V${P.y + P.h}" stroke="${T.hand}" stroke-width="1" stroke-dasharray="2 3"/>`;
    if (sel || dr) s += `<circle cx="${n2(x)}" cy="${n2(y)}" r="${g.halo}" fill="none" stroke="${T.hand}" stroke-width="${S.bracket}"/><circle cx="${n2(x)}" cy="${n2(y)}" r="${g.rSelected}" fill="${T.hand}" stroke="${T.ground}" stroke-width="2"/>`;
    else s += `<circle cx="${n2(x)}" cy="${n2(y)}" r="${hv ? g.rHover : g.r}" fill="${T.ground}" stroke="${T.hand}" stroke-width="${S.pointRing}"/>`;
    if (sel || hv || dr) {
      const t = `${fmtF(p.f)}  ${Math.round(p.a * 100)}%  ${p.w.toFixed(1)} oct`, w = chipW(t, 11) + 2;
      const cx = clamp(x - w / 2, P.x + 6, P.x + P.w - 6 - w), ty = y < P.y + 46 ? y + 22 : y - 39;
      chip = `<rect x="${n2(cx)}" y="${n2(ty)}" width="${n2(w)}" height="19" fill="${T.readout}"/>` + text(cx + 7, ty + 13.5, t, "value", T.onReadout, "start", ' xml:space="preserve" style="white-space:pre"');
    }
    ps += s;
  });
  return s + (st.bypass ? `<g opacity="0.45">${ps}</g>` : ps) + chip;
}
function wrap(s, maxW, size) {
  const per = Math.floor(maxW / (size * 0.52)), out = [];
  let cur = "";
  for (const w of s.split(" ")) { if ((cur + " " + w).trim().length > per && cur) { out.push(cur); cur = w; } else cur = (cur + " " + w).trim(); }
  if (cur) out.push(cur);
  return out;
}
function tooltip(ctx) {
  const key = ctx.ui.tip; if (!key) return "";
  const msg = TIPS[key] || TIPS[key.split(":")[0]]; if (!msg) return "";
  let h = null;
  if (key.startsWith("point:")) { const q = ctx.st.points[+key.split(":")[1]]; if (q) { const near = cyv(q.a) < P.y + 46; h = { box: [fx(q.f) - 12, cyv(q.a) - 12, 24, near ? 53 : 24] }; } }
  else if (key !== "display") h = ctx.hits.find(x => (x.key || x.id) === key);
  const g = L.tooltip, t = TYPE.tooltip, lines = wrap(msg, g.maxWidth - 2 * g.padX, t.size);
  const w = Math.min(g.maxWidth, Math.max(...lines.map(l => l.length)) * t.size * 0.52 + 2 * g.padX), hh = lines.length * t.lineHeight + 2 * g.padY - 4;
  let bx, by;
  if (h) { const [x, y, bw, bh] = h.tipBox || h.box; bx = x + bw / 2 - w / 2; by = y + bh + g.gap; if (by + hh > H - 8) by = y - g.gap - hh; }
  else if (ctx.ui.tipAt) { bx = ctx.ui.tipAt[0] - w / 2; by = ctx.ui.tipAt[1] + 18; }
  else return "";
  bx = clamp(bx, 8, W - 8 - w);
  let s = `<rect x="${n2(bx)}" y="${n2(by)}" width="${n2(w)}" height="${n2(hh)}" fill="${T.readout}"/>`;
  lines.forEach((l, i) => { s += text(bx + g.padX, by + g.padY + 9 + i * t.lineHeight, l, "tooltip", T.onReadout); });
  return s;
}

/* ---------- the window ---------- */
function context(st, ui) {
  useTheme(st.theme);
  const c = curves(st);
  return { st, ui: Object.assign({ hover: null, drag: null, sel: null, note: null, tip: null }, ui || {}), hits: [], drawn: c.drawn, applied: c.applied };
}
function controls(ctx) {
  let s = header(ctx) + amount(ctx);
  for (const [id, label, cx] of L.small.knobs) s += smallKnob(ctx, id, label, cx);
  s += pill(ctx, "autoGain", L.autoGain, "Auto gain", T.selected, T.onSelected, "label");
  s += shape(ctx) + credit(ctx) + segmented(ctx, "theme");
  // the analyser's own hit area comes after the controls
  rectHit(ctx, "display", P.x, P.y, P.w, P.h, { type: "display", key: "display" });
  return s;
}
function ui(ctx) { const c = controls(ctx); return c + legend(ctx.st) + overlay(ctx) + tooltip(ctx); }
function combSVG(st, orig, red) {
  const { keep, top } = comb(orig, red), sty = combStyle(st);
  let k = "", r = "";
  for (let j = 0; j < COMB_X.length; j++) {
    const x = COMB_X[j].toFixed(2);
    if (keep[j] < COMB_BASE - 0.05) k += `<line x1="${x}" x2="${x}" y1="${COMB_BASE}" y2="${keep[j].toFixed(1)}"/>`;
    if (top[j] < keep[j] - 0.05) r += `<line x1="${x}" x2="${x}" y1="${keep[j].toFixed(1)}" y2="${top[j].toFixed(1)}"/>`;
  }
  let s = `<g stroke="${sty.keep[0]}" stroke-opacity="${ALPHA.body}" stroke-width="${sty.keep[1]}">${k}</g>`;
  if (sty.removed) s += `<g stroke="${sty.removed[0]}" stroke-width="${sty.removed[1]}">${r}</g>`;
  if (sty.original) s += `<path d="${contour(orig)}" fill="none" stroke="${sty.original[0]}" stroke-width="${sty.original[1]}" stroke-linejoin="round"/>`;
  s += `<path d="${contour(orig, red)}" fill="none" stroke="${sty.result[0]}" stroke-width="${sty.result[1]}" stroke-linejoin="round"/>`;
  return s;
}
// the whole window as one SVG string (state renders, spec pictures)
function render(st, uiState, orig, opts) {
  opts = opts || {};
  const ctx = context(st, uiState);
  const red = reduce(st, ctx.applied, orig || SILENCE);
  const id = opts.id || "ds";
  const body = `<defs><clipPath id="${id}-plot"><rect x="${P.x}" y="${P.y}" width="${P.w}" height="${P.h}"/></clipPath></defs>` +
    background() + `<g clip-path="url(#${id}-plot)">${combSVG(st, orig || SILENCE, red)}</g>` + ui(ctx) + (opts.extra || "");
  const w = opts.width || W, h = Math.round(w * H / W);
  return { svg: `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${W} ${H}" width="${w}" height="${h}" role="img" aria-label="${esc(opts.label || "De-Saturator")}">${body}</svg>`, hits: ctx.hits };
}
function hitTest(ctx, p) {
  for (const h of ctx.hits) {
    if (h.id === "display") continue;
    if (h.k === "circle" && Math.hypot(p.x - h.cx, p.y - h.cy) <= h.r) return h;
    if (h.k === "rect" && p.x >= h.x && p.x <= h.x + h.w && p.y >= h.y && p.y <= h.y + h.h) return h;
  }
  if (p.x >= P.x && p.x <= P.x + P.w && p.y >= P.y && p.y <= P.y + P.h) {
    let best = null, bd = L.point.hit;
    ctx.st.points.forEach((q, i) => { const d = Math.hypot(p.x - fx(q.f), p.y - cyv(q.a)); if (d < bd) { bd = d; best = i; } });
    if (best != null) return { id: "point", i: best, type: "point", key: "point:" + best, box: [fx(ctx.st.points[best].f) - 10, cyv(ctx.st.points[best].a) - 10, 20, 20] };
    return ctx.hits.find(h => h.id === "display");
  }
  return null;
}

const DS = {
  VERSION, W, H, FMIN, FMAX, OCT, DBMIN, DBMAX, TILT, NB, BINF, COLOR, ROLE, T, ALPHA, THEMES, useTheme, STROKE, S, TYPE, FONTSTACK, MONO_ADV, L,
  PARAMS, PM, SETTINGS, CURVE, DRAG, TIPS, NOTES, defaults, fmt, fmtF, pct, clamp, lerp,
  fx, xf, dy, cyv, yv, Signal, snapshot, SILENCE, curveAt, curves, reduce,
  COMB_X, COMB_BASE, comb, combStyle, contour, legend, background, ui, context, render, hitTest, assets, esc,
};
if (typeof module !== "undefined" && module.exports) module.exports = DS;
root.DS = DS;
})(typeof globalThis !== "undefined" ? globalThis : this);
