// Renders every state of the window from ui/core.js to PNG, at 1x and 2x, plus the redline sheet.
//   node states.mjs            all
//   node states.mjs default    only the states named
// Chrome draws them, with the same fonts the plugin embeds.
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { createRequire } from "node:module";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = join(HERE, "..");
const require = createRequire(import.meta.url);
const DS = require("./ui/core.js");
const { chromium } = createRequire(process.env.DESAT_PLAYWRIGHT_FROM ?? "C:/Users/AriaA/Desktop/All/Other/Work/Kareone/UI-UX/Archive/Pendar/ui/package.json")("playwright");

const json = (p) => JSON.parse(readFileSync(join(ROOT, p), "utf8"));
if (existsSync(join(ROOT, "logo/wordmark-ui.json"))) DS.assets.wordmark = json("logo/wordmark-ui.json");
if (existsSync(join(ROOT, "logo/credit.json"))) DS.assets.credit = json("logo/credit.json");

const SNAP = DS.snapshot();
const pts = () => [{ f: 140, a: 0.35, w: 1.4 }, { f: 3300, a: 1.75, w: 0.7 }, { f: 9500, a: 1.35, w: 0.5 }];
const def = (o = {}) => Object.assign(DS.defaults(), o);
const work = (o = {}) => Object.assign(DS.defaults(), { amount: 55, shape: 40, restore: 15, output: -1.5, points: pts() }, o);
const six = [{ f: 60, a: 0.2, w: 1.2 }, { f: 220, a: 0.6, w: 1 }, { f: 900, a: 1.25, w: 1 }, { f: 2600, a: 1.8, w: 0.8 }, { f: 6000, a: 1.4, w: 0.7 }, { f: 13000, a: 0.9, w: 0.9 }];

export const STATES = [
  { slug: "default", name: "Default", desc: "As it opens: Amount 30%, no points, the curve flat at 100% (it follows Amount everywhere).", st: def() },
  { slug: "in-use", name: "In use", desc: "Amount 55%, three points: less in the lows, more around 3 kHz and 9.5 kHz.", st: work() },
  { slug: "amount-hover", name: "Amount, hover", desc: "The ticks darken and the value turns into a chip.", st: def(), ui: { hover: "amount" } },
  { slug: "amount-drag", name: "Amount, drag", desc: "The pointer dot grows while you drag. The ring thins as the value rises.", st: work({ amount: 78 }), ui: { hover: "amount", drag: { id: "amount" } } },
  { slug: "knob-hover", name: "Small knob, hover", desc: "The outline thickens and the value turns into a chip.", st: work(), ui: { hover: "mix" } },
  { slug: "knob-drag", name: "Small knob, drag", desc: "Restore being dragged.", st: work({ restore: 42 }), ui: { hover: "restore", drag: { id: "restore" } } },
  { slug: "shape-drag", name: "Shape, drag", desc: "A ring around the thumb while it's held. Both ends show their share.", st: work({ shape: 72 }), ui: { hover: "shape", drag: { id: "shape" } } },
  { slug: "switch-hover", name: "Switch, hover", desc: "The segment under the pointer gets a thin outline.", st: work(), ui: { hover: "character:character" } },
  { slug: "point-hover", name: "Point, hover", desc: "The point grows, shows its width bracket and a readout.", st: work(), ui: { hover: "point:2" } },
  { slug: "point-drag", name: "Point, drag", desc: "Filled, with a halo, a drop line to the frequency scale and the readout.", st: work({ points: [pts()[0], { f: 2400, a: 1.9, w: 0.7 }, pts()[2]] }), ui: { sel: 1, hover: "point:1", drag: { id: "point", i: 1 } } },
  { slug: "point-selected", name: "Point, selected", desc: "Stays filled with its bracket and readout after you let go. Scroll on it to change the width.", st: work(), ui: { sel: 1 } },
  { slug: "points-full", name: "Six points", desc: "The most there can be. Double-clicking empty space says so.", st: work({ points: six }), ui: { note: DS.NOTES.full } },
  { slug: "delta-on", name: "Delta on", desc: "Only what's removed plays. The removed part turns heavy and klein; the result drops to a hairline.", st: work({ delta: true }) },
  { slug: "clean", name: "Clean", desc: "Transparent: the gentler range.", st: work() },
  { slug: "character", name: "Character", desc: "Goes further: the same settings take out more.", st: work({ character: "character" }) },
  { slug: "hq", name: "HQ", desc: "Full resolution, with latency. One curve.", st: work() },
  { slug: "live", name: "Live", desc: "No latency. Your curve turns dashed; the smoothed curve that's applied is drawn solid.", st: work({ quality: "live" }), ui: { sel: 1 } },
  { slug: "bypassed", name: "Bypassed", desc: "Nothing thins: one heavy line, no hatching, the curve and the value arcs fade.", st: work({ bypass: true }) },
  { slug: "silent", name: "No signal", desc: "The analyser is empty; the curve stays.", st: work(), orig: DS.SILENCE },
  { slug: "tooltip", name: "Tooltip", desc: "After 600 ms over a control. Ink box, paper text.", st: def(), ui: { hover: "delta", tip: "delta" } },
  { slug: "tooltip-point", name: "Tooltip on a point", desc: "How to use a point, once.", st: work(), ui: { hover: "point:1", tip: "point:1" } },
  { slug: "scale-75", name: "At 75%", desc: "720 × 450, the smallest size.", st: work(), scale: 0.75 },
  { slug: "scale-200", name: "At 200%", desc: "1920 × 1200, the largest size.", st: work(), scale: 2 },
];
// the same window in the dark theme
for (const slug of ["default", "in-use", "amount-drag", "point-drag", "delta-on", "live", "bypassed", "tooltip"]) {
  const s = STATES.find((x) => x.slug === slug);
  STATES.push({ ...s, slug: `${slug}-dark`, name: `${s.name}, dark`, st: { ...s.st, theme: "dark" } });
}
STATES.push({ slug: "theme-hover", name: "Theme switch, hover", desc: "Light and Dark, bottom right. A window preference saved with the plugin's state, not a parameter.", st: work(), ui: { hover: "theme:dark", tip: "theme:dark" } });

