// Store and social pictures: the window in use beside the name and the line that sells it.
//   node store.mjs   ->  store/*.png (each at 1x and @2x)
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { createRequire } from "node:module";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = join(HERE, "..");
const require = createRequire(import.meta.url);
const DS = require("./ui/core.js");
const { chromium } = createRequire(process.env.DESAT_PLAYWRIGHT_FROM ?? "C:/Users/AriaA/Desktop/All/Other/Work/Kareone/UI-UX/Archive/Pendar/ui/package.json")("playwright");
const read = (p) => readFileSync(join(ROOT, p), "utf8");
DS.assets.wordmark = JSON.parse(read("logo/wordmark-ui.json"));
DS.assets.credit = JSON.parse(read("logo/credit.json"));

const SNAP = DS.snapshot();
const pts = [{ f: 140, a: 0.35, w: 1.4 }, { f: 3300, a: 1.75, w: 0.7 }, { f: 9500, a: 1.35, w: 0.5 }];
const win = (theme, width) => DS.render(Object.assign(DS.defaults(), { amount: 62, shape: 40, restore: 15, output: -1.5, points: pts, theme }), { sel: 1 }, SNAP, { width, id: `s${width}${theme}` }).svg;
const svgFile = (p, width) => read(p).replace(/<title>.*?<\/title>/, "").replace(/\swidth="[^"]*"/, ` width="${width}"`).replace(/\sheight="[^"]*"/, "");
const LINE = "One knob that makes it thinner.";
const SUB = "Free. VST3 and AU, for Windows and macOS.";
const C = DS.COLOR;

const PIECES = [
  { slug: "hero", name: "Hero", w: 1600, h: 900, desc: "For the product page: the name, the line, and the window in use.", html: (t) => `
    <div style="position:absolute;left:96px;top:290px;width:560px;display:grid;gap:22px">${svgFile(`logo/de-saturator-wordmark-${t.ink}.svg`, 560)}
      <p style="font:600 34px/1.2 Anybody;color:${t.fg};margin:0">${LINE}</p><p style="font:500 20px/1.4 Anybody;color:${t.muted};margin:0">${SUB}</p></div>
    <div style="position:absolute;left:712px;top:194px;box-shadow:6px 6px 0 ${t.fg};border:1.5px solid ${t.fg}">${win(t.theme, 800)}</div>` },
  { slug: "square", name: "Square", w: 1080, h: 1080, desc: "For social posts.", html: (t) => `
    <div style="position:absolute;left:72px;top:96px">${svgFile(`logo/de-saturator-wordmark-${t.ink}.svg`, 620)}</div>
    <p style="position:absolute;left:72px;top:196px;font:600 32px/1.2 Anybody;color:${t.fg};margin:0">${LINE}</p>
    <div style="position:absolute;left:72px;top:300px;box-shadow:6px 6px 0 ${t.fg};border:1.5px solid ${t.fg}">${win(t.theme, 930)}</div>
    <p style="position:absolute;left:72px;top:932px;font:500 20px/1.4 Anybody;color:${t.muted};margin:0">${SUB}</p>` },
  { slug: "github-social", name: "GitHub social preview", w: 1280, h: 640, desc: "The repository's social preview, 1280 × 640.", html: (t) => `
    <div style="position:absolute;left:72px;top:214px;width:440px;display:grid;gap:18px">${svgFile(`logo/de-saturator-wordmark-${t.ink}.svg`, 440)}
      <p style="font:600 26px/1.25 Anybody;color:${t.fg};margin:0">${LINE}</p><p style="font:500 17px/1.4 Anybody;color:${t.muted};margin:0">${SUB}</p></div>
    <div style="position:absolute;left:568px;top:98px;box-shadow:6px 6px 0 ${t.fg};border:1.5px solid ${t.fg}">${win(t.theme, 660)}</div>` },
];
const THEMES = {
  light: { theme: "light", bg: C.paper, fg: C.ink, muted: C.graphiteDeep, ink: "ink" },
  dark: { theme: "dark", bg: C.ink, fg: C.paper, muted: C.graphite, ink: "paper" },
};

const font = (f) => readFileSync(join(ROOT, "fonts", f)).toString("base64");
const CSS = `@font-face{font-family:Anybody;src:url(data:font/ttf;base64,${font("Anybody-Variable.ttf")});font-weight:100 900;font-stretch:50% 150%}
@font-face{font-family:'Departure Mono';src:url(data:font/otf;base64,${font("DepartureMono-Regular.otf")})}
html,body{margin:0}.piece{position:relative;overflow:hidden}.piece svg{display:block}`;
const jobs = [];
for (const p of PIECES) for (const [tn, t] of Object.entries(THEMES)) jobs.push({ id: `${p.slug}-${tn}`, p, t });
const html = `<!doctype html><html><head><meta charset="utf-8"><style>${CSS}</style></head><body>${jobs.map(({ id, p, t }) => `<div class="piece" id="${id}" style="width:${p.w}px;height:${p.h}px;background:${t.bg}">${p.html(t)}</div>`).join("")}</body></html>`;

mkdirSync(join(ROOT, "store"), { recursive: true });
const browser = await chromium.launch({ headless: true, channel: "chrome" });
for (const dpr of [1, 2]) {
  const page = await browser.newPage({ viewport: { width: 1700, height: 1200 }, deviceScaleFactor: dpr });
  await page.setContent(html);
  await page.evaluate(() => document.fonts.ready);
  for (const { id } of jobs) await (await page.$(`#${id}`)).screenshot({ path: join(ROOT, "store", `${id}${dpr === 2 ? "@2x" : ""}.png`) });
  await page.close();
}
await browser.close();
const man = [];
for (const { id, p, t } of jobs) {
  man.push({ collection: "store", file: `store/${id}.png`, slug: id, name: `${p.name}, ${t.theme}`, description: `${p.desc} ${p.w} × ${p.h}.`, tags: ["store", t.theme] });
  man.push({ collection: "store", file: `store/${id}@2x.png`, slug: `${id}-2x`, name: `${p.name}, ${t.theme} (2x)`, description: `${p.desc} ${p.w * 2} × ${p.h * 2}.`, tags: ["store", t.theme, "2x"] });
}
mkdirSync(join(HERE, "manifest"), { recursive: true });
writeFileSync(join(HERE, "manifest", "store.json"), JSON.stringify(man, null, 1));
console.log(`store: ${jobs.length} pictures at 1x and 2x`);
