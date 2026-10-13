// CK_PHYS: actor movement, clipping and the state machine (segment 09DC).
#include <cstdio>
#include "game/ck_def.h"
#include "exe/keen4exe.h"

sword xtry, ytry;                                               // C923 C921
boolean playerkludgeclipcancel;                                 // C91F
static word oldtileright, oldtiletop, oldtileleft, oldtilebottom, oldtilemidx;  // C919..C915
static word oldright, oldtop, oldleft, oldbottom, oldmidx;      // C90F..C90B
static sword leftmoved, rightmoved, topmoved, bottommoved, midxmoved;  // C909..C901

void ExplodeShot(objtype* shot);  // CK_KEEN2

namespace {

// DGROUP 079C: where a sloped wall's surface is, per slope type and pixel.
sword WallClip(int wall, int pix) {
  return keen4::exe::g_exe.Int(static_cast<std::uint16_t>(0x079C + ((wall & 7) * 16 + pix) * 2));
}

inline word* MapPlane1(word x, word y) { return mapsegs[1] + mapbwidthtable[y] / 2 + x; }

const spritetabletype& Shape(word shapenum) { return spritetable[shapenum - STARTSPRITES]; }

inline sword sabs(sword v) { return static_cast<sword>(v < 0 ? -v : v); }

// Recomputes the hit box and tile bounds from the sprite shape.
void SetBoundsFromShape(objtype* ob, const spritetabletype& shape) {
  ob->left = static_cast<word>(ob->x + shape.xl);
  ob->right = static_cast<word>(ob->x + shape.xh);
  ob->top = static_cast<word>(ob->y + shape.yl);
  ob->bottom = static_cast<word>(ob->y + shape.yh);
  ob->midx = static_cast<word>(ob->left + static_cast<word>(ob->right - ob->left) / 2);
}

void SetTileBounds(objtype* ob) {
  ob->tileleft = ob->left >> G_T_SHIFT;
  ob->tileright = ob->right >> G_T_SHIFT;
  ob->tiletop = ob->top >> G_T_SHIFT;
  ob->tilebottom = ob->bottom >> G_T_SHIFT;
  ob->tilemidx = ob->midx >> G_T_SHIFT;
}

// 09DC:0005
void MoveObjVert(objtype* ob, sword ymove) {
  ob->y = static_cast<word>(ob->y + ymove);
  ob->top = static_cast<word>(ob->top + ymove);
  ob->bottom = static_cast<word>(ob->bottom + ymove);
  ob->tiletop = ob->top >> G_T_SHIFT;
  ob->tilebottom = ob->bottom >> G_T_SHIFT;
}

// 09DC:002D
void MoveObjHoriz(objtype* ob, sword xmove) {
  ob->x = static_cast<word>(ob->x + xmove);
  ob->left = static_cast<word>(ob->left + xmove);
  ob->right = static_cast<word>(ob->right + xmove);
  ob->tileleft = ob->left >> G_T_SHIFT;
  ob->tileright = ob->right >> G_T_SHIFT;
}

// 09DC:0055: lets Keen walk down onto a slope instead of falling off it.
void PlayerBottomKludge(objtype* ob) {
  word* map = mapsegs[1] + mapbwidthtable[ob->tilebottom - 1] / 2;
  int xpix;
  sword xmove;
  if (ob->xdir == 1) {
    xpix = 0;
    map += ob->tileright;
    xmove = static_cast<sword>(ob->right - ob->midx);
    if (TINF_W(*(map - mapwidth)) || TINF_W(*map)) return;
  } else {
    xpix = 15;
    map += ob->tileleft;
    xmove = static_cast<sword>(ob->left - ob->midx);
    if (TINF_E(*(map - mapwidth)) || TINF_E(*map)) return;
  }
  if (TINF_N(*map)) return;
  map += mapwidth;
  word wall = TINF_N(*map);
  if (wall != 1) return;
  sword move = static_cast<sword>((ob->tilebottom << G_T_SHIFT) + WallClip(wall, xpix) - 1 - ob->bottom);
  if (move > 0 || -bottommoved > move) return;
  ob->hitnorth = static_cast<sword>(wall);
  MoveObjVert(ob, move);
  MoveObjHoriz(ob, xmove);
}

// 09DC:019F
void PlayerTopKludge(objtype* ob) {
  word* map = mapsegs[1] + mapbwidthtable[ob->tiletop + 1] / 2;
  int xpix;
  if (ob->xdir == 1) {
    xpix = 0;
    map += ob->tileright;
    if (TINF_W(*(map + mapwidth)) || TINF_W(*(map + 2 * mapwidth))) return;
  } else {
    xpix = 15;
    map += ob->tileleft;
    if (TINF_E(*(map + mapwidth)) || TINF_E(*(map + 2 * mapwidth))) return;
  }
  if (TINF_S(*map)) return;
  map -= mapwidth;
  word wall = TINF_S(*map);
  if (!wall) return;
  sword move = static_cast<sword>(((ob->tiletop + 1) << G_T_SHIFT) - WallClip(wall, xpix) - ob->top);
  if (move < 0 || -topmoved < move) return;
  ob->hitsouth = static_cast<sword>(wall);
  MoveObjVert(ob, move);
}

}  // namespace

