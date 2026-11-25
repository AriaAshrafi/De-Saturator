"""Release art for Windows and macOS, and the VST3 bundle's files.

    python installers.py        writes the SVGs and the render jobs (build/jobs/installers.json)
    node render.mjs jobs/installers.json
    python installers.py pack   makes BMP, ICO and ICNS from the renders, and the VST3 snapshots

The motif is the analyser in small: a comb of lines, heavy up to the result line, hairlines
up to the original. Klein carries the mark, as on the icon tiles.
"""
import json
import math
import shutil
import sys

from PIL import Image

from ds import GRAPHITE_DEEP, INK, KLEIN, NAME, PAPER, ROOT, Manifest, Renders, fmt, sine, svg, wordmark, write

PACK = len(sys.argv) > 1 and sys.argv[1] == "pack"
WIZARD = [(164, 314), (192, 386), (246, 459), (273, 556), (328, 604), (355, 700), (410, 797)]  # Inno Setup, 100% to 250%
WIZARD_SMALL = [(55, 58), (64, 68), (83, 80), (92, 97), (110, 106), (119, 123), (138, 140)]
ICO = [16, 20, 24, 32, 40, 48, 64, 128, 256]
ICNS = [16, 32, 64, 128, 256, 512, 1024]
GRAPHITE = "#8A8983"


def rect(x, y, w, h, fill, extra=""):
    return f'<rect x="{fmt(x)}" y="{fmt(y)}" width="{fmt(w)}" height="{fmt(h)}" fill="{fill}"{extra}/>'


def mark_at(cx, cy, width, fill, small=False):
    d, b = sine(thin=5.0 if small else 1.4)
    s = width / (b[2] - b[0])
    tx, ty = cx - (b[0] + (b[2] - b[0]) / 2) * s, cy - (b[1] + (b[3] - b[1]) / 2) * s
    return f'<path transform="translate({fmt(tx)} {fmt(ty)}) scale({s:.5f})" d="{d}" fill="{fill}"/>'


def name_at(x, y_top, width, fill):
    """The name, width wide, its top at y_top. The small cut when it's under 24 px tall."""
    full = wordmark(100)
    b = full.bounds()
    h = (b[3] - b[1]) * width / (b[2] - b[0])
    r = wordmark(100, small=h < 24)
    b = r.bounds()
    s = width / (b[2] - b[0])
    return f'<path transform="translate({fmt(x - b[0] * s)} {fmt(y_top - b[1] * s)}) scale({s:.5f})" d="{r.d}" fill="{fill}"/>', (b[3] - b[1]) * s


CREDIT = json.loads((ROOT / "logo/credit.json").read_text(encoding="utf-8"))
CVB = [float(v) for v in CREDIT["vb"].split()]


def credit_at(x, y, h, fill):
    s = h / CVB[3]
    return f'<path transform="translate({fmt(x - CVB[0] * s)} {fmt(y - CVB[1] * s)}) scale({s:.5f})" d="{CREDIT["d"]}" fill="{fill}"/>', CVB[2] * s


def comb(x0, x1, base, hmax, n, ink, unit=1.0, phase=0.0):
    """The analyser in small: n lines from base, heavy body to the result line, hairlines to the original."""
    xs = [x0 + (x1 - x0) * (i + 0.5) / n for i in range(n)]
    orig = [hmax * (0.62 + 0.24 * math.sin(2.3 * math.pi * t + 0.7 + phase) + 0.1 * math.sin(9.1 * t + phase)) for t in (i / (n - 1) for i in range(n))]
    res = [o * (1 - 0.42 * (i / (n - 1)) ** 1.3) for i, o in enumerate(orig)]
    s = ""
    for x, o, r in zip(xs, orig, res):
        s += f'<path d="M{fmt(x)} {fmt(base)}V{fmt(base - r)}" stroke="{ink}" stroke-opacity="0.18" stroke-width="{fmt(2.6 * unit)}"/>'
        s += f'<path d="M{fmt(x)} {fmt(base - r)}V{fmt(base - o)}" stroke="{ink}" stroke-width="{fmt(max(1.0, unit))}"/>'
    line = lambda ys: "M" + "L".join(f"{fmt(x)} {fmt(base - y)}" for x, y in zip(xs, ys))
    s += f'<path d="{line(orig)}" fill="none" stroke="{ink}" stroke-width="{fmt(max(1.0, unit))}" stroke-linejoin="round"/>'
    s += f'<path d="{line(res)}" fill="none" stroke="{ink}" stroke-width="{fmt(2 * unit)}" stroke-linejoin="round"/>'
    return s


