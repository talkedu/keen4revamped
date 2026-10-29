#!/bin/sh
# Downloads what the "anbernic" preset needs to cross-compile keen4 for the
# Linux aarch64 handhelds (Anbernic RG35XX/RG40XX/RG353 family) without root:
#
#   - Zig, used as a C/C++ cross compiler. It targets an old glibc (2.28, see
#     cmake/aarch64-linux-zig.cmake) so the binary runs on stock firmware,
#     muOS, Knulli and ROCKNIX alike, and links libc++ statically. The
#     compiler wrappers go in $KEEN4_AARCH64_ROOT/bin.
#   - Debian 11 (bullseye) arm64 headers and libraries for the parts of SDL3
#     that talk to the device: KMS/DRM, Wayland, ALSA, PulseAudio and udev.
#     Wayland comes from Debian 12 (bookworm): SDL's protocol code is made by
#     the build machine's wayland-scanner, which needs libwayland 1.20+.
#     SDL loads them at run time, so they are only used to compile against.
#
# Everything lands in $KEEN4_AARCH64_ROOT (default ~/.cache/keen4revamped/aarch64)
# and is checked against pinned SHA-256 sums. Running it again does nothing
# once everything is in place.
set -eu

root=${KEEN4_AARCH64_ROOT:-${XDG_CACHE_HOME:-$HOME/.cache}/keen4revamped/aarch64}
# glibc 2.28 (2018) or newer is on every firmware for these devices
target=aarch64-linux-gnu.2.28
zig_version=0.15.2
zig_sha256=02aa270f183da276e5b5920b1dac44a63f1a49e55050ebde3aecc9eb82f93239
debian_mirrors="https://deb.debian.org/debian https://archive.debian.org/debian"

# pool path and SHA-256 of each Debian arm64 package
debs='
pool/main/libd/libdrm/libdrm-dev_2.4.104-1_arm64.deb dbbbc3d05b470d6d35b5f2daed4be893e41b74f9c7e58bf5be3d18849b14f78a
pool/main/libd/libdrm/libdrm2_2.4.104-1_arm64.deb 1dc606aa361307a8c5277c2a5ddedea40a4125874e887c98e82b33dacaa853b0
pool/main/m/mesa/libgbm-dev_20.3.5-1_arm64.deb 4ef3b10914072d465282bffaa10e310dcabc88fa8ab02c200771cbe1c37f527d
pool/main/m/mesa/libgbm1_20.3.5-1_arm64.deb 991e5db7978e44ef745a1f7f18100927d304ddf1eaaab840167b5f07e3bdc7e8
pool/main/a/alsa-lib/libasound2-dev_1.2.4-1.1_arm64.deb 5acd1f3cd1aa11aeeb9dcd2467e8142ced0bdf112b813d134e63baebf342ee29
pool/main/a/alsa-lib/libasound2_1.2.4-1.1_arm64.deb 8fabd77256593c42e2ad1578f938b56d0a5ed19aa61c6c72c890fd761fd129a8
pool/main/p/pulseaudio/libpulse-dev_14.2-2_arm64.deb a83746a514f457d515fec31e34fcd985728eaeae77560bbd5e11496fef3f6058
pool/main/p/pulseaudio/libpulse0_14.2-2_arm64.deb bb6533d2f67cd7fb94bf2ae1d7f13ae6b3c5b70c098038f21bc47fce30c88621
pool/main/w/wayland/libwayland-dev_1.21.0-1_arm64.deb 932405c58195d299495230792e374ad304adadf3d3ffdc07eda3466b756adc33
pool/main/w/wayland/libwayland-client0_1.21.0-1_arm64.deb ae390cc04c2eb1de90b9a6373505b22c730ada5e72daa50c507b7f99c12faf06
pool/main/w/wayland/libwayland-egl1_1.21.0-1_arm64.deb c08124a4d9af24f058b45c88867a84697882e755ea94965795d152fe26fe57c9
pool/main/w/wayland/libwayland-cursor0_1.21.0-1_arm64.deb 8c8d1a3942bc3fc9dd3ea7c679c1a314e59cdaaa700883d54d1bb3ec89db256b
pool/main/libf/libffi/libffi-dev_3.3-6_arm64.deb 31166c3c814374c520e4c47cc779291888a2a7eca09fc93b5d2918bd7e2ed085
pool/main/libf/libffi/libffi7_3.3-6_arm64.deb eb748e33ae4ed46f5a4c14b7a2a09792569f2029ede319d0979c373829ba1532
pool/main/libx/libxkbcommon/libxkbcommon-dev_1.0.3-2_arm64.deb 704aa9acb1643344c7ecd237f6d93efd96d6067fa5e9ec41ca2aba4b88a92f06
pool/main/libx/libxkbcommon/libxkbcommon0_1.0.3-2_arm64.deb 91c19f642af34ae8cb909d00d08bc83f0b4f8e87ddde6984bd8bff0f7bf83204
pool/main/s/systemd/libudev-dev_247.3-7+deb11u5_arm64.deb 608eecca1c50230a5488ce436e30926f3b93c57ed018fd7318f1fd278070bdef
pool/main/s/systemd/libudev1_247.3-7+deb11u5_arm64.deb df88275dafcc3b0759ef03d5027fa1f90f48bf7aeec461887798bd18298fe799
pool/main/libg/libglvnd/libegl-dev_1.3.2-1_arm64.deb e2ad8f861b423620dd202ee6dbd1f79ff09e427656d84b395cb19ff43cc77f2d
pool/main/libg/libglvnd/libegl1_1.3.2-1_arm64.deb 4b531a79399010d3377ce9b6094c8f5f3508bd18ea5b32008a6a1ec16e019a81
'