// 09DC:02DA
void ClipToEnds(objtype* ob) {
  int midxpix = (ob->midx & 0xF0) >> 4;

  sword maxmove = static_cast<sword>(-sabs(midxmoved) - bottommoved - 16);
  word* map = mapsegs[1] + mapbwidthtable[oldtilebottom - 1] / 2 + ob->tilemidx;
  for (word mapy = static_cast<word>(oldtilebottom - 1); ob->tilebottom >= mapy; mapy++, map += mapwidth) {
    word wall = TINF_N(*map);
    if (wall) {
      sword move = static_cast<sword>(((mapy << G_T_SHIFT) + WallClip(wall, midxpix)) - 1 - ob->bottom);
      if (move < 0 && move >= maxmove) {
        ob->hitnorth = static_cast<sword>(wall);
        MoveObjVert(ob, move);
        return;
      }
    }
  }

  maxmove = static_cast<sword>(sabs(midxmoved) - topmoved + 16);
  map = mapsegs[1] + mapbwidthtable[oldtiletop + 1] / 2 + ob->tilemidx;
  for (word mapy = static_cast<word>(oldtiletop + 1); ob->tiletop <= mapy; mapy--, map -= mapwidth) {
    word wall = TINF_S(*map);
    if (wall) {
      sword move = static_cast<sword>((((mapy + 1) << G_T_SHIFT) - WallClip(wall, midxpix)) - ob->top);
      if (move > 0 && move <= maxmove) {
        sword totalmove = static_cast<sword>(ytry + move);
        if (totalmove < 0x100 && totalmove > -0x100) {
          ob->hitsouth = static_cast<sword>(wall);
          MoveObjVert(ob, move);
        }
      }
    }
  }
}

// 09DC:0469
void ClipToSides(objtype* ob) {
  sword top = static_cast<sword>(ob->tiletop);
  if (ob->hitsouth > 1) top++;
  sword bottom = static_cast<sword>(ob->tilebottom);
  if (ob->hitnorth > 1) bottom--;

  for (sword y = top; y <= bottom; y++) {
    word* map = MapPlane1(ob->tileleft, static_cast<word>(y));
    if ((ob->hiteast = TINF_E(*map)) != 0) {
      sword move = static_cast<sword>(((ob->tileleft + 1) << G_T_SHIFT) - ob->left);
      MoveObjHoriz(ob, move);
      return;
    }
  }
  for (sword y = top; y <= bottom; y++) {
    word* map = MapPlane1(ob->tileright, static_cast<word>(y));
    if ((ob->hitwest = TINF_W(*map)) != 0) {
      sword move = static_cast<sword>((ob->tileright << G_T_SHIFT) - 1 - ob->right);
      MoveObjHoriz(ob, move);
      return;
    }
  }
}

// 09DC:0558
boolean CheckPosition(objtype* ob) {
  word* map = MapPlane1(ob->tileleft, ob->tiletop);
  word rowdelta = static_cast<word>(mapwidth - (ob->tileright - ob->tileleft + 1));
  for (word y = ob->tiletop; ob->tilebottom >= y; y++, map += rowdelta) {
    for (word x = ob->tileleft; ob->tileright >= x; x++) {
      word tile = *map++;
      if (TINF_N(tile) || TINF_E(tile) || TINF_S(tile) || TINF_W(tile)) return false;
    }
  }
  return true;
}

// 09DC:05F9
boolean StatePositionOk(objtype* ob, statetype* state) {
  if (ob->xdir > 0)
    ob->shapenum = static_cast<word>(state->rightshapenum);
  else
    ob->shapenum = static_cast<word>(state->leftshapenum);
  SetBoundsFromShape(ob, Shape(ob->shapenum));
  SetTileBounds(ob);
  return CheckPosition(ob);
}

