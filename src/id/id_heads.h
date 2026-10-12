#pragma once

// Common definitions for the ported id engine (ID_HEADS.H in id's sources).
//
// The original is 16-bit Borland C: `int` is 16 bits and `long` 32. Game logic
// depends on that (wrap-around, unsigned compares, shifts), so ported code uses
// the explicit fixed-width types below wherever the original used int/long.

#include <cstdint>
#include <cstring>

using byte = std::uint8_t;
using word = std::uint16_t;
using sword = std::int16_t;
using longword = std::uint32_t;
using boolean = std::int16_t;

// Graphics chunk layout of EGAGRAPH.CK4 v1.4 (from CAL_ExpandGrChunk and
// CAL_CacheSprite, segment 12A6).
inline constexpr int NUMCHUNKS = 4751;
inline constexpr int STARTFONT = 3;
inline constexpr int STARTPICS = 6;
inline constexpr int NUMPICS = 115;
inline constexpr int STARTPICM = 121;
inline constexpr int NUMPICM = 3;
inline constexpr int STARTSPRITES = 124;
inline constexpr int NUMSPRITES = 397;
inline constexpr int STARTTILE8 = 521;
inline constexpr int NUMTILE8 = 108;
inline constexpr int STARTTILE8M = 522;
inline constexpr int NUMTILE8M = 36;
inline constexpr int STARTTILE16 = 523;
inline constexpr int NUMTILE16 = 1296;
inline constexpr int STARTTILE16M = 1819;
inline constexpr int NUMTILE16M = 2916;
inline constexpr int STARTEXTERNS = 4735;

inline constexpr int NUMMAPS = 30;
inline constexpr int MAPPLANES = 3;
inline constexpr int NUMSNDCHUNKS = 162;

// Global coordinates: 1 tile = 16 pixels = 256 global units.
inline constexpr int G_T_SHIFT = 8;
inline constexpr int G_P_SHIFT = 4;
inline constexpr int TILEGLOBAL = 256;
inline constexpr int PIXGLOBAL = 16;

// Called on fatal errors, like the original Quit() in CK_MAIN.
[[noreturn]] void Quit(const char* error);
