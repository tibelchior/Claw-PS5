#!/bin/bash
# Packages the port for sharing WITHOUT the parts that come from the CD: run ./build.sh title first (package is not needed).
# Result: dist/release/Claw-PS5-<date>.zip. The player adds CLAW.REZ, the movies and the SoundFonts from their own CD
# with tools/prepare-game-files.sh logic (see the note inside the zip).
set -euo pipefail
HERE=$(cd "$(dirname "$0")/.." && pwd)
source "$HERE/tools/pins.env"
SRC=$HERE/dist/$TITLE_ID
[ -f "$SRC/eboot.bin" ] || { echo "error: run ./build.sh title first" >&2; exit 1; }
[ -f "$SRC/Claw/ASSETS.ZIP" ] || { echo "error: run ./build.sh package first (it creates ASSETS.ZIP and the config)" >&2; exit 1; }
OUT=$HERE/dist/release/$TITLE_ID
rm -rf "$HERE/dist/release"; mkdir -p "$OUT/Claw/MOVIES" "$OUT/Claw/MUSIC" "$OUT/data"
cp -R "$SRC/eboot.bin" "$SRC/sce_sys" "$SRC/sce_module" "$SRC/port" "$OUT/"
cp "$SRC/Claw/ASSETS.ZIP" "$SRC/Claw/clacon.ttf" "$SRC/Claw/console02.tga" "$SRC/Claw/SAVES.XML" "$OUT/Claw/"
cp "$SRC/Claw/MOVIES/movies.txt" "$OUT/Claw/MOVIES/"
cp "$SRC/data/debug.txt.example" "$OUT/data/"
cp "$HERE/LICENSE" "$OUT/LICENSE.txt"
cat > "$OUT/Claw/PUT-YOUR-GAME-FILES-HERE.txt" <<'TXT'
Copy these from your own Captain Claw CD (nothing from the game is included in this package):
  DATA/CLAW.REZ        ->  Claw/CLAW.REZ
  MUSIC/CLAW4.SF2      ->  Claw/MUSIC/CLAW4.SF2   (and CLAW.SF2; optional, without them there is no music)
  MOVIES/CLAW.FEC      ->  split into Claw/MOVIES/movie00.smk ... movie11.smk (optional; without them there are no cutscenes)
The easiest way is to build from the repository (./build.sh package does all of it). To split CLAW.FEC by hand: python3 tools/extract_fec.py CLAW.FEC Claw/MOVIES
TXT
cat > "$OUT/README.txt" <<'TXT'
Captain Claw (1997) on PS5 (title PPSA99782), based on OpenClaw (GPL-3.0, https://github.com/pjasicek/OpenClaw).
Install: copy this folder to /data/homebrew/PPSA99782, add your game files to Claw/ (see the note there), rescan with ShadowMountPlus.
Controls: D-pad / left stick move, Cross jump / select, Square sword, Circle pistol, Triangle / L1 / R1 change ammo,
L2 alternate ammo key (E), Options pause / back. Space, Enter or Escape equivalents skip cutscenes (Cross or Options).
Settings: port/config.xml (re-copied to data/home/.config/openclaw/ at every start). Log: data/claw.log (previous run claw.prev.log).
Test mode: copy data/debug.txt.example to data/debug.txt (start in any level with cheats); delete it for normal play.
TXT
cd "$HERE/dist/release" && zip -qr "Claw-PS5-$(date +%Y%m%d).zip" "$TITLE_ID" && ls -la
