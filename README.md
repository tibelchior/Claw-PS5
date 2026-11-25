# Captain Claw (1997) on PS5

A PS5 port of the 1997 platformer *Captain Claw* (Monolith), built on [OpenClaw](https://github.com/pjasicek/OpenClaw),
the open-source C++/SDL2 reimplementation of the game. The result is a signed homebrew title (`PPSA99782`) that plays
the full game from your own copy of the CD, in 16:9, with the original cutscenes and music, using the pad.

> **Bring your own game files.** This repository contains no game data. You need your own Captain Claw CD (or its image).
> Claw has been abandonware for decades, but the rights still belong to their owners: see [LICENSING.md](LICENSING.md).

## What the port does

| | |
| --- | --- |
| Engine | OpenClaw (pinned commit), unchanged game logic, plus the small changes listed below |
| Video | SDL2 software renderer into a 1728x972 frame (16:9 widescreen view of 864x486 game pixels, like the community widescreen mod), presented through Vulkan (RADV) at 1080p |
| Audio | SDL2/SDL_mixer to the console's audio output; MIDI music through a built-in software synthesizer using the CD's own SoundFonts |
| Cutscenes | The 12 Smacker movies inside `MOVIES/CLAW.FEC`, played by the engine, skippable |
| Input | DualSense: the pad is turned into the keyboard keys OpenClaw expects |
| Exit | Quitting asks the system to close the title (no error screen) |

### Changes to OpenClaw
Everything lives in [`ps5/openclaw/`](ps5/openclaw): a patch (`patches/0001-ps5-port.patch`, about 50 lines) and new files (`overlay/`).

* `Engine/Video/MoviePlayer.*`, `ThirdParty/libsmacker` (LGPL-2.1): cutscene player; the engine did not play movies at all.
* `Engine/Audio/MidiSynth.*`, `ThirdParty/TinySoundFont` (MIT): MIDI music. SDL_mixer has no MIDI backend on the console.
* `BaseGameLogic.cpp`: starts the cutscenes (startup logo, level intros; not after dying), reads startup commands from the
  file named in `CLAW_STARTUP_COMMANDS` (the console cannot `chdir`).
* `EnemyAIStateComponent.cpp`: **crash fix** for an upstream bug (attack-action index read one past the end of a vector).
* `ControllableComponent.cpp`, `ClawGameLogic.cpp`: three log lines around Claw's death.

### What the `improvements/` folder of the community is for
* *No-CD EXE*: not needed, OpenClaw only reads `CLAW.REZ`.
* *Widescreen mod*: it patches the original EXE. Not used; the engine's render scale gives the same 864x486 view
  (`tools/make_config.py`). The mod's extra level files are not used either.

## Requirements
* macOS or Linux with Docker (the toolchain runs in a Fedora container; on Apple silicon it cross-compiles natively).
* About 15 GB of disk for the toolchain volume and 1 to 2 hours the first time (RADV and the PS5 SDK are built once and
  cached in a Docker volume, shared with the other ports of this stack).
* A PS5 that runs homebrew titles from `/data/homebrew` (the port is built with the [mihawk-99](https://github.com/mihawk-99) PS5
  toolchain: PS5_Vulkan, PS5_PayloadSDK, PS5_Mesa) and ShadowMountPlus (or an equivalent) to mount it.
* Your Captain Claw CD image (the retail CD; the GOG/Steam releases are untested).

## Build

```bash
git clone <this repository> && cd Claw-PS5

# 1. your game files: the CD's DATA, MOVIES and MUSIC folders go to ./game-files
tools/prepare-game-files.sh /path/to/CLAW.iso        # needs bsdtar or 7z

# 2. everything: toolchain, SDL2, libraries, OpenClaw, link/sign, game files
./build.sh all
```

Steps can be run on their own: `toolchain`, `sdl2`, `libs`, `compile`, `title`, `package` (see the top of [`build.sh`](build.sh)).
Options: `GAME_DIR=<folder>` (default `./game-files`), `BUILD_VOLUME=<docker volume>` (default `dirt2ps5-build-data`, shared with the
DiRT/Motorhead/UT99 ports; a new name builds a separate toolchain). All upstream revisions are pinned in
[`tools/pins.env`](tools/pins.env). Artwork: `art/icon.png` (1000x1000) and `art/wallpaper.png` (2752x1536).

Result: `dist/PPSA99782/`. For a release zip without any CD content: `tools/package-release.sh 1.0.0` makes `dist/Claw-PS5-v1.0.0.zip`.

## Install
Copy `dist/PPSA99782` to `/data/homebrew/PPSA99782` on the console and rescan with ShadowMountPlus.

```
PPSA99782/
  eboot.bin, sce_sys/, sce_module/    the title
  Claw/                               read only: CLAW.REZ, ASSETS.ZIP, clacon.ttf, console02.tga, SAVES.XML,
                                      MOVIES/ (movie00..11.smk + movies.txt), MUSIC/ (CLAW4.SF2, CLAW.SF2)
  port/config.xml                     the config; copied to data/home/.config/openclaw/ at every start
  data/                               everything the game writes
    claw.log                          the one log (the previous run is claw.prev.log)
    home/.config/openclaw/            config.xml and SAVES.XML (your progress)
    debug.txt.example                 see "Testing"
```
If `/app0/data` is not writable the port falls back to `/data/claw`.

## Controls

| Pad | Game |
| --- | --- |
| D-pad, left stick | Move, climb, menu navigation |
| Cross | Jump, menu select |
| Square | Sword (Left Ctrl) |
| Circle | Pistol (Left Alt) |
| Triangle, L1, R1 | Change ammo (Left Shift) |
| L2 | `E` (ammo change when "alternate controls" are on) |
| Options | Pause, menu back (Escape) |
| Cross / Options | Skip a cutscene |

## Configuration
* `port/config.xml` is generated by [`tools/make_config.py`](tools/make_config.py) from OpenClaw's stock config. Display size
  (default `1728x972` at `Scale 2` = a 864x486 view). For a cheaper frame use `864x486` with `Scale 1`, for a native 1080p frame
  `1920x1080` with `Scale 2.2222222`. Music volume is capped at 20 by the engine.
* `Claw/MOVIES/movies.txt` says which cutscene plays where (`startup N`, `level L N`). **The default mapping is a best guess** from
  the pictures: the CD's index inside `CLAW.FEC` is encrypted. Movies 0 (logo) and 1 (intro story) are certain; 2-7 are placed
  at the level whose scenery they show; 8-11 (credits/ending candidates) are not assigned. Edit and retest freely.

## Testing other levels
Copy `data/debug.txt.example` to `data/debug.txt` on the console:
```
level 5            # start straight in level 5 (1..13), no menu
invincible on      # console commands run at level start
infinitejump on
```
Other commands: `catnip`, `firesword`, `frostsword`, `lightningsword`, `invulnerable`, `invisible` (30 s powerups) and `teleport X Y`.
Delete the file for normal play (the launcher also turns on OpenClaw's "all level saves" while it exists).

## Host test (no console)
`tools/host/test-linux.sh build` fetches and patches OpenClaw into `work/` and builds it on Linux in Docker;
`tools/host/test-linux.sh run` starts it headless with SDL's dummy drivers. Handy to check a change of the engine patch
before a console build.

## Project layout
```
build.sh                       entry point (Docker)
tools/pins.env                 pinned upstream revisions, title id
tools/container/*.sh           steps that run inside the container (toolchain, sdl2, libs, sources, compile, title, package)
tools/apply-openclaw.sh        pinned OpenClaw + patch + overlay
tools/package-release.sh       release zip (title only)
tools/extract_fec.py           splits CLAW.FEC into its Smacker movies
tools/make_config.py           the port's config.xml
tools/host/                    Linux host test
ps5/glue/                      title glue: launcher (paths, config, debug), Vulkan presenter, pad + audio (scePad / sceAudioOut), keyboard overlay
ps5/patches/sdl2/              SDL2 changes: dummy video/audio hand frames and sound to the presenter / sceAudioOut
ps5/openclaw/                  the changes to OpenClaw (patch + overlay)
ps5/*.c                        libc compatibility shims for the console
ps5/title/param.json           title metadata
```
The PS5 side (glue, SDL2 patches, compatibility shims) is shared with the sibling ports UT99-PS5 and Motorhead-PS5.

## Known issues / not done
* Not tested: most levels and bosses on the console. Level 1 has been played; jumping straight into other levels (`debug.txt`) crashed once on a
  missing start save, which is fixed but not yet confirmed on the console. Please attach `data/claw.log` to reports.
* The options pages (players, difficulty, controls, display) are not implemented in OpenClaw itself.
* The music volume factor (`GainFor` in `MidiSynth.cpp`) is tuned by ear.
* The `Unknown ammo type` lines in the log come from upstream and are harmless as far as seen.
* Not in OpenClaw yet: level 14, the original game's replay-movies menu.
* No network or save-game cloud features.

## Credits
OpenClaw by pjasicek and contributors; Captain Claw by Monolith Productions; libsmacker by Greg Kennedy; TinySoundFont by
Bernhard Schelling; SDL2 and its libraries by the SDL project; the PS5 toolchain (PS5_Vulkan, PS5_PayloadSDK, PS5_Mesa) by mihawk-99.

## Licence
GPL-3.0, like OpenClaw (see [LICENSE](LICENSE) and [LICENSING.md](LICENSING.md)).
