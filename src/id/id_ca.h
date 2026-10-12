#pragma once

// ID_CA: cache manager for EGAGRAPH, GAMEMAPS and AUDIO (segment 12A6).
//
// The DOS original juggles a purgeable memory pool; the port simply keeps
// every chunk it has loaded. Marks and levels are kept so callers behave the
// same, but nothing is ever purged.

#include "id/id_heads.h"

struct pictabletype {
  sword width;   // in bytes (8 pixels)
  sword height;
};

struct spritetabletype {
  sword width, height;  // width in bytes
  sword orgx, orgy;
  sword xl, yl, xh, yh;
  sword shifts;
};

struct maptype {
  std::int32_t planestart[3];
  word planelength[3];
  word width, height;
  char name[16];
};

extern byte* grsegs[NUMCHUNKS];
extern std::uint32_t grsize[NUMCHUNKS];  // expanded byte size of each cached chunk
extern byte grneeded[NUMCHUNKS];
extern byte ca_levelbit, ca_levelnum;

extern word* mapsegs[MAPPLANES];
extern maptype* mapheaderseg[NUMMAPS];
extern sword mapon;

extern byte* audiosegs[NUMSNDCHUNKS];

extern pictabletype* pictable;
extern pictabletype* picmtable;
extern spritetabletype* spritetable;

// Tile info (MAPHEAD + 0x192): per-tile animation and wall flags.
extern const byte* tinf;
inline constexpr int SPEED = 0;                    // background anim time
inline constexpr int ANIM = NUMTILE16;             // background anim delta (signed)
inline constexpr int NORTHWALL = 2 * NUMTILE16;
inline constexpr int EASTWALL = NORTHWALL + NUMTILE16M;
inline constexpr int SOUTHWALL = EASTWALL + NUMTILE16M;
inline constexpr int WESTWALL = SOUTHWALL + NUMTILE16M;
inline constexpr int MANIM = WESTWALL + NUMTILE16M;   // foreground anim delta (signed)
inline constexpr int INTILE = MANIM + NUMTILE16M;
inline constexpr int MSPEED = INTILE + NUMTILE16M;    // foreground anim time

void CA_Startup();
void CA_Shutdown();

bool CA_ReadFile(const char* filename, void* dest, std::uint32_t maxsize);
bool CA_WriteFile(const char* filename, const void* source, std::uint32_t length);

void CAL_HuffExpand(const byte* source, byte* dest, std::uint32_t length, const word* hufftable);
void CAL_CarmackExpand(const word* source, word* dest, word length);
void CA_RLEWexpand(const word* source, word* dest, std::uint32_t length, word rlewtag);
std::uint32_t CA_RLEWCompress(const word* source, std::uint32_t length, word* dest, word rlewtag);

void CA_CacheAudioChunk(int chunk);
void CA_LoadAllSounds();

void CA_UpLevel();
void CA_DownLevel();
void CA_ClearMarks();
void CA_ClearAllMarks();
inline void CA_MarkGrChunk(int chunk) { grneeded[chunk] |= ca_levelbit; }
void CA_CacheGrChunk(int chunk);
void CA_CacheMap(int mapnum);
bool CA_MapExists(int mapnum);  // port, for tools
void CA_CacheMarks(const char* title);
// Loading dialog hooks (DGROUP 7AE0..7AEA), set by the game.
extern void (*drawcachebox)(const char* title, word numcache);
extern void (*updatecachebox)();
extern void (*finishcachebox)();
void CA_SetAllPurge();
void CA_SetGrPurge();
