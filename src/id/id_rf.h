#pragma once

// ID_RF: tile scroller and sprites (segments 1654 ID_RF.C, 1826 ID_RF_A.ASM).
//
// The original keeps a tile buffer in EGA memory, scrolls it with hardware
// panning and only redraws tiles that changed or were covered by sprites.
// The port redraws the whole 320x200 view each refresh: background tiles,
// foreground tiles, sprites by priority, then foreground tiles flagged as
// "in front" over sprites of priority below 3. The visible result is the same.
// Everything that affects gameplay is kept exactly: origin bounds and scroll
// blocks, tile animation timing, the animated-tile list that writes into the
// map planes, and the info-plane words RF_MarkTileGraphics overwrites.

#include "id/id_heads.h"

inline constexpr int SCREENTILESWIDE = 20;
inline constexpr int SCREENTILESHIGH = 13;
inline constexpr int PORTTILESWIDE = 21;
inline constexpr int PORTTILESHIGH = 14;
inline constexpr int MAXSPRITES = 60;
inline constexpr int MAXANIMTILES = 90;
inline constexpr int MAXANIMTYPES = 65;
inline constexpr int MAXMAPHEIGHT = 200;
inline constexpr int PRIORITIES = 4;
inline constexpr int MASKEDTILEPRIORITY = 3;

enum drawtype : sword { spritedraw, maskdraw };

extern word mapwidth, mapheight, mapbyteswide;
extern word mapbwidthtable[MAXMAPHEIGHT];
extern word originxglobal, originyglobal;
extern word originxtile, originytile;
extern word originxmin, originxmax, originymin, originymax;
extern word tics;
extern longword lasttimecount;
extern boolean compatability;  // DGROUP 7ADE: sprites without fine shifting

// Map plane access, as the original computes it.
inline word* MAPSPOT_PTR(word* plane, int x, int y) {
  return plane + (mapbwidthtable[y] / 2) + x;
}

void RF_Startup();
void RF_Shutdown();
void RF_FixOfs();
void RF_NewMap();
void RF_MarkTileGraphics();
void RF_SetScrollBlock(int x, int y, boolean horizontal);
void RF_NewPosition(word x, word y);
void RF_Scroll(int x, int y);
void RF_MapToMap(word srcx, word srcy, word destx, word desty, word width, word height);
void RF_MemToMap(const word* source, word plane, word destx, word desty, word width, word height);
void RF_ClearBlock(int x, int y, int width, int height);
void RF_RedrawBlock(int x, int y, int width, int height);
void RF_PlaceSprite(void** user, word globalx, word globaly, word spritenumber, drawtype draw,
                    int priority);
void RF_RemoveSprite(void** user);
void RF_CalcTics();
void RF_Refresh();
void RF_ForceRefresh();
void RF_SetRefreshHook(void (*func)());
// 1654:06E2. Empties the list of animating tiles in view (the status window
// freezes tile animation this way).
void RFL_InitAnimList();
// Port: the persistent background the game can draw on (see id_rf.cpp).
byte* RF_MasterScreen();
