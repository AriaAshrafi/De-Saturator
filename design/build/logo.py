"""The name, the mark and their lockups, all outlined.

The name is Anybody, losing weight letter by letter: Black (900) at the D down to ExtraLight (200)
at the last r, every a opened to width 150 (the parent's rule: A gets the room). Under 24 px tall the
thin end breaks up, so the small cut stops at 420.

The mark is one period of the parent's sine wave, losing weight from left to right: a sound getting
thinner. Its small cut keeps the thin end at a third of the thick one.

    python logo.py   ->  logo/, ui/assets/, build/jobs/logo.json
"""
import json
import re

from ds import INK, KLEIN, PAPER, PARENT, NAME, Manifest, Renders, fmt, sine, svg, wordmark, write

m = Manifest("logo")
jobs = Renders("logo")
COLOURWAYS = {"ink": INK, "paper": PAPER, "current": "currentColor"}
GROUND = {"ink": PAPER, "paper": INK, "current": PAPER}   # what each colourway is shown on


def box(b, pad=0.0):
    x0, y0, x1, y1 = b
    return x0 - pad, y0 - pad, x1 - x0 + 2 * pad, y1 - y0 + 2 * pad


# ---- the name
for small in (False, True):
    r = wordmark(100, small)
    x0, y0, w, h = box(r.bounds(), 1)
    tag = "-small" if small else ""
    for cw, col in COLOURWAYS.items():
        f = f"logo/de-saturator-wordmark{tag}-{cw}.svg"
        write(f, svg(w, h, f'<path d="{r.d}" fill="{col}"/>', NAME, x0, y0))
        nm = f"Name{' small' if small else ''}, {cw}"
        m.add("logo", f, f"wordmark{tag}-{cw}", nm,
              ("For 24 px tall and under: the weight stops at 420 so the thin end holds. " if small else "Black at the D to ExtraLight at the end. ") + f"In {cw}." , ["wordmark"] + (["small"] if small else []))
        if cw != "current":
            png = f"logo/png/de-saturator-wordmark{tag}-{cw}.png"
            jobs.add(f, png, 600 if small else 1600)
            m.add("logo-png", png, f"wordmark{tag}-{cw}-png", f"{nm} (PNG)", "Transparent ground.", ["wordmark", "png"])
    if not small:
        write("logo/wordmark-ui.json", json.dumps({"size": 100, "d": r.d, "width": round(r.width, 2), "bounds": [round(v, 2) for v in r.bounds()]}))

# ---- the mark
for small in (False, True):
    d, b = sine(thin=5.0 if small else 1.4)
    x0, y0, w, h = box(b, 0.5)
    tag = "-small" if small else ""
    for cw, col in dict(COLOURWAYS, klein=KLEIN).items():
        f = f"logo/de-saturator-mark{tag}-{cw}.svg"
        write(f, svg(w, h, f'<path d="{d}" fill="{col}"/>', f"{NAME} mark", x0, y0))
        nm = f"Mark{' small' if small else ''}, {cw}"
        m.add("symbol", f, f"mark{tag}-{cw}", nm, ("For 32 px and under: the thin end stays a third as heavy as the thick one. " if small else "The parent's sine, losing weight left to right. ") + f"In {cw}.", ["mark"] + (["small"] if small else []))
        if cw != "current":
            png = f"logo/png/de-saturator-mark{tag}-{cw}.png"
            jobs.add(f, png, 256 if small else 1024)
            m.add("symbol-png", png, f"mark{tag}-{cw}-png", f"{nm} (PNG)", "Transparent ground.", ["mark", "png"])

# ---- tiles: the mark on a square ground, hard corners (icons, installers, the web)
TILES = {"paper": (PAPER, INK), "ink": (INK, PAPER), "klein": (KLEIN, PAPER)}
for small in (False, True):
    d, b = sine(thin=5.0 if small else 1.4)
    x0, y0, w, h = box(b)
    s = (66 if small else 62) / w
    tx, ty = 50 - (x0 + w / 2) * s, 50 - (y0 + h / 2) * s
    tag = "-small" if small else ""
    for name, (ground, ink) in TILES.items():
        f = f"logo/de-saturator-tile{tag}-{name}.svg"
        body = f'<rect width="100" height="100" fill="{ground}"/><path transform="translate({fmt(tx)} {fmt(ty)}) scale({fmt(s)})" d="{d}" fill="{ink}"/>'
        write(f, svg(100, 100, body, f"{NAME} icon"))
        m.add("symbol", f, f"tile{tag}-{name}", f"Icon tile{' small' if small else ''}, {name}", ("For 16 and 32 px. " if small else "") + f"The mark on a {name} square.", ["tile", "icon"])
        for px in ((16, 32) if small else (48, 64, 128, 256, 512, 1024)):
            png = f"logo/png/de-saturator-tile-{name}-{px}.png"
            jobs.add(f, png, px, ground)
            m.add("symbol-png", png, f"tile-{name}-{px}", f"Icon tile, {name}, {px} px", f"{px} × {px}.", ["tile", "png"])

