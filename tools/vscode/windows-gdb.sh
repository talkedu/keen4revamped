#!/usr/bin/env bash
# Runs the Windows gdb (WinLibs) from WSL so VS Code can debug the cross-compiled keen4.exe.
# Windows gdb reports source files as \\wsl.localhost\<distro>\home\..., which VS Code in WSL
# can't open, so this rewrites them back to /home/... on the way out.
gdb="${KEEN4_WINDOWS_GDB:-}"
if [ -z "$gdb" ]; then
  gdb=$(command -v gdb.exe || ls /mnt/c/Users/*/AppData/Local/Microsoft/WinGet/Packages/BrechtSanders.WinLibs*/mingw64/bin/gdb.exe 2>/dev/null | head -n 1)
fi
if [ -z "$gdb" ]; then
  echo "windows-gdb.sh: gdb.exe not found. Install it on Windows with" \
       "'winget install BrechtSanders.WinLibs.POSIX.UCRT' or set KEEN4_WINDOWS_GDB." >&2
  exit 1
fi

# VS Code passes --tty=<Linux terminal>, which a Windows gdb can't open.
args=()
for a in "$@"; do
  case "$a" in --tty=*) ;; *) args+=("$a") ;; esac
done

# \x5c is a backslash; MI strings escape each one, so "\\\\wsl..." has four.
"$gdb" "${args[@]}" | perl -pe 'BEGIN { $| = 1 }
  s{"\x5c{4}wsl[^\x5c"]*\x5c{2}[^\x5c"]+((?:\x5c{2}[^\x5c"]+)+)"}{ (my $p = $1) =~ s{\x5c{2}}{/}g; "\"$p\"" }ge'
