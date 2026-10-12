// ID_RF: tile scroller and sprites (segments 1654 and 1826).
#include "id/id_rf.h"

#include <algorithm>
#include <cstring>

#include "id/id_ca.h"
#include "id/id_in.h"
#include "id/id_sd.h"
#include "id/id_sys.h"
#include "id/id_vw.h"
#include "render/scene.h"

word mapwidth, mapheight, mapbyteswide;
word mapbwidthtable[MAXMAPHEIGHT];
word originxglobal, originyglobal;
word originxtile, originytile;
word originxmin, originxmax, originymin, originymax;
word tics;
longword lasttimecount;
boolean compatability;

namespace {

byte masterscreen[SCREENWIDTH * SCREENHEIGHT];
boolean masteractive;

// Animation state for each animated tile type (DGROUP DDAE, 4 bytes each).
// The info plane stores "pointers" to these entries; we store the same
// numbers the original did.
constexpr word kAllAnimsBase = 0xDDAE;
struct tiletype {
  word current;  // high bit set for foreground tiles
  sword count;
};
tiletype allanims[MAXANIMTYPES + 1];  // zero-terminated
word numanimchunks;

// Tiles in view that animate (DGROUP D078, linked list).
struct animtiletype {
  word x, y;
  word tile;
  word chunk;       // index into allanims
  word* mapplane;
  animtiletype* next;
};
animtiletype animarray[MAXANIMTILES];
animtiletype* animfree;
animtiletype* animhead;

struct spritelisttype {
  int worldx, worldy;  // pixel position after the original's shift granularity
  int exactx;          // pixel position before that rounding (remaster renderer)
  int artx, arty;      // image offset from the object's own position (remaster renderer)
  int width, height;   // width in bytes
  word grchunk;
  drawtype draw;
  word priority;
  spritelisttype** prevptr;
  spritelisttype* next;
};
spritelisttype spritearray[MAXSPRITES];
spritelisttype* spritefreeptr;
spritelisttype* prioritystart[PRIORITIES];

word hscrollblocks[6], vscrollblocks[6];
word hscrolledge, vscrolledge;

void (*refreshvector)();

word panx, pany;

word ReadInfo(const byte* table, int index) { return table[index]; }
sword ReadInfoSigned(const byte* table, int index) { return static_cast<signed char>(table[index]); }

word* MapPlane(int plane, int x, int y) { return MAPSPOT_PTR(mapsegs[plane], x, y); }

// 1654:0A78
void RFL_CalcOriginStuff(word x, word y) {
  originxglobal = x;
  originyglobal = y;
  originxtile = static_cast<word>(originxglobal >> G_T_SHIFT);
  originytile = static_cast<word>(originyglobal >> G_T_SHIFT);
  panx = static_cast<word>((originxglobal >> G_P_SHIFT) & 15);
  pany = static_cast<word>((originyglobal >> G_P_SHIFT) & 15);
}

// 1654:0A31
void RFL_InitSpriteList() {
  spritefreeptr = &spritearray[0];
  for (int i = 0; i < MAXSPRITES - 1; i++) spritearray[i].next = &spritearray[i + 1];
  spritearray[MAXSPRITES - 1].next = nullptr;
  for (auto& p : prioritystart) p = nullptr;
}

}  // namespace

// 1654:06E2
void RFL_InitAnimList() {
  animfree = &animarray[0];
  for (int i = 0; i < MAXANIMTILES - 1; i++) animarray[i].next = &animarray[i + 1];
  animarray[MAXANIMTILES - 1].next = nullptr;
  animhead = nullptr;
}

