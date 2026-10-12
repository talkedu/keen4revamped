// CK_KEEN2: score box, Keen on the world map, flags, stun shots and doors
// (segment 0D8F).
#include <cstdio>
#include <cstring>

#include "exe/keen4exe.h"
#include "game/ck_def.h"

extern sword xtry, ytry;
void CantSwim();  // K4_SPEC

namespace {

using keen4::exe::g_exe;

constexpr word s_score = 0x1456;
constexpr word s_demo = 0x1474;
constexpr word s_scoredummy = 0x089C;
constexpr word s_keenonfoot1 = 0x1492;
constexpr word s_worldswim = 0x14CE;
constexpr word s_worldkeen = 0x14EC;
constexpr word s_worldkeenwalk = 0x15A0;
constexpr word s_flagwave1 = 0x15EE;
constexpr word s_throwflag0 = 0x1666;
constexpr word s_stunray1 = 0x1738;
constexpr word s_stunhit1 = 0x17B0;

constexpr int SCOREBOXSPR = 253;
constexpr int DEMOPLAQUESPR = 129;

constexpr int SND_WORLDWALK1 = 0, SND_WORLDWALK2 = 1, SND_KEENFIRE = 4, SND_ENTERLEVEL = 12,
              SND_NOWAY = 14, SND_NOAMMO = 21, SND_SHOTEXPLODE = 25, SND_SWIM1 = 26, SND_SWIM2 = 27,
              SND_FLAGSPIN = 43, SND_FLAGLAND = 44;

struct Point {
  word x, y;
};
Point flagpath[30];  // DGROUP CABF

word worldshapes(int dir) { return g_exe.Word(static_cast<std::uint16_t>(0x15BE + dir * 2)); }
word worldanims(int frame) { return g_exe.Word(static_cast<std::uint16_t>(0x15CE + frame * 2)); }
word swimshapes(int dir) { return g_exe.Word(static_cast<std::uint16_t>(0x15D6 + dir * 2)); }
sword tiledir(int index) { return g_exe.Int(static_cast<std::uint16_t>(0x15E6 + index * 2)); }
sword oppositedir(int dir) { return g_exe.Int(static_cast<std::uint16_t>(0x1446 + dir * 2)); }

inline word* FGSpot(word x, word y) { return mapsegs[1] + mapbwidthtable[y] / 2 + x; }
inline word* InfoSpot(word x, word y) { return mapsegs[2] + mapbwidthtable[y] / 2 + x; }

// 0D8F:0078: copies an 8x8 status digit into the score box sprite.
void MemDrawChar(int ch, byte* dest, int width, int planesize) {
  const byte* src = grsegs[STARTTILE8] + ch * 32;
  for (int plane = 0; plane < 4; plane++) {
    for (int row = 0; row < 8; row++) dest[row * width] = *src++;
    dest += planesize;
  }
}

// 0D8F:00CB rebuilt the shifted copies of the score box. The port draws
// sprites unshifted, so there is nothing to do.
void ShiftScore() {}

// Draws `text` right-aligned in a `field` digit wide slot of the score box.
void DrawScoreField(byte* dest, int width, int planesize, int field, const char* text) {
  int length = static_cast<int>(std::strlen(text));
  for (int i = field; i > length; i--) MemDrawChar(41, ++dest, width, planesize);
  for (const char* p = text; *p; p++) MemDrawChar(*p - 6, ++dest, width, planesize);
}

// 0D8F:04B8
void DrawDemoPlaque(objtype* ob) {
  if (ob->x != originxglobal || ob->y != originyglobal) {
    ob->x = originxglobal;
    ob->y = originyglobal;
    RF_PlaceSprite(&ob->sprite, static_cast<word>(ob->x + 160 * PIXGLOBAL - 32 * PIXGLOBAL),
                   static_cast<word>(ob->y + 8 * PIXGLOBAL), DEMOPLAQUESPR, spritedraw, 3);
  }
}

// 0D8F:06F0
void T_KeenWorldWalk(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  if (ob->temp3) {
    ob->temp3 = static_cast<sword>(ob->temp3 - 4);
    if (ob->temp3 < 0) ob->temp3 = 0;
  } else {
    ob->xdir = ps.c.xaxis;
    ob->ydir = ps.c.yaxis;
    if (ps.pogobutton || ps.firebutton || ps.jumpbutton) CheckEnterLevel(ob);
    if (ps.c.dir == dir_None) {
      ob->state = ST(s_worldkeen);
      ob->shapenum = static_cast<word>(worldshapes(ob->temp1) + 3);
      return;
    }
    ob->temp1 = ps.c.dir;
  }
  if (++ob->temp2 == 4) ob->temp2 = 0;
  ob->shapenum = static_cast<word>(worldshapes(ob->temp1) + worldanims(ob->temp2));
  if (ob->temp2 == 1)
    SD_PlaySound(SND_WORLDWALK1);
  else if (ob->temp2 == 3)
    SD_PlaySound(SND_WORLDWALK2);
}

// 0D8F:06AF
void T_KeenWorld(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  if (ps.c.dir != dir_None) {
    ob->state = ST(s_worldkeenwalk);
    ob->temp2 = 0;
    T_KeenWorldWalk(ob);
  }
  if (ps.jumpbutton || ps.pogobutton || ps.firebutton) CheckEnterLevel(ob);
}

// 0D8F:07AB: riding the foot to the other side of the world.
void T_FlyKeen(objtype* ob) {
  ob->temp1 = static_cast<sword>(ob->temp1 - tics);
  xtry = static_cast<sword>(ob->xspeed * static_cast<sword>(tics));
  ytry = static_cast<sword>(ob->yspeed * static_cast<sword>(tics));
  if (ob->temp1 <= 0) {
    xtry = static_cast<sword>(xtry - ob->xspeed * -ob->temp1);
    ytry = static_cast<sword>(ytry - ob->yspeed * -ob->temp1);
    ob->priority = 1;
    ob->temp1 = 6;
    ob->temp2 = 3;
    ob->temp3 = 0;
    ob->xdir = ob->ydir = 0;
    ob->state = ST(s_worldkeen);
    ob->shapenum = 256;
    ob->needtoclip = cl_midclip;
  }
}

// 0D8F:0826
void T_WorldKeenSwim(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  if (ob->temp3) {
    ob->temp3 = static_cast<sword>(ob->temp3 - 6);
    if (ob->temp3 < 0) ob->temp3 = 0;
  } else {
    ob->xdir = ps.c.xaxis;
    ob->ydir = ps.c.yaxis;
    if (ps.c.xaxis || ps.c.yaxis) ob->temp1 = ps.c.dir;
  }
  ob->shapenum = static_cast<word>(swimshapes(ob->temp1) + ob->temp2);
  if (++ob->temp2 == 2) ob->temp2 = 0;
  if (ob->temp2 == 0)
    SD_PlaySound(SND_SWIM1);
  else
    SD_PlaySound(SND_SWIM2);
}

// 0D8F:0BC4
void TossThink(objtype* ob) {
  if (screenfaded) return;
  SD_StopSound();
  SD_PlaySound(SND_FLAGSPIN);
  ob->state = ob->state->nextstate;
}

// 0D8F:0BEF
void PathThink(objtype* ob) {
  ob->temp3 = static_cast<sword>(ob->temp3 + tics);
  if (ob->temp3 > 58) ob->temp3 = 58;
  ob->x = flagpath[ob->temp3 / 2].x;
  ob->y = flagpath[ob->temp3 / 2].y;
  ob->needtoreact = true;
  if (!ob->temp1) SD_PlaySound(SND_FLAGSPIN);
}

// 0D8F:0C51
void FlagAlign(objtype* ob) {
  ob->x = static_cast<word>(ob->temp1);
  ob->y = static_cast<word>(ob->temp2 + 8 * PIXGLOBAL);
  SD_PlaySound(SND_FLAGLAND);
}

// 0D8F:0D6A: off-screen shots still stun inactive actors close by.
void T_Shot(objtype* ob) {
  // Port: by every camera's view.
  ViewBounds views[MAXCAMERAS];
  int count = GetViewBounds(views);
  bool far = true;
  for (int i = 0; i < count; i++) {
    const ViewBounds& v = views[i];
    if (ob->tileright >= v.xtile && ob->tilebottom >= v.ytile && ob->tileleft <= v.xtilemax &&
        ob->tiletop <= v.ytilemax)
      return;
    if (!(static_cast<word>(ob->tileright + 10) < v.xtile || static_cast<word>(ob->tileleft - 10) > v.xtilemax ||
          static_cast<word>(ob->tilebottom + 6) < v.ytile || static_cast<word>(ob->tiletop - 6) > v.ytilemax))
      far = false;
  }
  if (far) {
    RemoveObj(ob);
    return;
  }

  for (objtype* check = player->next; check; check = check->next) {
    if (!check->active && ob->right > check->left && ob->left < check->right && ob->top < check->bottom &&
        ob->bottom > check->top) {
      if (check->state->contact) {
        check->state->contact(check, ob);
        check->needtoreact = true;
        check->active = ac_yes;
      }
      if (ob->obclass == nothing) return;
    }
  }
}

// 0D8F:0E32
void R_Shot(objtype* ob) {
  if (ob->hitnorth == 1 && ob->tileleft != ob->tileright) {
    word tile = *FGSpot(ob->tileright, static_cast<word>(ob->tiletop - 1));
    if (TINF_N(tile) == 17) {
      ob->hitnorth = 17;
      ob->x = static_cast<word>(ob->x + TILEGLOBAL - (ob->x & 0xFF));
    }
  } else if (ob->hitnorth == 17 && ob->tileleft != ob->tileright) {
    ob->x &= 0xFF00;
  }

  if (ob->hitsouth == 1 && ob->tileleft != ob->tileright) {
    word tile = *FGSpot(ob->tileright, static_cast<word>(ob->tilebottom + 1));
    if (TINF_S(tile) == 17) {
      ob->hitsouth = 17;
      ob->x = static_cast<word>(ob->x + TILEGLOBAL - (ob->x & 0xFF));
    }
  } else if (ob->hitsouth == 17 && ob->tileleft != ob->tileright) {
    ob->x &= 0xFF00;
  }

  if (ob->hitnorth == 17 || ob->hitsouth == 17) {
    // passing through a pole hole
    ytry = static_cast<sword>(ob->state->ymove * static_cast<sword>(tics) * ob->ydir);
    ob->y = static_cast<word>(ob->y + ytry);
    ob->top = static_cast<word>(ob->top + ytry);
    ob->bottom = static_cast<word>(ob->bottom + ytry);
    ob->tiletop = ob->top >> G_T_SHIFT;
    ob->tilebottom = ob->bottom >> G_T_SHIFT;
  } else if (ob->hitnorth || ob->hitsouth || ob->hiteast || ob->hitwest) {
    ExplodeShot(ob);
  }

  RF_PlaceSprite(&ob->sprite, ob->x, ob->y, ob->shapenum, spritedraw, static_cast<int>(ob->priority));
}

// 0D8F:0F85: a door opening, one tile step per state.
void T_Door(objtype* ob) {
  word tiles[64];
  word* map = FGSpot(ob->x, ob->y);
  int count = ob->temp1 + 2;
  for (int i = 0; i < count && i < 64; i++) {
    tiles[i] = static_cast<word>(*map + 1);
    map += mapwidth;
  }
  RF_MemToMap(tiles, 1, ob->x, ob->y, 1, static_cast<word>(count));
}

}  // namespace