// the redline sheet: every element's box over the default window
function redlines() {
  const L = DS.L, K = DS.COLOR.klein, out = [];
  const box = (x, y, w, h, label, below) => {
    out.push(`<rect x="${x}" y="${y}" width="${w}" height="${h}" fill="none" stroke="${K}" stroke-width="1" stroke-dasharray="3 2"/>`);
    const t = label, tw = t.length * 7 + 8, ty = below ? y + h + 2 : y - 15;
    out.push(`<rect x="${x}" y="${ty}" width="${tw}" height="13" fill="${K}"/><text x="${x + 4}" y="${ty + 10}" font-family="Departure Mono, monospace" font-size="11" fill="#ECEBE6">${t}</text>`);
  };
  const circ = (cx, cy, r, label) => {
    out.push(`<circle cx="${cx}" cy="${cy}" r="${r}" fill="none" stroke="${K}" stroke-width="1" stroke-dasharray="3 2"/><path d="M${cx - 4} ${cy}h8M${cx} ${cy - 4}v8" stroke="${K}"/>`);
    const tw = label.length * 7 + 8, ly = cy - r - 15 < 2 ? cy + r + 2 : cy - r - 15;
    if (label) out.push(`<rect x="${cx - tw / 2}" y="${ly}" width="${tw}" height="13" fill="${K}"/><text x="${cx - tw / 2 + 4}" y="${ly + 10}" font-family="Departure Mono, monospace" font-size="11" fill="#ECEBE6">${label}</text>`);
  };
  const seg = (g) => g.segs.reduce((a, s) => a + s[2], 0);
  box(L.character.x, L.character.y, seg(L.character), L.character.h, `${L.character.x},${L.character.y} ${seg(L.character)}x${L.character.h}`, true);
  box(L.quality.x, L.quality.y, seg(L.quality), L.quality.h, `${L.quality.x},${L.quality.y} ${seg(L.quality)}x${L.quality.h}`, true);
  box(L.delta.x, L.delta.y, L.delta.w, L.delta.h, `${L.delta.x},${L.delta.y}`, true);
  circ(L.bypass.cx, L.bypass.cy, L.bypass.r, `r${L.bypass.r}`);
  box(L.analyser.x, L.analyser.y, L.analyser.w, L.analyser.h, `analyser ${L.analyser.x},${L.analyser.y} ${L.analyser.w}x${L.analyser.h}`, true);
  box(L.plot.x + 40, L.plot.y + 40, L.plot.w - 80, L.plot.h - 80, `plot ${L.plot.x},${L.plot.y} ${L.plot.w}x${L.plot.h}, drawn inset`, false);
  circ(L.amount.cx, L.amount.cy, L.amount.r, `${L.amount.cx},${L.amount.cy} r${L.amount.r}`);
  circ(L.amount.cx, L.amount.cy, L.amount.track, "");
  for (const [, , cx] of L.small.knobs) circ(cx, L.small.cy, L.small.r, `${cx},${L.small.cy}`);
  box(L.autoGain.x, L.autoGain.y, L.autoGain.w, L.autoGain.h, `${L.autoGain.x},${L.autoGain.y} ${L.autoGain.w}x${L.autoGain.h}`, true);
  box(L.shape.x1, L.shape.y - 15, L.shape.x2 - L.shape.x1, 30, `shape ${L.shape.x1}..${L.shape.x2} y${L.shape.y}`, true);
  box(L.credit.x, L.credit.y, L.credit.w, L.credit.h, `${L.credit.x},${L.credit.y} ${L.credit.w}x${L.credit.h}`, false);
  return out.join("");
}