// 09DC:06A5
void ClipToWalls(objtype* ob) {
  word oldx = ob->x, oldy = ob->y;
  boolean pushed = false;

  if (ob->state->pushtofloor) {
    if (ob->hitnorth == 25) {
      ytry = 145;
    } else {
      if (xtry > 0)
        ytry = static_cast<sword>(xtry + 16);
      else
        ytry = static_cast<sword>(-xtry + 16);
      pushed = true;
    }
  }

  if (xtry > 239)
    xtry = 239;
  else if (xtry < -239)
    xtry = -239;
  if (ytry > 255)
    ytry = 255;
  else if (ytry < -239)
    ytry = -239;

  ob->x = static_cast<word>(ob->x + xtry);
  ob->y = static_cast<word>(ob->y + ytry);
  ob->needtoreact = true;

  if (!ob->shapenum) return;
  const spritetabletype& shape = Shape(ob->shapenum);

  oldtileright = ob->tileright;
  oldtiletop = ob->tiletop;
  oldtileleft = ob->tileleft;
  oldtilebottom = ob->tilebottom;
  oldtilemidx = ob->tilemidx;
  oldright = ob->right;
  oldtop = ob->top;
  oldleft = ob->left;
  oldbottom = ob->bottom;
  oldmidx = ob->midx;

  SetBoundsFromShape(ob, shape);
  SetTileBounds(ob);
  ob->hitnorth = ob->hiteast = ob->hitsouth = ob->hitwest = 0;

  if (ob->needtoclip) {
    leftmoved = static_cast<sword>(ob->left - oldleft);
    rightmoved = static_cast<sword>(ob->right - oldright);
    topmoved = static_cast<sword>(ob->top - oldtop);
    bottommoved = static_cast<sword>(ob->bottom - oldbottom);
    midxmoved = static_cast<sword>(ob->midx - oldmidx);

    ClipToEnds(ob);
    if (PlayerIndex(ob) >= 0 && !playerkludgeclipcancel) {
      if (!ob->hitnorth && bottommoved > 0) PlayerBottomKludge(ob);
      if (!ob->hitsouth && topmoved < 0) PlayerTopKludge(ob);
    }
    ClipToSides(ob);
  }

  if (pushed && !ob->hitnorth) {
    // only push to the floor if it is there
    ob->y = oldy;
    ob->x = static_cast<word>(oldx + xtry);
    SetBoundsFromShape(ob, shape);
    SetTileBounds(ob);
  }

  ob->xmove = static_cast<sword>(ob->xmove + static_cast<sword>(ob->x - oldx));
  ob->ymove = static_cast<sword>(ob->ymove + static_cast<sword>(ob->y - oldy));
}

// 09DC:0936. Used by actors that swim or fly through the world as a box.
void FullClipToWalls(objtype* ob) {
  word oldx = ob->x, oldy = ob->y;

  if (xtry > 239)
    xtry = 239;
  else if (xtry < -239)
    xtry = -239;
  if (ytry > 239)
    ytry = 239;
  else if (ytry < -239)
    ytry = -239;

  ob->x = static_cast<word>(ob->x + xtry);
  ob->y = static_cast<word>(ob->y + ytry);
  ob->needtoreact = true;

  const spritetabletype& shape = Shape(ob->shapenum);

  word width, height;
  switch (ob->obclass) {
    case keenobj: width = 0x280; height = 0x180; break;
    case eggbirdobj: width = 0x400; height = 0x200; break;
    case dopefishobj: width = 0x580; height = 0x400; break;
    case schoolfishobj: width = 0x100; height = 0x80; break;
    default:
      Quit("FullClipToWalls: Bad obclass");
  }

  ob->right = static_cast<word>(ob->x + width);
  ob->left = ob->x;
  ob->top = ob->y;
  ob->bottom = static_cast<word>(ob->y + height);
  ob->tileleft = ob->left >> G_T_SHIFT;
  ob->tileright = ob->right >> G_T_SHIFT;
  ob->tiletop = ob->top >> G_T_SHIFT;
  ob->tilebottom = ob->bottom >> G_T_SHIFT;
  ob->hitnorth = ob->hiteast = ob->hitsouth = ob->hitwest = 0;

  if (!CheckPosition(ob)) {
    MoveObjHoriz(ob, static_cast<sword>(-xtry));
    if (CheckPosition(ob)) {
      if (xtry > 0)
        ob->hitwest = 1;
      else
        ob->hiteast = 1;
    } else {
      if (ytry > 0)
        ob->hitnorth = 1;
      else
        ob->hitsouth = 1;
      MoveObjHoriz(ob, xtry);
      MoveObjVert(ob, static_cast<sword>(-ytry));
      if (!CheckPosition(ob)) {
        MoveObjHoriz(ob, static_cast<sword>(-xtry));
        if (xtry > 0)
          ob->hitwest = 1;
        else
          ob->hiteast = 1;
      }
    }
  }

  ob->xmove = static_cast<sword>(ob->xmove + static_cast<sword>(ob->x - oldx));
  ob->ymove = static_cast<sword>(ob->ymove + static_cast<sword>(ob->y - oldy));

  SetBoundsFromShape(ob, shape);
}