// 0D8F:0003
void SpawnScoreObj(objtype* ob) {
  ob->obclass = inertobj;
  ob->priority = 3;
  ob->active = ac_allways;
  ob->needtoclip = cl_noclip;
  ob->temp2 = ob->temp1 = ob->temp3 = ob->temp4 = -1;

  if (scorescreenkludge) {
    ob->state = ST(s_scoredummy);
    return;
  }
  if (!DemoMode) {
    NewState(ob, ST(s_score));
  } else {
    NewState(ob, ST(s_demo));
    CA_MarkGrChunk(DEMOPLAQUESPR);
  }
}

void SpawnScore() { SpawnScoreObj(scoreobj); }

// 0D8F:014F
void UpdateScore(objtype* ob) {
  // port: in co-op Tom has his own score box, at the top right
  const boolean tom = numplayers > 1 && ob == players[1].scorebox;
  const PlayerState& ps = tom ? players[1] : players[0];
  if (scorescreenkludge) return;
  if (DemoMode) {
    DrawDemoPlaque(ob);
    return;
  }
  if (!showscorebox) return;

  boolean changed = false;
  CA_CacheGrChunk(SCOREBOXSPR);
  byte* shape = tom ? CoopScoreBox() : grsegs[SCOREBOXSPR];
  const spritetabletype& spr = spritetable[SCOREBOXSPR - STARTSPRITES];
  int width = spr.width;
  int planesize = spr.width * spr.height;
  byte* plane0 = shape + planesize;  // after the mask
  char str[16];

  word scorehi = static_cast<word>(static_cast<std::uint32_t>(ps.score) >> 16);
  word scorelo = static_cast<word>(ps.score & 0xFFFF);
  if (static_cast<word>(ob->temp1) != scorehi || static_cast<word>(ob->temp2) != scorelo) {
    std::snprintf(str, sizeof str, "%ld", static_cast<long>(ps.score));
    DrawScoreField(plane0 + width * 4, width, planesize, 9, str);
    ShiftScore();
    ob->needtoreact = true;
    ob->temp1 = static_cast<sword>(scorehi);
    ob->temp2 = static_cast<sword>(scorelo);
    changed = true;
  }

  word ammo = static_cast<word>(ps.ammo);
  if (static_cast<word>(ob->temp3) != ammo) {
    if (ammo > 99)
      std::strcpy(str, "99");
    else
      std::snprintf(str, sizeof str, "%u", static_cast<unsigned>(ammo));
    DrawScoreField(plane0 + width * 20 + 7, width, planesize, 2, str);
    ShiftScore();
    ob->needtoreact = true;
    ob->temp3 = static_cast<sword>(ammo);
    changed = true;
  }

  if (ob->temp4 != gamestate.lives) {
    if (gamestate.lives > 99)
      std::strcpy(str, "99");
    else
      std::snprintf(str, sizeof str, "%d", static_cast<int>(gamestate.lives));
    DrawScoreField(plane0 + width * 20 + 2, width, planesize, 2, str);
    ShiftScore();
    ob->needtoreact = true;
    ob->temp4 = gamestate.lives;
    changed = true;
  }

  // port: in co-op each player's box has its place, and on a split screen
  // shows only in his view
  word x = originxglobal, y = originyglobal;
  int view = -1;
  if (numplayers > 1) CoopScoreBoxAt(tom ? 1 : 0, width * 8, &x, &y, &view);
  if (ob->x != x || ob->y != y) {
    ob->x = x;
    ob->y = y;
    changed = true;
  }

  if (changed) {
    RF_PlaceSprite(&ob->sprite, static_cast<word>(ob->x + 4 * PIXGLOBAL), static_cast<word>(ob->y + 4 * PIXGLOBAL),
                   SCOREBOXSPR, spritedraw, 3);
    if (tom) RF_SetSpriteVariant(ob->sprite, 1, shape);
  }
  if (numplayers > 1) RF_SetSpriteView(ob->sprite, view);
}

