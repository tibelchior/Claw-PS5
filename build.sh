#!/bin/bash
# Builds Captain Claw (OpenClaw) for the PS5 from the OpenClaw source and your own copy of the game.
#
#   ./build.sh toolchain   check/build the PS5 toolchain (cached in a Docker volume; 1-2 hours the first time)
#   ./build.sh sdl2        SDL2 for Prospero
#   ./build.sh libs        SDL2_image / ttf / mixer / gfx
#   ./build.sh compile     fetch OpenClaw, apply the PS5 changes, cross-compile it
#   ./build.sh title       everything above + link, convert and sign: dist/PPSA99782/eboot.bin
#   ./build.sh package     add the game files from your CD (needs GAME_DIR, see README) and the port's config
#   ./build.sh all         title + package
#
# Options (environment):
#   GAME_DIR=<folder>      the CD's contents (DATA/CLAW.REZ, MOVIES/CLAW.FEC, MUSIC/*.SF2), default ./game-files
#   BUILD_VOLUME=<name>    Docker volume with the toolchain and build trees (default dirt2ps5-build-data, shared with the
#                          other PS5 ports of this stack; any name works, a new volume builds the toolchain first)
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
source "$HERE/tools/pins.env"
STEP=${1:-title}
IMAGE=dirt2ps5-build VOLUME=${BUILD_VOLUME:-dirt2ps5-build-data} NAME=clawps5
GAME=${GAME_DIR:-$HERE/game-files}
die() { echo "error: $*" >&2; exit 1; }
command -v docker > /dev/null || die "Docker is not installed."
docker info > /dev/null 2>&1 || die "Docker is not running."
docker volume inspect "$VOLUME" > /dev/null 2>&1 || docker volume create "$VOLUME" > /dev/null
docker image inspect "$IMAGE" > /dev/null 2>&1 || docker build -q -t "$IMAGE" "$HERE/docker" > /dev/null
docker rm -f "$NAME" > /dev/null 2>&1 || true
mkdir -p "$HERE/dist" "$HERE/art" "$GAME"
docker run -d --name "$NAME" -v "$VOLUME":/build -v "$HERE":/work -v "$GAME":/game:ro "$IMAGE" sleep infinity > /dev/null
trap 'docker rm -f "$NAME" > /dev/null 2>&1 || true' EXIT
run() { docker exec -e VOL=/build "$NAME" bash "/work/tools/container/$1.sh"; }
case $STEP in
    toolchain) run toolchain ;;
    sdl2) run toolchain; run sources; run sdl2 ;;
    libs) run toolchain; run sources; run sdl2; run libs ;;
    compile) run toolchain; run sources; run sdl2; run libs; run compile ;;
    title) run toolchain; run sources; run sdl2; run libs; run compile; run title ;;
    package) run sources; run package ;;
    all) run toolchain; run sources; run sdl2; run libs; run compile; run title; run package ;;
    *) die "unknown step '$STEP' (toolchain, sdl2, libs, compile, title, package, all)" ;;
esac
