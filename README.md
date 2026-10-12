# keen4revamped

A remaster of Commander Keen 4: Secret of the Oracle for modern Windows, Linux and macOS.
The plan is to decompile the original `KEEN4E.EXE` ourselves into new C source, port it to
C++20 on SDL3, and then add a 16:9 hi-res renderer with new art on top. Gameplay stays exactly
as in the original.

Status: playable MVP. The whole game runs at the original 320x200 from our own port of the
decompiled code: title, demos, control panel, world map, all levels and enemies, save/load,
high scores, help screens, AdLib music and sound effects. Not ported yet: the scrolling intro
and the story crawl (the title picture stands in for them), and mouse input.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the design and milestones.

## You need your own copy of Keen 4

This repository contains no game data. The port loads the original `.CK4` files from a
copy you own (for example the Steam release). Never commit game files, the original
executable, or anything extracted from it; `.gitignore` blocks the usual names.

## Building

Requires CMake 3.25+ and a C++20 compiler (MSVC 2022, GCC 12+, Clang 15+).

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

To build a Windows `keen4.exe` from Linux or WSL, install MinGW-w64 (`sudo apt install mingw-w64`)
and use `cmake --preset windows-cross && cmake --build --preset windows-cross`. In VS Code under
WSL, the "run on Windows" task builds it and starts it on the Windows host.

SDL3 is fetched and built automatically. Pass `-DKEEN4_WITH_SDL3=OFF` for a headless build
(tests and CI only).

## Decompilation tools

Python scripts in [tools/decomp](tools/decomp) unpack the executable, extract the data
header tables and produce disassembly listings. Progress notes are in
[docs/decomp](docs/decomp).

## Running

Put `KEEN4E.EXE` (version 1.4 EGA) and the `.CK4` files in a folder and run:

```sh
keen4 --data path/to/keen4
```

Without `--data` the game looks in the current folder, `./gamedata`, and on Windows the
default Steam folder. It never writes there: configuration, saved games and high scores go to
`%APPDATA%\keen4revamped` on Windows or `~/.local/share/keen4revamped` elsewhere
(or `--save DIR`).

The default renderer is the remaster (milestone 6): a 16:9 view at the window's resolution,
the upscaled art in `art/` (or the original scaled 4x), and smooth motion between frames.
`--curated` adds the hand-curated art in `art-curated/` on top of it. `--classic` (or F11
while playing) shows the original 320x200 view instead; `--art DIR` (repeatable, first wins)
picks the art folders, F12 reloads them and Shift+F12 shows sprite hitboxes. See [docs/ART.md](docs/ART.md) for making
art with `keen4-extract`, `tools/keen4-upscale` and `tools/keen4-curate`.

Sound is the original AdLib and PC speaker sound on an emulated OPL2. `--remastered-audio`
plays the songs and effects listed in `audio/manifest.txt` instead (`--audio DIR` for another
folder; Shift+F11 then switches between remastered and original), and `--music-volume N` and
`--sfx-volume N` set the volumes. See [docs/AUDIO.md](docs/AUDIO.md).

The game's text is in English or Brazilian Portuguese: the LANGUAGE item of the main menu
switches between them and the choice is saved with the configuration. `--lang en` or
`--lang pt-BR` picks one for a single run. See [docs/LANGUAGES.md](docs/LANGUAGES.md).

Other options: `--fullscreen` (Alt+Enter toggles), `--scale N` (window size), `--nosound`,
`--vsync` (sync to the display; smoother on 70 Hz screens, stutters on 60 Hz ones).
The original's parameters still work, for example `/TEDLEVEL 3 HARD` to jump to a level.

Controls are the original defaults: arrows move, Ctrl jumps, Alt pogos, Space fires,
Enter shows the status window, Esc opens the control panel, F1 shows help.

Gamepads (Xbox, PlayStation, Switch Pro and others SDL knows) work as soon as they are
plugged in, also while the game runs. The game switches to the pad when it connects or a
button is pressed, and back to the keyboard when a key is pressed or the pad is unplugged.
In play: d-pad or left stick moves, A jumps, B pogos, X or Y fires, Back (View) shows the
status window and Start (Menu) opens the control panel. In menus: d-pad or stick moves,
A selects, B or Start goes back. The control panel's Gravis gamepad option remaps jump, pogo,
fire and status to any of A, B, X and Y.

`--headless --frames N --shot-every K --out DIR --keys "tic:scancode:down,..."` runs
without a window and saves frames as PNG, which is how the port is checked in CI and tests.