// 0D8F:050A
void SpawnWorldKeenObj(objtype* ob, word tilex, word tiley) {
  if (playstate == ex_foot) {
    ob->needtoclip = cl_noclip;
    ob->obclass = keenobj;
    ob->x = static_cast<word>(gamestate.worldx);
    ob->y = static_cast<word>(gamestate.worldy);
    ob->active = ac_allways;
    ob->priority = 3;
    ob->xdir = ob->ydir = 0;
    if (static_cast<word>(gamestate.worldx) < 20 * TILEGLOBAL) {
      ob->temp1 = 280;
      ob->xspeed = static_cast<sword>(static_cast<word>(30 * TILEGLOBAL - ob->x) / 280 + 1);
      ob->yspeed = static_cast<sword>(static_cast<word>(55 * TILEGLOBAL - ob->y) / 280 + 1);
    } else {
      ob->temp1 = 140;
      ob->xspeed = static_cast<sword>(static_cast<sword>(16 * TILEGLOBAL - ob->x) / 140 + 1);
      ob->yspeed = static_cast<sword>(static_cast<sword>(47 * TILEGLOBAL - ob->y) / 140 + 1);
    }
    NewState(ob, ST(s_keenonfoot1));
    return;
  }

  ob->obclass = keenobj;
  if (!gamestate.worldx) {
    ob->x = static_cast<word>(tilex << G_T_SHIFT);
    ob->y = static_cast<word>(tiley << G_T_SHIFT);
  } else {
    ob->x = static_cast<word>(gamestate.worldx);
    ob->y = static_cast<word>(gamestate.worldy);
  }
  ob->active = ac_allways;
  ob->priority = 1;
  ob->xdir = ob->ydir = 0;
  ob->temp1 = 6;
  ob->temp2 = 3;
  ob->temp3 = 0;
  ob->shapenum = 256;
  NewState(ob, ST(s_worldkeen));
}