// 09DC:0B49
void PushObj(objtype* ob) {
  word oldx = ob->x, oldy = ob->y;
  ob->x = static_cast<word>(ob->x + xtry);
  ob->y = static_cast<word>(ob->y + ytry);
  ob->needtoreact = true;

  if (!ob->shapenum) return;
  const spritetabletype& shape = Shape(ob->shapenum);

  oldtileright = ob->tileright;
  oldtiletop = ob->tiletop;
  oldtileleft = ob->tileleft;
  oldtilebottom = ob->tilebottom;
  oldtilemidx = ob->tilemidx;
  oldright = ob->right;
  oldtop = ob->top;
  oldleft = ob->left;
  oldbottom = ob->bottom;
  oldmidx = ob->midx;

  SetBoundsFromShape(ob, shape);
  SetTileBounds(ob);

  if (ob->needtoclip) {
    leftmoved = static_cast<sword>(ob->left - oldleft);
    rightmoved = static_cast<sword>(ob->right - oldright);
    topmoved = static_cast<sword>(ob->top - oldtop);
    bottommoved = static_cast<sword>(ob->bottom - oldbottom);
    midxmoved = static_cast<sword>(ob->midx - oldmidx);
    ClipToEnds(ob);
    ClipToSides(ob);
  }

  ob->xmove = static_cast<sword>(ob->xmove + static_cast<sword>(ob->x - oldx));
  ob->ymove = static_cast<sword>(ob->ymove + static_cast<sword>(ob->y - oldy));
}

// 09DC:0C9A
void ClipToSpriteSide(objtype* push, objtype* solid) {
  sword xmove = static_cast<sword>(solid->xmove - push->xmove);
  sword leftinto = static_cast<sword>(solid->right - push->left);
  sword rightinto = static_cast<sword>(push->right - solid->left);

  if (leftinto > 0 && leftinto <= xmove) {
    xtry = leftinto;
    if (push->state->pushtofloor) ytry = static_cast<sword>(leftinto + 16);
    ClipToWalls(push);
    push->hiteast = 1;
    return;
  }
  if (rightinto > 0 && -xmove >= rightinto) {
    xtry = static_cast<sword>(-rightinto);
    if (push->state->pushtofloor) ytry = static_cast<sword>(rightinto + 16);
    ClipToWalls(push);
    push->hitwest = 1;
  }
}

// 09DC:0D33
void ClipToSpriteTop(objtype* push, objtype* solid) {
  sword ymove = static_cast<sword>(push->ymove - solid->ymove);
  sword bottominto = static_cast<sword>(push->bottom - solid->top);
  if (bottominto >= 0 && bottominto <= ymove) {
    if (PlayerIndex(push) >= 0) PlayerOf(push).riding = solid;
    ytry = static_cast<sword>(-bottominto);
    boolean temp = push->state->pushtofloor;
    push->state->pushtofloor = false;
    ClipToWalls(push);
    push->state->pushtofloor = temp;
    if (!push->hitsouth) push->hitnorth = 25;
  }
}

// 09DC:0DA0
void ClipToSprite(objtype* push, objtype* solid, boolean squish) {
  sword xmove = static_cast<sword>(solid->xmove - push->xmove);
  xtry = ytry = 0;

  sword leftinto = static_cast<sword>(solid->right - push->left);
  sword rightinto = static_cast<sword>(push->right - solid->left);

  if (leftinto > 0 && xmove + 1 >= leftinto) {
    xtry = leftinto;
    push->xspeed = 0;
    PushObj(push);
    if (squish && push->hitwest) KillKeen(push);
    push->hiteast = 1;
    return;
  }
  if (rightinto > 0 && -xmove + 1 >= rightinto) {
    xtry = static_cast<sword>(-rightinto);
    push->xspeed = 0;
    PushObj(push);
    if (squish && push->hiteast) KillKeen(push);
    push->hitwest = 1;
    return;
  }

  sword ymove = static_cast<sword>(push->ymove - solid->ymove);
  sword topinto = static_cast<sword>(solid->bottom - push->top);
  sword bottominto = static_cast<sword>(push->bottom - solid->top);

  if (bottominto >= 0 && bottominto <= ymove) {
    if (PlayerIndex(push) >= 0) PlayerOf(push).riding = solid;
    ytry = static_cast<sword>(-bottominto);
    PushObj(push);
    if (squish && push->hitsouth) KillKeen(push);
    if (!push->hitsouth) push->hitnorth = 25;
    return;
  }
  if (topinto >= 0 && topinto <= ymove) {
    ytry = topinto;
    ClipToWalls(push);
    if (squish && push->hitnorth) KillKeen(push);
    push->hitsouth = 25;
  }
}

