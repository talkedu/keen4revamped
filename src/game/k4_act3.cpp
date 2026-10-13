// K4_ACT3: treasure eater, mimrock, dopefish and schoolfish, sprites
// (pixies), mines, Lindsey, dart guns and the wetsuit (segment 11A2).
#include "game/ck_def.h"

extern sword xtry, ytry;

namespace {

constexpr int SND_KEENFIRE = 4, SND_MIMROCKBOUNCE = 15, SND_KEENSWIMDIE = 23, SND_TREASUREEATER = 29,
              SND_EATERVANISH = 30, SND_SPRITESHOOT = 38, SND_MINEEXPLODE = 37, SND_DARTSHOOT = 41,
              SND_BURP = 42, SND_GETWETSUIT = 45;

const word blanktile = 0;  // DGROUP 1424

void PlaceSprite(objtype* ob) {
  RF_PlaceSprite(&ob->sprite, ob->x, ob->y, ob->shapenum, spritedraw, ob->priority);
}

// Items the treasure eater goes after (INTILE 4 is a drop, 21..28 are points
// and keys... everything that is collected by touching it).
bool IsTreasure(word intile) { return intile == 4 || (intile >= 21 && intile <= 28); }

void StunInline(objtype* ob, objtype* shot, word state, sword ykick) {
  ob->temp1 = ob->temp2 = ob->temp3 = 0;
  ob->temp4 = ob->obclass;
  ob->obclass = stunnedobj;
  ExplodeShot(shot);
  ChangeState(ob, ST(state));
  ob->yspeed = static_cast<sword>(ob->yspeed - ykick);
}

// 11A2:0079
void T_EaterJump(objtype* ob) {
  ob->state = ST(0x330E);

  // jump at a bonus object just above
  for (objtype* o = player->next; o; o = o->next) {
    if (o->obclass == bonusobj && o->active == ac_yes && o->right > ob->left && o->left < ob->right &&
        o->bottom < ob->top && static_cast<word>(o->bottom + 3 * TILEGLOBAL) > ob->top) {
      ob->xspeed = 0;
      ob->yspeed = -48;
      return;
    }
  }

  // jump at a treasure tile in the three rows above
  word width = static_cast<word>(ob->tileright - ob->tileleft + 1);
  for (word y = 0; y < 3; y++) {
    for (word x = 0; x < width; x++) {
      word tile = *MAPSPOT_PTR(mapsegs[1], ob->tileleft + x, ob->tiletop - 3 + y);
      if (IsTreasure(tinf[INTILE + tile])) {
        ob->xspeed = 0;
        ob->yspeed = -48;
        return;
      }
    }
  }

  if (ob->temp1 >= 2) {
    ob->state = ST(0x321E);
    return;
  }

  // hop forward if there is floor ahead
  for (word y = 0; y < 4; y++) {
    word tile = *MAPSPOT_PTR(mapsegs[1], ob->tilemidx + ob->xdir * 4, ob->tilebottom - 2 + y);
    if (tinf[NORTHWALL + tile]) {
      ob->xspeed = static_cast<sword>(ob->xdir * 20);
      ob->yspeed = -24;
      return;
    }
  }

  if (++ob->temp1 == 2) {
    SD_PlaySound(SND_EATERVANISH);
    ob->state = ST(0x321E);
    return;
  }
  ob->xdir = static_cast<sword>(-ob->xdir);
  ob->xspeed = static_cast<sword>(ob->xdir * 20);
  ob->yspeed = -24;
}

// 11A2:022A: reappear at the next bonus object, or leave the level.
void T_EaterTeleport(objtype* ob) {
  ob->temp1 = 0;
  for (objtype* o = player->next; o; o = o->next) {
    if (o->obclass == bonusobj) {
      ob->x = static_cast<word>(o->x - 8 * PIXGLOBAL);
      ob->y = o->y;
      NewState(ob, ST(0x3296));
      return;
    }
  }
  RemoveObj(ob);
}

// 11A2:0279
void C_Eater(objtype* ob, objtype* hit) {
  if (hit->obclass == bonusobj) {
    hit->obclass = inertobj;
    hit->priority = 3;
    ChangeState(hit, ST(0x33C2));
    SD_PlaySound(SND_TREASUREEATER);
  } else if (hit->obclass == stunshotobj) {
    ob->temp1 = ob->temp2 = ob->temp3 = 0;
    ob->temp4 = ob->obclass;
    ob->obclass = stunnedobj;
    ExplodeShot(hit);
    ChangeState(ob, ST(0x3386));
    ob->yspeed -= 16;
  }
}

// 11A2:02EA
void EatAtTiles(objtype* ob) {
  for (word y = ob->tiletop; y <= ob->tilebottom; y++) {
    for (word x = ob->tileleft; x <= ob->tileright; x++) {
      word tile = *MAPSPOT_PTR(mapsegs[1], x, y);
      if (IsTreasure(tinf[INTILE + tile] & 0x7F)) {
        RF_MemToMap(&blanktile, 1, x, y, 1, 1);
        GetNewObj(true);
        newobj->obclass = inertobj;
        newobj->priority = 3;
        newobj->needtoclip = cl_noclip;
        newobj->x = static_cast<word>(x << G_T_SHIFT);
        newobj->y = static_cast<word>(y << G_T_SHIFT);
        newobj->active = ac_removable;
        ChangeState(newobj, ST(0x33C2));
        break;
      }
    }
  }
}

// 11A2:03DE
void R_EaterAir(objtype* ob) {
  EatAtTiles(ob);
  if (ob->hitnorth) ChangeState(ob, ST(0x31E2));
  if (ob->hiteast || ob->hitwest) {
    ob->temp1++;
    ob->xdir = static_cast<sword>(-ob->xdir);
    ob->xspeed = 0;
  }
  if (ob->hitnorth) ob->yspeed = 0;
  PlaceSprite(ob);
}

sword Abs(sword v) { return v < 0 ? static_cast<sword>(-v) : v; }

// 11A2:0494: a mimrock sneaks up only while Keen walks away from it.
void T_MimrockWait(objtype* ob) {
  objtype* target = NearestPlayer(ob);
  if (Abs(static_cast<sword>(ob->bottom - target->bottom)) > 5 * TILEGLOBAL) return;
  if (Abs(static_cast<sword>(ob->x - target->x)) <= 3 * TILEGLOBAL) return;
  if (target->x < ob->x) {
    if (target->xdir == -1) {
      ob->xdir = -1;
      ob->state = ST(0x3458);
    }
  } else if (target->xdir == 1) {
    ob->xdir = 1;
    ob->state = ST(0x3458);
  }
}

// 11A2:04F8
void T_MimrockSneak(objtype* ob) {
  objtype* target = NearestPlayer(ob);
  if (Abs(static_cast<sword>(ob->bottom - target->bottom)) > 5 * TILEGLOBAL || ob->xdir != target->xdir) {
    ob->state = ST(0x343A);
  } else if (Abs(static_cast<sword>(ob->x - target->x)) < 4 * TILEGLOBAL) {
    ob->xspeed = static_cast<sword>(ob->xdir * 20);
    ob->yspeed = -40;
    ytry = static_cast<sword>(ob->yspeed * static_cast<sword>(tics));
    ob->state = ST(0x350C);
  }
}

// 11A2:0559
void C_Mimrock(objtype* ob, objtype* hit) {
  if (hit->obclass == stunshotobj) StunInline(ob, hit, 0x3584, 16);
}

// 11A2:059F
void C_MimrockJump(objtype* ob, objtype* hit) {
  if (hit->obclass == keenobj) {
    KillKeen(hit);
    return;
  }
  C_Mimrock(ob, hit);
}

// 11A2:05C1
void R_MimrockJump(objtype* ob) {
  if (ob->hitnorth) {
    SD_PlaySound(SND_MIMROCKBOUNCE);
    ob->yspeed = -20;
    ChangeState(ob, ST(0x3566));
  }
  if (ob->hiteast || ob->hitwest) ob->xspeed = 0;
  if (ob->hitnorth || ob->hitsouth) ob->yspeed = 0;
  PlaceSprite(ob);
}

// 11A2:062E
void R_MimrockBounce(objtype* ob) {
  if (ob->hitnorth) {
    SD_PlaySound(SND_MIMROCKBOUNCE);
    ChangeState(ob, ST(0x343A));
  }
  if (ob->hiteast || ob->hitwest) ob->xspeed = 0;
  if (ob->hitnorth) ob->yspeed = 0;
  PlaceSprite(ob);
}

// 11A2:0700: Keen has been swallowed.
void T_EatenKeen(objtype* ob) {
  if (numplayers > 1)
    CoopKeenGone(ob);
  else
    playstate = ex_died;
}

// 11A2:0711 and 0AF6: dopefish and schoolfish chase Keen.
void T_FishSwim(objtype* ob) {
  objtype* target = NearestPlayer(ob);
  if (!ob->temp1) ob->xdir = ob->x < target->x ? 1 : -1;
  AccelerateXv(ob, ob->xdir, 10);
  if (ob->y < target->y)
    AccelerateY(ob, 1, 10);
  else
    AccelerateY(ob, -1, 10);
}

void MoveToward(sword xdist, sword ydist) {
  if (xdist < 0) {
    xtry = static_cast<sword>(tics * -32);
    if (xtry < xdist) xtry = xdist;
  } else {
    xtry = static_cast<sword>(tics << 5);
    if (xtry > xdist) xtry = xdist;
  }
  if (ydist < 0) {
    ytry = static_cast<sword>(tics * -32);
    if (ytry < ydist) ytry = ydist;
  } else {
    ytry = static_cast<sword>(tics << 5);
    if (ytry > ydist) ytry = ydist;
  }
}

// 11A2:077A
void T_DopefishEat(objtype* ob) {
  objtype* target = ob->tempobj;
  sword ydist = static_cast<sword>(target->y - TILEGLOBAL - ob->y);
  sword xdist;
  if (ob->xdir == 1)
    xdist = static_cast<sword>(target->right + 2 * PIXGLOBAL - ob->right);
  else
    xdist = static_cast<sword>(target->left - 2 * PIXGLOBAL - ob->left);
  MoveToward(xdist, ydist);
  if (xtry == xdist && ytry == ydist) {
    if (PlayerIndex(target) >= 0)
      ChangeState(target, ST(0x36CE));
    else if (target->state->nextstate)
      ChangeState(target, target->state->nextstate);
    else
      RemoveObj(target);
    ob->state = ST(0x361A);
  }
}

// 11A2:086E
void T_DopefishReturn(objtype* ob) {
  sword ydist = static_cast<sword>(ob->temp3 - ob->y);
  sword xdist = static_cast<sword>(ob->temp2 - ob->x);
  MoveToward(xdist, ydist);
  if (xtry == xdist && ytry == ydist) {
    ob->state = ob->state->nextstate;
    ob->needtoclip = cl_fullclip;
  }
}

// 11A2:0912
void T_DopefishBurp(objtype* ob) {
  GetNewObj(true);
  newobj->x = static_cast<word>(ob->x + 56 * PIXGLOBAL);
  newobj->y = static_cast<word>(ob->y + 32 * PIXGLOBAL);
  newobj->obclass = inertobj;
  newobj->priority = 3;
  newobj->active = ac_removable;
  newobj->needtoclip = cl_noclip;
  newobj->yspeed = -20;
  newobj->xspeed = 4;
  NewState(newobj, ST(0x36EC));
  SD_PlaySound(SND_BURP);
}

// 11A2:0974
void T_Bubble(objtype* ob) {
  T_Velocity(ob);
  if (static_cast<word>(US_RndT()) < static_cast<word>(tics << 4)) ob->xspeed = static_cast<sword>(-ob->xspeed);
  if (ob->y < 3 * TILEGLOBAL) RemoveObj(ob);
}

// 11A2:09B0
void C_Dopefish(objtype* ob, objtype* hit) {
  if (hit->obclass == schoolfishobj) {
    ChangeState(hit, ST(0x3692));
  } else if (hit->obclass == keenobj && !godmode && !PlayerOf(hit).flicker) {
    hit->obclass = inertobj;
    hit->needtoclip = cl_noclip;
    SD_PlaySound(SND_KEENSWIMDIE);
    ChangeState(hit, ST(0x36B0));
  } else {
    return;
  }
  ob->temp2 = static_cast<sword>(ob->x);
  ob->temp3 = static_cast<sword>(ob->y);
  ob->tempobj = hit;
  if (hit->midx < ob->midx)
    ob->xdir = -1;
  else
    ob->xdir = 1;
  ChangeState(ob, ST(0x35FC));
  ob->needtoclip = cl_noclip;
}

// 11A2:0A3C
void R_Fish(objtype* ob) {
  if ((ob->hitsouth || ob->hitnorth) && !ob->temp1) ob->temp1++;
  if (ob->hiteast || ob->hitwest) {
    ob->xspeed = 0;
    ob->xdir = static_cast<sword>(-ob->xdir);
    ob->temp1 = 1;
  }
  if (!ob->hitsouth && !ob->hitnorth) ob->temp1 = 0;
  PlaceSprite(ob);
}

// 11A2:0BB1: hovers up and down, faces Keen when level with him.
void T_Pixie(objtype* ob) {
  AccelerateY(ob, ob->ydir, 8);
  if (static_cast<sword>(ob->temp1 - ob->y) > 2 * PIXGLOBAL) ob->ydir = 1;
  if (static_cast<sword>(ob->y - ob->temp1) > 2 * PIXGLOBAL) ob->ydir = -1;
  objtype* target = NearestPlayer(ob);
  if (target->top < ob->bottom && target->bottom > ob->top) {
    ob->xdir = target->x < ob->x ? -1 : 1;
    ob->state = ST(0x37BE);
  }
}

// 11A2:0C18
void T_PixieCheck(objtype* ob) {
  objtype* target = NearestPlayer(ob);
  if (target->top < ob->bottom && target->bottom > ob->top) ob->state = ST(0x37DC);
}

// 11A2:0C3B
void T_PixieShoot(objtype* ob) {
  GetNewObj(true);
  newobj->x = ob->x;
  newobj->y = static_cast<word>(ob->y + 8 * PIXGLOBAL);
  newobj->priority = 0;
  newobj->obclass = mshotobj;
  newobj->active = ac_removable;
  SD_PlaySound(SND_KEENFIRE);
  newobj->xdir = ob->xdir;
  NewState(newobj, ST(0x3818));
  SD_PlaySound(SND_SPRITESHOOT);
}

// 11A2:0CA1
void R_Mshot(objtype* ob) {
  if (ob->hitnorth || ob->hiteast || ob->hitsouth || ob->hitwest) {
    RemoveObj(ob);
    return;
  }
  PlaceSprite(ob);
}

// 11A2:0D86
void C_Mine(objtype* ob, objtype* hit) {
  if (hit->obclass == keenobj) {
    ChangeState(ob, ST(0x38AE));
    SD_PlaySound(SND_MINEEXPLODE);
    KillKeen(hit);
  }
}

// 11A2:0E01
void T_Lindsey(objtype* ob) {
  AccelerateY(ob, ob->ydir, 8);
  if (static_cast<sword>(ob->temp1 - ob->y) > 2 * PIXGLOBAL) ob->ydir = 1;
  if (static_cast<sword>(ob->y - ob->temp1) > 2 * PIXGLOBAL) ob->ydir = -1;
}

// 11A2:0EE6
void T_DartShoot(objtype* ob) {
  GetNewObj(true);
  newobj->x = ob->x;
  newobj->y = ob->y;
  newobj->obclass = mshotobj;
  newobj->active = ac_removable;
  switch (ob->temp1) {
    case 0:
      newobj->xdir = 0;
      newobj->ydir = -1;
      NewState(newobj, ST(0x39BC));
      break;
    case 1:
      newobj->xdir = 1;
      newobj->ydir = 0;
      NewState(newobj, ST(0x3980));
      break;
    case 2:
      newobj->xdir = 0;
      newobj->ydir = 1;
      NewState(newobj, ST(0x39F8));
      break;
    case 3:
      newobj->xdir = -1;
      newobj->ydir = 0;
      NewState(newobj, ST(0x3980));
      break;
  }
  SD_PlaySound(SND_DARTSHOOT);
}

// 11A2:1010
void C_Scuba(objtype*, objtype* hit) {
  if (hit->obclass == keenobj && hit->hitnorth) {
    gamestate.wetsuit = true;
    SD_PlaySound(SND_GETWETSUIT);
    GotScuba();
    RF_ForceRefresh();
    playstate = ex_completed;
  }
}

}  // namespace

