#!/bin/bash
# SDL2_image (stb_image), SDL2_ttf (bundled freetype), SDL2_mixer (WAV only for now), SDL2_gfx for Prospero, installed into $OUT/prefix.
source "$(dirname "$0")/common.sh"
install_crt
P=$OUT/prefix; DL=$VOL/downloads; mkdir -p "$DL" "$P"
TC=$PS5_SDK_DIR/toolchain/prospero.cmake
fetch() { [ -f "$DL/$2" ] || curl -fsSL -o "$DL/$2" "$1"; rm -rf "$TREE/$3"; mkdir -p "$TREE/$3"; tar -xf "$DL/$2" -C "$TREE/$3" --strip-components=1; }
cm() {  # name srcdir extra...
    local n=$1 s=$2; shift 2
    log "$n"
    cmake -S "$s" -B "$OUT/b-$n" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$TC" -DCMAKE_INSTALL_PREFIX="$P" \
        -DBUILD_SHARED_LIBS=OFF -DCMAKE_PREFIX_PATH="$P" -DCMAKE_FIND_ROOT_PATH="$P" \
        -DCMAKE_C_FLAGS="-I$OUT/sdl2-inc/SDL2 -I$OUT/sdl2-inc -I$P/include" -DCMAKE_CXX_FLAGS="-I$OUT/sdl2-inc/SDL2 -I$OUT/sdl2-inc -I$P/include" \
        "$@" > "$OUT/$n-configure.log" 2>&1 || { tail -30 "$OUT/$n-configure.log"; exit 1; }
    cmake --build "$OUT/b-$n" > "$OUT/$n-build.log" 2>&1 || { grep -E 'error:|FAILED' "$OUT/$n-build.log" | head -20; exit 1; }
    cmake --install "$OUT/b-$n" > /dev/null
}
rm -rf "$OUT"/b-*; cmake --install "$OUT/sdl2" --prefix "$P" > /dev/null
mkdir -p "$OUT/sdl2-inc/SDL2" && cp "$TREE"/SDL2/include/*.h "$OUT/sdl2-inc/SDL2/"
cp "$OUT/sdl2/include-config-release/SDL2/SDL_config.h" "$OUT/sdl2-inc/SDL2/SDL_config.h" 2>/dev/null || true
GH=https://github.com/libsdl-org
fetch $GH/SDL_image/releases/download/release-2.8.2/SDL2_image-2.8.2.tar.gz img.tgz sdl_image
fetch $GH/SDL_ttf/releases/download/release-2.22.0/SDL2_ttf-2.22.0.tar.gz ttf.tgz sdl_ttf
fetch $GH/SDL_mixer/releases/download/release-2.8.0/SDL2_mixer-2.8.0.tar.gz mix.tgz sdl_mixer
fetch https://www.ferzkopp.net/Software/SDL2_gfx/SDL2_gfx-1.0.4.tar.gz gfx.tgz sdl_gfx
OFF=""; for f in AVIF BMP GIF JPG JXL LBM PCX PNM QOI SVG TGA TIF WEBP XCF XPM XV; do OFF="$OFF -DSDL2IMAGE_$f=OFF"; done
cm sdl_image "$TREE/sdl_image" -DSDL2IMAGE_SAMPLES=OFF -DSDL2IMAGE_VENDORED=OFF -DSDL2IMAGE_PNG=ON -DSDL2IMAGE_PNG_SHARED=OFF -DSDL2IMAGE_BACKEND_STB=ON -DSDL2IMAGE_TGA=ON -DSDL2IMAGE_PCX=ON -DSDL2IMAGE_BMP=ON -DSDL2IMAGE_INSTALL=ON
cm sdl_ttf "$TREE/sdl_ttf" -DSDL2TTF_VENDORED=ON -DSDL2TTF_HARFBUZZ=OFF -DSDL2TTF_SAMPLES=OFF -DSDL2TTF_INSTALL=ON -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_BROTLI=ON -DFT_DISABLE_PNG=ON
cm sdl_mixer "$TREE/sdl_mixer" -DSDL2MIXER_VENDORED=OFF -DSDL2MIXER_SAMPLES=OFF -DSDL2MIXER_INSTALL=ON -DSDL2MIXER_DEPS_SHARED=OFF \
    -DSDL2MIXER_CMD=OFF -DSDL2MIXER_FLAC=OFF -DSDL2MIXER_MOD=OFF -DSDL2MIXER_MIDI=OFF -DSDL2MIXER_MP3=OFF -DSDL2MIXER_OPUS=OFF -DSDL2MIXER_VORBIS=OFF -DSDL2MIXER_WAVE=ON -DSDL2MIXER_WAVPACK=OFF -DSDL2MIXER_GME=OFF -DSDL2MIXER_MOD_XMP=OFF -DSDL2MIXER_MIDI_FLUIDSYNTH=OFF -DSDL2MIXER_MIDI_TIMIDITY=OFF -DSDL2MIXER_MIDI_NATIVE=OFF -DSDL2MIXER_OPUS=OFF -DSDL2MIXER_FLAC=OFF
# SDL2_gfx ships autotools only: build its C files directly.
cat > "$TREE/sdl_gfx/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.10)
project(SDL2_gfx C)
add_library(SDL2_gfx STATIC SDL2_framerate.c SDL2_gfxPrimitives.c SDL2_imageFilter.c SDL2_rotozoom.c)
target_include_directories(SDL2_gfx PRIVATE ${CMAKE_PREFIX_PATH}/include/SDL2)
install(TARGETS SDL2_gfx ARCHIVE DESTINATION lib)
install(FILES SDL2_framerate.h SDL2_gfxPrimitives.h SDL2_gfxPrimitives_font.h SDL2_imageFilter.h SDL2_rotozoom.h DESTINATION include/SDL2)
EOF
cm sdl_gfx "$TREE/sdl_gfx"
find "$P" -name '*.a'