// 09DC:0EEE. Runs one state for up to `tics`; returns tics left over when the
// state finished early.
static sword StateMachineTics(objtype* ob, sword tics) {
  statetype* state = ob->state;

  if (state->progress == think) {
    if (state->think) {
      if (ob->nothink)
        ob->nothink--;
      else
        state->think(ob);
    }
    return 0;
  }

  sword excesstics = static_cast<sword>(ob->ticcount + tics);
  if (state->tictime > excesstics || state->tictime == 0) {
    ob->ticcount = excesstics;
    if (state->progress == slide || state->progress == slidethink) {
      if (ob->xdir) {
        if (ob->xdir == 1)
          xtry = static_cast<sword>(xtry + state->xmove * tics);
        else
          xtry = static_cast<sword>(xtry + (-tics) * state->xmove);
      }
      if (ob->ydir) {
        if (ob->ydir == 1)
          ytry = static_cast<sword>(ytry + state->ymove * tics);
        else
          ytry = static_cast<sword>(ytry + (-tics) * state->ymove);
      }
    }
    if ((state->progress == slidethink || state->progress == stepthink) && state->think) {
      if (ob->nothink)
        ob->nothink--;
      else
        state->think(ob);
    }
    return 0;
  }

  sword ticksleft = static_cast<sword>(state->tictime - ob->ticcount);
  sword newtics = static_cast<sword>(excesstics - state->tictime);
  ob->ticcount = 0;
  if (state->progress == slide || state->progress == slidethink) {
    if (ob->xdir) {
      if (ob->xdir == 1)
        xtry = static_cast<sword>(xtry + state->xmove * ticksleft);
      else
        xtry = static_cast<sword>(xtry + (-ticksleft) * state->xmove);
    }
    if (ob->ydir) {
      if (ob->ydir == 1)
        ytry = static_cast<sword>(ytry + state->ymove * ticksleft);
      else
        ytry = static_cast<sword>(ytry + (-ticksleft) * state->ymove);
    }
  } else {
    if (ob->xdir) xtry = static_cast<sword>(xtry + (ob->xdir == 1 ? state->xmove : -state->xmove));
    if (ob->ydir) ytry = static_cast<sword>(ytry + (ob->ydir == 1 ? state->ymove : -state->ymove));
  }

  if (state->think) {
    if (ob->nothink)
      ob->nothink--;
    else
      state->think(ob);
  }

  if (state == ob->state)
    ob->state = state->nextstate;
  else if (!ob->state)
    return 0;
  return newtics;
}

// 09DC:1097
void DoActor(objtype* ob) {
  xtry = ytry = ob->xmove = ob->ymove = 0;
  word oldshape = ob->shapenum;
  statetype* state = ob->state;

  sword ticcount = StateMachineTics(ob, static_cast<sword>(tics));
  if (ob->state != state) {
    ob->ticcount = 0;
    state = ob->state;
  }
  while (ticcount) {
    // A state chain that ends with tics left over leaves state NULL. The
    // original then reads DS:0 as a state; those bytes make it a long,
    // skippable non-think state, so the loop ends without moving the object.
    if (!state) break;
    if (!state->skippable && state->tictime <= ticcount)
      ticcount = StateMachineTics(ob, static_cast<sword>(state->tictime - 1));
    else
      ticcount = StateMachineTics(ob, ticcount);
    if (ob->state != state) {
      ob->ticcount = 0;
      state = ob->state;
    }
  }

  if (!state) {
    RemoveObj(ob);
    return;
  }

  if (state->rightshapenum) {
    if (ob->xdir > 0)
      ob->shapenum = static_cast<word>(state->rightshapenum);
    else
      ob->shapenum = static_cast<word>(state->leftshapenum);
  }
  if (static_cast<sword>(ob->shapenum) == -1) ob->shapenum = 0;

  if (xtry || ytry || ob->shapenum != oldshape || ob->hitnorth == 25) {
    if (ob->needtoclip == cl_fullclip)
      FullClipToWalls(ob);
    else
      ClipToWalls(ob);
  }
}

