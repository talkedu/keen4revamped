# Cross-compiles a Linux aarch64 build for the Anbernic handhelds (RG35XX,
# RG40XX, RG353 and the like) with Zig as the C/C++ compiler. Used by the
# anbernic preset. Run packaging/anbernic/fetch-toolchain.sh first; it puts the
# compiler wrappers and an arm64 sysroot in KEEN4_AARCH64_ROOT.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_LIBRARY_ARCHITECTURE aarch64-linux-gnu)

if(DEFINED ENV{KEEN4_AARCH64_ROOT})
  set(_keen4_root "$ENV{KEEN4_AARCH64_ROOT}")
elseif(DEFINED ENV{XDG_CACHE_HOME})
  set(_keen4_root "$ENV{XDG_CACHE_HOME}/keen4revamped/aarch64")
else()
  set(_keen4_root "$ENV{HOME}/.cache/keen4revamped/aarch64")
endif()
if(NOT EXISTS "${_keen4_root}/bin/c++")
  message(FATAL_ERROR "No aarch64 toolchain in ${_keen4_root}. Run packaging/anbernic/fetch-toolchain.sh first.")
endif()

set(CMAKE_C_COMPILER "${_keen4_root}/bin/cc")
set(CMAKE_CXX_COMPILER "${_keen4_root}/bin/c++")
set(CMAKE_AR "${_keen4_root}/bin/ar" CACHE FILEPATH "")
set(CMAKE_RANLIB "${_keen4_root}/bin/ranlib" CACHE FILEPATH "")

# The sysroot holds only the device libraries SDL loads at run time (libc
# comes from Zig), so it is a search root, not --sysroot.
set(CMAKE_FIND_ROOT_PATH "${_keen4_root}/sysroot")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${_keen4_root}/sysroot")
set(ENV{PKG_CONFIG_LIBDIR} "${_keen4_root}/sysroot/usr/lib/aarch64-linux-gnu/pkgconfig:${_keen4_root}/sysroot/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_PATH} "")
