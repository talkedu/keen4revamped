#!/bin/bash
# Commander Keen 4 Revamped for Linux handhelds (Anbernic RG35XX, RG40XX,
# RG353 and the like), as a PortMaster port. It also runs without PortMaster:
# keep this script next to the keen4revamped folder.
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
  [ -f "$controlfolder/mod_${CFW_NAME}.txt" ] && source "$controlfolder/mod_${CFW_NAME}.txt"
  get_controls
fi

# The game folder sits next to this script, wherever the firmware keeps its
# ports (ports, PORTS, Roms/PORTS, on either SD card); PortMaster's usual
# /$directory/ports is only a fallback.
here="$(cd "$(dirname "$0")" && pwd)"
GAMEDIR="$here/keen4revamped"
if [ ! -x "$GAMEDIR/keen4" ] && [ ! -f "$GAMEDIR/keen4" ] && [ -n "$directory" ]; then
  GAMEDIR="/$directory/ports/keen4revamped"
fi

if ! cd "$GAMEDIR"; then
  echo "keen4revamped: no game folder at $GAMEDIR" > "$here/keen4revamped-log.txt"
  exit 1
fi
> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1
echo "keen4revamped: $GAMEDIR, firmware ${CFW_NAME:-unknown}, device ${DEVICE_NAME:-unknown}"

# The game reads the pad itself (SDL3 gamepads), so there is no gptokeyb
# mapping; PortMaster's description of this device's buttons is enough.
[ -n "$sdl_controllerconfig" ] && export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

export KEEN4_DATA_DIR="$GAMEDIR/gamedata"
export KEEN4_SAVE_DIR="$GAMEDIR/saves"
mkdir -p "$KEEN4_DATA_DIR" "$KEEN4_SAVE_DIR"

# Upscaled art in art/ and remastered audio in audio/ are picked up from here
# as on the desktop (see docs/ART.md and docs/AUDIO.md).
args=(--fullscreen)
[ -d audio ] && args+=(--remastered-audio)

chmod +x ./keen4 2>/dev/null
if type pm_platform_helper >/dev/null 2>&1; then
  pm_platform_helper "$GAMEDIR/keen4"
fi

./keen4 "${args[@]}"
status=$?

if type pm_finish >/dev/null 2>&1; then
  pm_finish
fi
exit $status
