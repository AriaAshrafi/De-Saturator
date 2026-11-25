"""Builds board.html: three directions for De-Saturator, each a working plugin window.

The template holds the page, board.js the windows. This fills in the parent brand's fonts
(all SIL OFL) and its credit lockup, so the page carries everything it needs.

    python board.py   ->  board.html

Set DESAT_PARENT_BRAND if the parent brand's dist folder is somewhere else.
"""
import base64
import os
import pathlib
import re

HERE = pathlib.Path(__file__).parent
PARENT = pathlib.Path(os.environ.get(
    "DESAT_PARENT_BRAND", r"C:/Users/AriaA/Desktop/All/Other/Projects/AriaAshrafi Site/design/brand/dist"))
FONTS = PARENT / "fonts"


def font(name):
    return base64.b64encode((FONTS / name).read_bytes()).decode()


credit = (PARENT / "logo" / "lockup-credit-paper.svg").read_text(encoding="utf-8")
box = re.search(r'viewBox="([^"]+)"', credit).group(1)
paths = "".join(re.findall(r"<path[^>]*/>", credit)).replace(' fill="#ECEBE6"', "")
symbol = f'<symbol id="credit" viewBox="{box}">{paths}</symbol>'

html = (HERE / "board.template.html").read_text(encoding="utf-8")
for key, value in {
    "%%ANYBODY%%": font("Anybody-Variable.woff2"),
    "%%BODONI_IT%%": font("BodoniModa-Italic-Variable.woff2"),
    "%%DEPARTURE%%": font("DepartureMono-Regular.woff2"),
    "%%CREDIT%%": symbol,
    "%%SCRIPT%%": (HERE / "board.js").read_text(encoding="utf-8"),
}.items():
    html = html.replace(key, value)
(HERE / "board.html").write_text(html, encoding="utf-8", newline="\n")
print("board.html", round(len(html) / 1e6, 2), "MB")