namespace {

void AddAnimTile(word x, word y, word tile, word* mapplane, word chunkptr) {
  if (!animfree) Quit("RF_CheckForAnimTile: No free spots in tilearray!");
  animtiletype* anim = animfree;
  animfree = animfree->next;
  anim->next = animhead;
  animhead = anim;
  anim->x = x;
  anim->y = y;
  anim->tile = tile;
  anim->mapplane = mapplane;
  anim->chunk = static_cast<word>((chunkptr - kAllAnimsBase) / 4);
  if (anim->chunk >= MAXANIMTYPES) anim->chunk = 0;
}

// 1654:071A
void RF_CheckForAnimTile(word x, word y) {
  word* map = MapPlane(0, x, y);
  word tile = *map;
  word info = *MapPlane(2, x, y);
  if (tinf[ANIM + tile] && tinf[SPEED + tile]) AddAnimTile(x, y, tile, map, info);
  map = MapPlane(1, x, y);
  tile = *map;
  if (tinf[MANIM + tile] && tinf[MSPEED + tile]) AddAnimTile(x, y, tile, map, info);
}

template <typename Pred>
void RFL_RemoveAnimsIf(Pred pred) {
  animtiletype** link = &animhead;
  while (*link) {
    animtiletype* anim = *link;
    if (pred(anim)) {
      *link = anim->next;
      anim->next = animfree;
      animfree = anim;
    } else {
      link = &anim->next;
    }
  }
}

// 1654:0868, 08AB, 08EF
void RFL_RemoveAnimsOnX(word x) { RFL_RemoveAnimsIf([x](animtiletype* a) { return a->x == x; }); }
void RFL_RemoveAnimsOnY(word y) { RFL_RemoveAnimsIf([y](animtiletype* a) { return a->y == y; }); }
void RFL_RemoveAnimsInBlock(word x, word y, word width, word height) {
  RFL_RemoveAnimsIf([=](animtiletype* a) {
    return static_cast<word>(a->x - x) < width && static_cast<word>(a->y - y) < height;
  });
}

// 1654:0946
void RFL_AnimateTiles() {
  for (word i = 0; allanims[i].current; i++) {
    tiletype* anim = &allanims[i];
    anim->count = static_cast<sword>(anim->count - tics);
    while (anim->count < 1) {
      word tile;
      if (anim->current & 0x8000) {
        tile = anim->current & 0x7FFF;
        tile = static_cast<word>(tile + ReadInfoSigned(tinf, MANIM + tile));
        anim->count = static_cast<sword>(anim->count + ReadInfo(tinf, MSPEED + tile));
        tile |= 0x8000;
      } else {
        tile = anim->current;
        tile = static_cast<word>(tile + ReadInfoSigned(tinf, ANIM + tile));
        anim->count = static_cast<sword>(anim->count + ReadInfo(tinf, SPEED + tile));
      }
      anim->current = tile;
    }
  }

  for (animtiletype* anim = animhead; anim; anim = anim->next) {
    word tile = allanims[anim->chunk].current;
    if (tile != anim->tile) {
      anim->tile = tile;
      *anim->mapplane = tile & 0x7FFF;
      word x = static_cast<word>(anim->x - originxtile);
      word y = static_cast<word>(anim->y - originytile);
      if (x >= PORTTILESWIDE || y >= PORTTILESHIGH) Quit("RFL_AnimateTiles: Out of bounds!");
    }
  }
}

// 1654:0B61
void RFL_BoundScroll(int x, int y) {
  originxglobal = static_cast<word>(originxglobal + x);
  originyglobal = static_cast<word>(originyglobal + y);
  word xtile = static_cast<word>(originxglobal >> G_T_SHIFT);
  word ytile = static_cast<word>(originyglobal >> G_T_SHIFT);

  if (x > 0) {
    xtile = static_cast<word>(xtile + SCREENTILESWIDE);
    for (word i = 0; i < vscrolledge; i++)
      if (vscrollblocks[i] == xtile) {
        originxglobal &= 0xFF00;
        break;
      }
  } else if (x < 0) {
    for (word i = 0; i < vscrolledge; i++)
      if (vscrollblocks[i] == xtile) {
        originxglobal = static_cast<word>((originxglobal & 0xFF00) + TILEGLOBAL);
        break;
      }
  }

  if (y > 0) {
    ytile = static_cast<word>(ytile + SCREENTILESHIGH);
    for (word i = 0; i < hscrolledge; i++)
      if (hscrollblocks[i] == ytile) {
        originyglobal &= 0xFF00;
        break;
      }
  } else if (y < 0) {
    for (word i = 0; i < hscrolledge; i++)
      if (hscrollblocks[i] == ytile) {
        originyglobal = static_cast<word>((originyglobal & 0xFF00) + TILEGLOBAL);
        break;
      }
  }

  RFL_CalcOriginStuff(originxglobal, originyglobal);
}

// 1654:1025
void RFL_NewPosition(word x, word y) {
  if (x < originxmin)
    x = originxmin;
  else if (x > originxmax)
    x = originxmax;
  if (y < originymin)
    y = originymin;
  else if (y > originymax)
    y = originymax;

  originxtile = static_cast<word>(x >> G_T_SHIFT);
  originytile = static_cast<word>(y >> G_T_SHIFT);

  for (word i = 0; i < vscrolledge; i++) {
    word edge = vscrollblocks[i];
    if (edge >= originxtile && edge <= originxtile + 10) {
      x = static_cast<word>((edge << G_T_SHIFT) + TILEGLOBAL);
      break;
    }
    if (edge >= originxtile + 11 && edge <= originxtile + 20) {
      x = static_cast<word>((edge << G_T_SHIFT) - 20 * TILEGLOBAL);
      break;
    }
  }
  for (word i = 0; i < hscrolledge; i++) {
    word edge = hscrollblocks[i];
    if (edge >= originytile && edge <= originytile + 6) {
      y = static_cast<word>((edge << G_T_SHIFT) + TILEGLOBAL);
      break;
    }
    if (edge >= originytile + 7 && edge <= originytile + 13) {
      y = static_cast<word>((edge << G_T_SHIFT) - 13 * TILEGLOBAL);
      break;
    }
  }
  RFL_CalcOriginStuff(x, y);
}

// 1654:0C62. dir: 0 up, 1 right, 2 down, 3 left (the new row or column).
void RFL_NewRow(int dir) {
  word x, y;
  int dx, dy, count;
  switch (dir) {
    case 0:  // top row
      x = originxtile; y = originytile; dx = 1; dy = 0; count = PORTTILESWIDE;
      break;
    case 1:  // right column
      x = static_cast<word>(originxtile + PORTTILESWIDE - 1); y = originytile; dx = 0; dy = 1;
      count = PORTTILESHIGH;
      break;
    case 2:  // bottom row
      x = originxtile; y = static_cast<word>(originytile + PORTTILESHIGH - 1); dx = 1; dy = 0;
      count = PORTTILESWIDE;
      break;
    case 3:  // left column
      x = originxtile; y = originytile; dx = 0; dy = 1; count = PORTTILESHIGH;
      break;
    default:
      Quit("RFL_NewRow: Bad dir!");
  }
  while (count--) {
    RF_CheckForAnimTile(x, y);
    x = static_cast<word>(x + dx);
    y = static_cast<word>(y + dy);
  }
}

// --- drawing ---------------------------------------------------------------

// Background tile with an optional foreground tile on top, per plane
// (bg & mask) | fg, as RFL_NewTile does.
void DrawTileCell(int sx, int sy, word bgtile, word fgtile) {
  CA_CacheGrChunk(STARTTILE16 + bgtile);
  const byte* bg = grsegs[STARTTILE16 + bgtile];
  const byte* fg = nullptr;
  if (fgtile) {
    CA_CacheGrChunk(STARTTILE16M + fgtile);
    fg = grsegs[STARTTILE16M + fgtile];
  }
  for (int row = 0; row < 16; row++) {
    int y = sy + row;
    if (static_cast<unsigned>(y) >= SCREENHEIGHT) continue;
    for (int b = 0; b < 2; b++) {
      int idx = row * 2 + b;
      byte planes[4];
      for (int p = 0; p < 4; p++) {
        byte value = bg ? bg[p * 32 + idx] : 0;
        if (fg) value = static_cast<byte>((value & fg[idx]) | fg[32 + p * 32 + idx]);
        planes[p] = value;
      }
      for (int bit = 0; bit < 8; bit++) {
        int x = sx + b * 8 + bit;
        if (static_cast<unsigned>(x) >= SCREENWIDTH) continue;
        byte sel = static_cast<byte>(0x80 >> bit);
        vw_screen[y * SCREENWIDTH + x] = static_cast<byte>(((planes[0] & sel) ? 1 : 0) | ((planes[1] & sel) ? 2 : 0) |
                                                           ((planes[2] & sel) ? 4 : 0) | ((planes[3] & sel) ? 8 : 0));
      }
    }
  }
}

// 1826:0344, RFL_MaskForegroundTiles: foreground tiles flagged in INTILE bit 7
// are drawn again over the sprites below priority 3.
void MaskForegroundTile(int sx, int sy, word fgtile) {
  const byte* fg = grsegs[STARTTILE16M + fgtile];
  if (!fg) return;
  for (int row = 0; row < 16; row++) {
    int y = sy + row;
    if (static_cast<unsigned>(y) >= SCREENHEIGHT) continue;
    for (int b = 0; b < 2; b++) {
      int idx = row * 2 + b;
      for (int bit = 0; bit < 8; bit++) {
        int x = sx + b * 8 + bit;
        if (static_cast<unsigned>(x) >= SCREENWIDTH) continue;
        byte sel = static_cast<byte>(0x80 >> bit);
        byte& pixel = vw_screen[y * SCREENWIDTH + x];
        byte out = 0;
        for (int p = 0; p < 4; p++) {
          byte value = (pixel & (1 << p)) ? sel : 0;
          value = static_cast<byte>((value & fg[idx]) | (fg[32 + p * 32 + idx] & sel));
          if (value & sel) out = static_cast<byte>(out | (1 << p));
        }
        pixel = out;
      }
    }
  }
}

int ViewPixelX() { return originxglobal >> G_P_SHIFT; }
int ViewPixelY() { return originyglobal >> G_P_SHIFT; }

void DrawTiles() {
  int px0 = originxtile * 16 - ViewPixelX();
  int py0 = originytile * 16 - ViewPixelY();
  for (int ty = 0; ty < PORTTILESHIGH; ty++) {
    if (originytile + ty >= mapheight) break;
    for (int tx = 0; tx < PORTTILESWIDE; tx++) {
      if (originxtile + tx >= mapwidth) break;
      word bg = *MapPlane(0, originxtile + tx, originytile + ty);
      word fg = *MapPlane(1, originxtile + tx, originytile + ty);
      DrawTileCell(px0 + tx * 16, py0 + ty * 16, bg, fg);
    }
  }
}

void MaskForegroundTiles() {
  int px0 = originxtile * 16 - ViewPixelX();
  int py0 = originytile * 16 - ViewPixelY();
  for (int ty = 0; ty < PORTTILESHIGH; ty++) {
    if (originytile + ty >= mapheight) break;
    for (int tx = 0; tx < PORTTILESWIDE; tx++) {
      if (originxtile + tx >= mapwidth) break;
      word fg = *MapPlane(1, originxtile + tx, originytile + ty);
      if (fg && (tinf[INTILE + fg] & 0x80)) MaskForegroundTile(px0 + tx * 16, py0 + ty * 16, fg);
    }
  }
}

// 1654:1A6E
void RFL_UpdateSprites() {
  for (int priority = 0; priority < PRIORITIES; priority++) {
    if (priority == MASKEDTILEPRIORITY) MaskForegroundTiles();
    for (spritelisttype* sprite = prioritystart[priority]; sprite; sprite = sprite->next) {
      const byte* data = grsegs[sprite->grchunk];
      if (!data) continue;
      int sx = sprite->worldx - ViewPixelX();
      int sy = sprite->worldy - ViewPixelY();
      if (sprite->draw == spritedraw)
        VWL_DrawPlanar(sx, sy, sprite->width, sprite->height, data, true);
      else
        VWL_DrawInverseMask(sx, sy, sprite->width, sprite->height, data);
    }
  }
}

// Port: records the frame just drawn for the remaster renderer.
void CaptureScene() {
  keen4::render::Scene& s = keen4::render::BeginCapture();
  s.time = lasttimecount / 70.0;
  s.map = mapon;
  s.viewx = ViewPixelX();
  s.viewy = ViewPixelY();
  s.minx = originxmin >> G_P_SHIFT;
  s.maxx = (originxmax >> G_P_SHIFT) + keen4::render::kClassicWidth;
  // tiles under the wide view, one spare on each side for smooth scrolling
  s.tilex0 = (s.viewx - keen4::render::kWideMargin) / 16 - 1;
  s.tiley0 = s.viewy / 16 - 1;
  s.cols = keen4::render::kWideWidth / 16 + 4;
  s.rows = keen4::render::kClassicHeight / 16 + 4;
  const std::size_t cells = static_cast<std::size_t>(s.cols) * s.rows;
  s.bg.assign(cells, 0);
  s.fg.assign(cells, 0);
  s.fgfront.assign(cells, 0);
  for (int ty = 0; ty < s.rows; ty++) {
    int my = s.tiley0 + ty;
    if (my < 0 || my >= mapheight) continue;
    for (int tx = 0; tx < s.cols; tx++) {
      int mx = s.tilex0 + tx;
      if (mx < 0 || mx >= mapwidth) continue;
      std::size_t i = static_cast<std::size_t>(ty) * s.cols + tx;
      s.bg[i] = *MapPlane(0, mx, my);
      word fg = *MapPlane(1, mx, my);
      s.fg[i] = fg;
      s.fgfront[i] = (fg && (tinf[INTILE + fg] & 0x80)) ? 1 : 0;
    }
  }
  for (int priority = 0; priority < PRIORITIES; priority++) {
    for (spritelisttype* sprite = prioritystart[priority]; sprite; sprite = sprite->next) {
      if (!grsegs[sprite->grchunk]) continue;
      s.sprites.push_back({sprite, sprite->grchunk, sprite->exactx, sprite->worldy, sprite->artx,
                           sprite->arty, priority, sprite->draw != spritedraw});
    }
  }
  // art to prepare ahead: tiles up to half a screen beyond the view, and every
  // sprite the level has cached
  for (int my = std::max(0, s.tiley0 - 6); my < std::min<int>(mapheight, s.tiley0 + s.rows + 6); my++) {
    for (int mx = std::max(0, s.tilex0 - 10); mx < std::min<int>(mapwidth, s.tilex0 + s.cols + 10); mx++) {
      s.nearbg.push_back(*MapPlane(0, mx, my));
      if (word fg = *MapPlane(1, mx, my)) s.nearfg.push_back(fg);
    }
  }
  for (int chunk = STARTSPRITES; chunk < STARTSPRITES + NUMSPRITES; chunk++)
    if (grsegs[chunk]) s.loadedsprites.push_back(static_cast<std::uint16_t>(chunk));
  std::memcpy(s.classic.data(), vw_screen, s.classic.size());
  keen4::render::EndCapture();
}

}  // namespace

