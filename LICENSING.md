# Licences

The port's code is released under the GNU GPL v3 (see `LICENSE`), because it is built on and distributed with OpenClaw, which is GPL-3.0.
That covers only what the port's authors wrote. Everything below belongs to its owners and stays under their terms.

## What this repository contains

| Path | Is | Terms |
| --- | --- | --- |
| `build.sh`, `tools/`, `docker/`, `ps5/glue/`, `ps5/*.c`, `ps5/patches/sdl2/`, `ps5/title/` | The port's own scripts and code (parts follow the sibling PS5 ports on the same stack) | GPL-3.0 |
| `ps5/openclaw/patches/`, `ps5/openclaw/overlay/Engine/` | Changes to / additions for OpenClaw | GPL-3.0 (same as OpenClaw) |
| `ps5/openclaw/overlay/ThirdParty/libsmacker/` | libsmacker, Greg Kennedy | LGPL-2.1 (`COPYING` inside) |
| `ps5/openclaw/overlay/ThirdParty/TinySoundFont/` | TinySoundFont / TinyMidiLoader, Bernhard Schelling | MIT (`LICENSE` inside) |
| `art/` | Home-screen icon and wallpaper that show Captain Claw's logo and artwork | Monolith's / the rights holders' artwork, used as the title's art only |

## What is not here, and where it comes from

| Part | Project | Licence |
| --- | --- | --- |
| The engine | [OpenClaw](https://github.com/pjasicek/OpenClaw), fetched at the commit in `tools/pins.env` | GPL-3.0 |
| `ASSETS.ZIP`, `clacon.ttf`, `console02.tga`, `SAVES.XML`, `config.xml` in a built title | Taken from OpenClaw's `Build_Release` at build time | As OpenClaw's repository |
| Game data: `CLAW.REZ`, the movies (`CLAW.FEC`), the SoundFonts (`CLAW.SF2`, `CLAW4.SF2`) | Captain Claw (1997), Monolith Productions (now part of Warner Bros.) | Rights holders'. **Never part of this repository or a release**; each player adds their own copy |
| SDL2 2.28.5, SDL2_image, SDL2_ttf, SDL2_mixer | libsdl.org | zlib |
| SDL2_gfx | ferzkopp | zlib |
| FreeType (inside SDL2_ttf) | FreeType project | FreeType licence / GPLv2 |
| The Vulkan driver | RADV from [PS5_Mesa](https://github.com/mihawk-99/PS5_Mesa), linked by [PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan) | MIT (Mesa); the stack's tools GPL-3.0 |
| The platform layer and SDK | [PS5_PayloadSDK](https://github.com/mihawk-99/PS5_PayloadSDK) | GPL-3.0 |

## Releases

A release zip (`tools/release.sh`) holds a built `eboot.bin`, OpenClaw's own assets and the config, but none of the CD's content. It links the GPL-3.0
platform layer, whose source is the PS5_PayloadSDK project linked above, and OpenClaw, whose source is this repository's pinned commit plus
`ps5/openclaw/`. If a rights holder objects to the release or to the artwork, it is withdrawn.
