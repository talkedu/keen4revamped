#!/bin/sh
# Packs an anbernic-preset build as a PortMaster port zip, which players unzip
# into the handheld's ports folder:
#
#   Commander Keen 4 Revamped.sh       launcher (goes in the ports folder)
#   keen4revamped/keen4                the game, Linux aarch64
#   keen4revamped/keen4revamped.gptk   gptokeyb with every button off (exit only)
#   keen4revamped/gamedata/            where players copy their Keen 4 files
#   keen4revamped/port.json            PortMaster metadata
#   keen4revamped/README.md, docs/, licenses/
#
# --portmaster DIR also writes the folder a PortMaster-New pull request adds
# as ports/keen4revamped/: port.json, README.md, gameinfo.xml, the launcher
# and the keen4revamped folder. screenshot.png (and an optional cover.png)
# are captured on a device and added by hand; see packaging/anbernic/portmaster.
#
# Usage: make-package.sh [--portmaster DIR] BUILD_DIR OUTPUT.zip [UPDATE.tar.gz]
#        (run from the source tree)
set -eu

pmdir=
if [ "${1:-}" = --portmaster ]; then
  pmdir=$2
  shift 2
fi
build=$1
out=$2
tarball=${3:-}
launcher="Commander Keen 4 Revamped.sh"
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT

port="$stage/keen4revamped"
mkdir -p "$port/gamedata" "$port/docs" "$port/licenses"
cp "$build/keen4" "$port/keen4"
cp "packaging/anbernic/$launcher" "$stage/$launcher"
chmod +x "$port/keen4" "$stage/$launcher"
cp packaging/anbernic/port.json README.md packaging/anbernic/portmaster/keen4revamped.gptk "$port"
cp packaging/anbernic/gamedata-README.txt "$port/gamedata/README.txt"
cp docs/ART.md docs/AUDIO.md docs/LANGUAGES.md "$port/docs"
sh packaging/copy-licenses.sh "$build" "$port/licenses"

rm -f "$out"
case $out in /*) dest=$out ;; *) dest="$PWD/$out" ;; esac
(cd "$stage" && zip -qr -X "$dest" "$launcher" keen4revamped)
echo "wrote $out"

# The port folder alone, as a .tar.gz, for the in-game updater of older
# handheld releases (it unpacks with tar, which on these devices may not read
# .zip). Current handheld builds have no updater (KEEN4_UPDATER=OFF).
if [ -n "$tarball" ]; then
  case $tarball in /*) dest=$tarball ;; *) dest="$PWD/$tarball" ;; esac
  tar -czf "$dest" -C "$stage" keen4revamped
  echo "wrote $tarball"
fi

if [ -n "$pmdir" ]; then
  rm -rf "$pmdir"
  mkdir -p "$pmdir"
  cp -R "$port" "$pmdir/keen4revamped"
  rm "$pmdir/keen4revamped/port.json"  # PortMaster keeps it next to the launcher
  cp "$stage/$launcher" packaging/anbernic/port.json \
     packaging/anbernic/portmaster/README.md packaging/anbernic/portmaster/gameinfo.xml "$pmdir"
  echo "wrote $pmdir"
  echo "  add screenshot.png (4:3 gameplay, 640x480 or more) to it and to keen4revamped/, and optionally cover.png"
fi
