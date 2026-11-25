#!/bin/bash
# Usage: apply-openclaw.sh <target dir>
# Clones OpenClaw at the pinned commit into <target dir> (when missing), resets it, applies ps5/openclaw/patches/*.patch and
# copies ps5/openclaw/overlay/ over it. Safe to run again.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/.." && pwd)
source "$HERE/tools/pins.env"
DEST=${1:?usage: apply-openclaw.sh <target dir>}
if [ ! -d "$DEST/.git" ]; then
    mkdir -p "$(dirname "$DEST")"
    git clone -q "$OPENCLAW_REPO" "$DEST"
fi
git -C "$DEST" cat-file -e "$OPENCLAW_COMMIT^{commit}" 2>/dev/null || git -C "$DEST" fetch -q origin
git -C "$DEST" checkout -q --detach -f "$OPENCLAW_COMMIT"
git -C "$DEST" reset -q --hard "$OPENCLAW_COMMIT"
git -C "$DEST" clean -q -fdx
for p in "$HERE"/ps5/openclaw/patches/*.patch; do git -C "$DEST" apply --whitespace=nowarn "$p"; done
cp -R "$HERE"/ps5/openclaw/overlay/. "$DEST"/
echo "OpenClaw $OPENCLAW_COMMIT + PS5 changes in $DEST"