// 09DC:118C
void NewState(objtype* ob, statetype* state) {
  ob->state = state;
  if (state->rightshapenum) {
    if (ob->xdir > 0)
      ob->shapenum = static_cast<word>(state->rightshapenum);
    else
      ob->shapenum = static_cast<word>(state->leftshapenum);
  }
  if (static_cast<sword>(ob->shapenum) == -1) ob->shapenum = 0;

  cliptype temp = ob->needtoclip;
  ob->needtoclip = cl_noclip;
  xtry = ytry = 0;
  ClipToWalls(ob);  // just calculate values
  ob->needtoclip = temp;
  if (ob->needtoclip == cl_fullclip)
    FullClipToWalls(ob);
  else if (ob->needtoclip == cl_midclip)
    ClipToWalls(ob);
}

// 09DC:120A
void ChangeState(objtype* ob, statetype* state) {
  ob->state = state;
  ob->ticcount = 0;
  if (state->rightshapenum) {
    if (ob->xdir > 0)
      ob->shapenum = static_cast<word>(state->rightshapenum);
    else
      ob->shapenum = static_cast<word>(state->leftshapenum);
  }
  if (static_cast<sword>(ob->shapenum) == -1) ob->shapenum = 0;
  ob->needtoreact = true;
  xtry = ytry = 0;
  if (ob->hitnorth != 25) ClipToWalls(ob);
}

// 09DC:1260
// Port: on screen in any camera's view.
boolean OnScreen(objtype* ob) {
  ViewBounds views[MAXCAMERAS];
  int count = GetViewBounds(views);
  for (int i = 0; i < count; i++) {
    const ViewBounds& v = views[i];
    if (!(ob->tileright < v.xtile || ob->tilebottom < v.ytile || ob->tileleft > v.xtilemax ||
          ob->tiletop > v.ytilemax))
      return true;
  }
  return false;
}

namespace {
// Iterates the tics of this frame like `for (i = lasttimecount-tics; i < lasttimecount; i++)`.
template <typename F>
void ForEachTic(F f) {
  std::int32_t last = static_cast<std::int32_t>(lasttimecount);
  for (std::int32_t i = last - tics; i < last; i++) {
    if (!f(i)) return;
  }
}
}  // namespace

// 09DC:1296
void DoGravity(objtype* ob) {
  ForEachTic([ob](std::int32_t i) {
    if (i & 1) {
      if (ob->yspeed < 0 && ob->yspeed >= -4) {
        ytry = static_cast<sword>(ytry + ob->yspeed);
        ob->yspeed = 0;
        return false;
      }
      ob->yspeed = static_cast<sword>(ob->yspeed + 4);
      if (ob->yspeed > 70) ob->yspeed = 70;
    }
    ytry = static_cast<sword>(ytry + ob->yspeed);
    return true;
  });
}

// 09DC:1315
void DoWeakGravity(objtype* ob) {
  ForEachTic([ob](std::int32_t i) {
    if (i & 1) {
      if (ob->yspeed < 0 && ob->yspeed >= -3) {
        ytry = static_cast<sword>(ytry + ob->yspeed);
        ob->yspeed = 0;
        return false;
      }
      ob->yspeed = static_cast<sword>(ob->yspeed + 3);
      if (ob->yspeed > 70) ob->yspeed = 70;
    }
    ytry = static_cast<sword>(ytry + ob->yspeed);
    return true;
  });
}

// 09DC:1394. The original tests (!i) & 3, which is only true for i == 0.
void DoTinyGravity(objtype* ob) {
  ForEachTic([ob](std::int32_t i) {
    if ((!i) & 3) {
      ob->yspeed++;
      if (ob->yspeed > 70) ob->yspeed = 70;
    }
    ytry = static_cast<sword>(ytry + ob->yspeed);
    return true;
  });
}

// 09DC:13FE
void AccelerateX(objtype* ob, sword dir, sword maxspeed) {
  word oldsign = ob->xspeed & 0x8000;
  ForEachTic([&](std::int32_t i) {
    if (i & 1) {
      ob->xspeed = static_cast<sword>(ob->xspeed + dir);
      if ((ob->xspeed & 0x8000) != oldsign) {
        oldsign = ob->xspeed & 0x8000;
        ob->xdir = oldsign ? -1 : 1;
      }
      if (ob->xspeed > maxspeed)
        ob->xspeed = maxspeed;
      else if (ob->xspeed < -maxspeed)
        ob->xspeed = static_cast<sword>(-maxspeed);
    }
    xtry = static_cast<sword>(xtry + ob->xspeed);
    return true;
  });
}

