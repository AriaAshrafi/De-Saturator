// Builds prototype/prototype.html: one file with the fonts, core.js, the outlined name and app.js inside.
//   node prototype.mjs
import { readFileSync, mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = join(HERE, "..");
const PARENT = process.env.DESAT_PARENT_BRAND ?? "C:/Users/AriaA/Desktop/All/Other/Projects/AriaAshrafi Site/design/brand";
const b64 = (p) => readFileSync(p).toString("base64");
const assets = `DS.assets.wordmark = ${readFileSync(join(ROOT, "logo/wordmark-ui.json"), "utf8")};\nDS.assets.credit = ${readFileSync(join(ROOT, "logo/credit.json"), "utf8")};`;
let html = readFileSync(join(HERE, "ui/prototype.template.html"), "utf8");
for (const [k, v] of Object.entries({
  "%%ANYBODY%%": b64(join(PARENT, "dist/fonts/Anybody-Variable.woff2")),
  "%%DEPARTURE%%": b64(join(PARENT, "dist/fonts/DepartureMono-Regular.woff2")),
  "%%CORE%%": readFileSync(join(HERE, "ui/core.js"), "utf8"),
  "%%ASSETS%%": assets,
  "%%APP%%": readFileSync(join(HERE, "ui/app.js"), "utf8"),
})) html = html.split(k).join(v);
mkdirSync(join(ROOT, "prototype"), { recursive: true });
writeFileSync(join(ROOT, "prototype/prototype.html"), html);
mkdirSync(join(HERE, "manifest"), { recursive: true });
writeFileSync(join(HERE, "manifest", "prototype.json"), JSON.stringify([{ collection: "ui-handoff", file: "prototype/prototype.html", slug: "prototype", name: "Prototype (HTML)", description: "The window wired to the spec: open it in a browser. The analyser runs on a stand-in signal.", tags: ["prototype"] }], null, 1));
console.log("prototype/prototype.html", (html.length / 1e6).toFixed(2), "MB");
