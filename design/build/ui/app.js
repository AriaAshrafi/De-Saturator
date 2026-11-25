/* The prototype: the window from core.js, wired up. Pointer, wheel and keys behave as the spec says;
 * the analyser runs on the stand-in signal. Nothing plays. */
(function () {
"use strict";
const DS = window.DS, L = DS.L, P = L.plot, NS = "http://www.w3.org/2000/svg";
const clamp = DS.clamp;
let running = !matchMedia("(prefers-reduced-motion: reduce)").matches;

const pts = () => [{ f: 140, a: 0.35, w: 1.4 }, { f: 3300, a: 1.75, w: 0.7 }, { f: 9500, a: 1.35, w: 0.5 }];
const work = (o) => Object.assign(DS.defaults(), { amount: 55, shape: 40, restore: 15, output: -1.5, points: pts() }, o || {});
const PRESETS = [
  ["Default", () => [DS.defaults(), {}]],
  ["In use", () => [work(), {}]],
  ["Point selected", () => [work(), { sel: 1 }]],
  ["Six points", () => [work({ points: [{ f: 60, a: 0.2, w: 1.2 }, { f: 220, a: 0.6, w: 1 }, { f: 900, a: 1.25, w: 1 }, { f: 2600, a: 1.8, w: 0.8 }, { f: 6000, a: 1.4, w: 0.7 }, { f: 13000, a: 0.9, w: 0.9 }] }), {}]],
  ["Delta on", () => [work({ delta: true }), {}]],
  ["Character", () => [work({ character: "character" }), {}]],
  ["Live", () => [work({ quality: "live" }), { sel: 1 }]],
  ["Bypassed", () => [work({ bypass: true }), {}]],
];

class Win {
  constructor(svg) {
    this.svg = svg; this.sig = new DS.Signal(11); this.silent = false; this.red = new Float64Array(DS.NB);
    this.st = work(); this.ui = { hover: null, drag: null, sel: 1, note: null, tip: null, tipAt: null };
    svg.setAttribute("viewBox", `0 0 ${DS.W} ${DS.H}`);
    svg.innerHTML = `<defs><clipPath id="plot-clip"><rect x="${P.x}" y="${P.y}" width="${P.w}" height="${P.h}"/></clipPath></defs>` +
      `<g class="bg"></g><g clip-path="url(#plot-clip)"><g class="k"></g><g class="r"></g><path class="o" fill="none" stroke-linejoin="round"/><path class="s" fill="none" stroke-linejoin="round"/></g><g class="ui"></g>`;
    this.gBg = svg.querySelector(".bg"); this.gk = svg.querySelector(".k"); this.gr = svg.querySelector(".r"); this.po = svg.querySelector(".o"); this.ps = svg.querySelector(".s"); this.gUI = svg.querySelector(".ui");
    this.lk = []; this.lr = [];
    for (const x of DS.COMB_X) {
      for (const [g, arr] of [[this.gk, this.lk], [this.gr, this.lr]]) {
        const l = document.createElementNS(NS, "line"); l.setAttribute("x1", x.toFixed(2)); l.setAttribute("x2", x.toFixed(2));
        l.setAttribute("y1", DS.COMB_BASE); l.setAttribute("y2", DS.COMB_BASE); g.appendChild(l); arr.push(l);
      }
    }
    for (let i = 0; i < 120; i++) this.sig.step(1 / 60);
    this.changed();
    this.bind();
  }
  load(st, ui) { this.st = st; this.ui = Object.assign({ hover: null, drag: null, sel: null, note: null, tip: null, tipAt: null }, ui); this.changed(); }
  changed() {
    this.ctx = DS.context(this.st, this.ui);
    if (this.theme !== this.st.theme) {
      this.theme = this.st.theme;
      this.gBg.innerHTML = DS.background();
      this.svg.parentElement.style.background = DS.T.ground;
    }
    this.gUI.innerHTML = DS.ui(this.ctx);
    const sty = DS.combStyle(this.st);
    const set = (el, s, op) => { if (!s) { el.setAttribute("visibility", "hidden"); return; } el.removeAttribute("visibility"); el.setAttribute("stroke", s[0]); el.setAttribute("stroke-width", s[1]); if (op) el.setAttribute("stroke-opacity", op); };
    set(this.gk, sty.keep, DS.ALPHA.body); set(this.gr, sty.removed); set(this.po, sty.original); set(this.ps, sty.result);
    if (!running) this.draw();
  }
  draw() {
    const orig = this.silent ? DS.SILENCE : this.sig.lv, red = DS.reduce(this.st, this.ctx.applied, orig, this.red), c = DS.comb(orig, red);
    for (let j = 0; j < this.lk.length; j++) {
      this.lk[j].setAttribute("y2", c.keep[j].toFixed(1));
      this.lr[j].setAttribute("y1", c.keep[j].toFixed(1)); this.lr[j].setAttribute("y2", c.top[j].toFixed(1));
    }
    this.po.setAttribute("d", DS.contour(orig)); this.ps.setAttribute("d", DS.contour(orig, red));
  }
  pt(e) { return new DOMPoint(e.clientX, e.clientY).matrixTransform(this.svg.getScreenCTM().inverse()); }
  hover(h, p) {
    const k = h ? h.key || h.id : null;
    if (k === this.ui.hover) return;
    clearTimeout(this.tipTimer);
    this.ui.hover = k; this.ui.tip = null;
    if (k && !this.ui.drag) this.tipTimer = setTimeout(() => { this.ui.tip = k.startsWith("point:") ? k : k; this.ui.tipAt = [p.x, p.y]; this.changed(); }, L.tooltip.delayMs);
    this.changed();
  }
  note(t) { this.ui.note = t; clearTimeout(this.noteTimer); this.noteTimer = setTimeout(() => { this.ui.note = null; this.changed(); }, 2000); }
  bind() {
    const svg = this.svg, st = () => this.st, ui = this.ui;
    const cursor = h => !h ? "default" : h.type === "knob" ? "ns-resize" : h.type === "slider" ? "ew-resize" : h.type === "point" ? "grab" : h.type === "display" ? (this.st.points.length < DS.CURVE.maxPoints ? "crosshair" : "default") : "pointer";
    const setShape = x => { this.st.shape = clamp((x - L.shape.x1) / (L.shape.x2 - L.shape.x1) * 100, 0, 100); };
    svg.addEventListener("pointerdown", e => {
      if (e.button !== 0) return;
      this.engaged = true; clearTimeout(this.tipTimer); this.ui.tip = null;
      const p = this.pt(e), h = DS.hitTest(this.ctx, p), s = this.st; if (!h) return;
      svg.setPointerCapture(e.pointerId);
      if (h.type === "knob") this.ui.drag = { id: h.id, y0: e.clientY, v0: s[h.id] };
      else if (h.type === "slider") { this.ui.drag = { id: "shape" }; setShape(p.x); }
      else if (h.type === "choice") s[h.id] = h.v;
      else if (h.type === "toggle") s[h.id] = !s[h.id];
      else if (h.type === "point") { this.ui.sel = h.i; this.ui.drag = { id: "point", i: h.i }; svg.style.cursor = "grabbing"; }
      else if (h.type === "display") this.ui.sel = null;
      this.changed();
    });
    svg.addEventListener("pointermove", e => {
      const p = this.pt(e), s = this.st, d = this.ui.drag;
      if (d) {
        if (d.id === "shape") setShape(p.x);
        else if (d.id === "point") { const q = s.points[d.i]; q.f = clamp(DS.xf(p.x), DS.FMIN, DS.FMAX); q.a = clamp(DS.yv(p.y), 0, 2); }
        else { const R = DS.PM[d.id]; s[d.id] = clamp(d.v0 + (d.y0 - e.clientY) / (e.shiftKey ? DS.DRAG.fineRangePx : DS.DRAG.fullRangePx) * (R.max - R.min), R.min, R.max); }
        this.changed(); return;
      }
      const h = DS.hitTest(this.ctx, p);
      this.hover(h, p);
      svg.style.cursor = cursor(h);
    });
    const end = () => { if (this.ui.drag) { this.ui.drag = null; this.changed(); } };
    svg.addEventListener("pointerup", end);
    svg.addEventListener("pointercancel", end);
    svg.addEventListener("pointerleave", () => { this.engaged = false; if (!this.ui.drag) this.hover(null); });
    svg.addEventListener("dblclick", e => {
      const p = this.pt(e), h = DS.hitTest(this.ctx, p), s = this.st; if (!h) return;
      if (h.type === "knob" || h.type === "slider") s[h.id] = DS.PM[h.id].def;
      else if (h.type === "point") { s.points.splice(h.i, 1); this.ui.sel = null; this.ui.hover = null; }
      else if (h.type === "display") {
        if (s.points.length >= DS.CURVE.maxPoints) this.note(DS.NOTES.full);
        else { s.points.push({ f: clamp(DS.xf(p.x), DS.FMIN, DS.FMAX), a: clamp(DS.yv(p.y), 0, 2), w: DS.CURVE.widthDefault }); this.ui.sel = s.points.length - 1; this.ui.hover = "point:" + this.ui.sel; }
      } else return;
      this.changed();
    });
    svg.addEventListener("wheel", e => {
      if (!this.engaged) return;
      const h = DS.hitTest(this.ctx, this.pt(e)), s = this.st, sgn = e.deltaY < 0 ? 1 : -1;
      if (h && h.type === "knob") { const R = DS.PM[h.id]; s[h.id] = clamp(Math.round((s[h.id] + sgn * R.step) / R.step) * R.step, R.min, R.max); }
      else if (h && h.type === "point") { const q = s.points[h.i]; q.w = clamp(Math.round((q.w + sgn * DS.CURVE.widthStep) * 10) / 10, DS.CURVE.width[0], DS.CURVE.width[1]); }
      else return;
      e.preventDefault(); this.changed();
    }, { passive: false });
    // No keyboard handling at all: the window never takes keys from the host.
  }
}

function boot() {
  const svg = document.getElementById("window");
  const w = new Win(svg);
  const presets = document.getElementById("presets");
  for (const [name, make] of PRESETS) {
    const b = document.createElement("button"); b.type = "button"; b.textContent = name;
    b.addEventListener("click", () => { const [st, ui] = make(); st.theme = w.st.theme; w.load(st, ui); });
    presets.appendChild(b);
  }
  const frame = document.getElementById("frame");
  document.querySelectorAll("[data-scale]").forEach(b => b.addEventListener("click", () => {
    frame.style.width = `${DS.W * Number(b.dataset.scale)}px`;
    document.querySelectorAll("[data-scale]").forEach(x => x.setAttribute("aria-pressed", String(x === b)));
  }));
  const run = document.getElementById("run"), silent = document.getElementById("silent");
  const label = () => { run.textContent = running ? "Pause the analyser" : "Run the analyser"; run.setAttribute("aria-pressed", String(running)); silent.setAttribute("aria-pressed", String(w.silent)); };
  run.addEventListener("click", () => { running = !running; label(); if (!running) w.draw(); });
  silent.addEventListener("click", () => { w.silent = !w.silent; label(); w.draw(); });
  label();
  let last = performance.now();
  const tick = now => {
    const dt = Math.min(0.05, (now - last) / 1000); last = now;
    if (running && !document.hidden) { w.sig.step(dt); w.draw(); }
    requestAnimationFrame(tick);
  };
  w.draw();
  requestAnimationFrame(tick);
}
(document.fonts && document.fonts.ready ? document.fonts.ready : Promise.resolve()).then(boot);
})();