if not PACK:
    m, r = Manifest("installers"), Renders("installers")

    # macOS icon: Apple's 1024 grid, an 824 tile with a 185 corner, klein, the mark in paper
    for cut in ("full", "small"):
        body = rect(100, 100, 824, 824, KLEIN, ' rx="185"') + mark_at(512, 512, 824 * 0.66, PAPER, cut == "small")
        write(f"installers/macos/src/icon-{cut}.svg", svg(1024, 1024, body, f"{NAME} icon"))
    for s in ICNS:
        r.add(f"installers/macos/src/icon-{'small' if s <= 32 else 'full'}.svg", f"build/_r/icns-{s}.png", s)
    r.add("installers/macos/src/icon-full.svg", "installers/icon-big.png", 1024)
    m.add("installers", "installers/icon-big.png", "icon-big", "Plugin icon, 1024 (ICON_BIG)", "Klein, in Apple's rounded template with its transparent margin. Give it to JUCE as ICON_BIG.", ["icon", "juce"])
    m.add("installers", "installers/macos/src/icon-full.svg", "macos-icon-source", "macOS icon, source", "1024 grid: an 824 tile, 185 corner, per Apple's template.", ["macos", "icon"])

    # Windows and the favicon: the klein tile, full bleed, the small mark at 32 px and under
    for s in ICO:
        r.add(f"logo/de-saturator-tile{'-small' if s <= 32 else ''}-klein.svg", f"build/_r/ico-{s}.png", s, KLEIN)

    # Inno Setup: the mark above, the analyser in small, the name and the credit below
    for w, h in WIZARD:
        k = w / 164
        body = rect(0, 0, w, h, PAPER)
        body += mark_at(w / 2, h * 0.15, w * 0.6, KLEIN)
        body += comb(14 * k, w - 14 * k, h * 0.70, h * 0.30, 21, INK, k, 0.0)
        nm, nh = name_at(14 * k, h * 0.77, w - 28 * k, INK)
        cr, _ = credit_at(14 * k, h * 0.77 + nh + 10 * k, 9 * k, GRAPHITE_DEEP)
        f = f"installers/windows/src/wizard-{w}x{h}.svg"
        write(f, svg(w, h, body + nm + cr, f"{NAME} setup"))
        r.add(f, f"build/_r/wizard-{w}x{h}.png", w)
    for w, h in WIZARD_SMALL:
        f = f"installers/windows/src/wizard-small-{w}x{h}.svg"
        write(f, svg(w, h, rect(0, 0, w, h, KLEIN) + mark_at(w / 2, h / 2, min(w, h) * 0.72, PAPER, min(w, h) < 80), NAME))
        r.add(f, f"build/_r/wizard-small-{w}x{h}.png", w)

    # macOS .pkg background: art only in the bottom 70 px, so the installer's own text stays clear
    for theme, (ground, ink) in {"light": (PAPER, INK), "dark": (INK, PAPER)}.items():
        w, h = 620, 418
        body = rect(0, 0, w, h, ground) + comb(24, w - 24, h - 14, 52, 40, ink, 1.0, 1.3)
        f = f"installers/macos/src/pkg-background-{theme}.svg"
        write(f, svg(w, h, body, f"{NAME} installer"))
        for scale in (1, 2):
            p = f"installers/macos/pkg/background-{theme}{'' if scale == 1 else '@2x'}.png"
            r.add(f, p, w * scale)
            m.add("installers", p, f"pkg-background-{theme}-{scale}x", f"macOS .pkg background, {theme}, {scale}x", "Aligned bottom left, not scaled. Only the bottom 70 px carry art.", ["macos", "pkg"])

    # macOS disk image window
    w, h = 660, 400
    nm, nh = name_at(32, 32, 260, INK)
    body = rect(0, 0, w, h, PAPER) + nm + comb(32, w - 32, h - 28, 60, 44, INK, 1.0, 2.1)
    f = "installers/macos/src/dmg-background.svg"
    write(f, svg(w, h, body, f"{NAME} disk image"))
    for scale in (1, 2):
        p = f"installers/macos/dmg/background{'' if scale == 1 else '@2x'}.png"
        r.add(f, p, w * scale)
        m.add("installers", p, f"dmg-background-{scale}x", f"macOS disk image background, {scale}x", "A 660 × 400 window. Put the installer at (200, 190) and the read-me at (460, 190); labels in ink.", ["macos", "dmg"])

    write("installers/vst3/desktop.ini", "[.ShellClassInfo]\nIconResource=PlugIn.ico,0\n")
    m.add("installers", "installers/vst3/desktop.ini", "vst3-desktop-ini", "desktop.ini", "Beside PlugIn.ico in the Windows .vst3 bundle folder, so Explorer shows the icon. Mark the folder read-only or system.", ["vst3", "windows"])
    for f, slug, name, desc, tags, coll in [
        ("installers/windows/de-saturator.ico", "windows-ico", "de-saturator.ico", "Setup and uninstall icon: 16 to 256 px, the small mark at 32 px and under.", ["windows", "icon"], "installers"),
        ("installers/vst3/PlugIn.ico", "vst3-plugin-ico", "PlugIn.ico", "The icon inside the Windows .vst3 bundle folder.", ["vst3", "windows"], "installers"),
        ("installers/macos/de-saturator.icns", "macos-icns", "de-saturator.icns", "Installer and bundle icon: 16 to 1024 px.", ["macos", "icon"], "installers"),
        ("installers/macos/dmg/VolumeIcon.icns", "macos-volume-icns", "VolumeIcon.icns", "The mounted disk image's icon.", ["macos", "dmg"], "installers"),
        ("favicons/favicon.ico", "favicon-ico", "favicon.ico", "16, 32 and 48 px, for the product page.", ["favicon"], "favicons"),
        ("favicons/apple-touch-icon.png", "apple-touch-icon", "apple-touch-icon.png", "180 px.", ["favicon"], "favicons"),
    ]:
        m.add(coll, f, slug, name, desc, tags)
    r.add("logo/de-saturator-tile-klein.svg", "favicons/apple-touch-icon.png", 180, KLEIN)
    for scale, name in {1: "snapshot", 2: "snapshot_2.0x"}.items():
        p = f"installers/vst3/Resources/Snapshots/PROCESSOR-UID_{name}.png"
        m.add("installers", p, f"vst3-{name.replace('_', '-').replace('.', '-')}", f"VST3 {name.replace('_', ' ')}", "The default window, light, for DAW plugin browsers. Rename PROCESSOR-UID to the processor's class ID.", ["vst3"])
    m.add("installers", "installers/windows/wizard/preview.png", "inno-wizard-preview", "Inno Setup wizard image, preview", "The 200% size as a PNG, to look at. The installer takes the BMPs.", ["windows", "inno"])
    for w, h in WIZARD:
        m.add("installers", f"installers/windows/wizard/WizardImage-{w}x{h}.bmp", f"inno-wizard-{w}x{h}", f"Inno Setup wizard image, {w} × {h}", "24-bit BMP. Pass all seven to WizardImageFile, smallest first.", ["windows", "inno"])
    for w, h in WIZARD_SMALL:
        m.add("installers", f"installers/windows/wizard/WizardSmallImage-{w}x{h}.bmp", f"inno-wizard-small-{w}x{h}", f"Inno Setup small image, {w} × {h}", "24-bit BMP. Pass all seven to WizardSmallImageFile, smallest first.", ["windows", "inno"])
    m.save()
    r.save()
    print("installers: SVGs and jobs written; now node render.mjs jobs/installers.json, then python installers.py pack")
