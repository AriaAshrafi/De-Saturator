#!/usr/bin/env bash
# Rebuilds the whole design kit from its sources. Run from Git Bash (PowerShell's bash is WSL):
#   bash design/build/make.sh
# Then load it into Specimen with: node design/build/load.mjs
set -euo pipefail
cd "$(dirname "$0")"
python fonts.py            # fonts/: static Anybody cuts, Departure Mono, licences
python logo.py             # logo/, ui/assets/: the name, the mark, lockups, tiles
node render.mjs jobs/logo.json
node states.mjs            # states/: every state at 1x and 2x, and the redlines
node spec.mjs              # spec/, tokens/: the build spec, read from ui/core.js
node prototype.mjs         # prototype/prototype.html
python guide.py            # guidelines/: the Specimen pages
echo "done"
