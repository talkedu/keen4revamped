#!/bin/sh
# Installs keen4revamped for the current user (no root needed):
#
#   ~/.local/lib/keen4revamped/      the game, keen4-extract, docs and tools
#   ~/.local/bin/keen4, keen4-extract
#   an app-menu entry and a desktop shortcut, both with the game icon
#
# Run it from the unpacked linux-x64 release folder, or run the
# keen4revamped-*-linux-x64-installer.sh release asset, which calls it.
#
#   ./install.sh [--prefix DIR] [--no-desktop-shortcut]
#   ./install.sh --uninstall [--prefix DIR]
#
# Game data is not part of the package and is never touched: keen4 finds your
# copy of Keen 4 on its own or with --data (see README.md). Saved games and the
# configuration in ~/.local/share/keen4revamped stay when you uninstall.
set -eu

prefix="$HOME/.local"
desktop_shortcut=1
uninstall=0
while [ $# -gt 0 ]; do
  case "$1" in
    --prefix) prefix="$2"; shift ;;
    --prefix=*) prefix="${1#--prefix=}" ;;
    --no-desktop-shortcut) desktop_shortcut=0 ;;
    --uninstall) uninstall=1 ;;
    -h|--help) sed -n '2,17p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "install.sh: unknown option $1 (try --help)" >&2; exit 2 ;;
  esac
  shift
done

app="$prefix/lib/keen4revamped"
bin="$prefix/bin"
data_home="${XDG_DATA_HOME:-$HOME/.local/share}"
[ "$prefix" = "$HOME/.local" ] || data_home="$prefix/share"
menu_entry="$data_home/applications/keen4revamped.desktop"
icon="$data_home/icons/hicolor/256x256/apps/keen4revamped.png"
desktop_dir="$(xdg-user-dir DESKTOP 2>/dev/null || true)"
[ -n "$desktop_dir" ] || desktop_dir="$HOME/Desktop"
desktop_link="$desktop_dir/keen4revamped.desktop"

refresh_menus() {
  command -v update-desktop-database >/dev/null 2>&1 &&
    update-desktop-database -q "$data_home/applications" 2>/dev/null || true
}

if [ "$uninstall" = 1 ]; then
  # Only what install.sh put there; anything else in $app (for example game
  # files copied in by hand) stays, and then so does the folder.
  for link in "$bin/keen4" "$bin/keen4-extract"; do
    case "$(readlink "$link" 2>/dev/null)" in "$app"/*) rm -f "$link" ;; esac
  done
  rm -f "$menu_entry" "$icon" "$desktop_link"
  rm -f "$app/keen4" "$app/keen4-extract" "$app/README.md" "$app/keen4revamped.png" \
        "$app/install.sh" "$app/uninstall.sh"
  rm -rf "$app/docs" "$app/tools"
  rmdir "$app" 2>/dev/null || echo "Kept $app: it has files the installer did not add."
  refresh_menus
  echo "keen4revamped is uninstalled. Saved games are still in ~/.local/share/keen4revamped."
  exit 0
fi

src="$(cd "$(dirname "$0")" && pwd)"
if [ ! -x "$src/keen4" ]; then
  echo "install.sh: run it from the unpacked keen4revamped linux-x64 folder ($src has no keen4)" >&2
  exit 1
fi

mkdir -p "$app" "$bin" "$(dirname "$menu_entry")" "$(dirname "$icon")"
if [ "$src" != "$app" ]; then
  rm -rf "$app/docs" "$app/tools"
  cp -R "$src/keen4" "$src/keen4-extract" "$src/README.md" "$src/keen4revamped.png" \
        "$src/docs" "$src/tools" "$src/install.sh" "$src/uninstall.sh" "$app/"
fi
chmod 755 "$app/keen4" "$app/keen4-extract" "$app/install.sh" "$app/uninstall.sh"
ln -sf "$app/keen4" "$bin/keen4"
ln -sf "$app/keen4-extract" "$bin/keen4-extract"
cp "$app/keen4revamped.png" "$icon"

# The launcher starts keen4 in its own folder, like running ./keen4 there.
write_entry() {
  cat > "$1" <<EOF
[Desktop Entry]
Type=Application
Name=Commander Keen 4
GenericName=Platform game
Comment=Commander Keen 4 (keen4revamped), with your own copy of the game data
Comment[pt_BR]=Commander Keen 4 (keen4revamped), com a sua própria cópia dos dados do jogo
Exec="$app/keen4"
Path=$app
Icon=keen4revamped
Terminal=false
Categories=Game;ArcadeGame;
Keywords=keen;commander;platformer;
StartupWMClass=keen4
EOF
}
write_entry "$menu_entry"
refresh_menus

if [ "$desktop_shortcut" = 1 ] && [ -d "$desktop_dir" ]; then
  write_entry "$desktop_link"
  chmod 755 "$desktop_link"
  # GNOME asks before launching an untrusted desktop file; mark ours trusted.
  command -v gio >/dev/null 2>&1 && gio set "$desktop_link" metadata::trusted true 2>/dev/null || true
fi

echo "keen4revamped is installed in $app."
echo "Start it from the app menu (Commander Keen 4) or run keen4."
case ":$PATH:" in *":$bin:"*) ;; *) echo "Note: $bin is not on your PATH." ;; esac
echo "It needs your own copy of Keen 4 v1.4 EGA; see $app/README.md for where it looks."
echo "To uninstall: $app/uninstall.sh"
