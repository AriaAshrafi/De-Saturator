// Loads De-Saturator into Specimen through its API, as a sub-brand of aria-ashrafi: the brand, its
// collections and files, its tokens and its guideline pages. Safe to run again: it adds what's missing,
// uploads a new version only when a file changed, and publishes a page only when its text changed.
//
//   node load.mjs            load
//   node load.mjs --check    only list what would change
//
// Reads the API token from ~/.specimen-token. It is never printed.
import { createHash } from "node:crypto";
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { homedir } from "node:os";
import { basename, dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = join(HERE, "..");
const BASE = process.env.SPECIMEN_URL ?? "https://specimen.ariaashrafi.com";
const CHECK = process.argv.includes("--check");
const TOKEN = readFileSync(process.env.SPECIMEN_TOKEN_FILE ?? join(homedir(), ".specimen-token"), "utf8").trim();
const PARENT = "aria-ashrafi";
const BRAND = {
  slug: "de-saturator",
  name: "De-Saturator",
  extends: PARENT,
  description: "A free plugin that thins a sound: the opposite of a saturator. Hairline: what stays is drawn heavy, what's removed is a hairline, in the analyser, the Amount ring and the name. A sub-brand of Aria Ashrafi. In development.",
};

const COLLECTIONS = [
  { slug: "symbol", name: "Mark", kind: "logo" },
  { slug: "symbol-png", name: "Mark PNG", kind: "logo" },
  { slug: "logo", name: "Logo", kind: "logo" },
  { slug: "logo-png", name: "Logo PNG", kind: "logo" },
  { slug: "fonts", name: "Fonts", kind: "font" },
  { slug: "ui-handoff", name: "Hand-off", kind: "file" },
  { slug: "ui-assets", name: "Window assets", kind: "logo" },
  { slug: "ui-states", name: "States", kind: "image" },
  { slug: "ui-states-2x", name: "States 2x", kind: "image" },
  { slug: "data", name: "Data", kind: "file" },
];

function assets() {
  const out = [];
  const dir = join(HERE, "manifest");
  for (const f of readdirSync(dir).filter((x) => x.endsWith(".json")).sort()) out.push(...JSON.parse(readFileSync(join(dir, f), "utf8")));
  const seen = new Set();
  for (const a of out) {
    const k = `${a.collection}/${a.slug}`;
    if (seen.has(k)) throw new Error(`Listed twice: ${k}`);
    seen.add(k);
    if (a.name.length > 80) throw new Error(`Name over 80 characters: ${a.name}`);
    if (!COLLECTIONS.some((c) => c.slug === a.collection)) throw new Error(`No collection ${a.collection} for ${a.slug}`);
  }
  return out;
}

async function api(method, path, body) {
  const init = { method, headers: { Authorization: `Bearer ${TOKEN}` } };
  if (body instanceof FormData) init.body = body;
  else if (body !== undefined) {
    init.headers["Content-Type"] = "application/json";
    init.body = JSON.stringify(body);
  }
  let res;
  for (let attempt = 1, limited = 0; ; attempt++) {
    try {
      res = await fetch(BASE + path, { ...init, signal: AbortSignal.timeout(180_000) });
    } catch (e) {
      if (method !== "GET" || attempt === 4) throw e;
      await new Promise((r) => setTimeout(r, 1000 * attempt));
      continue;
    }
    if (res.status !== 429 || ++limited > 8) break;
    await new Promise((r) => setTimeout(r, 65_000));
  }
  const text = await res.text();
  let json = null;
  try {
    json = JSON.parse(text);
  } catch {
    /* not JSON */
  }
  return { status: res.status, data: json?.data, error: json?.error, text };
}

function must(r, what) {
  if (r.status >= 400) throw new Error(`${what}: ${r.status} ${r.error?.code ?? ""} ${r.error?.message ?? r.text.slice(0, 200)}${r.error?.details ? `\n${JSON.stringify(r.error.details, null, 2)}` : ""}`);
  return r.data;
}

const sha = (buf) => createHash("sha256").update(buf).digest("hex");
const done = [];
const P = `/api/v1/brands/${BRAND.slug}`;

const me = must(await api("GET", "/api/v1/me"), "Checking the token");
console.log(`Signed in as ${me.name} (${me.role}).${CHECK ? " Checking only." : ""}`);

const list_ = assets();
const missingFiles = list_.filter((f) => !existsSync(join(ROOT, f.file)));
if (missingFiles.length) throw new Error(`Missing files:\n${missingFiles.map((f) => f.file).join("\n")}`);

const got = await api("GET", P);
if (got.status !== 200) {
  if (CHECK) done.push("would create the brand");
  else {
    const made = must(await api("POST", "/api/v1/brands", BRAND), "Creating the brand");
    if (made?.extends !== PARENT) throw new Error("Specimen made the brand without its parent.");
    done.push(`created brand ${BRAND.slug} inside ${PARENT}`);
  }
} else if (!CHECK && (got.data?.description !== BRAND.description || got.data?.name !== BRAND.name)) {
  must(await api("PATCH", P, { name: BRAND.name, description: BRAND.description }), "Updating the brand");
}

if (!CHECK || got.status === 200) {
  const have = new Set((must(await api("GET", `${P}/collections`), "Listing collections") ?? []).map((c) => c.slug));
  for (const c of COLLECTIONS) {
    if (have.has(c.slug)) continue;
    if (CHECK) done.push(`would create collection ${c.slug}`);
    else must(await api("POST", `${P}/collections`, c), `Creating collection ${c.slug}`);
  }
  let added = 0,
    versions = 0,
    same = 0,
    n = 0;
  const upload = async (a, buf, path, cur) => {
    const form = new FormData();
    form.set("file", new File([buf], basename(a.file)));
    if (cur.status === 200) {
      must(await api("POST", `${path}/versions`, form), `New version of ${a.collection}/${a.slug}`);
      versions++;
    } else {
      form.set("slug", a.slug);
      form.set("name", a.name);
      if (a.description) form.set("description", a.description);
      form.set("tags", a.tags.join(","));
      must(await api("POST", `${P}/assets/${a.collection}`, form), `Adding ${a.collection}/${a.slug}`);
      added++;
    }
  };
  for (const a of list_) {
    if (++n % 50 === 0) console.log(`  ${n}/${list_.length}`);
    const buf = readFileSync(join(ROOT, a.file));
    const path = `${P}/assets/${a.collection}/${a.slug}`;
    const cur = await api("GET", path);
    if (cur.status === 200 && cur.data?.current?.sha256 === sha(buf)) {
      same++;
      continue;
    }
    if (CHECK) {
      done.push(`would ${cur.status === 200 ? "update" : "add"} ${a.collection}/${a.slug}`);
      continue;
    }
    // an upload can stall on the network: ask again what's there, then retry
    for (let attempt = 1; ; attempt++) {
      try {
        const now = attempt === 1 ? cur : await api("GET", path);
        if (now.status === 200 && now.data?.current?.sha256 === sha(buf)) break;
        await upload(a, buf, path, now);
        break;
      } catch (e) {
        console.log(`  ${a.collection}/${a.slug} (${buf.length} bytes), attempt ${attempt}: ${e.cause?.code ?? e.message}`);
        if (attempt === 3) throw e;
      }
    }
  }
  done.push(`assets: ${added} added, ${versions} new versions, ${same} unchanged`);

  // The window is light only in v1, so its colours hold in every mode.
  const files = [{ name: "de-saturator.tokens.json", modes: [], content: JSON.parse(readFileSync(join(ROOT, "tokens", "de-saturator.tokens.json"), "utf8")) }];
  const check = must(await api("POST", `${P}/token-versions/check`, { files }), "Checking tokens");
  for (const p of check.problems ?? []) console.log(`  ${p.level}: ${p.file} → ${p.path}: ${p.message}`);
  const changed = !check.diff || ["added", "removed", "changed"].some((k) => (check.diff[k] ?? []).length);
  if (!changed) done.push("tokens: unchanged");
  else if (CHECK) done.push("tokens would publish");
  else done.push(`tokens: published version ${must(await api("POST", `${P}/token-versions`, { files }), "Publishing tokens").version.number}`);

  const published = new Map();
  for (const s of must(await api("GET", `${P}/guidelines`), "Listing guidelines") ?? []) for (const p of s.pages) if (!p.inheritedFrom) published.set(p.slug, p);
  let created = 0,
    updated = 0,
    unchanged = 0;
  for (const file of readdirSync(join(ROOT, "guidelines")).filter((f) => f.endsWith(".md")).sort()) {
    const src = readFileSync(join(ROOT, "guidelines", file), "utf8").replace(/\r\n/g, "\n");
    const m = /^---\ntitle: (.+)\nsection: (.+)\n---\n([\s\S]*)$/.exec(src);
    if (!m) throw new Error(`${file}: expected title and section between --- lines`);
    const p = { title: m[1].trim(), section: m[2].trim(), markdown: m[3].trim() + "\n", body: m[3].trim() + "\n" };
    const slug = p.title.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "");
    if (published.has(slug)) {
      const cur = must(await api("GET", `${P}/guidelines/${slug}`), `Reading ${slug}`);
      if (cur.markdown === p.body && cur.title === p.title && cur.section === p.section) {
        unchanged++;
        continue;
      }
      if (CHECK) done.push(`would update page ${slug}`);
      else {
        must(await api("PUT", `${P}/guidelines/${slug}`, p), `Updating ${slug}`);
        updated++;
      }
    } else if (CHECK) done.push(`would create page ${slug}`);
    else {
      must(await api("POST", `${P}/guidelines`, p), `Creating ${slug}`);
      created++;
    }
  }
  done.push(`guidelines: ${created} created, ${updated} updated, ${unchanged} unchanged`);
  if (!CHECK) {
    const missing = [];
    for (const s of must(await api("GET", `${P}/guidelines`), "Listing guidelines") ?? [])
      for (const p of s.pages) {
        const g = must(await api("GET", `${P}/guidelines/${p.slug}`), `Reading ${p.slug}`);
        for (const x of g.text.matchAll(/\(missing [^)]+\)/g)) missing.push(`${p.slug}: ${x[0]}`);
      }
    done.push(missing.length ? `MISSING EMBEDS:\n  ${missing.join("\n  ")}` : "every embed on every page resolves");
  }
}
console.log(done.map((d) => `- ${d}`).join("\n"));