// 1654:000F
void RF_Startup() {
  originxmin = originymin = 2 * TILEGLOBAL;
  RFL_InitSpriteList();
  RFL_InitAnimList();
}

// 1654:0134
void RF_Shutdown() {}

// 1654:0139
void RF_FixOfs() {}

// 1654:01C2
void RF_NewMap() {
  masteractive = false;
  const maptype* header = mapheaderseg[mapon];
  mapwidth = header->width;
  mapbyteswide = static_cast<word>(mapwidth * 2);
  mapheight = header->height;
  if (mapheight > MAXMAPHEIGHT) Quit("RF_NewMap: Map too tall!");
  word spot = 0;
  for (word i = 0; i < mapheight; i++) {
    mapbwidthtable[i] = spot;
    spot = static_cast<word>(spot + mapbyteswide);
  }

  originxmax = static_cast<word>((mapwidth - 2 - SCREENTILESWIDE) << G_T_SHIFT);
  originymax = static_cast<word>((mapheight - 2 - SCREENTILESHIGH) << G_T_SHIFT);
  if (originxmax < originxmin) originxmax = originxmin;
  if (originymax < originymin) originymax = originymin;

  RFL_InitSpriteList();
  RFL_InitAnimList();
  hscrolledge = vscrolledge = 0;

  RF_SetScrollBlock(0, 1, true);
  RF_SetScrollBlock(0, mapheight - 2, true);
  RF_SetScrollBlock(1, 0, false);
  RF_SetScrollBlock(mapwidth - 2, 0, false);

  lasttimecount = TimeCount;
  tics = 1;
}

