#!/bin/bash
# Commander Keen 4 Revamped for Linux handhelds (Anbernic RG35XX, RG40XX,
# RG353 and the like), as a PortMaster port. It also runs without PortMaster
# (the stock firmware of some devices): keep this script next to the
# keen4revamped folder.
#
# Your Keen 4 files (KEEN4E.EXE v1.4 EGA and the .CK4 files) go in
# keen4revamped/gamedata. Saves and settings go in keen4revamped/saves.

# some firmwares start ports with sh, not bash
if [ -z "$BASH_VERSION" ]; then exec bash "$0" "$@"; fi

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

if [ -f "$controlfolder/control.txt" ]; then
  source "$controlfolder/control.txt"
  [ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
  get_controls
fi

# PortMaster's folder first. Firmwares without PortMaster keep their ports
# elsewhere (PORTS, Roms/PORTS, on either SD card), so the folder next to this
# script is the fallback.
here="$(cd "$(dirname "$0")" && pwd)"
GAMEDIR="/$directory/ports/keen4revamped"
if [ -z "$directory" ] || [ ! -f "$GAMEDIR/keen4" ]; then
  GAMEDIR="$here/keen4revamped"
fi

if ! cd "$GAMEDIR"; then
  echo "keen4revamped: no game folder at $GAMEDIR" > "$here/keen4revamped-log.txt"
  exit 1
fi
> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1
echo "keen4revamped: $GAMEDIR, firmware ${CFW_NAME:-unknown}, device ${DEVICE_NAME:-unknown}," \
  "screen ${DISPLAY_WIDTH:-?}x${DISPLAY_HEIGHT:-?}"

# The game reads the pad itself (SDL3 gamepads), with PortMaster's description
# of this device's buttons.
[ -n "$sdl_controllerconfig" ] && export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

export KEEN4_DATA_DIR="$GAMEDIR/gamedata"
export KEEN4_SAVE_DIR="$GAMEDIR/saves"
mkdir -p "$KEEN4_DATA_DIR" "$KEEN4_SAVE_DIR"
# The files may sit in a subfolder, such as Steam's base4 copied as a whole.
exe=$(find "$KEEN4_DATA_DIR" -maxdepth 4 -type f -iname keen4e.exe 2>/dev/null | head -n 1)
if [ -n "$exe" ]; then
  KEEN4_DATA_DIR=$(dirname "$exe")
else
  echo "keen4revamped: no KEEN4E.EXE in $KEEN4_DATA_DIR. Its files are:"
  ls -laR "$KEEN4_DATA_DIR" 2>&1 | head -n 40
fi
echo "keen4revamped: game data in $KEEN4_DATA_DIR"

# Upscaled art in art/ and remastered audio in audio/ are picked up from here
# as on the desktop (see docs/ART.md and docs/AUDIO.md).
args=(--fullscreen)
[ -d audio ] && args+=(--remastered-audio)

# On a 4:3 or square screen (640x480, 720x720) the 16:9 view leaves wide black
# bars, while the original 4:3 view fills the screen. Without upscaled art,
# the first run starts in the original view; the game saves the choice, and
# VIDEO in the menu switches it back.
if [ ! -f "$KEEN4_SAVE_DIR/CONFIG.CK4" ] && [ ! -f art/manifest.txt ] &&
   [ -n "$DISPLAY_WIDTH" ] && [ -n "$DISPLAY_HEIGHT" ] &&
   [ $((DISPLAY_WIDTH * 3)) -le $((DISPLAY_HEIGHT * 4)) ]; then
  args+=(--classic)
fi

chmod +x ./keen4 2>/dev/null

# gptokeyb only for PortMaster's Select+Start exit: its mapping turns every
# button off, so the game does not also get them as keys.
if [ -n "$GPTOKEYB" ]; then
  $GPTOKEYB "keen4" -c "$GAMEDIR/keen4revamped.gptk" &
fi
if type pm_platform_helper >/dev/null 2>&1; then
  pm_platform_helper "$GAMEDIR/keen4"
fi

./keen4 "${args[@]}"
status=$?

if type pm_finish >/dev/null 2>&1; then
  pm_finish  # also stops gptokeyb
fi
exit $status