void SpawnWorldKeen(word tilex, word tiley) { SpawnWorldKeenObj(player, tilex, tiley); }

// 0D8F:062E
void CheckEnterLevel(objtype* ob) {
  for (word y = ob->tiletop; ob->tilebottom >= y; y++) {
    for (word x = ob->tileleft; ob->tileright >= x; x++) {
      word info = *InfoSpot(x, y);
      if (info > 0xC000 && info <= 0xC012) {
        gamestate.worldx = static_cast<sword>(ob->x);
        gamestate.worldy = static_cast<sword>(ob->y);
        gamestate.mapon = static_cast<sword>(info - 0xC000);
        playstate = ex_completed;
        SD_PlaySound(SND_ENTERLEVEL);
      }
    }
  }
}

// 0D8F:08A9: water edges on the world map.
void CheckWorldInTiles(objtype* ob) {
  if (ob->temp3) return;

  word x = ob->tilemidx;
  word y = static_cast<word>((ob->top + static_cast<word>(ob->bottom - ob->top) / 2) >> G_T_SHIFT);
  word intile = TINF_IN(*FGSpot(x, y));
  if (intile != 11 && intile != 13 && intile != 14 && intile != 12) return;

  if (!gamestate.wetsuit) {
    SD_PlaySound(SND_NOWAY);
    CantSwim();
    RF_ForceRefresh();
    xtry = static_cast<sword>(-ob->xmove);
    ytry = static_cast<sword>(-ob->ymove);
    ob->xdir = ob->ydir = 0;
    ClipToWalls(ob);
    return;
  }

  ob->temp1 = tiledir(intile - 11);
  if (ob->state == ST(s_worldswim)) ob->temp1 = oppositedir(ob->temp1);
  switch (ob->temp1) {
    case 0: ob->xdir = 0; ob->ydir = -1; break;
    case 2: ob->xdir = 1; ob->ydir = 0; break;
    case 4: ob->xdir = 0; ob->ydir = 1; break;
    case 6: ob->xdir = -1; ob->ydir = 0; break;
    default: break;
  }
  ob->temp2 = 0;
  ob->temp3 = 18;
  if (ob->state == ST(s_worldswim))
    ChangeState(ob, ST(s_worldkeenwalk));
  else
    ChangeState(ob, ST(s_worldswim));
}

