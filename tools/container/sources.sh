#!/bin/bash
# OpenClaw at the pinned commit with the PS5 changes (patch + overlay) applied, inside the build volume.
source "$(dirname "$0")/common.sh"
log "sources"
mkdir -p "$TREE"
git config --global --add safe.directory '*' > /dev/null 2>&1 || true
[ -d "$TREE/OpenClaw" ] && [ ! -d "$TREE/OpenClaw/.git" ] && rm -rf "$TREE/OpenClaw"   # older layout (plain copy)
bash /work/tools/apply-openclaw.sh "$TREE/OpenClaw"