else:
    rd = ROOT / "build" / "_r"
    (ROOT / "installers/windows/wizard").mkdir(parents=True, exist_ok=True)
    for w, h in WIZARD:
        Image.open(rd / f"wizard-{w}x{h}.png").convert("RGB").save(ROOT / f"installers/windows/wizard/WizardImage-{w}x{h}.bmp")
    for w, h in WIZARD_SMALL:
        Image.open(rd / f"wizard-small-{w}x{h}.png").convert("RGB").save(ROOT / f"installers/windows/wizard/WizardSmallImage-{w}x{h}.bmp")
    Image.open(rd / "wizard-328x604.png").convert("RGB").save(ROOT / "installers/windows/wizard/preview.png")
    ims = {s: Image.open(rd / f"ico-{s}.png").convert("RGBA") for s in ICO}

    def save_ico(path, sizes):
        path = ROOT / path
        path.parent.mkdir(parents=True, exist_ok=True)
        top = max(sizes)
        ims[top].save(path, format="ICO", sizes=[(s, s) for s in sizes], append_images=[ims[s] for s in sizes if s != top])

    save_ico("installers/windows/de-saturator.ico", ICO)
    save_ico("installers/vst3/PlugIn.ico", ICO)
    save_ico("favicons/favicon.ico", [16, 32, 48])
    ic = {s: Image.open(rd / f"icns-{s}.png").convert("RGBA") for s in ICNS}
    for path in ("installers/macos/de-saturator.icns", "installers/macos/dmg/VolumeIcon.icns"):
        p = ROOT / path
        p.parent.mkdir(parents=True, exist_ok=True)
        ic[1024].save(p, format="ICNS", append_images=[ic[s] for s in ICNS if s != 1024])
    snap = ROOT / "installers/vst3/Resources/Snapshots"
    snap.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(ROOT / "states/default.png", snap / "PROCESSOR-UID_snapshot.png")
    shutil.copyfile(ROOT / "states/default@2x.png", snap / "PROCESSOR-UID_snapshot_2.0x.png")
    print("installers: packed")
