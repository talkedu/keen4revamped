#!/bin/sh
# Removes the keen4revamped install this folder belongs to (see install.sh).
# Saved games and the configuration in ~/.local/share/keen4revamped stay.
app="$(cd "$(dirname "$0")" && pwd)"
exec sh "$app/install.sh" --uninstall --prefix "$(dirname "$(dirname "$app")")" "$@"
