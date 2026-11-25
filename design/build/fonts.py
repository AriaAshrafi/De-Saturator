"""The two typefaces, both SIL OFL 1.1, so they can ship inside the free plugin.

Anybody is the window's voice; the window uses three static cuts of it at normal width, so the
plugin never has to instance a variable font. Departure Mono is the readout face: a pixel font,
crisp at 11 px and 22 px at 100%. The name is never set from the font: it is an outlined drawing
(logo/), because its weight changes letter by letter.

    python fonts.py   ->  fonts/
"""
import shutil

from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont

from ds import PARENT, ROOT, Manifest

SRC = PARENT / "dist" / "fonts"
OUT = ROOT / "fonts"
OUT.mkdir(parents=True, exist_ok=True)

# file name: (wdth, wght, style name, what it's for)
STATIC = {
    "Anybody-Medium-500.ttf": (100, 500, "Medium", "Tooltips."),
    "Anybody-SemiBold-600.ttf": (100, 600, "SemiBold", "Labels, switches, Delta, Auto gain."),
    "Anybody-Bold-700.ttf": (100, 700, "Bold", "The Amount title."),
}


def rename(font, style):
    """A static cut keeps the family name (neither licence reserves one) with its own style name."""
    name = font["name"]
    for rec in list(name.names):
        if rec.nameID in (2, 17):
            rec.string = style
        elif rec.nameID in (4,):
            rec.string = f"Anybody {style}"
        elif rec.nameID in (6,):
            rec.string = f"Anybody-{style}"
        elif rec.nameID in (1, 16):
            rec.string = "Anybody"
    name.removeNames(nameID=25)


m = Manifest("fonts")
for f in ("DepartureMono-Regular.otf", "DepartureMono-OFL.txt", "Anybody-OFL.txt", "Anybody-Variable.ttf"):
    shutil.copyfile(SRC / f, OUT / f)
for file, (wdth, wght, style, use) in STATIC.items():
    inst = instantiateVariableFont(TTFont(SRC / "Anybody-Variable.ttf"), {"wdth": wdth, "wght": wght}, inplace=False, updateFontNames=False)
    rename(inst, style)
    inst["OS/2"].usWeightClass = wght
    inst.save(OUT / file)
    m.add("fonts", f"fonts/{file}", f"anybody-{style.lower()}", f"Anybody {style} {wght} (static)", f"{use} Embed this cut.", ["anybody", "static"])
m.add("fonts", "fonts/DepartureMono-Regular.otf", "departure-mono", "Departure Mono", "Values and readouts, at 11 or 22 px at 100%. Embed this.", ["departure-mono"])
m.add("fonts", "fonts/Anybody-Variable.ttf", "anybody-variable", "Anybody, variable", "The whole family (wdth 50-150, wght 100-900), for print and the web. The plugin uses the static cuts.", ["anybody", "variable"])
m.add("data", "fonts/Anybody-OFL.txt", "anybody-ofl", "Anybody licence (OFL 1.1)", "Ship it with the plugin.", ["licence"])
m.add("data", "fonts/DepartureMono-OFL.txt", "departure-mono-ofl", "Departure Mono licence (OFL 1.1)", "Ship it with the plugin.", ["licence"])
m.save()
print("fonts/")