// 1654:0318
void RF_MarkTileGraphics() {
  std::memset(allanims, 0, sizeof allanims);
  numanimchunks = 0;
  int size = mapwidth * mapheight;

  auto find_or_add = [](word current, word count) -> word {
    word i;
    for (i = 0; i < numanimchunks; i++)
      if (allanims[i].current == current) return static_cast<word>(kAllAnimsBase + i * 4);
    if (i >= MAXANIMTYPES) Quit("RF_MarkTileGraphics: Too many unique animated tiles!");
    allanims[i].current = current;
    allanims[i].count = static_cast<sword>(count);
    numanimchunks++;
    return static_cast<word>(kAllAnimsBase + i * 4);
  };

  // Background
  for (int i = 0; i < size; i++) {
    sword tile = static_cast<sword>(mapsegs[0][i]);
    if (tile < 0) continue;
    CA_MarkGrChunk(STARTTILE16 + tile);
    if (!tinf[ANIM + tile]) continue;
    if (tinf[SPEED + tile]) {
      if (!tinf[ANIM + tile]) Quit("RF_MarkTileGraphics: Background anim of 0");
      mapsegs[2][i] = find_or_add(static_cast<word>(tile), tinf[SPEED + tile]);
    }
    int delta = ReadInfoSigned(tinf, ANIM + tile);
    int next = tile + delta;
    int steps = 0;
    while (delta && next != tile) {
      CA_MarkGrChunk(STARTTILE16 + next);
      delta = ReadInfoSigned(tinf, ANIM + next);
      next += delta;
      if (++steps > 20) Quit("RF_MarkTileGraphics: Unending background animation");
    }
  }

  // Foreground
  for (int i = 0; i < size; i++) {
    sword tile = static_cast<sword>(mapsegs[1][i]);
    if (tile < 0) continue;
    CA_MarkGrChunk(STARTTILE16M + tile);
    if (!tinf[MANIM + tile]) continue;
    if (tinf[MSPEED + tile]) {
      mapsegs[2][i] = find_or_add(static_cast<word>(tile | 0x8000), tinf[MSPEED + tile]);
    }
    int delta = ReadInfoSigned(tinf, MANIM + tile);
    int next = tile + delta;
    int steps = 0;
    while (delta && next != tile) {
      CA_MarkGrChunk(STARTTILE16M + next);
      delta = ReadInfoSigned(tinf, MANIM + next);
      next += delta;
      if (++steps > 20) Quit("RF_MarkTileGraphics: Unending foreground animation");
    }
  }
}

