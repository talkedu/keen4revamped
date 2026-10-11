# Architecture

The full design doc is kept as a shared Claude Doc:
https://claude.ai/code/artifact/d838702b-54f6-4ff1-8792-a2837b152c66

Key decisions (2026-10-11):

- **Source:** our own decompilation of `KEEN4E.EXE` (v1.4, Borland C++ 1991, medium model),
  not the community reconstruction. Engine modules are named by matching id's GPL releases
  (Keen Dreams, Wolfenstein 3D, Catacomb 3-D).
- **Verification:** per-function differential tests (original 16-bit code in the Unicorn
  emulator vs our C), and an Open Watcom DOS build that must replay the four built-in demos
  with the same frame hashes as the original in DOSBox.
- **Platform:** SDL3 behind the interfaces in `src/platform/platform.h`; a headless backend for tests.
- **Rendering:** 8-bit software renderer at 320x200 (classic mode and test reference) and a GPU
  renderer for the 16:9 remaster, both consuming a per-tic scene list.
- **Art:** 4x density, loaded through a manifest keyed by original chunk numbers. First pass is
  AI upscaling; curated and redrawn art replaces it asset by asset.
- **Timing:** original 70 Hz tic logic kept exactly; display interpolated.
- **Audio:** emulated OPL2 (Nuked OPL3) and PC speaker; optional remastered tracks by song number.

## Milestones

1. Unpack and map the executable
2. Decompiled C source, verified
3. C++ port boots headless on Windows, Linux and macOS
4. First pixels
5. Classic mode playable (demos match DOS)
6. Remaster renderer with AI-upscaled art
7. Curated HD art and audio, remaster 1.0
