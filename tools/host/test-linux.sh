#!/bin/sh
# Host test of the patched OpenClaw on Linux (Docker, no PS5 needed): builds it, stages your game files, runs it headless.
#   tools/host/test-linux.sh build        fetch + patch OpenClaw into work/OpenClaw, build into work/run
#   tools/host/test-linux.sh run [secs]   run with SDL's dummy video/audio (log on stdout)
# Game files come from GAME_DIR (default ./game-files, the CD's contents).
set -e
HERE=$(cd "$(dirname "$0")/../.." && pwd)
GAME=${GAME_DIR:-$HERE/game-files}
W=$HERE/work
docker image inspect claw-host-test >/dev/null 2>&1 || docker build -q -t claw-host-test -f "$HERE/tools/host/Dockerfile.linux" "$HERE/tools/host"
case ${1:-build} in
  build)
    mkdir -p "$W/run" "$W/build"
    docker run --rm -v "$HERE":/work claw-host-test sh -c 'git config --global --add safe.directory "*"; bash /work/tools/apply-openclaw.sh /work/work/OpenClaw'
    docker run --rm -v "$W/OpenClaw":/src -v "$W/build":/build -v "$W/run":/Build_Release claw-host-test sh -c '
      [ -f /build/Makefile ] || cmake -S /src -B /build -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 >/dev/null
      cmake --build /build -j8 2>&1 | grep -E "error|Linking" | head -40'
    cp "$W/OpenClaw/Build_Release/"{config.xml,clacon.ttf,console02.tga,MENU.xml,SAVES.XML} "$W/run/"
    (cd "$W/OpenClaw/Build_Release/ASSETS" && zip -qr "$W/run/ASSETS.ZIP" .)
    [ -f "$GAME/DATA/CLAW.REZ" ] && ln -f "$GAME/DATA/CLAW.REZ" "$W/run/CLAW.REZ" 2>/dev/null || cp "$GAME/DATA/CLAW.REZ" "$W/run/CLAW.REZ"
    mkdir -p "$W/run/MUSIC"; cp "$GAME"/MUSIC/*.SF2 "$W/run/MUSIC/" 2>/dev/null || true
    [ -f "$GAME/MOVIES/CLAW.FEC" ] && [ ! -f "$W/run/MOVIES/movie00.smk" ] && python3 "$HERE/tools/extract_fec.py" "$GAME/MOVIES/CLAW.FEC" "$W/run/MOVIES" | tail -1 || true
    echo "built: $W/run/openclaw" ;;
  run)
    docker run --rm -v "$W/run":/game -w /game -e SDL_VIDEODRIVER=dummy -e SDL_AUDIODRIVER=dummy claw-host-test timeout ${2:-15} ./openclaw 2>&1 | tail -${LINES_MAX:-40} ;;
esac
