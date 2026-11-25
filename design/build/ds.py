"""What every De-Saturator generator shares: colours, the parent's type tool, files.

De-Saturator is a sub-brand of aria-ashrafi, direction B "Hairline": thinner, drawn literally.
What stays is heavy, what's removed is a hairline. Every colour here is one of the parent's.
"""
from __future__ import annotations

import json
import math
import os
import pathlib
import sys

HERE = pathlib.Path(__file__).parent
ROOT = HERE.parent
PARENT = pathlib.Path(os.environ.get("DESAT_PARENT_BRAND", r"C:/Users/AriaA/Desktop/All/Other/Projects/AriaAshrafi Site/design/brand"))
sys.path.insert(0, str(PARENT / "tools"))
from typeset import Setter, fmt  # noqa: E402

S = Setter()

INK = "#0C0C0B"
PAPER = "#ECEBE6"
PAPER_RAISED = "#E2E0D8"
PAPER_LINE = "#D6D4CB"
GRAPHITE_DEEP = "#5F5E59"
KLEIN = "#2E40FF"

NAME = "De-Saturator"


def ramp(lo=200, hi=900):
    """The name loses weight letter by letter. Every a opens to width 150, the parent's rule."""
    n = len(NAME)
    return [(c, {"wdth": 150 if c == "a" else 100, "wght": round(hi - (hi - lo) * i / (n - 1))}) for i, c in enumerate(NAME)]


def wordmark(size=100, small=False):
    """Outlined name, baseline at y=0, x from 0. small: a heavier floor for sizes under 24 px."""
    return S.run(ramp(420 if small else 200), size, tracking=-0.01 * size)


def voice(content, size, x=0.0, y=0.0, wght=600, wdth=100):
    r = S.run([(c, {"wdth": wdth, "wght": wght}) for c in content], size, 0, x, y)
    return r


def sine(width=100.0, amp=19.0, thick=15.0, thin=1.4, steps=160):
    """The mark: one period of the parent's sine, losing weight from left to right.
    Returns path data and bounds (x0, y0, x1, y1), with the wave's centre line at y=0."""
    up, dn = [], []
    for i in range(steps + 1):
        u = i / steps
        x = u * width
        t = thick + (thin - thick) * u
        y = -amp * math.sin(2 * math.pi * u)
        dydx = -amp * math.cos(2 * math.pi * u) * 2 * math.pi / width
        l = math.hypot(1, dydx)
        nx, ny = -dydx / l, 1 / l
        up.append((x - nx * t / 2, y - ny * t / 2))
        dn.append((x + nx * t / 2, y + ny * t / 2))
    pts = up + dn[::-1]
    d = "M" + "L".join(f"{fmt(x)} {fmt(y)}" for x, y in pts) + "Z"
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
    return d, (min(xs), min(ys), max(xs), max(ys))


def svg(w, h, body, title, x0=0.0, y0=0.0):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{fmt(x0)} {fmt(y0)} {fmt(w)} {fmt(h)}" width="{fmt(w)}" height="{fmt(h)}" role="img" aria-label="{title}">'
            f"<title>{title}</title>{body}</svg>\n")


def write(rel, content):
    p = ROOT / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    if isinstance(content, bytes):
        p.write_bytes(content)
    else:
        p.write_text(content, encoding="utf-8", newline="\n")
    return p


class Manifest:
    """Each generator lists what it made; load.mjs reads the lists."""

    def __init__(self, name):
        self.name, self.items = name, []

    def add(self, collection, file, slug, name, description="", tags=()):
        self.items.append(dict(collection=collection, file=file, slug=slug, name=name, description=description, tags=list(tags)))

    def save(self):
        write(f"build/manifest/{self.name}.json", json.dumps(self.items, indent=1, ensure_ascii=False))
        print(f"{self.name}: {len(self.items)} assets")


class Renders:
    """PNG jobs for render.mjs."""

    def __init__(self, name):
        self.name, self.jobs = name, []

    def add(self, svg_rel, png_rel, width, background=None):
        self.jobs.append(dict(svg=svg_rel, png=png_rel, width=width, background=background))

    def save(self):
        write(f"build/jobs/{self.name}.json", json.dumps(self.jobs, indent=0))
