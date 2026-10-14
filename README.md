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

## Installing

Each [release](https://github.com/talkedu/keen4revamped/releases) has installers next to the
plain archives. Neither includes game data; after installing, point the game at your copy as
described in [Running](#running).

- Windows: run `keen4revamped-vX.Y-windows-x64-installer.exe`. It installs to Program Files,
  adds Start menu and desktop shortcuts and an entry in Settings > Apps for uninstalling.
- Linux: run `sh keen4revamped-vX.Y-linux-x64-installer.sh` (no root needed). It installs to
  `~/.local/lib/keen4revamped`, links `keen4` and `keen4-extract` into `~/.local/bin` and adds
  an app-menu entry and a desktop shortcut. `--no-desktop-shortcut` skips the shortcut,
  `--prefix DIR` installs somewhere else, and `~/.local/lib/keen4revamped/uninstall.sh`
  removes it again. The same `install.sh` is in the `.tar.gz`.

Uninstalling removes only the installed files; saved games and the configuration stay.

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

The version is not stored in the source: a build takes it from the latest `v*` git tag
(0.0.0 without one), and `-DKEEN4_VERSION=1.2.3` overrides it.

## Releases

Every pull request merged to main becomes a release. The release workflow bumps the version
from the latest `v*` tag, builds and tests Windows and Linux, and publishes the packages on
the Releases page with the pull request's title and description as the notes. Start the
pull request title with `[MAJOR]`, `[MINOR]` or `[PATCH]` to pick the bump, for example
`[MINOR] Add a level editor`; a title without one is released as a patch.

## Decompilation tools

Python scripts in [tools/decomp](tools/decomp) unpack the executable, extract the data
header tables and produce disassembly listings. Progress notes are in
[docs/decomp](docs/decomp).

## Running

The game needs your own copy of Keen 4: `KEEN4E.EXE` (version 1.4 EGA) and its `.CK4`
files. If you own "Commander Keen" on Steam, just install it there and run `keen4`; the
game files are found automatically. Otherwise point the game at a folder with them:

```sh
keen4 --data path/to/keen4
```

or set `KEEN4_DATA_DIR=path/to/keen4` once in your environment. The game looks, in order:

1. `--data DIR`
2. the `KEEN4_DATA_DIR` environment variable
3. the current folder, then `gamedata/` and `base4/` under it, then the same three next to
   the `keen4` executable
4. `steamapps/common/Commander Keen/base4` in every Steam library: the Steam install itself
   and each extra library listed in its `steamapps/libraryfolders.vdf`. Steam is looked
   for in the registry and `Program Files (x86)\Steam` on Windows; `~/.steam/steam`,
   `~/.local/share/Steam` and the Flatpak and Snap folders on Linux (SteamOS, Bazzite and
   other distributions); `~/Library/Application Support/Steam` on macOS

File names are matched without regard to case, so `Keen4e.exe` and `Audio.ck4` from Steam
work as well as `KEEN4E.EXE` and `AUDIO.CK4` from a DOS copy. If nothing is found, `keen4`
says so and lists the folders it tried. `keen4-extract` searches the same way when `--data`
is left out.

The game data is only read, never changed, and is not part of this project or its releases.
Configuration, saved games and high scores go to `%APPDATA%\keen4revamped` on Windows or
`~/.local/share/keen4revamped` elsewhere (or `--save DIR`, or `KEEN4_SAVE_DIR`).

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

The game's text is in English or Brazilian Portuguese: the LANGUAGE item of the CONFIGURE
card switches between them and the choice is saved with the configuration. `--lang en` or
`--lang pt-BR` picks one for a single run. See [docs/LANGUAGES.md](docs/LANGUAGES.md).

The VIDEO card of the CONFIGURE menu sets SCREEN (full screen or window, also Alt+Enter),
VSYNC, GRAPHICS (upscaled or original, also F11) and, for the upscaled view, RESOLUTION:
AUTO draws it at the window's size, a fixed size draws it at that size and scales it to the
window or screen. In full screen, REFRESH RATE picks one of the rates the display offers at
its desktop resolution (switching the monitor to that mode), or AUTO for the desktop's own.
Released builds start in full screen, local builds in a window. The settings apply at once and are saved with the configuration;
`--fullscreen`/`--windowed`, `--vsync`/`--no-vsync` and `--classic` override them for one run.
The game runs on a 70 Hz clock whatever the display does. With the original graphics and VSYNC
ON, a 70 Hz refresh rate (or 140 Hz) is perfectly smooth, while 60 Hz makes motion uneven, so
turn VSYNC off there. The USE KEYBOARD and joystick items are on the CONTROLS
card.

Other options: `--scale N` (window size), `--nosound`.
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
`--gamepad` and `--gamepad2` script a first and second gamepad the same way.

## Two-player co-op

CO-OP GAME in the main menu starts a game for two: Billy, and his twin brother Tom in a red
helmet, green shirt and gray pants. It needs at least one gamepad. After the difficulty,
player 1 presses A on a gamepad or any key, and player 2 presses A on another gamepad, or
any key if player 1 took a gamepad: one plays on the keyboard and one on a gamepad, or each
on a gamepad of his own.

- Lives are shared, and so are keys, the wetsuit and the rescued council members. Score,
  raindrops, ammo and extra Keens at 20,000 points are each player's own, with a score box
  each.
- A Keen who dies comes back after 3 seconds where his brother last stood on solid ground,
  for a life, and flickers for 2 seconds, unhurt. The level fails when the last Keen still
  in play dies.
- The first Keen to walk off the map waits for the other; the level ends when both are out.
  Rescuing a council member or riding the foot ends it for both.
- The screen splits, side by side or one above the other, when the Keens are too far apart
  for one view, and joins again when they come back together.
- If a player's gamepad is unplugged the game pauses until one is plugged in. Co-op games
  save and load like any other.

`/TEDLEVEL n COOP` plays one level in co-op, player 1 on the keyboard and player 2 on the
first gamepad; `/TEDLEVEL n COOPPAD` the other way round.

## Updating

The main menu shows the game's version on its title line. CHECK FOR UPDATES, on the CONFIGURE
card, compares it with the latest release on GitHub and, if there is a newer one, downloads it
with a progress bar, puts its files in place of the running game's and restarts. It is greyed
out during a game.

The repository is private, so the first check signs in to GitHub: the game opens
github.com/login/device in the browser and shows a code to enter there (it is also on the
clipboard). The account needs access to the repository. The token is kept in
`github-token.txt` next to `CONFIG.CK4`; delete it to sign out. On Windows, a game installed
in Program Files asks for administrator rights to replace its files; on Linux the game folder
must be writable (the installer's `~/.local/lib/keen4revamped` is). Linux needs libcurl, which
desktop distributions have.

Builds sign in with a GitHub OAuth app or GitHub App that has device flow enabled. A GitHub
App needs the Contents (read-only) repository permission and must be installed on the
repository; turn off user token expiration in its optional features, or players sign in again
every 8 hours. Its client id is set when
configuring, `-DKEEN4_GITHUB_CLIENT_ID=...` (CI and releases take the repository variable
`KEEN4_GITHUB_CLIENT_ID`); without one the item says updates are off. `KEEN4_GITHUB_CLIENT_ID`,
`KEEN4_UPDATE_REPO` (owner/name) and `KEEN4_GITHUB_TOKEN` in the environment override the
client id, the repository and the saved token.