// 11A2:000E
void SpawnEater(word tilex, word tiley) {
  GetNewObj(false);
  newobj->obclass = treasureeaterobj;
  newobj->active = ac_yes;
  newobj->priority = 3;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>((tiley << G_T_SHIFT) - 24 * PIXGLOBAL);
  newobj->xdir = US_RndT() < 0x80 ? 1 : -1;
  newobj->ydir = 1;
  NewState(newobj, ST(0x31E2));
}

// 11A2:0447
void SpawnMimrock(word tilex, word tiley) {
  GetNewObj(false);
  newobj->obclass = mimrockobj;
  newobj->active = ac_yes;
  newobj->priority = 3;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>((tiley << G_T_SHIFT) - 13 * PIXGLOBAL);
  newobj->xdir = newobj->ydir = 1;
  NewState(newobj, ST(0x343A));
}

// 11A2:0690
void SpawnDopefish(word tilex, word tiley) {
  GetNewObj(false);
  newobj->obclass = dopefishobj;
  newobj->active = ac_yes;
  newobj->priority = 2;
  newobj->needtoclip = cl_fullclip;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>((tiley << G_T_SHIFT) - 3 * TILEGLOBAL);
  newobj->xdir = US_RndT() < 0x80 ? 1 : -1;
  newobj->ydir = 1;
  NewState(newobj, ST(0x35C0));
}