# ---- lockups
r = wordmark(100)
rb = r.bounds()
cap = -r.glyphs[0][4][1]                 # the D's height
d, b = sine()
mw, mh = b[2] - b[0], b[3] - b[1]
ms = cap / mh
gap = 34
mark_tf = f"translate({fmt(-b[0] * ms)} {fmt(-cap / 2 - (b[1] + mh / 2) * ms)}) scale({fmt(ms)})"
word_dx = mw * ms + gap
for cw, col in (("ink", INK), ("paper", PAPER), ("current", "currentColor")):
    body = f'<path transform="{mark_tf}" d="{d}" fill="{col}"/><path transform="translate({fmt(word_dx)} 0)" d="{r.d}" fill="{col}"/>'
    x0, y0, w, h = box((0, min(rb[1], -cap), word_dx + rb[2], rb[3]), 1)
    f = f"logo/de-saturator-lockup-{cw}.svg"
    write(f, svg(w, h, body, NAME, x0, y0))
    m.add("logo", f, f"lockup-{cw}", f"Lockup, {cw}", f"The mark beside the name, as tall as the D. In {cw}.", ["lockup"])
    if cw != "current":
        png = f"logo/png/de-saturator-lockup-{cw}.png"
        jobs.add(f, png, 2000)
        m.add("logo-png", png, f"lockup-{cw}-png", f"Lockup, {cw} (PNG)", "Transparent ground.", ["lockup", "png"])

# ---- the parent's credit, under the name
credit = (PARENT / "dist" / "logo" / "lockup-credit-paper.svg").read_text(encoding="utf-8")
cvb = [float(v) for v in re.search(r'viewBox="([^"]+)"', credit).group(1).split()]
cd = " ".join(re.findall(r'<path[^>]*\sd="([^"]+)"', credit))
write("logo/credit.json", json.dumps({"vb": " ".join(fmt(v) for v in cvb), "d": cd}))
ch = 24.0
cs = ch / cvb[3]
cw_ = cvb[2] * cs
for cw, col in (("ink", INK), ("paper", PAPER)):
    ty = rb[3] + 22
    body = (f'<path d="{r.d}" fill="{col}"/>'
            f'<path transform="translate({fmt(rb[2] - cw_ - cvb[0] * cs)} {fmt(ty - cvb[1] * cs)}) scale({fmt(cs)})" d="{cd}" fill="{col}"/>')
    x0, y0, w, h = box((rb[0], rb[1], rb[2], ty + ch), 1)
    f = f"logo/de-saturator-endorsed-{cw}.svg"
    write(f, svg(w, h, body, f"{NAME}, by Aria Ashrafi", x0, y0))
    m.add("logo", f, f"endorsed-{cw}", f"Name with the Aria Ashrafi credit, {cw}", "The parent's credit lockup under the name's end. For the store, the site and the installer.", ["endorsed"])
    png = f"logo/png/de-saturator-endorsed-{cw}.png"
    jobs.add(f, png, 1600)
    m.add("logo-png", png, f"endorsed-{cw}-png", f"Name with credit, {cw} (PNG)", "Transparent ground.", ["endorsed", "png"])

# ---- clear space: the thick end of the mark (15 units on a 100-unit mark), and half the D's height around the name
pad = cap / 2
body = (f'<rect x="{fmt(rb[0] - pad)}" y="{fmt(rb[1] - pad)}" width="{fmt(rb[2] - rb[0] + 2 * pad)}" height="{fmt(rb[3] - rb[1] + 2 * pad)}" fill="none" stroke="{KLEIN}" stroke-width="1.5" stroke-dasharray="6 5"/>'
        f'<rect x="{fmt(rb[0])}" y="{fmt(rb[1])}" width="{fmt(rb[2] - rb[0])}" height="{fmt(rb[3] - rb[1])}" fill="none" stroke="{KLEIN}" stroke-width="1"/>'
        f'<path d="{r.d}" fill="{INK}"/>')
x0, y0, w, h = box((rb[0] - pad, rb[1] - pad, rb[2] + pad, rb[3] + pad), 4)
write("logo/de-saturator-clear-space.svg", svg(w, h, f'<rect x="{fmt(x0)}" y="{fmt(y0)}" width="{fmt(w)}" height="{fmt(h)}" fill="{PAPER}"/>' + body, f"{NAME} clear space", x0, y0))
m.add("logo", "logo/de-saturator-clear-space.svg", "clear-space", "Clear space", "Keep half the D's height clear on every side of the name.", ["rules"])
jobs.add("logo/de-saturator-clear-space.svg", "logo/png/de-saturator-clear-space.png", 1600)

# ---- for the window: the name at its header size and the credit, as files JUCE can draw
hd = wordmark(27)
hb = hd.bounds()
x0, y0, w, h = box(hb)
write("ui/assets/wordmark-header.svg", svg(w, h, f'<path d="{hd.d}" fill="{INK}"/>', NAME, x0, y0))
m.add("ui-assets", "ui/assets/wordmark-header.svg", "wordmark-header", "Name for the window header, light theme", "At 27 px, ink. Its baseline sits at y 38, its origin at x 24.", ["window", "light"])
write("ui/assets/wordmark-header-dark.svg", svg(w, h, f'<path d="{hd.d}" fill="{PAPER}"/>', NAME, x0, y0))
m.add("ui-assets", "ui/assets/wordmark-header-dark.svg", "wordmark-header-dark", "Name for the window header, dark theme", "The same drawing in paper.", ["window", "dark"])
write("ui/assets/credit.svg", svg(cvb[2], cvb[3], f'<path d="{cd}" fill="#5F5E59"/>', "Aria Ashrafi", cvb[0], cvb[1]))
m.add("ui-assets", "ui/assets/credit.svg", "credit", "Aria Ashrafi credit for the window, light theme", "Graphite deep. Drawn 54 × 15 at x 24, y 566.", ["window", "light"])
write("ui/assets/credit-dark.svg", svg(cvb[2], cvb[3], f'<path d="{cd}" fill="#8A8983"/>', "Aria Ashrafi", cvb[0], cvb[1]))
m.add("ui-assets", "ui/assets/credit-dark.svg", "credit-dark", "Aria Ashrafi credit for the window, dark theme", "Graphite. Same place.", ["window", "dark"])

m.save()
jobs.save()
print("logo/")