// 0D8F:09FD
void SpawnFlag(word tilex, word tiley) {
  GetNewObj(false);
  newobj->needtoclip = cl_noclip;
  newobj->priority = 3;
  newobj->obclass = flagobj;
  newobj->active = ac_yes;
  newobj->x = static_cast<word>((tilex << G_T_SHIFT) + 6 * PIXGLOBAL);
  newobj->y = static_cast<word>((tiley << G_T_SHIFT) - 30 * PIXGLOBAL);
  newobj->ticcount = static_cast<sword>(US_RndT() / 16);
  NewState(newobj, ST(s_flagwave1));
}

// 0D8F:0A5B: the flag thrown onto a finished level.
void SpawnThrowFlag(word tilex, word tiley) {
  GetNewObj(false);
  newobj->needtoclip = cl_noclip;
  newobj->priority = 3;
  newobj->obclass = flagobj;
  newobj->active = ac_allways;
  newobj->x = static_cast<word>(gamestate.worldx - 16 * PIXGLOBAL);
  newobj->y = static_cast<word>(gamestate.worldy - 16 * PIXGLOBAL);
  newobj->temp1 = static_cast<sword>((tilex << G_T_SHIFT) + 6 * PIXGLOBAL);
  newobj->temp2 = static_cast<sword>((tiley << G_T_SHIFT) - 38 * PIXGLOBAL);

  std::int32_t dx = static_cast<std::int32_t>(static_cast<word>(newobj->temp1)) - newobj->x;
  std::int32_t dy = static_cast<std::int32_t>(static_cast<word>(newobj->temp2)) - newobj->y;

  for (int i = 0; i < 30; i++) {
    int xi = i < 24 ? i : 24;
    flagpath[i].x = static_cast<word>(newobj->x + (dx * xi) / 24);
    flagpath[i].y = static_cast<word>(newobj->y + (dy * i) / 30);
    if (i < 10)
      flagpath[i].y = static_cast<word>(flagpath[i].y - i * 0x30);
    else if (i < 15)
      flagpath[i].y = static_cast<word>(flagpath[i].y - (i * 16 + 0x140));
    else if (i < 20)
      flagpath[i].y = static_cast<word>(flagpath[i].y - ((20 - i) * 16 + 0x1E0));
    else
      flagpath[i].y = static_cast<word>(flagpath[i].y - (29 - i) * 0x30);
  }
  NewState(newobj, ST(s_throwflag0));
}

