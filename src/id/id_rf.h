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
// The original has room for 60; co-op adds a Keen and a score box.
inline constexpr int MAXSPRITES = 80;
// The original has room for 90; two views can show twice as many.
inline constexpr int MAXANIMTILES = 180;
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

// Port: co-op draws player 2 as Tom, Keen's twin in other colours: his
// sprites are variant 1. In the classic view a variant sprite is drawn
// through the variant's colour table; data, if set, replaces the chunk's
// graphics (Tom's own score box). Placing a sprite anew resets both.
void RF_SetVariantRemap(int variant, const byte* remap);
void RF_SetSpriteVariant(void* sprite, int variant, const byte* data = nullptr);

// Port: a split screen (co-op). Each view shows part of a camera's 320x200
// view: the window from (windowx, windowy) of the size of the view, drawn at
// (x, y) on the screen. Two views get a 2-pixel divider between them; fewer
// than two means the classic full view. RF_SetViews also names the cameras
// (RF_SetCameras). A sprite can be kept to one view (its owner's score box);
// placing it anew lets it show in every view again.
struct RF_View {
  word originx, originy;
  int windowx, windowy;
  int x, y, width, height;
};
void RF_SetViews(const RF_View* views, int count);
void RF_SetSpriteView(void* sprite, int view);
// Port: drawn over each refreshed frame after the scene is recorded, so the
// remaster renderer shows it from the classic frame (co-op badges).
void RF_SetOverlayHook(void (*func)());

// Port: several cameras (co-op).
//
// The original has one view, and only tiles in it animate: the animated-tile
// list follows the view as it scrolls, and animating writes the new tile into
// the map plane, which the game logic reads. In camera mode the game instead
// names the origin of every camera each frame (RF_SetCameras), the scroll
// routines leave the list alone, and RF_Refresh keeps it over the union of
// the cameras: a tile joins the list when it comes into any camera and leaves
// when it is in none. Single-player never turns camera mode on.
struct RF_Origin {
  word x, y;  // originxglobal, originyglobal of the camera
};
void RF_SetCameraMode(boolean on);
boolean RF_CameraMode();
void RF_SetCameras(const RF_Origin* origins, int count);
// Moves the current origin to (x, y) without touching the animated tiles, to
// make another camera the current one.
void RF_SetOrigin(word x, word y);
// True if map tile (x, y) is in the tile window of any camera (camera mode),
// or of the current view.
boolean RF_TileInView(word x, word y);