// 11A2:0AA7
void SpawnSchoolfish(word tilex, word tiley) {
  GetNewObj(false);
  newobj->obclass = schoolfishobj;
  newobj->active = ac_yes;
  newobj->needtoclip = cl_fullclip;
  newobj->priority = 0;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>(tiley << G_T_SHIFT);
  newobj->xdir = newobj->ydir = 1;
  NewState(newobj, ST(0x3764));
}

// 11A2:0B5F
void SpawnPixie(word tilex, word tiley) {
  GetNewObj(false);
  newobj->obclass = pixieobj;
  newobj->active = ac_yes;
  newobj->priority = 0;
  newobj->needtoclip = cl_noclip;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>(tiley << G_T_SHIFT);
  newobj->temp1 = static_cast<sword>(newobj->y);
  newobj->xdir = newobj->ydir = 1;
  NewState(newobj, ST(0x37A0));
}

// 11A2:0CEC
void SpawnMine(word tilex, word tiley, sword dir) {
  GetNewObj(false);
  newobj->obclass = mineobj;
  newobj->active = ac_allways;
  newobj->priority = 0;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>(tiley << G_T_SHIFT);
  switch (dir) {
    case 0: newobj->xdir = 0; newobj->ydir = -1; break;
    case 1: newobj->xdir = 1; newobj->ydir = 0; break;
    case 2: newobj->xdir = 0; newobj->ydir = 1; break;
    case 3: newobj->xdir = -1; newobj->ydir = 0; break;
  }
  NewState(newobj, ST(0x3890));
}

