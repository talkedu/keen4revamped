#!/bin/sh
# Wraps an unpacked linux-x64 release folder into one self-extracting
# installer script. Used by release.yml:
#
#   packaging/linux/make-installer.sh PACKAGE_DIR OUT.sh
#
# PACKAGE_DIR must already contain install.sh, uninstall.sh and keen4revamped.png.
# Players run OUT.sh (sh OUT.sh, or chmod +x and double-click/run it); any
# options are passed to install.sh, for example --no-desktop-shortcut.
set -eu
pkg="$1"
out="$2"
[ -f "$pkg/install.sh" ] || { echo "make-installer.sh: $pkg has no install.sh" >&2; exit 1; }

{
  cat <<'EOF'
#!/bin/sh
# keen4revamped installer for Linux x64. Installs for the current user under
# ~/.local; run with --help for options or --uninstall to remove it.
set -eu
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT INT TERM
line=$(awk '/^__PAYLOAD_BELOW__$/ { print NR + 1; exit }' "$0")
tail -n +"$line" "$0" | tar xzf - -C "$tmp"
sh "$tmp"/*/install.sh "$@"
exit 0
__PAYLOAD_BELOW__
EOF
  tar czf - -C "$(dirname "$pkg")" "$(basename "$pkg")"
} > "$out"
chmod 755 "$out"