// 1654:0B08
void RF_SetScrollBlock(int x, int y, boolean horizontal) {
  if (horizontal) {
    hscrollblocks[hscrolledge] = static_cast<word>(y);
    if (hscrolledge++ == 6) Quit("RF_SetScrollBlock: Too many horizontal scroll blocks");
  } else {
    vscrollblocks[vscrolledge] = static_cast<word>(x);
    if (vscrolledge++ == 6) Quit("RF_SetScrollBlock: Too many vertical scroll blocks");
  }
}

// 1654:1333
void RF_NewPosition(word x, word y) {
  RFL_NewPosition(x, y);
  RFL_InitAnimList();
  for (word my = 0; my < PORTTILESHIGH; my++)
    for (word mx = 0; mx < PORTTILESWIDE; mx++)
      RF_CheckForAnimTile(static_cast<word>(mx + originxtile), static_cast<word>(my + originytile));
}

// 1654:13DD
void RF_Scroll(int x, int y) {
  word oldxt = originxtile, oldyt = originytile;
  RFL_BoundScroll(x, y);
  int deltax = static_cast<sword>(originxtile - oldxt);
  int deltay = static_cast<sword>(originytile - oldyt);
  int absdx = deltax < 0 ? -deltax : deltax;
  int absdy = deltay < 0 ? -deltay : deltay;
  if (absdx > 1 || absdy > 1) {
    RF_NewPosition(originxglobal, originyglobal);
    return;
  }
  if (!absdx && !absdy) return;

  if (deltax) {
    if (deltax == 1) {
      RFL_NewRow(1);
      RFL_RemoveAnimsOnX(static_cast<word>(originxtile - 1));
    } else {
      RFL_NewRow(3);
      RFL_RemoveAnimsOnX(static_cast<word>(originxtile + PORTTILESWIDE));
    }
  }
  if (deltay) {
    if (deltay == 1) {
      RFL_NewRow(2);
      RFL_RemoveAnimsOnY(static_cast<word>(originytile - 1));
    } else {
      RFL_NewRow(0);
      RFL_RemoveAnimsOnY(static_cast<word>(originytile + PORTTILESHIGH));
    }
  }
}