// 0D8F:0C76
void SpawnShot(objtype* shooter, word x, word y, sword dir) {
  PlayerState& ps = PlayerOf(shooter);
  if (!ps.ammo) {
    SD_PlaySound(SND_NOAMMO);
    return;
  }
  ps.ammo--;
  GetNewObj(true);
  newobj->x = x;
  newobj->y = y;
  newobj->priority = 0;
  newobj->obclass = stunshotobj;
  newobj->active = ac_allways;
  SD_PlaySound(SND_KEENFIRE);
  switch (dir) {
    case 0: newobj->xdir = 0; newobj->ydir = -1; break;
    case 2: newobj->xdir = 1; newobj->ydir = 0; break;
    case 4: newobj->xdir = 0; newobj->ydir = 1; break;
    case 6: newobj->xdir = -1; newobj->ydir = 0; break;
    default: Quit("SpawnShot: Bad dir!");
  }
  NewState(newobj, ST(s_stunray1));
}

// 0D8F:0D43
void ExplodeShot(objtype* ob) {
  ob->obclass = inertobj;
  ChangeState(ob, ST(s_stunhit1));
  SD_PlaySound(SND_SHOTEXPLODE);
}

CK_FUNC(0x0D8F, 0x06AF, T_KeenWorld);
CK_FUNC(0x0D8F, 0x06F0, T_KeenWorldWalk);
CK_FUNC(0x0D8F, 0x07AB, T_FlyKeen);
CK_FUNC(0x0D8F, 0x0826, T_WorldKeenSwim);
CK_FUNC(0x0D8F, 0x0BC4, TossThink);
CK_FUNC(0x0D8F, 0x0BEF, PathThink);
CK_FUNC(0x0D8F, 0x0C51, FlagAlign);
CK_FUNC(0x0D8F, 0x0D6A, T_Shot);
CK_FUNC(0x0D8F, 0x0E32, R_Shot);
CK_FUNC(0x0D8F, 0x0F85, T_Door);
