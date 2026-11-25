#!/bin/bash
# Packs the built title into dist/Claw-PS5-v<version>.zip: the title only, with none of the game's files.
#   tools/package-release.sh 1.0.0
# Run ./build.sh title and ./build.sh package first. The zip holds one folder, PPSA99782:
#   eboot.bin, sce_sys/, sce_module/, port/config.xml, Claw/ (OpenClaw's own assets, the movie list, extract_fec.py)
# The player adds CLAW.REZ, the SoundFonts and the movies from their own CD (see the note inside).
set -euo pipefail
HERE=$(cd "$(dirname "$0")/.." && pwd)
source "$HERE/tools/pins.env"
VERSION=${1:?usage: tools/package-release.sh <version>}
SRC=$HERE/dist/$TITLE_ID
[ -f "$SRC/eboot.bin" ] || { echo "error: no title: run ./build.sh title first" >&2; exit 1; }
[ -f "$SRC/port/config.xml" ] && [ -f "$SRC/Claw/ASSETS.ZIP" ] || { echo "error: run ./build.sh package first" >&2; exit 1; }
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
T=$STAGE/$TITLE_ID
mkdir -p "$T/port" "$T/Claw/MOVIES" "$T/Claw/MUSIC" "$T/data"
cp "$SRC/eboot.bin" "$T/"
cp -R "$SRC/sce_sys" "$SRC/sce_module" "$T/"
cp "$SRC/port/config.xml" "$T/port/"
cp "$SRC/Claw/ASSETS.ZIP" "$SRC/Claw/clacon.ttf" "$SRC/Claw/console02.tga" "$SRC/Claw/SAVES.XML" "$T/Claw/"
cp "$HERE/tools/movies.txt" "$T/Claw/MOVIES/"
cp "$HERE/tools/extract_fec.py" "$T/Claw/"
cp "$HERE/tools/debug.txt.example" "$T/data/"
cat > "$T/Claw/PUT_THE_GAME_FILES_HERE.txt" <<'TXT'
Copy these from your own Captain Claw CD into this folder:

  DATA/CLAW.REZ       ->  CLAW.REZ                (required)
  MUSIC/CLAW4.SF2     ->  MUSIC/CLAW4.SF2         (music)
  MUSIC/CLAW.SF2      ->  MUSIC/CLAW.SF2          (music, smaller fallback)
  MOVIES/CLAW.FEC     ->  split into MOVIES/      (cutscenes), on your computer, from this folder:
                          python3 extract_fec.py /path/to/CD/MOVIES/CLAW.FEC MOVIES
                          (gives MOVIES/movie00.smk ... movie11.smk)

When done, this folder should look like this:

PPSA99782/Claw/
├── CLAW.REZ              (from your CD: DATA/CLAW.REZ)
├── ASSETS.ZIP            (in the release)
├── SAVES.XML             (in the release)
├── clacon.ttf            (in the release)
├── console02.tga         (in the release)
├── PUT_THE_GAME_FILES_HERE.txt   (this note)
├── extract_fec.py        (in the release, only needed to split the movies)
├── MOVIES/
│   ├── movies.txt        (in the release)
│   ├── movie00.smk       (movie00 to movie11: split from your CD's MOVIES/CLAW.FEC)
│   ├── movie01.smk
│   ├── movie02.smk
│   ├── movie03.smk
│   ├── movie04.smk
│   ├── movie05.smk
│   ├── movie06.smk
│   ├── movie07.smk
│   ├── movie08.smk
│   ├── movie09.smk
│   ├── movie10.smk
│   └── movie11.smk
└── MUSIC/
    ├── CLAW4.SF2         (from your CD: MUSIC/CLAW4.SF2)
    └── CLAW.SF2          (from your CD: MUSIC/CLAW.SF2)

File names must match exactly, including upper and lower case. Without CLAW.REZ the game does not start; without the
movies there are no cutscenes; without the .SF2 files there is no music.
TXT
find "$STAGE" -name .DS_Store -delete
OUT=$HERE/dist/Claw-PS5-v$VERSION.zip
rm -f "$OUT"
(cd "$STAGE" && zip -qr "$OUT" "$TITLE_ID")
echo "wrote $OUT ($(du -h "$OUT" | cut -f1))"
unzip -l "$OUT" | tail -n +4 | head -30