stamp="$root/.complete-$(printf '%s %s %s' "$target" "$zig_version" "$debs" | sha256sum | cut -c1-16)"
if [ -f "$stamp" ]; then
  echo "aarch64 toolchain ready in $root"
  exit 0
fi

# fetch URL FILE SHA256
fetch() {
  if [ -f "$2" ] && echo "$3  $2" | sha256sum -c --status; then return 0; fi
  curl -fsSL --retry 3 -o "$2.part" "$1" || return 1
  if ! echo "$3  $2.part" | sha256sum -c --status; then
    echo "checksum mismatch for $1" >&2
    rm -f "$2.part"
    exit 1
  fi
  mv "$2.part" "$2"
}

mkdir -p "$root/downloads"
cd "$root"

zig_dir="zig-x86_64-linux-$zig_version"
fetch "https://ziglang.org/download/$zig_version/$zig_dir.tar.xz" "downloads/$zig_dir.tar.xz" "$zig_sha256"
rm -rf zig && mkdir zig
tar -xJf "downloads/$zig_dir.tar.xz" -C zig --strip-components=1

rm -rf sysroot && mkdir sysroot
printf '%s\n' "$debs" | while read -r path sha; do
  [ -n "$path" ] || continue
  file="downloads/${path##*/}"
  ok=
  for mirror in $debian_mirrors; do
    if fetch "$mirror/$path" "$file" "$sha"; then ok=1; break; fi
  done
  [ -n "$ok" ] || { echo "could not download $path" >&2; exit 1; }
  dpkg-deb -x "$file" sysroot
done

# Absolute library symlinks would point into the build machine, not the sysroot.
find sysroot -type l | while read -r link; do
  target=$(readlink "$link")
  case $target in
    /*) ln -sf "$root/sysroot$target" "$link" ;;
  esac
done

# CMake takes a compiler path without arguments, so wrap "zig cc" and friends.
rm -rf bin && mkdir bin
for tool in cc c++ ar ranlib; do
  case $tool in
    cc|c++) args="$tool -target $target" ;;
    *) args=$tool ;;
  esac
  cat >"bin/$tool" <<EOF
#!/bin/sh
exec "$root/zig/zig" $args "\$@"
EOF
  chmod +x "bin/$tool"
done

rm -f .complete-*
touch "$stamp"
echo "aarch64 toolchain ready in $root"