// 1654:0D78
void RF_MapToMap(word srcx, word srcy, word destx, word desty, word width, word height) {
  RFL_RemoveAnimsInBlock(destx, desty, width, height);
  for (word y = 0; y < height; y++) {
    for (word x = 0; x < width; x++) {
      for (int plane = 0; plane < 3; plane++)
        *MapPlane(plane, destx + x, desty + y) = *MapPlane(plane, srcx + x, srcy + y);
      word sx = static_cast<word>(destx + x - originxtile);
      word sy = static_cast<word>(desty + y - originytile);
      if (sy < PORTTILESHIGH && sx < PORTTILESWIDE)
        RF_CheckForAnimTile(static_cast<word>(destx + x), static_cast<word>(desty + y));
    }
  }
}

// 1654:0F28
void RF_MemToMap(const word* source, word plane, word destx, word desty, word width, word height) {
  RFL_RemoveAnimsInBlock(destx, desty, width, height);
  for (word y = 0; y < height; y++) {
    for (word x = 0; x < width; x++) {
      *MapPlane(plane, destx + x, desty + y) = *source++;
      word sx = static_cast<word>(destx + x - originxtile);
      word sy = static_cast<word>(desty + y - originytile);
      if (sy < PORTTILESHIGH && sx < PORTTILESWIDE)
        RF_CheckForAnimTile(static_cast<word>(destx + x), static_cast<word>(desty + y));
    }
  }
}

