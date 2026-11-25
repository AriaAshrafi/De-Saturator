// Renders SVG to PNG in Chrome, so every PNG comes from a real browser engine.
//   node render.mjs <jobs.json> ...
// jobs: [{ svg, png, width, background }], paths relative to design/. A null background stays transparent.
import { existsSync, mkdirSync, readFileSync } from "node:fs";
import { createRequire } from "node:module";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = join(HERE, "..");
const { chromium } = createRequire(process.env.DESAT_PLAYWRIGHT_FROM ?? "C:/Users/AriaA/Desktop/All/Other/Work/Kareone/UI-UX/Archive/Pendar/ui/package.json")("playwright");

const font = (f) => (existsSync(join(ROOT, "fonts", f)) ? readFileSync(join(ROOT, "fonts", f)).toString("base64") : "");
const FONTS = `@font-face{font-family:Anybody;src:url(data:font/ttf;base64,${font("Anybody-Variable.ttf")});font-weight:100 900;font-stretch:50% 150%}
@font-face{font-family:'Departure Mono';src:url(data:font/otf;base64,${font("DepartureMono-Regular.otf")})}`;

const browser = await chromium.launch({ headless: true, channel: "chrome" });
const page = await browser.newPage({ deviceScaleFactor: 1 });
await page.setContent(`<!doctype html><html><head><style>${FONTS}</style></head><body></body></html>`);
let n = 0;
for (const file of process.argv.slice(2)) {
  for (const j of JSON.parse(readFileSync(resolve(file), "utf8"))) {
    const svg = readFileSync(join(ROOT, j.svg), "utf8");
    const vb = /viewBox="([\d.\s-]+)"/.exec(svg)[1].trim().split(/\s+/).map(Number);
    const width = j.width, height = Math.max(1, Math.round((width * vb[3]) / vb[2]));
    await page.setViewportSize({ width, height });
    const sized = svg.replace(/<svg([^>]*?)\swidth="[^"]*"/, "<svg$1").replace(/<svg([^>]*?)\sheight="[^"]*"/, "<svg$1").replace("<svg", `<svg width="${width}" height="${height}" style="display:block"`);
    await page.evaluate(([html, bg]) => { document.body.innerHTML = html; document.documentElement.style.cssText = `margin:0;background:${bg}`; document.body.style.cssText = `margin:0;background:${bg}`; }, [sized, j.background ?? "transparent"]);
    await page.evaluate(() => document.fonts.ready);
    const out = join(ROOT, j.png);
    mkdirSync(dirname(out), { recursive: true });
    await page.screenshot({ path: out, omitBackground: j.background == null, clip: { x: 0, y: 0, width, height } });
    n++;
  }
}
await browser.close();
console.log(`rendered ${n} PNGs`);