// 11A2:0DB5
void SpawnLindsey(word tilex, word tiley) {
  GetNewObj(false);
  newobj->obclass = lindseyobj;
  newobj->active = ac_yes;
  newobj->priority = 0;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>((tiley << G_T_SHIFT) - TILEGLOBAL);
  newobj->temp1 = static_cast<sword>(newobj->y);
  newobj->ydir = 1;
  NewState(newobj, ST(0x38EA));
}

// 11A2:0E3B
void SpawnDartShooter(word tilex, word tiley, sword dir) {
  GetNewObj(false);
  newobj->obclass = inertobj;
  newobj->active = ac_yes;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->needtoclip = cl_noclip;
  newobj->y = static_cast<word>(tiley << G_T_SHIFT);
  newobj->temp1 = dir;
  switch (dir) {
    case 0:
      newobj->y -= 3 * PIXGLOBAL;
      newobj->x += 9 * PIXGLOBAL;
      newobj->shapenum = 0x17C;
      break;
    case 1:
      newobj->x += 8 * PIXGLOBAL;
      newobj->y += 5 * PIXGLOBAL;
      newobj->shapenum = 0x180;
      break;
    case 2:
      newobj->x += 9 * PIXGLOBAL;
      newobj->shapenum = 0x17E;
      break;
    case 3:
      newobj->y += 7 * PIXGLOBAL;
      newobj->x -= 3 * PIXGLOBAL;
      newobj->shapenum = 0x182;
      break;
  }
  NewState(newobj, ST(0x3962));
}