// 09DC:149F
void AccelerateXv(objtype* ob, sword dir, sword maxspeed) {
  ForEachTic([&](std::int32_t i) {
    if (i & 1) {
      ob->xspeed = static_cast<sword>(ob->xspeed + dir);
      if (ob->xspeed > maxspeed)
        ob->xspeed = maxspeed;
      else if (ob->xspeed < -maxspeed)
        ob->xspeed = static_cast<sword>(-maxspeed);
    }
    xtry = static_cast<sword>(xtry + ob->xspeed);
    return true;
  });
}

// 09DC:1517
void AccelerateY(objtype* ob, sword dir, sword maxspeed) {
  ForEachTic([&](std::int32_t i) {
    if (i & 1) {
      ob->yspeed = static_cast<sword>(ob->yspeed + dir);
      if (ob->yspeed > maxspeed)
        ob->yspeed = maxspeed;
      else if (ob->yspeed < -maxspeed)
        ob->yspeed = static_cast<sword>(-maxspeed);
    }
    ytry = static_cast<sword>(ytry + ob->yspeed);
    return true;
  });
}

// 09DC:158F
void FrictionX(objtype* ob) {
  word oldsign = ob->xspeed & 0x8000;
  sword friction = ob->xspeed > 0 ? -1 : ob->xspeed < 0 ? 1 : 0;
  ForEachTic([&](std::int32_t i) {
    if (i & 1) {
      ob->xspeed = static_cast<sword>(ob->xspeed + friction);
      if ((ob->xspeed & 0x8000) != oldsign) ob->xspeed = 0;
    }
    xtry = static_cast<sword>(xtry + ob->xspeed);
    return true;
  });
}

// 09DC:1616. The original compares against an uninitialised "oldsign" (the
// caller's DI register); FrictionYSigned takes that value explicitly.
void FrictionYSigned(objtype* ob, word oldsign) {
  sword friction = ob->yspeed > 0 ? -1 : ob->yspeed < 0 ? 1 : 0;
  ForEachTic([&](std::int32_t i) {
    if (i & 1) {
      ob->yspeed = static_cast<sword>(ob->yspeed + friction);
      if ((ob->yspeed & 0x8000) != oldsign) ob->yspeed = 0;
    }
    ytry = static_cast<sword>(ytry + ob->yspeed);
    return true;
  });
}

// 09DC:1695
void StunObj(objtype* ob, objtype* shot, statetype* stunstate) {
  ExplodeShot(shot);
  ob->temp1 = ob->temp2 = ob->temp3 = 0;
  ob->temp3sprite = nullptr;
  ob->temp4 = ob->obclass;
  ChangeState(ob, stunstate);
  ob->obclass = stunnedobj;
}

// 09DC:16C9
void T_Projectile(objtype* ob) {
  DoGravity(ob);
  xtry = static_cast<sword>(ob->xspeed * static_cast<sword>(tics));
}

// 09DC:16E5
void T_WeakProjectile(objtype* ob) {
  DoWeakGravity(ob);
  xtry = static_cast<sword>(ob->xspeed * static_cast<sword>(tics));
}

// 09DC:1701
void T_Velocity(objtype* ob) {
  xtry = static_cast<sword>(ob->xspeed * static_cast<sword>(tics));
  ytry = static_cast<sword>(ob->yspeed * static_cast<sword>(tics));
}

// 09DC:171F
void SetReactThink(objtype* ob) { ob->needtoreact = true; }

// 09DC:172E
void T_Stunned(objtype* ob) {
  ob->temp1 = 0;
  ob->needtoreact = true;
  if (++ob->temp2 == 3) ob->temp2 = 0;
}

// 09DC:1752
void C_Lethal(objtype*, objtype* hit) {
  if (hit->obclass == keenobj) KillKeen(hit);
}

// 09DC:176E
void R_Draw(objtype* ob) {
  RF_PlaceSprite(&ob->sprite, ob->x, ob->y, ob->shapenum, spritedraw, static_cast<int>(ob->priority));
}

