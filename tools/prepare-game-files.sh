#!/bin/bash
# Extracts what the port needs from your Captain Claw CD image into ./game-files (the default GAME_DIR of build.sh).
#   tools/prepare-game-files.sh /path/to/CLAW.iso      (needs bsdtar or 7z)
set -euo pipefail
HERE=$(cd "$(dirname "$0")/.." && pwd)
ISO=${1:?usage: prepare-game-files.sh <CLAW.iso or a folder with the CD contents>}
OUT=$HERE/game-files
mkdir -p "$OUT"
if [ -d "$ISO" ]; then
    for d in DATA MOVIES MUSIC; do [ -d "$ISO/$d" ] && cp -R "$ISO/$d" "$OUT/"; done
elif command -v bsdtar > /dev/null; then
    bsdtar -xf "$ISO" -C "$OUT" DATA MOVIES MUSIC
elif command -v 7zz > /dev/null || command -v 7z > /dev/null; then
    "$(command -v 7zz || command -v 7z)" x -y -o"$OUT" "$ISO" DATA MOVIES MUSIC > /dev/null
else
    echo "need bsdtar or 7z to read the image" >&2; exit 1
fi
chmod -R u+rw "$OUT"
ls -la "$OUT/DATA/CLAW.REZ" "$OUT/MOVIES/CLAW.FEC" "$OUT/MUSIC/"*.SF2
