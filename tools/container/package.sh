#!/bin/bash
# Copies the game files (from your CD, mounted at /game) and the port's config next to the built title in dist/.
source "$(dirname "$0")/common.sh"
OUT=/work/dist/$TITLE_ID; OC=$TREE/OpenClaw/Build_Release
[ -f "$OUT/eboot.bin" ] || { echo "no title yet: run ./build.sh title first" >&2; exit 1; }
[ -f /game/DATA/CLAW.REZ ] || { echo "no DATA/CLAW.REZ in the game folder (see README: extract the CD)" >&2; exit 1; }
log "game files"
mkdir -p "$OUT/Claw" "$OUT/port" "$OUT/data"
cp /game/DATA/CLAW.REZ "$OUT/Claw/"; chmod 644 "$OUT/Claw/CLAW.REZ"
python3 - "$OC/ASSETS" "$OUT/Claw/ASSETS.ZIP" <<'PY'
import sys, os, zipfile
src, dst = sys.argv[1:3]
with zipfile.ZipFile(dst, 'w', zipfile.ZIP_DEFLATED) as z:
    for root, _, files in os.walk(src):
        for f in sorted(files):
            if f == '.DS_Store': continue
            p = os.path.join(root, f); z.write(p, os.path.relpath(p, src))
PY
cp "$OC/clacon.ttf" "$OC/console02.tga" "$OC/SAVES.XML" "$OUT/Claw/"
if [ -f /game/MOVIES/CLAW.FEC ]; then
    python3 /work/tools/extract_fec.py /game/MOVIES/CLAW.FEC "$OUT/Claw/MOVIES" | tail -1
    cp /work/tools/movies.txt "$OUT/Claw/MOVIES/movies.txt"
else echo "note: no MOVIES/CLAW.FEC: the game runs without cutscenes"; fi
mkdir -p "$OUT/Claw/MUSIC"
cp /game/MUSIC/*.SF2 "$OUT/Claw/MUSIC/" 2>/dev/null || echo "note: no MUSIC/*.SF2: the game runs without music"
chmod -R a+rX "$OUT/Claw"
cp /work/tools/debug.txt.example "$OUT/data/"
python3 /work/tools/make_config.py "$OC/config.xml" "$OUT/port/config.xml"
echo "Done: $OUT  ->  copy to /data/homebrew/$TITLE_ID on the console"
