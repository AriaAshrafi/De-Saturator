"""Compares the window's snapshots (desat_snapshot) with the design's pictures (design/states).

    python tools/compare_snapshots.py [--snapshots snapshots] [--design design/states] [--out snapshots/diff]

For every picture in design/states/states.json it prints three scores and writes a diff image:

  mean     the mean difference per channel, 0 to 255. Anti-aliasing and font rasterising alone give
           about 0.3 to 1.
  off %    the share of pixels that differ by more than 64 in some channel: edges that moved,
           things missing or extra.
  blur     the largest difference after both pictures are averaged over 8 x 8 px blocks (scaled with
           the picture), 0 to 255. Sub-pixel differences vanish in it; a wrong colour, a missing
           element or a shifted block of text does not. This is the one to sort by.

The diff image is the design's picture, the snapshot and their difference (amplified) side by side.
Exits 1 if a snapshot is missing or the wrong size.
"""

import argparse
import json
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageStat


def compare(design_path: Path, snapshot_path: Path, diff_path: Path):
    design = Image.open(design_path).convert("RGB")
    snapshot = Image.open(snapshot_path).convert("RGB")
    if design.size != snapshot.size:
        return None, f"size {snapshot.size[0]}x{snapshot.size[1]}, the design's is {design.size[0]}x{design.size[1]}"

    diff = ImageChops.difference(design, snapshot)
    mean = sum(ImageStat.Stat(diff).mean) / 3.0

    r, g, b = diff.split()
    worst = ImageChops.lighter(ImageChops.lighter(r, g), b)
    off = sum(worst.histogram()[65:]) / (design.size[0] * design.size[1]) * 100.0

    # Blocks of 8 px at 100%, so the score means the same at every size.
    block = max(1, round(8 * design.size[0] / 960))
    small = (max(1, design.size[0] // block), max(1, design.size[1] // block))
    blurred = ImageChops.difference(design.resize(small, Image.BOX), snapshot.resize(small, Image.BOX))
    blur = max(channel[1] for channel in blurred.getextrema())

    sheet = Image.new("RGB", (design.size[0] * 3, design.size[1]), "white")
    sheet.paste(design, (0, 0))
    sheet.paste(snapshot, (design.size[0], 0))
    sheet.paste(worst.point(lambda v: min(255, v * 4)).convert("RGB"), (design.size[0] * 2, 0))
    diff_path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(diff_path)

    return (mean, off, blur), None


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--snapshots", type=Path, default=root / "snapshots")
    parser.add_argument("--design", type=Path, default=root / "design" / "states")
    parser.add_argument("--out", type=Path, default=None, help="where the diff images go (default: <snapshots>/diff)")
    args = parser.parse_args()
    out = args.out or args.snapshots / "diff"

    states = json.loads((args.design / "states.json").read_text(encoding="utf-8"))
    rows, problems = [], []
    for state in states:
        for name in (state["file"], state["file2x"]):
            if name.startswith("redlines"):
                continue  # the default state with measurements drawn over it: not a state of the window
            snapshot = args.snapshots / name
            if not snapshot.exists():
                problems.append(f"{name}: no snapshot")
                continue
            scores, problem = compare(args.design / name, snapshot, out / name)
            if problem:
                problems.append(f"{name}: {problem}")
            else:
                rows.append((name, *scores))

    rows.sort(key=lambda row: (-row[3], -row[1]))
    print(f"{'picture':<28}{'mean':>8}{'off %':>8}{'blur':>7}")
    for name, mean, off, blur in rows:
        print(f"{name:<28}{mean:>8.2f}{off:>8.2f}{blur:>7d}")
    for problem in problems:
        print("PROBLEM  " + problem)
    print(f"{len(rows)} compared, diff images in {out}")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
