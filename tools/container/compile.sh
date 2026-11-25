#!/bin/bash
# Cross-compiles OpenClaw (+ libwap, Box2D, tinyxml) as static objects/libs for Prospero. The stock exe link is expected to fail; title.sh links.
source "$(dirname "$0")/common.sh"
install_crt
P=$OUT/prefix; B=$OUT/cmake; mkdir -p "$B"
log "configure"
cmake -S "$TREE/OpenClaw" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$PS5_SDK_DIR/toolchain/prospero.cmake" \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_PREFIX_PATH="$P" \
    -DCMAKE_CXX_FLAGS="-I$P/include -I$P/include/SDL2 -DCLAW_PS5 -fpermissive -w" -DCMAKE_C_FLAGS="-I$P/include -I$P/include/SDL2 -w" \
    > "$OUT/configure.log" 2>&1 || { tail -40 "$OUT/configure.log"; exit 1; }
log "build"
# The stock exe link cannot succeed (no SDL for the console in its search path); title.sh links. Anything else failing is an error.
if ! cmake --build "$B" -- -k 0 > "$OUT/build.log" 2>&1; then
    if grep -E '^FAILED' "$OUT/build.log" | grep -qv 'Build_Release/openclaw'; then
        grep -E 'error:|FAILED' "$OUT/build.log" | sort | uniq -c | sort -rn | head -40; echo "see $OUT/build.log (in the volume)"; exit 1
    fi
fi
find "$B" -name '*.a' | sed "s|$B/||"