// 11A2:0FD1
void SpawnScuba(word tilex, word tiley) {
  GetNewObj(false);
  newobj->obclass = scubaobj;
  newobj->active = ac_yes;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>((tiley << G_T_SHIFT) - TILEGLOBAL);
  NewState(newobj, ST(0x3A34));
}

CK_FUNC(0x11A2, 0x0079, T_EaterJump);
CK_FUNC(0x11A2, 0x022A, T_EaterTeleport);
CK_FUNC(0x11A2, 0x0279, C_Eater);
CK_FUNC(0x11A2, 0x03DE, R_EaterAir);
CK_FUNC(0x11A2, 0x0494, T_MimrockWait);
CK_FUNC(0x11A2, 0x04F8, T_MimrockSneak);
CK_FUNC(0x11A2, 0x0559, C_Mimrock);
CK_FUNC(0x11A2, 0x059F, C_MimrockJump);
CK_FUNC(0x11A2, 0x05C1, R_MimrockJump);
CK_FUNC(0x11A2, 0x062E, R_MimrockBounce);
CK_FUNC(0x11A2, 0x0700, T_EatenKeen);
CK_FUNC(0x11A2, 0x0711, T_FishSwim);
CK_FUNC(0x11A2, 0x077A, T_DopefishEat);
CK_FUNC(0x11A2, 0x086E, T_DopefishReturn);
CK_FUNC(0x11A2, 0x0912, T_DopefishBurp);
CK_FUNC(0x11A2, 0x0974, T_Bubble);
CK_FUNC(0x11A2, 0x09B0, C_Dopefish);
CK_FUNC(0x11A2, 0x0A3C, R_Fish);
static FuncRegistrar reg_T_SchoolfishSwim(0x11A2, 0x0AF6, reinterpret_cast<void*>(T_FishSwim),
                                          "T_SchoolfishSwim");
CK_FUNC(0x11A2, 0x0BB1, T_Pixie);
CK_FUNC(0x11A2, 0x0C18, T_PixieCheck);
CK_FUNC(0x11A2, 0x0C3B, T_PixieShoot);
CK_FUNC(0x11A2, 0x0CA1, R_Mshot);
CK_FUNC(0x11A2, 0x0D86, C_Mine);
CK_FUNC(0x11A2, 0x0E01, T_Lindsey);
CK_FUNC(0x11A2, 0x0EE6, T_DartShoot);
CK_FUNC(0x11A2, 0x1010, C_Scuba);
