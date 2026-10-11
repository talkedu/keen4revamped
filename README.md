# keen4revamped

A remaster of Commander Keen 4: Secret of the Oracle for modern Windows, Linux and macOS.
The plan is to decompile the original `KEEN4E.EXE` ourselves into new C source, port it to
C++20 on SDL3, and then add a 16:9 hi-res renderer with new art on top. Gameplay stays exactly
as in the original.

Status: scaffold. The decompilation tools work; no game code yet.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the design and milestones.

## You need your own copy of Keen 4

This repository contains no game data. The port loads the original `.CK4` files from a
copy you own (for example the Steam release). Never commit game files, the original
executable, or anything extracted from it; `.gitignore` blocks the usual names.

## Building

Requires CMake 3.25+ and a C++20 compiler (MSVC 2022, GCC 12+, Clang 15+).

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Pass `-DKEEN4_WITH_SDL3=ON` to build the SDL3 backend (not used yet).

## Decompilation tools

Python scripts in [tools/decomp](tools/decomp) unpack the executable, extract the data
header tables and produce disassembly listings. Progress notes are in
[docs/decomp](docs/decomp).