const only = process.argv.slice(2);
const list = only.length ? STATES.filter((s) => only.includes(s.slug)) : STATES;
const font = (f) => readFileSync(join(ROOT, "fonts", f)).toString("base64");
const CSS = `@font-face{font-family:Anybody;src:url(data:font/ttf;base64,${font("Anybody-Variable.ttf")});font-weight:100 900;font-stretch:50% 150%}
@font-face{font-family:'Departure Mono';src:url(data:font/otf;base64,${font("DepartureMono-Regular.otf")})}
html,body{margin:0;background:#fff}div{width:max-content;line-height:0}svg{display:block}`;
const items = list.map((s) => ({ s, svg: DS.render(s.st, s.ui, s.orig ?? SNAP, { id: s.slug, width: Math.round(DS.W * (s.scale ?? 1)), label: `De-Saturator, ${s.name}` }).svg }));
if (!only.length) items.push({ s: { slug: "redlines", name: "Redlines", desc: "Every element's position and size, in window pixels at 100%." }, svg: DS.render(DS.defaults(), {}, SNAP, { id: "redlines", extra: redlines(), label: "De-Saturator redlines" }).svg });
const html = `<!doctype html><html><head><meta charset="utf-8"><style>${CSS}</style></head><body>${items.map(({ s, svg }) => `<div id="${s.slug}">${svg}</div>`).join("")}</body></html>`;

mkdirSync(join(ROOT, "states"), { recursive: true });
const browser = await chromium.launch({ headless: true, channel: "chrome" });
for (const dpr of [1, 2]) {
  const page = await browser.newPage({ viewport: { width: 2000, height: 1300 }, deviceScaleFactor: dpr });
  await page.setContent(html);
  await page.evaluate(() => document.fonts.ready);
  for (const { s } of items) await (await page.$(`#${s.slug}`)).screenshot({ path: join(ROOT, "states", `${s.slug}${dpr === 2 ? "@2x" : ""}.png`) });
  await page.close();
}
await browser.close();

if (!only.length) {
  const man = [];
  for (const { s } of items) {
    man.push({ collection: "ui-states", file: `states/${s.slug}.png`, slug: s.slug, name: s.name, description: s.desc, tags: ["state"] });
    man.push({ collection: "ui-states-2x", file: `states/${s.slug}@2x.png`, slug: `${s.slug}-2x`, name: `${s.name} (2x)`, description: s.desc, tags: ["state", "2x"] });
  }
  mkdirSync(join(HERE, "manifest"), { recursive: true });
  writeFileSync(join(HERE, "manifest", "states.json"), JSON.stringify(man, null, 1));
  writeFileSync(join(ROOT, "states", "states.json"), JSON.stringify(items.map(({ s }) => ({ file: `${s.slug}.png`, file2x: `${s.slug}@2x.png`, name: s.name, description: s.desc })), null, 1));
}
console.log(`rendered ${items.length} states at 1x and 2x`);