// 1654:1137 and 1654:11AC: mark screen areas for redraw. The port redraws
// everything, so these have nothing to do.
void RF_ClearBlock(int, int, int, int) {}
void RF_RedrawBlock(int, int, int, int) {}

// 1654:1641
void RF_PlaceSprite(void** user, word globalx, word globaly, word spritenumber, drawtype draw,
                    int priority) {
  if (spritenumber == 0 || spritenumber == 0xFFFF) {
    RF_RemoveSprite(user);
    return;
  }

  spritelisttype* sprite = static_cast<spritelisttype*>(*user);
  bool link = true;
  if (sprite) {
    if (sprite->priority == priority) {
      link = false;
    } else {
      // unlink from its old priority list
      if (sprite->next) sprite->next->prevptr = sprite->prevptr;
      *sprite->prevptr = sprite->next;
    }
  } else {
    if (!spritefreeptr) Quit("RF_PlaceSprite: No free spots in spritearray!");
    sprite = spritefreeptr;
    spritefreeptr = spritefreeptr->next;
  }
  if (link) {
    spritelisttype* next = prioritystart[priority];
    if (next) next->prevptr = &sprite->next;
    sprite->next = next;
    prioritystart[priority] = sprite;
    sprite->prevptr = &prioritystart[priority];
  }

  CA_CacheGrChunk(spritenumber);
  const spritetabletype& spr = spritetable[spritenumber - STARTSPRITES];
  if (!grsegs[spritenumber]) Quit("RF_PlaceSprite: Placed an uncached sprite");

  const int objx = globalx >> G_P_SHIFT, objy = globaly >> G_P_SHIFT;
  globaly = static_cast<word>(globaly + spr.orgy);
  globalx = static_cast<word>(globalx + spr.orgx);
  word pixx = static_cast<word>(globalx >> G_P_SHIFT);
  word shift = compatability ? 0 : static_cast<word>((pixx & 7) / 2);
  // The cached sprite has spr.shifts copies; shift index n draws n*2 pixels
  // right of the byte, but only 8/shifts-pixel steps exist.
  int step = spr.shifts == 4 ? 2 : spr.shifts == 2 ? 4 : 8;
  sprite->worldx = (pixx >> 3) * 8 + (shift * 2 / step) * step;
  sprite->exactx = pixx;
  sprite->worldy = static_cast<word>(globaly >> G_P_SHIFT);
  sprite->artx = sprite->exactx - objx;
  sprite->arty = sprite->worldy - objy;
  sprite->width = spr.width;
  sprite->height = spr.height;
  sprite->grchunk = spritenumber;
  sprite->draw = draw;
  sprite->priority = static_cast<word>(priority);
  *user = sprite;
}

