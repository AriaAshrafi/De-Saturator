"use strict";
/* De-Saturator direction board: three working windows over one stand-in signal.
   Window 960 x 600. Shared model and layout; each direction draws the analyser,
   the Amount knob and the name its own way. Nothing plays: the analyser is fed by
   a synthetic spectrum (a bass line, a pad, kick, snare and hats at 120 BPM). */
(() => {
const W = 960, H = 600;
const FMIN = 20, FMAX = 20000, OCT = Math.log2(FMAX / FMIN);
const DBMIN = -90, DBMAX = 6;
const NB = 192;
const BINF = new Float32Array(NB);
for (let i = 0; i < NB; i++) BINF[i] = FMIN * Math.pow(2, OCT * i / (NB - 1));
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
const lerp = (a, b, t) => a + (b - a) * t;
const r1 = v => Math.round(v * 10) / 10;

/* Layout (window units) */
const D = { x: 24, y: 72, w: 632, h: 368 };        // analyser frame, frequency strip included
const P = { x: 24, y: 72, w: 632, h: 346 };        // plot area
const KN = { cx: 808, cy: 222 };
const SMALL = [["restore", "Restore", 724], ["mix", "Mix", 808], ["output", "Output", 892]];
const SY = 428;
const SH = { x1: 120, x2: 560, y: 514 };
const fx = f => P.x + P.w * Math.log2(f / FMIN) / OCT;
const xf = x => FMIN * Math.pow(2, OCT * (x - P.x) / P.w);
const dy = db => P.y + P.h * (DBMAX - db) / (DBMAX - DBMIN);
const cyv = v => P.y + P.h * (1 - v / 2);
const yv = y => 2 * (1 - (y - P.y) / P.h);
const ANY = 'font-family="Anybody, system-ui, sans-serif"';
const MONO = 'font-family="Departure Mono, ui-monospace, monospace"';
const XS = Array.from(BINF, fx);

const PARAMS = {
  amount: { min: 0, max: 100, def: 30, step: 1 },
  shape: { min: 0, max: 100, def: 50, step: 1 },
  restore: { min: 0, max: 100, def: 0, step: 1 },
  mix: { min: 0, max: 100, def: 100, step: 1 },
  output: { min: -24, max: 12, def: 0, step: 0.5 },
};
const pct = v => Math.round(v) + "%";
const fmt = (id, v) => id === "output" ? (v > 0.04 ? "+" : v < -0.04 ? "−" : "") + Math.abs(v).toFixed(1) + " dB" : pct(v);
const fmtF = f => f < 1000 ? Math.round(f) + " Hz" : (f < 9950 ? (f / 1000).toFixed(1) : Math.round(f / 1000)) + " kHz";

function polar(cx, cy, r, deg) { const a = (deg - 90) * Math.PI / 180; return [cx + r * Math.cos(a), cy + r * Math.sin(a)]; }
function arc(cx, cy, r, a0, a1) {
  if (Math.abs(a1 - a0) < 0.05) return "";
  const [x0, y0] = polar(cx, cy, r, a0), [x1, y1] = polar(cx, cy, r, a1);
  return `M${x0.toFixed(2)} ${y0.toFixed(2)}A${r} ${r} 0 ${Math.abs(a1 - a0) > 180 ? 1 : 0} ${a1 > a0 ? 1 : 0} ${x1.toFixed(2)} ${y1.toFixed(2)}`;
}
const ang = n => -135 + 270 * n;
const norm = id => v => (v - PARAMS[id].min) / (PARAMS[id].max - PARAMS[id].min);

/* ---------- stand-in signal ---------- */
const HW = new Float32Array(NB), BW = new Float32Array(NB);
for (let i = 0; i < NB; i++) {
  HW[i] = 1 / (1 + Math.exp(-Math.log2(BINF[i] / 1400) / 0.5));
  const b = Math.log2(BINF[i] / 170) / 1.1; BW[i] = Math.exp(-0.5 * b * b);
}
const sig = {
  t: 0, lv: new Float32Array(NB).fill(DBMIN), tg: new Float32Array(NB), nz: new Float32Array(NB),
  target() {
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
      this.nz[i] = this.nz[i] * 0.8 + (Math.random() - 0.5) * 1.2;
      this.tg[i] = 10 * Math.log10(p) + 4.5 * o + this.nz[i];
    }
  },
  step(dt) {
    this.t += dt; this.target();
    for (let i = 0; i < NB; i++) {
      const v = clamp(0.25 * this.tg[Math.max(0, i - 1)] + 0.5 * this.tg[i] + 0.25 * this.tg[Math.min(NB - 1, i + 1)], DBMIN, DBMAX);
      this.lv[i] = v > this.lv[i] ? lerp(this.lv[i], v, 0.7) : Math.max(v, this.lv[i] - 28 * dt);
    }
  },
};
// a frozen frame for the still windows: just after a kick, with the snare tail
for (let i = 0; i < 200; i++) sig.step(1 / 60);
const SNAP = Float32Array.from(sig.lv);

function curveDrawn(points, f) {
  let v = 1;
  for (const p of points) { const d = Math.log2(f / p.f) / (p.w / 2.3548); v += (p.a - 1) * Math.exp(-0.5 * d * d); }
  return clamp(v, 0, 2);
}

/* ---------- shared pieces ---------- */
function segmented(w, id, x, y, opts) {
  const T = w.T, total = opts.reduce((a, o) => a + o[2], 0);
  let s = `<rect x="${x}" y="${y}" width="${total}" height="32" rx="16" fill="none" stroke="${T.line}" stroke-width="1.5"/>`;
  let cx = x;
  for (const [v, label, wd] of opts) {
    const on = w.st[id] === v, hv = w.hk === id + ":" + v;
    if (on) s += `<rect x="${cx + 3}" y="${y + 3}" width="${wd - 6}" height="26" rx="13" fill="${T.onFill}"/>`;
    else if (hv) s += `<rect x="${cx + 3.5}" y="${y + 3.5}" width="${wd - 7}" height="25" rx="12.5" fill="none" stroke="${T.line}" stroke-width="1"/>`;
    s += `<text x="${cx + wd / 2}" y="${y + 20.5}" text-anchor="middle" ${ANY} font-size="13" font-weight="600" fill="${on ? T.onText : T.text}">${label}</text>`;
    w.hits.push({ id, v, type: "seg", k: "rect", x: cx, y, w: wd, h: 32 });
    cx += wd;
  }
  return s;
}
function pill(w, id, x, y, wd, h, label, fill, text) {
  const T = w.T, on = !!w.st[id], hv = w.hk === id;
  w.hits.push({ id, type: "toggle", k: "rect", x, y, w: wd, h });
  return `<rect x="${x}" y="${y}" width="${wd}" height="${h}" rx="${h / 2}" fill="${on ? fill : "none"}" stroke="${T.line}" stroke-width="${hv ? 2.5 : 1.5}"/>` +
    `<text x="${x + wd / 2}" y="${y + h / 2 + 4.5}" text-anchor="middle" ${ANY} font-size="${h > 28 ? 13 : 12}" font-weight="600" fill="${on ? text : T.text}">${label}</text>`;
}
function bypassBtn(w) {
  const T = w.T, on = w.st.bypass, hv = w.hk === "bypass", cx = 920, cy = 28;
  w.hits.push({ id: "bypass", type: "toggle", k: "circle", cx, cy, r: 18 });
  const ic = on ? T.onText : T.text;
  return `<circle cx="${cx}" cy="${cy}" r="16" fill="${on ? T.onFill : "none"}" stroke="${T.line}" stroke-width="${hv ? 2.5 : 1.5}"/>` +
    `<path d="${arc(cx, cy + 1, 7, 38, 322)}" fill="none" stroke="${ic}" stroke-width="2"/><path d="M${cx} ${cy - 9}V${cy - 1}" stroke="${ic}" stroke-width="2"/>`;
}
function smallKnob(w, id, label, cx) {
  const T = w.T, st = w.st, n = norm(id)(st[id]), a = ang(n), hv = w.hk === id || (w.drag && w.drag.id === id);
  const a0 = id === "output" ? ang(norm("output")(0)) : -135;
  w.hits.push({ id, type: "knob", k: "circle", cx, cy: SY, r: 32 });
  const [px0, py0] = polar(cx, SY, 7, a), [px1, py1] = polar(cx, SY, 18, a);
  let s = `<path d="${arc(cx, SY, 29, -135, 135)}" fill="none" stroke="${T.track}" stroke-width="3.5"/>`;
  s += `<path d="${a > a0 ? arc(cx, SY, 29, a0, a) : arc(cx, SY, 29, a, a0)}" fill="none" stroke="${T.arc}" stroke-width="3.5"/>`;
  s += `<circle cx="${cx}" cy="${SY}" r="22" fill="${T.knobBody}" ${T.knobRing ? `stroke="${T.knobRing}" stroke-width="1.5"` : ""}/>`;
  if (hv) s += `<circle cx="${cx}" cy="${SY}" r="19" fill="none" stroke="${T.pointer}" stroke-width="1"/>`;
  s += `<path d="M${px0.toFixed(2)} ${py0.toFixed(2)}L${px1.toFixed(2)} ${py1.toFixed(2)}" stroke="${T.pointer}" stroke-width="3" stroke-linecap="square"/>`;
  s += `<text x="${cx}" y="${SY + 46}" text-anchor="middle" ${ANY} font-size="12" font-weight="600" fill="${T.text}">${label}</text>`;
  const val = fmt(id, st[id]);
  if (hv) {
    const cw = val.length * 6.6 + 12;
    s += `<rect x="${cx - cw / 2}" y="${SY + 53}" width="${cw}" height="17" fill="${T.chip}"/>`;
    s += `<text x="${cx}" y="${SY + 65.5}" text-anchor="middle" ${MONO} font-size="11" fill="${T.chipText}">${val}</text>`;
  } else s += `<text x="${cx}" y="${SY + 65.5}" text-anchor="middle" ${MONO} font-size="11" fill="${T.sub}">${val}</text>`;
  return s;
}
function shapeSlider(w) {
  const T = w.T, st = w.st, x = lerp(SH.x1, SH.x2, st.shape / 100), y = SH.y, hv = w.hk === "shape" || (w.drag && w.drag.id === "shape");
  w.hits.push({ id: "shape", type: "slider", k: "rect", x: SH.x1 - 12, y: y - 18, w: SH.x2 - SH.x1 + 24, h: 36 });
  let s = `<text x="${(SH.x1 + SH.x2) / 2}" y="${y - 22}" text-anchor="middle" ${ANY} font-size="12" font-weight="600" fill="${T.text}">Shape</text>`;
  s += `<path d="M${SH.x1} ${y}H${SH.x2}" stroke="${T.track}" stroke-width="3"/><path d="M${(SH.x1 + SH.x2) / 2} ${y - 6}V${y + 6}" stroke="${T.track}" stroke-width="1.5"/>`;
  s += `<text x="${SH.x1 - 14}" y="${y - 2}" text-anchor="end" ${ANY} font-size="12" font-weight="600" fill="${T.text}">Harmonics</text>`;
  s += `<text x="${SH.x1 - 14}" y="${y + 13}" text-anchor="end" ${MONO} font-size="11" fill="${T.sub}">${100 - Math.round(st.shape)}%</text>`;
  s += `<text x="${SH.x2 + 14}" y="${y - 2}" ${ANY} font-size="12" font-weight="600" fill="${T.text}">Body</text>`;
  s += `<text x="${SH.x2 + 14}" y="${y + 13}" ${MONO} font-size="11" fill="${T.sub}">${Math.round(st.shape)}%</text>`;
  s += `<rect x="${x - 8}" y="${y - 15}" width="16" height="30" rx="8" fill="${T.thumb}" ${T.thumbRing ? `stroke="${T.thumbRing}" stroke-width="1.5"` : ""}/>`;
  s += `<path d="M${x} ${y - 7}V${y + 7}" stroke="${T.thumbMark}" stroke-width="2"/>`;
  if (hv) s += `<rect x="${x - 12}" y="${y - 19}" width="24" height="38" rx="12" fill="none" stroke="${T.line}" stroke-width="1.5"/>`;
  return s;
}
function credit(w) {
  const T = w.T;
  return `<use href="#credit" x="24" y="566" width="54" height="15" fill="${T.sub}"/><text x="86" y="578" ${MONO} font-size="11" fill="${T.sub}">1.0.0</text>`;
}
function amountChrome(w) {
  const T = w.T, st = w.st, hv = w.hk === "amount" || (w.drag && w.drag.id === "amount");
  let s = `<text x="${KN.cx}" y="104" text-anchor="middle" ${ANY} font-size="15" font-weight="700" fill="${T.text}">Amount</text>`;
  const val = pct(st.amount);
  if (hv) s += `<rect x="${KN.cx - 40}" y="${334}" width="80" height="32" fill="${T.chip}"/>`;
  s += `<text x="${KN.cx}" y="358" text-anchor="middle" ${MONO} font-size="22" fill="${hv ? T.chipText : T.text}">${val}</text>`;
  w.hits.push({ id: "amount", type: "knob", k: "circle", cx: KN.cx, cy: KN.cy, r: 106 });
  return s;
}
function header(w) {
  const T = w.T, st = w.st;
  let s = w.dir.wordmark(w);
  s += segmented(w, "character", 472, 12, [["clean", "Clean", 64], ["character", "Character", 96]]);
  s += segmented(w, "quality", 648, 12, [["live", "Live", 52], ["hq", "HQ", 52]]);
  s += `<text x="760" y="32" ${MONO} font-size="11" fill="${T.sub}">${st.quality === "hq" ? "43 ms" : "0 ms"}</text>`;
  s += pill(w, "delta", 820, 12, 72, 32, "Delta", T.deltaFill, T.deltaText);
  s += bypassBtn(w);
  return s;
}
function plotOverlay(w) {
  const T = w.T, st = w.st;
  let s = "";
  // the curve: drawn (points sit on it) and effective (what is applied)
  const pd = Array.from(w.drawn, (v, i) => (i ? "L" : "M") + XS[i].toFixed(1) + " " + cyv(v).toFixed(1)).join("");
  const pe = Array.from(w.eff, (v, i) => (i ? "L" : "M") + XS[i].toFixed(1) + " " + cyv(v).toFixed(1)).join("");
  const dim = st.bypass ? 0.45 : 1;
  if (st.quality === "live") {
    s += `<path d="${pe}" fill="none" stroke="${T.curveUnder}" stroke-width="5" stroke-linejoin="round" opacity="${dim}"/>`;
    s += `<path d="${pd}" fill="none" stroke="${T.curve}" stroke-width="1.25" stroke-dasharray="4 4" opacity="${0.8 * dim}"/>`;
    s += `<path d="${pe}" fill="none" stroke="${T.curve}" stroke-width="2.25" stroke-linejoin="round" opacity="${dim}"/>`;
  } else {
    s += `<path d="${pd}" fill="none" stroke="${T.curveUnder}" stroke-width="5" stroke-linejoin="round" opacity="${dim}"/>`;
    s += `<path d="${pd}" fill="none" stroke="${T.curve}" stroke-width="2" stroke-linejoin="round" opacity="${dim}"/>`;
  }
  // scale on the right
  const lab = (y, t) => { const cw = t.length * 6.6 + 8; return `<rect x="${P.x + P.w - 6 - cw}" y="${y - 11}" width="${cw}" height="15" fill="${T.screen}" opacity=".85"/><text x="${P.x + P.w - 10}" y="${y}" text-anchor="end" ${MONO} font-size="11" fill="${T.screenSub}">${t}</text>`; };
  s += lab(P.y + 16, "200%") + lab(cyv(1) - 7, "100%") + lab(P.y + P.h - 5, "0%");
  // state line, top left
  const note = st.bypass ? "Bypassed" : st.delta ? "Delta: you hear only what's removed" : st.quality === "live" ? "Live: the applied curve is smoothed" : "";
  if (note) { const cw = note.length * 6.6 + 14; s += `<rect x="${P.x + 8}" y="${P.y + 8}" width="${cw}" height="19" fill="${T.screen}" opacity=".9"/><text x="${P.x + 15}" y="${P.y + 21.5}" ${MONO} font-size="11" fill="${T.screenText}">${note}</text>`; }
  // points
  let chip = "";
  st.points.forEach((p, i) => {
    const x = fx(p.f), y = cyv(p.a), sel = w.sel === i, hv = w.hk === "point:" + i, dr = w.drag && w.drag.id === "point" && w.drag.i === i;
    if (sel || hv || dr) {
      const xa = fx(clamp(p.f * Math.pow(2, -p.w / 2), FMIN, FMAX)), xb = fx(clamp(p.f * Math.pow(2, p.w / 2), FMIN, FMAX));
      s += `<path d="M${xa} ${y}H${xb}M${xa} ${y - 5}v10M${xb} ${y - 5}v10" fill="none" stroke="${T.curve}" stroke-width="1.25" opacity=".85"/>`;
    }
    if (dr) s += `<path d="M${x} ${y + 12}V${P.y + P.h}" stroke="${T.curve}" stroke-width="1" stroke-dasharray="2 3" opacity=".7"/>`;
    if (sel || dr) s += `<circle cx="${x}" cy="${y}" r="12" fill="none" stroke="${T.curve}" stroke-width="1.25"/><circle cx="${x}" cy="${y}" r="7" fill="${T.curve}" stroke="${T.curveUnder}" stroke-width="2"/>`;
    else s += `<circle cx="${x}" cy="${y}" r="${hv ? 8.5 : 6.5}" fill="${T.pointFill}" stroke="${T.curve}" stroke-width="2"/>`;
    if (sel || hv || dr) {
      const t = `${fmtF(p.f)}  ${Math.round(p.a * 100)}%  ${p.w.toFixed(1)} oct`, cw = t.length * 6.6 + 14;
      const cx = clamp(x - cw / 2, P.x + 6, P.x + P.w - 6 - cw), ty = y < P.y + 46 ? y + 22 : y - 39;
      chip = `<rect x="${cx}" y="${ty}" width="${cw}" height="19" fill="${T.chip}"/><text x="${cx + 7}" y="${ty + 13.5}" ${MONO} font-size="11" fill="${T.chipText}" xml:space="preserve">${t}</text>`;
    }
  });
  return s + chip;
}
function bgLayer(w) {
  const T = w.T;
  let s = `<rect width="${W}" height="${H}" fill="${T.ground}"/>`;
  s += `<rect x="${D.x}" y="${D.y}" width="${D.w}" height="${D.h}" fill="${T.screen}" ${T.frame ? `stroke="${T.frame}" stroke-width="1.5"` : ""}/>`;
  for (const db of [-72, -48, -24, 0]) s += `<path d="M${P.x} ${dy(db).toFixed(1)}H${P.x + P.w}" stroke="${T.grid}" stroke-width="1" stroke-dasharray="2 4"/>`;
  for (const f of [50, 100, 200, 500, 1000, 2000, 5000, 10000]) {
    const x = fx(f).toFixed(1), major = f === 100 || f === 1000 || f === 10000;
    s += `<path d="M${x} ${P.y}V${P.y + P.h}" stroke="${T.grid}" stroke-width="1" opacity="${major ? 1 : 0.55}"/>`;
    s += `<text x="${x}" y="${D.y + D.h - 7}" text-anchor="middle" ${MONO} font-size="11" fill="${T.screenSub}">${f === 50 ? "50 Hz" : f >= 1000 ? f / 1000 + "k" : f}</text>`;
  }
  s += `<path d="M${P.x} ${P.y + P.h}H${P.x + P.w}" stroke="${T.grid}" stroke-width="1"/>`;
  return s;
}

/* ---------- the window ---------- */
let UID = 0;
class Win {
  constructor(svg, dir, st, opts = {}) {
    this.svg = svg; this.dir = dir; this.T = dir.T; this.st = st; this.still = !!opts.still;
    this.sel = opts.sel ?? null; this.hk = null; this.drag = null; this.hits = []; this.uid = "w" + (++UID); this.visible = false;
    this.red = new Float32Array(NB); this.ver = 0;
    svg.setAttribute("viewBox", `0 0 ${W} ${H}`);
    svg.innerHTML = `<defs>${dir.defs ? dir.defs(this) : ""}<clipPath id="${this.uid}-clip"><rect x="${P.x}" y="${P.y}" width="${P.w}" height="${P.h}"/></clipPath></defs>` +
      `<g class="bg">${bgLayer(this)}</g><g class="spec" clip-path="url(#${this.uid}-clip)"></g><g class="ui"></g>`;
    this.gSpec = svg.querySelector(".spec"); this.gUI = svg.querySelector(".ui");
    dir.specInit(this);
    this.computeCurve(); this.renderUI(); this.drawSpec(SNAP);
    if (!this.still) this.bind();
  }
  computeCurve() {
    const st = this.st, dr = new Float32Array(NB);
    for (let i = 0; i < NB; i++) dr[i] = curveDrawn(st.points, BINF[i]);
    this.drawn = dr;
    if (st.quality === "hq") { this.eff = dr; return; }
    const sg = 0.5 * (NB - 1) / OCT, k = Math.ceil(sg * 3), e = new Float32Array(NB);
    for (let i = 0; i < NB; i++) {
      let s = 0, ws = 0;
      for (let j = -k; j <= k; j++) { const wt = Math.exp(-0.5 * (j / sg) ** 2); s += wt * dr[clamp(i + j, 0, NB - 1)]; ws += wt; }
      e[i] = s / ws;
    }
    this.eff = e;
  }
  reduce(orig) {
    const st = this.st, eff = this.eff, out = this.red;
    if (st.bypass) { out.fill(0); return out; }
    const s = st.shape / 100, amt = st.amount / 100, mx = st.character === "character" ? 36 : 18, mix = st.mix / 100, rs = st.restore / 100;
    for (let i = 0; i < NB; i++) {
      let sm = 0, n = 0;
      for (let j = i - 4; j <= i + 4; j++) if (j >= 0 && j < NB) { sm += orig[j]; n++; }
      const pk = clamp((orig[i] - sm / n) / 6 + 0.5, 0, 1);
      const wgt = (1 - s) * HW[i] * (0.45 + 0.55 * pk) + s * BW[i] * (0.8 + 0.2 * pk);
      out[i] = Math.min(amt * eff[i] * wgt * mx * (1 - 0.55 * rs * pk) * mix, orig[i] - DBMIN);
    }
    return out;
  }
  drawSpec(orig) { this.dir.specDraw(this, orig, this.reduce(orig)); }
  changed() {
    this.ver++; this.computeCurve(); this.renderUI();
    if (this.dir.specStyle) this.dir.specStyle(this);
    if (this.still || !running) this.drawSpec(this.still ? SNAP : sig.lv);
  }
  renderUI() {
    this.hits = [];
    let s = header(this);
    s += amountChrome(this) + this.dir.amountKnob(this);
    for (const [id, label, cx] of SMALL) s += smallKnob(this, id, label, cx);
    s += pill(this, "autoGain", 852, 504, 80, 24, "Auto gain", this.T.onFill, this.T.onText);
    s += shapeSlider(this) + credit(this) + plotOverlay(this);
    this.gUI.innerHTML = s;
  }
  pt(e) { return new DOMPoint(e.clientX, e.clientY).matrixTransform(this.svg.getScreenCTM().inverse()); }
  hitTest(p) {
    for (const h of this.hits) {
      if (h.k === "circle" && Math.hypot(p.x - h.cx, p.y - h.cy) <= h.r) return h;
      if (h.k === "rect" && p.x >= h.x && p.x <= h.x + h.w && p.y >= h.y && p.y <= h.y + h.h) return h;
    }
    if (p.x >= P.x && p.x <= P.x + P.w && p.y >= P.y && p.y <= P.y + P.h) {
      let best = null, bd = 13;
      this.st.points.forEach((q, i) => { const d = Math.hypot(p.x - fx(q.f), p.y - cyv(q.a)); if (d < bd) { bd = d; best = i; } });
      return best != null ? { id: "point", i: best, type: "point" } : { id: "display", type: "display" };
    }
    return null;
  }
  key(h) { return !h ? null : h.id + (h.v ? ":" + h.v : "") + (h.i != null ? ":" + h.i : ""); }
  setShape(x) { this.st.shape = clamp((x - SH.x1) / (SH.x2 - SH.x1) * 100, 0, 100); }
  bind() {
    const svg = this.svg;
    const cursor = h => !h ? "default" : h.type === "knob" ? "ns-resize" : h.type === "slider" ? "ew-resize" : h.type === "point" ? "grab" : h.type === "display" ? (this.st.points.length < 6 ? "crosshair" : "default") : "pointer";
    svg.addEventListener("pointerdown", e => {
      if (e.button !== 0) return;
      this.engaged = true;
      const p = this.pt(e), h = this.hitTest(p); if (!h) return;
      svg.setPointerCapture(e.pointerId);
      const st = this.st;
      if (h.type === "knob") this.drag = { id: h.id, y0: e.clientY, v0: st[h.id] };
      else if (h.type === "slider") { this.drag = { id: "shape" }; this.setShape(p.x); }
      else if (h.type === "seg") st[h.id] = h.v;
      else if (h.type === "toggle") st[h.id] = !st[h.id];
      else if (h.type === "point") { this.sel = h.i; this.drag = { id: "point", i: h.i }; svg.style.cursor = "grabbing"; }
      else if (h.type === "display") this.sel = null;
      this.changed();
    });
    svg.addEventListener("pointermove", e => {
      const p = this.pt(e), st = this.st, d = this.drag;
      if (d) {
        if (d.id === "shape") this.setShape(p.x);
        else if (d.id === "point") { const q = st.points[d.i]; q.f = clamp(xf(p.x), FMIN, FMAX); q.a = clamp(yv(p.y), 0, 2); }
        else { const R = PARAMS[d.id]; st[d.id] = clamp(d.v0 + (d.y0 - e.clientY) / (e.shiftKey ? 1200 : 240) * (R.max - R.min), R.min, R.max); }
        this.changed(); return;
      }
      const h = this.hitTest(p), k = this.key(h);
      if (k !== this.hk) { this.hk = k; this.renderUI(); }
      svg.style.cursor = cursor(h);
    });
    const end = () => { if (this.drag) { this.drag = null; this.changed(); } };
    svg.addEventListener("pointerup", end);
    svg.addEventListener("pointercancel", end);
    svg.addEventListener("pointerleave", () => { this.engaged = false; if (!this.drag && this.hk) { this.hk = null; this.renderUI(); } });
    svg.addEventListener("dblclick", e => {
      const p = this.pt(e), h = this.hitTest(p), st = this.st; if (!h) return;
      if (h.type === "knob" || h.type === "slider") st[h.id] = PARAMS[h.id].def;
      else if (h.type === "point") { st.points.splice(h.i, 1); this.sel = null; this.hk = null; }
      else if (h.type === "display" && st.points.length < 6) { st.points.push({ f: clamp(xf(p.x), FMIN, FMAX), a: clamp(yv(p.y), 0, 2), w: 1 }); this.sel = st.points.length - 1; this.hk = "point:" + this.sel; }
      else return;
      this.changed();
    });
    svg.addEventListener("wheel", e => {
      if (!this.engaged) return;
      const h = this.hitTest(this.pt(e)), st = this.st, sgn = e.deltaY < 0 ? 1 : -1;
      if (h && h.type === "knob") { const R = PARAMS[h.id]; st[h.id] = clamp(Math.round((st[h.id] + sgn * R.step) / R.step) * R.step, R.min, R.max); }
      else if (h && h.type === "point") { const q = st.points[h.i]; q.w = clamp(r1(q.w + sgn * 0.1), 0.3, 4); }
      else return;
      e.preventDefault(); this.changed();
    }, { passive: false });
    svg.addEventListener("keydown", e => {
      const st = this.st, q = this.sel != null ? st.points[this.sel] : null;
      if ((e.key === "Delete" || e.key === "Backspace") && q) { st.points.splice(this.sel, 1); this.sel = null; }
      else if (q && e.key === "ArrowLeft") q.f = clamp(q.f / Math.pow(2, 1 / 12), FMIN, FMAX);
      else if (q && e.key === "ArrowRight") q.f = clamp(q.f * Math.pow(2, 1 / 12), FMIN, FMAX);
      else if (q && e.key === "ArrowUp") q.a = clamp(q.a + 0.05, 0, 2);
      else if (q && e.key === "ArrowDown") q.a = clamp(q.a - 0.05, 0, 2);
      else if (!q && e.key === "ArrowUp") st.amount = clamp(st.amount + 1, 0, 100);
      else if (!q && e.key === "ArrowDown") st.amount = clamp(st.amount - 1, 0, 100);
      else if (e.key === "d" || e.key === "D") st.delta = !st.delta;
      else if (e.key === "b" || e.key === "B") st.bypass = !st.bypass;
      else return;
      e.preventDefault(); this.changed();
    });
  }
}

/* ---------- direction A: Grey card ---------- */
const INK = "#0C0C0B", INK_R = "#1A1A18", INK_L = "#2A2A27", PAPER = "#ECEBE6", PAPER_L = "#D6D4CB",
  GRAPH = "#8A8983", GRAPH_D = "#5F5E59", SIGNAL = "#FF4B1F", KLEIN = "#2E40FF", BUBBLE = "#FF9BD0";

const A = {
  key: "a",
  T: { ground: GRAPH, text: INK, sub: INK, line: INK, screen: INK, grid: INK_L, screenText: PAPER, screenSub: GRAPH,
    onFill: INK, onText: PAPER, deltaFill: SIGNAL, deltaText: INK, curve: PAPER, curveUnder: INK, pointFill: INK,
    knobBody: INK, pointer: PAPER, track: INK, arc: PAPER, chip: PAPER, chipText: INK, thumb: INK, thumbMark: PAPER },
  wordmark(w) {
    const on = !w.st.bypass;
    return `<rect x="24" y="15" width="26" height="26" fill="none" stroke="${INK}" stroke-width="2"/>` +
      `<path d="M50 18A10 10 0 0 1 50 38Z" fill="${on ? SIGNAL : INK}"/><path d="M50 18A10 10 0 0 0 50 38" fill="none" stroke="${INK}" stroke-width="1.5"/>` +
      `<text x="72" y="37" ${ANY} font-size="24" font-weight="800" fill="${INK}" letter-spacing="-0.2">De-S<tspan style="font-stretch:150%">a</tspan>tur<tspan style="font-stretch:150%">a</tspan>tor</text>`;
  },
  amountKnob(w) {
    const st = w.st, { cx, cy } = KN, a = ang(st.amount / 100), hv = w.hk === "amount" || (w.drag && w.drag.id === "amount");
    const [x0, y0] = polar(cx, cy, 34, a), [x1, y1] = polar(cx, cy, 70, a);
    let s = `<path d="${arc(cx, cy, 96, -135, 135)}" fill="none" stroke="${INK}" stroke-width="11"/>`;
    s += `<path d="${arc(cx, cy, 96, -135, a)}" fill="none" stroke="${st.bypass ? GRAPH_D : SIGNAL}" stroke-width="7"/>`;
    s += `<circle cx="${cx}" cy="${cy}" r="80" fill="${INK}"/>`;
    if (hv) s += `<circle cx="${cx}" cy="${cy}" r="75" fill="none" stroke="${PAPER}" stroke-width="${w.drag ? 2 : 1.25}"/>`;
    s += `<path d="M${x0.toFixed(2)} ${y0.toFixed(2)}L${x1.toFixed(2)} ${y1.toFixed(2)}" stroke="${PAPER}" stroke-width="5" stroke-linecap="square"/>`;
    return s;
  },
  specInit(w) {
    w.gSpec.innerHTML = `<path class="f" fill="${PAPER}"/><path class="b" fill="${SIGNAL}"/><path class="g" fill="none" stroke="${PAPER}" stroke-width="1.25" stroke-linejoin="round"/><path class="r" fill="none" stroke="${PAPER}" stroke-width="1.75" stroke-linejoin="round"/>`;
    w.el = { f: w.gSpec.querySelector(".f"), b: w.gSpec.querySelector(".b"), g: w.gSpec.querySelector(".g"), r: w.gSpec.querySelector(".r") };
    A.specStyle(w);
  },
  specStyle(w) {
    const st = w.st, e = w.el;
    e.f.setAttribute("fill-opacity", st.delta && !st.bypass ? 0.03 : 0.07);
    e.b.setAttribute("opacity", st.bypass ? 0 : 1);
    e.g.setAttribute("opacity", st.delta ? 0.3 : 0.5);
    e.r.setAttribute("opacity", st.bypass ? 0.85 : st.delta ? 0.25 : 1);
  },
  specDraw(w, orig, red) {
    const base = (P.y + P.h + 2).toFixed(1);
    let po = "", pr = "", rb = "";
    for (let i = 0; i < NB; i++) {
      const x = XS[i].toFixed(1), yo = dy(orig[i]).toFixed(1), yr = dy(orig[i] - red[i]).toFixed(1);
      po += (i ? "L" : "M") + x + " " + yo; pr += (i ? "L" : "M") + x + " " + yr;
    }
    for (let i = NB - 1; i >= 0; i--) rb += "L" + XS[i].toFixed(1) + " " + dy(orig[i] - red[i]).toFixed(1);
    w.el.f.setAttribute("d", `M${P.x} ${base}` + pr.replace(/^M/, "L") + `L${P.x + P.w} ${base}Z`);
    w.el.b.setAttribute("d", po + rb + "Z");
    w.el.g.setAttribute("d", po);
    w.el.r.setAttribute("d", pr);
  },
};

/* ---------- direction B: Hairline ---------- */
const NL = 120;
const B = {
  key: "b",
  T: { ground: PAPER, text: INK, sub: GRAPH_D, line: INK, screen: PAPER, frame: INK, grid: PAPER_L, screenText: INK, screenSub: GRAPH_D,
    onFill: INK, onText: PAPER, deltaFill: KLEIN, deltaText: PAPER, curve: KLEIN, curveUnder: PAPER, pointFill: PAPER,
    knobBody: PAPER, knobRing: INK, pointer: INK, track: PAPER_L, arc: KLEIN, chip: INK, chipText: PAPER, thumb: INK, thumbMark: PAPER },
  wordmark(w) {
    const L = "De-Saturator".split("");
    const sp = L.map((c, i) => `<tspan font-weight="${Math.round(900 - 700 * i / (L.length - 1))}"${c === "a" ? ' style="font-stretch:150%"' : ""}>${c}</tspan>`).join("");
    return `<text x="24" y="38" ${ANY} font-size="27" fill="${INK}">${sp}</text>`;
  },
  amountKnob(w) {
    const st = w.st, { cx, cy } = KN, n = st.amount / 100, a = ang(n), hv = w.hk === "amount" || (w.drag && w.drag.id === "amount");
    const sw = lerp(44, 1.5, n), col = st.bypass ? GRAPH_D : INK;
    let s = "";
    for (let t = 0; t <= 10; t++) {
      const ta = ang(t / 10), [x0, y0] = polar(cx, cy, 104, ta), [x1, y1] = polar(cx, cy, t % 5 ? 109 : 113, ta);
      s += `<path d="M${x0.toFixed(2)} ${y0.toFixed(2)}L${x1.toFixed(2)} ${y1.toFixed(2)}" stroke="${hv ? INK : GRAPH_D}" stroke-width="1.25"/>`;
    }
    s += `<path d="${arc(cx, cy, 96, -135, 135)}" fill="none" stroke="${PAPER_L}" stroke-width="2"/>`;
    s += `<path d="${arc(cx, cy, 96, -135, a)}" fill="none" stroke="${st.bypass ? GRAPH_D : KLEIN}" stroke-width="2"/>`;
    s += `<circle cx="${cx}" cy="${cy}" r="${84 - sw / 2}" fill="none" stroke="${col}" stroke-width="${sw}"/>`;
    const [px, py] = polar(cx, cy, 96, a);
    s += `<circle cx="${px.toFixed(2)}" cy="${py.toFixed(2)}" r="${w.drag && w.drag.id === "amount" ? 7.5 : 5.5}" fill="${st.bypass ? GRAPH_D : KLEIN}" stroke="${PAPER}" stroke-width="2"/>`;
    return s;
  },
  specInit(w) {
    let k = "", r = "";
    w.bx = []; w.bi = [];
    for (let j = 0; j < NL; j++) {
      const x = P.x + (j + 0.5) * P.w / NL; w.bx.push(x);
      w.bi.push(Math.log2(xf(x) / FMIN) / OCT * (NB - 1));
      k += `<line x1="${x.toFixed(2)}" x2="${x.toFixed(2)}"/>`; r += `<line x1="${x.toFixed(2)}" x2="${x.toFixed(2)}"/>`;
    }
    w.gSpec.innerHTML = `<g class="k">${k}</g><g class="r">${r}</g>`;
    w.gk = w.gSpec.querySelector(".k"); w.gr = w.gSpec.querySelector(".r");
    w.lk = [...w.gk.children]; w.lr = [...w.gr.children];
    B.specStyle(w);
  },
  specStyle(w) {
    const st = w.st, heavy = 2.6, thin = 1;
    const set = (g, c, sw) => { g.setAttribute("stroke", c); g.setAttribute("stroke-width", sw); };
    if (st.bypass) { set(w.gk, INK, heavy); set(w.gr, INK, heavy); }
    else if (st.delta) { set(w.gk, GRAPH_D, thin); set(w.gr, KLEIN, heavy); }
    else { set(w.gk, INK, heavy); set(w.gr, GRAPH_D, thin); }
  },
  specDraw(w, orig, red) {
    const base = P.y + P.h + 2;
    for (let j = 0; j < NL; j++) {
      const fi = w.bi[j], i0 = Math.floor(fi), i1 = Math.min(NB - 1, i0 + 1), t = fi - i0;
      const o = lerp(orig[i0], orig[i1], t), rr = lerp(red[i0], red[i1], t);
      const yo = dy(o), yr = dy(o - rr);
      w.lk[j].setAttribute("y1", base); w.lk[j].setAttribute("y2", yr.toFixed(1));
      w.lr[j].setAttribute("y1", yr.toFixed(1)); w.lr[j].setAttribute("y2", yo.toFixed(1));
    }
  },
};

/* ---------- direction C: Halftone ---------- */
const COLS = 48, ROWS = 26;
const C = {
  key: "c",
  T: { ground: INK, text: PAPER, sub: GRAPH, line: GRAPH, screen: INK_R, grid: INK_L, screenText: PAPER, screenSub: GRAPH,
    onFill: PAPER, onText: INK, deltaFill: BUBBLE, deltaText: INK, curve: PAPER, curveUnder: INK_R, pointFill: INK_R,
    knobBody: INK_R, knobRing: INK_L, pointer: PAPER, track: INK_L, arc: PAPER, chip: PAPER, chipText: INK, thumb: PAPER, thumbMark: INK },
  defs(w) {
    let s = "";
    for (let k = 0; k < 9; k++) s += `<pattern id="${w.uid}-ht${k}" width="3.2" height="3.2" patternUnits="userSpaceOnUse"><circle cx="1.6" cy="1.6" r="${(1.95 - 1.05 * k / 8).toFixed(2)}" fill="${PAPER}"/></pattern>`;
    return s;
  },
  wordmark(w) {
    const L = "Saturator".split("");
    const sp = L.map((c, k) => `<tspan fill="url(#${w.uid}-ht${k})"${c === "a" ? ' style="font-stretch:150%"' : ""}>${c}</tspan>`).join("");
    return `<text x="24" y="38" ${ANY} font-size="27" font-weight="900" fill="${PAPER}">De-${sp}</text>`;
  },
  amountKnob(w) {
    const st = w.st, { cx, cy } = KN, n = st.amount / 100, a = ang(n), hv = w.hk === "amount" || (w.drag && w.drag.id === "amount");
    let s = `<path d="${arc(cx, cy, 97, -135, 135)}" fill="none" stroke="${INK_L}" stroke-width="7"/>`;
    s += `<path d="${arc(cx, cy, 97, -135, a)}" fill="none" stroke="${st.bypass ? GRAPH_D : BUBBLE}" stroke-width="7"/>`;
    s += `<circle cx="${cx}" cy="${cy}" r="84" fill="${INK_R}" stroke="${hv ? GRAPH : INK_L}" stroke-width="1.5"/>`;
    const rd = lerp(4.1, 0.8, n), sp = 9.2;
    for (let row = -9; row <= 9; row++) for (let col = -10; col <= 10; col++) {
      const x = col * sp + (row & 1 ? sp / 2 : 0), y = row * sp * 0.866;
      if (x * x + y * y > 74 * 74) continue;
      s += `<circle cx="${(cx + x).toFixed(1)}" cy="${(cy + y).toFixed(1)}" r="${rd.toFixed(2)}" fill="${PAPER}"/>`;
    }
    const [x0, y0] = polar(cx, cy, 56, a), [x1, y1] = polar(cx, cy, 84, a);
    s += `<path d="M${x0.toFixed(2)} ${y0.toFixed(2)}L${x1.toFixed(2)} ${y1.toFixed(2)}" stroke="${INK}" stroke-width="10"/>`;
    s += `<path d="M${x0.toFixed(2)} ${y0.toFixed(2)}L${x1.toFixed(2)} ${y1.toFixed(2)}" stroke="${PAPER}" stroke-width="4"/>`;
    return s;
  },
  specInit(w) {
    const cw = P.w / COLS, ch = P.h / ROWS; w.R = Math.min(cw, ch) * 0.42;
    let s = "";
    w.ci = [];
    for (let c = 0; c < COLS; c++) {
      const xa = P.x + c * cw, xb = xa + cw;
      w.ci.push([Math.max(0, Math.floor(Math.log2(xf(xa) / FMIN) / OCT * (NB - 1))), Math.min(NB - 1, Math.ceil(Math.log2(xf(xb) / FMIN) / OCT * (NB - 1)))]);
      for (let r = 0; r < ROWS; r++) s += `<circle cx="${(xa + cw / 2).toFixed(1)}" cy="${(P.y + P.h - (r + 0.5) * ch).toFixed(1)}" r="1" class="e"/>`;
    }
    w.gSpec.innerHTML = `<style>#${w.svg.id} .k{fill:${PAPER}}#${w.svg.id} .x{fill:${BUBBLE}}#${w.svg.id} .e{fill:${INK_L}}#${w.svg.id}.dl .k{fill:${GRAPH_D}}</style><g>${s}</g>`;
    w.dots = [...w.gSpec.querySelectorAll("circle")];
    w.dc = new Array(w.dots.length).fill(""); w.dr = new Float32Array(w.dots.length);
    C.specStyle(w);
  },
  specStyle(w) { w.svg.classList.toggle("dl", !!(w.st.delta && !w.st.bypass)); },
  specDraw(w, orig, red) {
    const st = w.st, R = w.R, delta = st.delta && !st.bypass;
    for (let c = 0; c < COLS; c++) {
      const [i0, i1] = w.ci[c];
      let o = DBMIN, rr = 0;
      for (let i = i0; i <= i1; i++) if (orig[i] > o) { o = orig[i]; rr = red[i]; }
      const ro = (o - DBMIN) / (DBMAX - DBMIN) * ROWS, rk = (o - rr - DBMIN) / (DBMAX - DBMIN) * ROWS;
      for (let r = 0; r < ROWS; r++) {
        const idx = c * ROWS + r, kf = clamp(rk - r, 0, 1), of = clamp(ro - r, 0, 1);
        let cls, rad;
        if (kf > 0.04) { cls = "k"; rad = delta ? 1.6 : R * Math.sqrt(kf); }
        else if (of > 0.04) { cls = "x"; rad = R * Math.sqrt(of) * (delta ? 1 : 0.92); }
        else { cls = "e"; rad = 1.1; }
        if (cls !== w.dc[idx]) { w.dots[idx].setAttribute("class", cls); w.dc[idx] = cls; }
        if (Math.abs(rad - w.dr[idx]) > 0.05) { w.dots[idx].setAttribute("r", rad.toFixed(2)); w.dr[idx] = rad; }
      }
    }
  },
};
const DIRS = { a: A, b: B, c: C };

/* ---------- marks and lockups for the parts ---------- */
function markA(card, ghost) {
  return `<rect x="6" y="14" width="72" height="72" fill="${card}"/><path d="M78 20A30 30 0 0 1 78 80Z" fill="${SIGNAL}"/><path d="M78 20A30 30 0 0 0 78 80" fill="none" stroke="${ghost}" stroke-width="2.5"/>`;
}
function sinePoly() {
  const up = [], dn = [];
  for (let i = 0; i <= 120; i++) {
    const x = i / 120 * 100, t = lerp(15, 1.4, i / 120), y = 30 - 19 * Math.sin(2 * Math.PI * x / 100);
    const dydx = -19 * Math.cos(2 * Math.PI * x / 100) * 2 * Math.PI / 100, l = Math.hypot(1, dydx), nx = -dydx / l, ny = 1 / l;
    up.push(`${(x - nx * t / 2).toFixed(2)} ${(y - ny * t / 2).toFixed(2)}`); dn.unshift(`${(x + nx * t / 2).toFixed(2)} ${(y + ny * t / 2).toFixed(2)}`);
  }
  return "M" + up.join("L") + "L" + dn.join("L") + "Z";
}
const SINE = sinePoly();
function markC(solid) {
  let s = "";
  const sp = 7.6;
  for (let row = -7; row <= 7; row++) for (let col = -7; col <= 7; col++) {
    const x = 50 + col * sp + (row & 1 ? sp / 2 : 0), y = 50 + row * sp * 0.866;
    if ((x - 50) ** 2 + (y - 50) ** 2 > 45 * 45) continue;
    const t = clamp((x - 6) / 88, 0, 1), r = lerp(4.3, 0.55, t);
    s += `<circle cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" r="${r.toFixed(2)}" fill="${x < 50 ? solid : BUBBLE}"/>`;
  }
  return s;
}
const tile = (bg, vb, inner, h = 96, label = "") => `<svg viewBox="${vb}" height="${h}" role="img" aria-label="${label}" style="background:${bg};padding:12px">${inner}</svg>`;
function parts() {
  const put = (id, html) => { const el = document.getElementById(id); if (el) el.innerHTML = html; };
  put("marks-a", tile(PAPER, "0 0 116 100", markA(GRAPH, INK), 96, "Grey card mark on paper") + tile(INK, "0 0 116 100", markA(GRAPH, INK), 96, "Grey card mark on ink") + tile(GRAPH, "0 0 116 100", markA(GRAPH, INK).replace(`fill="${GRAPH}"`, `fill="none" stroke="${INK}" stroke-width="3"`), 96, "Grey card mark on the window") + tile(INK, "0 0 116 100", markA(GRAPH, INK), 28, "Small"));
  put("lockup-a", `<svg viewBox="0 0 560 104" role="img" aria-label="De-Saturator lockup, Grey card" style="background:${GRAPH}"><g transform="translate(16 4)">${markA("none", INK).replace('fill="none"/>', `fill="none" stroke="${INK}" stroke-width="3"/>`)}</g><text x="150" y="68" ${ANY} font-size="48" font-weight="800" fill="${INK}">De-S<tspan style="font-stretch:150%">a</tspan>tur<tspan style="font-stretch:150%">a</tspan>tor</text></svg>`);
  put("marks-b", tile(PAPER, "-10 0 120 60", `<path d="${SINE}" fill="${INK}"/>`, 64, "Hairline mark on paper") + tile(INK, "-10 0 120 60", `<path d="${SINE}" fill="${PAPER}"/>`, 64, "Hairline mark on ink") + tile(KLEIN, "-10 0 120 60", `<path d="${SINE}" fill="${PAPER}"/>`, 64, "Hairline mark on klein") + tile(PAPER, "-10 0 120 60", `<path d="${SINE}" fill="${INK}"/>`, 20, "Small"));
  const Lb = "De-Saturator".split("").map((c, i) => `<tspan font-weight="${Math.round(900 - 700 * i / 11)}"${c === "a" ? ' style="font-stretch:150%"' : ""}>${c}</tspan>`).join("");
  put("lockup-b", `<svg viewBox="0 0 560 104" role="img" aria-label="De-Saturator wordmark, Hairline" style="background:${PAPER}"><text x="20" y="70" ${ANY} font-size="56" fill="${INK}">${Lb}</text></svg>`);
  put("marks-c", tile(INK, "0 0 100 100", markC(PAPER), 96, "Halftone mark on ink") + tile(PAPER, "0 0 100 100", markC(INK), 96, "Halftone mark on paper") + tile(INK_R, "0 0 100 100", markC(PAPER), 96, "Halftone mark on ink raised") + tile(INK, "0 0 100 100", markC(PAPER), 28, "Small"));
  let pat = "";
  for (let k = 0; k < 9; k++) pat += `<pattern id="lc-ht${k}" width="5" height="5" patternUnits="userSpaceOnUse"><circle cx="2.5" cy="2.5" r="${(3 - 1.6 * k / 8).toFixed(2)}" fill="${PAPER}"/></pattern>`;
  const Lc = "Saturator".split("").map((c, k) => `<tspan fill="url(#lc-ht${k})"${c === "a" ? ' style="font-stretch:150%"' : ""}>${c}</tspan>`).join("");
  put("lockup-c", `<svg viewBox="0 0 560 104" role="img" aria-label="De-Saturator wordmark, Halftone" style="background:${INK}"><defs>${pat}</defs><text x="20" y="72" ${ANY} font-size="58" font-weight="900" fill="${PAPER}">De-${Lc}</text></svg>`);
}

/* ---------- boot ---------- */
const demo = () => ({
  amount: 55, shape: 40, restore: 15, mix: 100, output: -1.5, autoGain: true, delta: false, character: "clean", quality: "hq", bypass: false,
  points: [{ f: 140, a: 0.35, w: 1.4 }, { f: 3300, a: 1.75, w: 0.7 }, { f: 9500, a: 1.35, w: 0.5 }],
});
const reduced = matchMedia("(prefers-reduced-motion: reduce)").matches;
let running = !reduced;
const live = [];
function boot() {
  for (const k of ["a", "b", "c"]) {
    const svg = document.getElementById("win-" + k);
    if (svg) live.push(new Win(svg, DIRS[k], demo(), { sel: 1 }));
    const stills = [["delta", s => { s.delta = true; }], ["live", s => { s.quality = "live"; }], ["character", s => { s.character = "character"; }], ["bypass", s => { s.bypass = true; }]];
    for (const [name, f] of stills) {
      const el = document.getElementById(`still-${k}-${name}`);
      if (!el) continue;
      const st = demo(); f(st);
      new Win(el, DIRS[k], st, { still: true, sel: name === "live" ? 1 : null });
    }
  }
  parts();
  const io = new IntersectionObserver(es => es.forEach(e => { const w = live.find(x => x.svg === e.target); if (w) w.visible = e.isIntersecting; }));
  live.forEach(w => io.observe(w.svg));
  const btn = document.getElementById("run");
  if (btn) {
    const label = () => { btn.textContent = running ? "Pause the analysers" : "Run the analysers"; btn.setAttribute("aria-pressed", String(running)); };
    label();
    btn.addEventListener("click", () => { running = !running; label(); });
  }
  let last = performance.now(), n = 0;
  const frame = now => {
    const dt = Math.min(0.05, (now - last) / 1000); last = now; n++;
    if (running && live.some(w => w.visible)) {
      sig.step(dt);
      for (const w of live) if (w.visible && !(w.dir.key === "c" && n & 1)) w.drawSpec(sig.lv);
    }
    requestAnimationFrame(frame);
  };
  requestAnimationFrame(frame);
}
(document.fonts && document.fonts.ready ? document.fonts.ready : Promise.resolve()).then(boot);
})();