// 09DC:1795
void R_Walk(objtype* ob) {
  if (ob->xdir == 1 && ob->hitwest) {
    ob->x = static_cast<word>(ob->x - ob->xmove);
    ob->xdir = -1;
    ob->nothink = static_cast<word>(US_RndT() >> 5);
    ChangeState(ob, ob->state);
  } else if (ob->xdir == -1 && ob->hiteast) {
    ob->x = static_cast<word>(ob->x - ob->xmove);
    ob->xdir = 1;
    ob->nothink = static_cast<word>(US_RndT() >> 5);
    ChangeState(ob, ob->state);
  } else if (!ob->hitnorth) {
    ob->x = static_cast<word>(ob->x - ob->xmove);
    ob->xdir = static_cast<sword>(-ob->xdir);
    ob->nothink = static_cast<word>(US_RndT() >> 5);
    ChangeState(ob, ob->state);
  }
  RF_PlaceSprite(&ob->sprite, ob->x, ob->y, ob->shapenum, spritedraw, static_cast<int>(ob->priority));
}

// 09DC:1847
void R_WalkNormal(objtype* ob) {
  if (ob->xdir == 1 && ob->hitwest) {
    ob->x = static_cast<word>(ob->x - ob->xmove);
    ob->xdir = -1;
    ob->nothink = static_cast<word>(US_RndT() >> 5);
    ChangeState(ob, ob->state);
  } else if (ob->xdir == -1 && ob->hiteast) {
    ob->x = static_cast<word>(ob->x - ob->xmove);
    ob->xdir = 1;
    ob->nothink = static_cast<word>(US_RndT() >> 5);
    ChangeState(ob, ob->state);
  } else if (!ob->hitnorth || (ob->hitnorth & ~7)) {
    ob->x = static_cast<word>(ob->x - ob->xmove * 2);
    ob->xdir = static_cast<sword>(-ob->xdir);
    ob->nothink = static_cast<word>(US_RndT() >> 5);
    ChangeState(ob, ob->state);
  }
  RF_PlaceSprite(&ob->sprite, ob->x, ob->y, ob->shapenum, spritedraw, static_cast<int>(ob->priority));
}

// 09DC:1902
void BadState(objtype*) { Quit("Object with bad state!"); }

// 09DC:1913
void R_Stunned(objtype* ob) {
  if (ob->hitwest || ob->hiteast) ob->xspeed = 0;
  if (ob->hitsouth) ob->yspeed = 0;
  if (ob->hitnorth) {
    ob->xspeed = ob->yspeed = 0;
    if (ob->state->nextstate) ChangeState(ob, ob->state->nextstate);
  }
  RF_PlaceSprite(&ob->sprite, ob->x, ob->y, ob->shapenum, spritedraw, static_cast<int>(ob->priority));

  sword starx = 0, stary = 0;
  switch (ob->temp4) {
    case slugobj: stary = -0x80; break;
    case eggobj: starx = 0x80; stary = -0x80; break;
    case treasureeaterobj: starx = 0x80; break;
    case bounderobj: starx = 0x40; stary = -0x80; break;
    case wormmouthobj: starx = 0x40; stary = -0x15E; break;
    case lickobj: stary = -0x80; break;
    case inchwormobj: starx = -0x40; stary = -0x80; break;
    case mimrockobj: stary = -0x40; break;
    default: break;
  }

  ob->temp1 = static_cast<sword>(ob->temp1 + tics);
  if (ob->temp1 > 10) {
    ob->temp1 -= 10;
    if (++ob->temp2 == 3) ob->temp2 = 0;
  }
  RF_PlaceSprite(&ob->temp3sprite, static_cast<word>(ob->x + starx), static_cast<word>(ob->y + stary),
                 static_cast<word>(ob->temp2 + 0xA4), spritedraw, 3);
}

CK_FUNC(0x09DC, 0x16C9, T_Projectile);
CK_FUNC(0x09DC, 0x16E5, T_WeakProjectile);
CK_FUNC(0x09DC, 0x1701, T_Velocity);
CK_FUNC(0x09DC, 0x171F, SetReactThink);
CK_FUNC(0x09DC, 0x172E, T_Stunned);
CK_FUNC(0x09DC, 0x1752, C_Lethal);
CK_FUNC(0x09DC, 0x176E, R_Draw);
CK_FUNC(0x09DC, 0x1795, R_Walk);
CK_FUNC(0x09DC, 0x1847, R_WalkNormal);
CK_FUNC(0x09DC, 0x1902, BadState);
CK_FUNC(0x09DC, 0x1913, R_Stunned);
CK_FUNC(0x09DC, 0x1296, DoGravity);
CK_FUNC(0x09DC, 0x1315, DoWeakGravity);
CK_FUNC(0x09DC, 0x1394, DoTinyGravity);