// 1654:185B
void RF_RemoveSprite(void** user) {
  spritelisttype* sprite = static_cast<spritelisttype*>(*user);
  if (!sprite) return;
  if (sprite->next) sprite->next->prevptr = sprite->prevptr;
  *sprite->prevptr = sprite->next;
  sprite->next = spritefreeptr;
  spritefreeptr = sprite;
  *user = nullptr;
}

// 1654:1220
void RF_CalcTics() {
  SYS_Poll();
  if (lasttimecount > TimeCount) TimeCount = lasttimecount;

  if (DemoMode != demo_Off) {
    // demo recording and playback need a constant frame rate
    while (TimeCount < lasttimecount + 6) SYS_Idle();
    lasttimecount += 3;
    TimeCount = lasttimecount + 3;
    tics = 3;
    return;
  }

  longword newtime;
  do {
    SYS_Idle();
    newtime = TimeCount;
    tics = static_cast<word>(newtime - lasttimecount);
  } while (tics < 2);
  lasttimecount = newtime;
  if (tics > 5) {
    TimeCount -= (tics - 5);
    tics = 5;
  }
}

// 1654:1C6E
void RF_Refresh() {
  RFL_AnimateTiles();
  if (masteractive)
    std::memcpy(vw_screen, masterscreen, sizeof masterscreen);
  else
    DrawTiles();
  RFL_UpdateSprites();
  // A background drawn on (the high score table) has no tile scene to redraw.
  if (masteractive)
    keen4::render::InvalidateScene();
  else
    CaptureScene();
  if (refreshvector) refreshvector();
  VW_UpdateScreen();
  RF_CalcTics();
}

// Port: the original can draw into its master screen (bufferofs = masterofs),
// the tile background every refresh copies from. The port renders the
// background once into a copy and keeps using it until the next map.
byte* RF_MasterScreen() {
  if (!masteractive) {
    byte* saved = vw_screen;
    vw_screen = masterscreen;
    DrawTiles();
    vw_screen = saved;
    masteractive = true;
  }
  return masterscreen;
}

// 1654:0D58
void RF_ForceRefresh() {
  RF_NewPosition(originxglobal, originyglobal);
  RF_Refresh();
  RF_Refresh();
}

// 1654:0C50
void RF_SetRefreshHook(void (*func)()) { refreshvector = func; }
