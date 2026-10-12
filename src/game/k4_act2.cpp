// K4_ACT2: wormmouths, thunderclouds, berkeloids, inchworms and the foot,
// bounders, lick, moving and dropping platforms (segment 1080).
#include "game/ck_def.h"

extern sword xtry, ytry;

namespace {

constexpr int SND_WORMOUTHATTACK = 5, SND_BOUNCE2 = 16, SND_LICKATTACK = 32, SND_BERKELOIDATTACK = 33,
              SND_THUNDER = 39, SND_FIREBALLLAND = 40, SND_MAKEFOOT = 45;
constexpr word PLATFORMBLOCK = 0x1F;  // info-plane marker that turns platforms around

word InfoAt(word tilex, word tiley) { return *MAPSPOT_PTR(mapsegs[2], tilex, tiley); }

void FacePlayer(objtype* ob) {
  if (ob->x > player->x)
    ob->xdir = -1;
  else
    ob->xdir = 1;
}

// 1080:0071
void T_WormLookRight(objtype* ob) {
  if (player->x > ob->x) {
    ob->xdir = 1;
    ob->state = ST(0x26DE);
  }
}

// 1080:0091
void T_WormLook(objtype* ob) {
  if (player->x > ob->x)
    ob->xdir = 1;
  else
    ob->xdir = -1;
}

// 1080:00B4
void T_WormLookLeft(objtype* ob) {
  if (player->x < ob->x) {
    ob->xdir = -1;
    ob->state = ST(0x26DE);
  }
}

// 1080:00D4
void T_Worm(objtype* ob) {
  sword xdist = static_cast<sword>(player->x - ob->x);
  sword ydist = static_cast<sword>(player->bottom - ob->bottom);
  if ((xdist < -3 * TILEGLOBAL || xdist > 3 * TILEGLOBAL) && US_RndT() < 6) {
    ob->state = ST(0x26FC);
    return;
  }
  if (ydist >= -TILEGLOBAL && ydist <= TILEGLOBAL) {
    if ((ob->xdir == 1 && xdist > 0x80 && xdist < 0x180) ||
        (ob->xdir == -1 && xdist < -0x80 && xdist > -0x200)) {
      SD_PlaySound(SND_WORMOUTHATTACK);
      ob->state = ST(0x27EC);
    }
  }
}

// 1080:015D
void C_Worm(objtype* ob, objtype* hit) {
  if (hit->obclass == stunshotobj) StunObj(ob, hit, ST(0x2882));
}

// 1080:017C: the bite only hurts in front of the mouth.
void C_WormKill(objtype* ob, objtype* hit) {
  if (hit->obclass == keenobj) {
    if (ob->xdir == 1 && ob->x > hit->x) return;
    if (ob->xdir == -1 && ob->x < hit->x) return;
    KillKeen();
    return;
  }
  C_Worm(ob, hit);
}

// 1080:020C
void T_Cloud(objtype* ob) {
  if (static_cast<word>(US_RndT()) < tics) ob->xdir = ob->x > player->x ? -1 : 1;
  AccelerateX(ob, ob->xdir, 10);
  if (player->bottom >= ob->bottom && static_cast<sword>(player->top - ob->bottom) <= 4 * TILEGLOBAL &&
      ob->left < player->right && ob->right > player->left) {
    ob->state = ST(0x28FA);
  }
}

// 1080:0278: drift until lined up with a tile, then strike.
void T_CloudAlign(objtype* ob) {
  AccelerateX(ob, ob->xdir, 10);
  if (xtry < 0 && static_cast<sword>((ob->x & 0x7F) + xtry) <= 0) {
    xtry = static_cast<sword>(-(ob->x & 0x7F));
    ob->state = ST(0x2936);
  }
  if (xtry > 0 && static_cast<word>((ob->x & 0x7F) + xtry) >= 0x80) {
    xtry = static_cast<sword>(0x80 - (ob->x & 0x7F));
    ob->state = ST(0x2936);
  }
}

// 1080:02E1
void R_Cloud(objtype* ob) {
  if (ob->hitwest) {
    ob->xspeed = 0;
    ob->xdir = -1;
  } else if (ob->hiteast) {
    ob->xspeed = 0;
    ob->xdir = 1;
  }
  R_Draw(ob);
}

// 1080:032A
void T_CloudShoot(objtype* ob) {
  GetNewObj(true);
  newobj->obclass = lightningobj;
  newobj->active = ac_removable;
  newobj->needtoclip = cl_noclip;
  newobj->x = static_cast<word>(ob->x + TILEGLOBAL);
  newobj->y = static_cast<word>(ob->y + TILEGLOBAL);
  NewState(newobj, ST(0x2A44));
  SD_PlaySound(SND_THUNDER);
}

// 1080:037D
void C_CloudSleep(objtype* ob, objtype* hit) {
  if (hit->obclass == keenobj) ChangeState(ob, ST(0x28BE));
}

// 1080:040B
void T_Berkeloid(objtype* ob) {
  if (US_RndT() < 0x20) ob->xdir = player->x < ob->x ? -1 : 1;
  if (US_RndT() < 8) {
    ob->state = ST(0x2B70);
    return;
  }
  if (US_RndT() <= 0x40) {
    sword xdist = static_cast<sword>(player->x - ob->x);
    sword ydist = static_cast<sword>(player->y - ob->y);
    if (ydist >= -TILEGLOBAL && ydist <= TILEGLOBAL &&
        ((ob->xdir == 1 && xdist > 0) || (ob->xdir == -1 && xdist < 0))) {
      ob->state = ST(0x2B70);
      SD_PlaySound(SND_BERKELOIDATTACK);
    }
  }
}

// 1080:04A4
void T_BerkThrow(objtype* ob) {
  GetNewObj(true);
  newobj->active = ac_removable;
  newobj->obclass = berkeloidobj;
  newobj->y = static_cast<word>(ob->y + 8 * PIXGLOBAL);
  newobj->yspeed = -8;
  if (ob->xdir == 1) {
    newobj->xspeed = 48;
    newobj->x = static_cast<word>(ob->x + 2 * TILEGLOBAL);
    newobj->xdir = 1;
  } else {
    newobj->xspeed = -48;
    newobj->x = static_cast<word>(ob->x - TILEGLOBAL);
    newobj->xdir = -1;
  }
  NewState(newobj, ST(0x2CD8));
  ob->needtoreact = true;
}

// 1080:051C
void T_BerkThrowDone(objtype* ob) {
  ob->nothink = 4;
  ob->needtoreact = true;
}

// 1080:0530
void C_Berkeloid(objtype*, objtype* hit) {
  if (hit->obclass == stunshotobj)
    ExplodeShot(hit);
  else if (hit->obclass == keenobj)
    KillKeen();
}

// 1080:055E
void R_Fire(objtype* ob) {
  if (ob->hiteast || ob->hitwest) ob->xspeed = 0;
  if (ob->hitnorth) {
    SD_PlaySound(SND_FIREBALLLAND);
    ChangeState(ob, ST(0x2D14));
  }
  R_Draw(ob);
}

// 1080:05B5: berkeloids float up and down while drawn.
void R_Berkeloid(objtype* ob) {
  ob->temp1 = static_cast<sword>(ob->temp1 + ob->temp2 * static_cast<sword>(tics));
  if (ob->temp1 > 0) {
    ob->temp1 = 0;
    ob->temp2 = -8;
  } else if (ob->temp1 < -TILEGLOBAL) {
    ob->temp1 = -TILEGLOBAL;
    ob->temp2 = 8;
  }
  RF_PlaceSprite(&ob->sprite, ob->x, static_cast<word>(ob->y + ob->temp1), ob->shapenum, spritedraw, 0);
}

// 1080:0610
void R_BerkeloidWalk(objtype* ob) {
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
    ob->x = static_cast<word>(ob->x - ob->xmove * 2);
    ob->xdir = static_cast<sword>(-ob->xdir);
    ob->nothink = static_cast<word>(US_RndT() >> 5);
    ChangeState(ob, ob->state);
  }
  R_Berkeloid(ob);
}

// 1080:0770
void T_Inchworm(objtype* ob) { FacePlayer(ob); }

void SpawnSmoke(word x, word y) {
  GetNewObj(true);
  newobj->x = x;
  newobj->y = y;
  newobj->priority = 3;
  NewState(newobj, ST(0x2E22));
}

// 1080:0793: eleven inchworms touching in one frame become the foot.
void C_Inchworm(objtype* ob, objtype* hit) {
  if (hit->obclass != inchwormobj) return;
  if (ob->temp1 != static_cast<sword>(lasttimecount)) {
    ob->temp1 = static_cast<sword>(lasttimecount);
    ob->temp2 = 0;
  }
  if (++ob->temp2 != 11) return;

  SD_PlaySound(SND_MAKEFOOT);
  ob->y = static_cast<word>(ob->y - 5 * TILEGLOBAL);
  ob->obclass = footobj;
  ChangeState(ob, ST(0x2EF4));
  SpawnSmoke(static_cast<word>(ob->x - 8 * PIXGLOBAL), static_cast<word>(ob->y + 16 * PIXGLOBAL));
  SpawnSmoke(static_cast<word>(ob->x + 16 * PIXGLOBAL), static_cast<word>(ob->y + 24 * PIXGLOBAL));
  SpawnSmoke(static_cast<word>(ob->x + 40 * PIXGLOBAL), static_cast<word>(ob->y + 16 * PIXGLOBAL));
  SpawnSmoke(ob->x, static_cast<word>(ob->y - 8 * PIXGLOBAL));
  for (objtype* o = player->next; o; o = o->next) {
    if (o->obclass == inchwormobj) RemoveObj(o);
  }
}

// 1080:08D9: the foot's contact does nothing; Keen's own contact handles it.
void C_Foot(objtype*, objtype*) {}

// 1080:0938
void C_Bounder(objtype* ob, objtype* hit) {
  if (hit->obclass == stunshotobj) {
    ob->temp1 = ob->temp2 = ob->temp3 = 0;
    ob->temp4 = ob->obclass;
    ExplodeShot(hit);
    ChangeState(ob, ST(0x2F8A));
    ob->obclass = stunnedobj;
    ob->yspeed -= 32;
  }
}

// 1080:097E
void R_Bounder(objtype* ob) {
  if (ob->hitsouth) ob->yspeed = 0;
  if (ob->hitnorth) {
    ob->temp2++;
    if (OnScreen(ob)) SD_PlaySound(SND_BOUNCE2);
    ob->yspeed = -50;
    if (gamestate.riding == ob) {
      ob->temp2 = 0;
      if (player->left < static_cast<word>(ob->left - 4 * PIXGLOBAL))
        ob->xdir = -1;
      else if (player->right > static_cast<word>(ob->right + 4 * PIXGLOBAL))
        ob->xdir = 1;
      else
        ob->xdir = 0;
      ob->xspeed = static_cast<sword>(ob->xdir * 24);
    } else if (ob->temp2 <= 2 || ob->temp1 == 0) {
      ob->temp1 = 1;
      ob->xspeed = ob->xdir = 0;
      ChangeState(ob, ST(0x2F12));
    } else {
      ob->temp1 = 0;
      sword r = static_cast<sword>(US_RndT());
      if (r < 100)
        ob->xdir = -1;
      else if (r < 200)
        ob->xdir = 1;
      else
        ob->xdir = 0;
      ob->xspeed = static_cast<sword>(ob->xdir * 24);
    }
    if (ob->xdir)
      ChangeState(ob, ST(0x2F4E));
    else
      ChangeState(ob, ST(0x2F12));
  }
  if (ob->hiteast || ob->hitwest) {
    ob->xdir = static_cast<sword>(-ob->xdir);
    ob->xspeed = static_cast<sword>(-ob->xspeed);
  }
  R_Draw(ob);
}

// 1080:0B42
void T_Lick(objtype* ob) {
  FacePlayer(ob);
  sword xdist = static_cast<sword>(player->x - ob->x);
  sword ydist = static_cast<sword>(player->y - ob->y);
  if (ydist >= -TILEGLOBAL && ydist <= TILEGLOBAL &&
      ((ob->xdir == 1 && xdist > -2 * PIXGLOBAL && xdist < 0x180) ||
       (ob->xdir == -1 && xdist < 2 * PIXGLOBAL && xdist > -0x200))) {
    SD_PlaySound(SND_LICKATTACK);
    ob->state = ST(0x303E);
    return;
  }
  sword adist = xdist < 0 ? static_cast<sword>(-xdist) : xdist;
  if (adist > 3 * TILEGLOBAL) {
    ob->xspeed = static_cast<sword>(ob->xdir << 5);
    ob->yspeed = -32;
  } else {
    ob->xspeed = static_cast<sword>((ob->xdir << 5) / 2);
    ob->yspeed = -16;
  }
}

// 1080:0BFD
void C_Lick(objtype* ob, objtype* hit) {
  if (hit->obclass == stunshotobj) {
    StunObj(ob, hit, ST(0x312E));
    ob->yspeed -= 16;
  }
}

// 1080:0C23: the flame only hurts in front of the lick.
void C_LickKill(objtype* ob, objtype* hit) {
  if (hit->obclass == keenobj) {
    if ((ob->xdir == 1 && player->x > ob->x) || (ob->xdir == -1 && player->x < ob->x)) KillKeen();
    return;
  }
  C_Lick(ob, hit);
}

// 1080:0C71
void R_Lick(objtype* ob) {
  if (ob->hitnorth) ChangeState(ob, ST(0x3020));
  R_Draw(ob);
}

// 1080:0F56: platforms draw their jet exhaust as extra sprites.
void R_Platform(objtype* ob) {
  RF_PlaceSprite(&ob->sprite, ob->x, ob->y, ob->shapenum, spritedraw, ob->priority);
  word frame = static_cast<word>((lasttimecount >> 2) & 1);
  if (ob->xdir == 1) {
    RF_PlaceSprite(&ob->temp2sprite, static_cast<word>(ob->x - 1 * PIXGLOBAL),
                   static_cast<word>(ob->y + 3 * PIXGLOBAL), 0x1E5 + frame, spritedraw, 0);
    if (ob->temp3sprite) RF_RemoveSprite(&ob->temp3sprite);
  } else if (ob->xdir == -1) {
    if (ob->temp2sprite) RF_RemoveSprite(&ob->temp2sprite);
    RF_PlaceSprite(&ob->temp3sprite, static_cast<word>(ob->x + 48 * PIXGLOBAL),
                   static_cast<word>(ob->y + 5 * PIXGLOBAL), 0x1E5 + frame, spritedraw, 1);
  } else if (ob->ydir == -1 || (ob->ydir == 1 && frame)) {
    RF_PlaceSprite(&ob->temp2sprite, static_cast<word>(ob->x + 2 * PIXGLOBAL),
                   static_cast<word>(ob->y + 9 * PIXGLOBAL), 0x1E9 + frame, spritedraw, 0);
    RF_PlaceSprite(&ob->temp3sprite, static_cast<word>(ob->x + 46 * PIXGLOBAL),
                   static_cast<word>(ob->y + 8 * PIXGLOBAL), 0x1E7 + frame, spritedraw, 0);
  } else if (ob->ydir == 1) {
    if (ob->temp2sprite) RF_RemoveSprite(&ob->temp2sprite);
    if (ob->temp3sprite) RF_RemoveSprite(&ob->temp3sprite);
  }
}

// 1080:114F
void T_DropPlatSit(objtype* ob) {
  if (gamestate.riding == ob) {
    ytry = static_cast<sword>(tics << 4);
    ob->yspeed = 0;
    if (static_cast<word>(ob->y + ytry - ob->temp1) >= 8 * PIXGLOBAL) ob->state = ST(0x31A6);
  }
}

// 1080:1182
void T_DropPlatFall(objtype* ob) {
  DoGravity(ob);
  if (ytry >= 15 * PIXGLOBAL) ytry = 15 * PIXGLOBAL;
  word newpos = static_cast<word>(ob->bottom + ytry);
  word newtile = static_cast<word>(newpos >> G_T_SHIFT);
  if (ob->tilebottom != newtile && InfoAt(ob->tileleft, newtile) == PLATFORMBLOCK) {
    ytry = static_cast<sword>(0xFF - (ob->bottom & 0xFF));
    if (gamestate.riding != ob) ob->state = ST(0x31C4);
  }
}

// 1080:11FB
void T_DropPlatRise(objtype* ob) {
  if (gamestate.riding == ob) {
    ob->yspeed = 0;
    ob->state = ST(0x31A6);
  } else if (ob->y <= static_cast<word>(ob->temp1)) {
    ytry = static_cast<sword>(ob->temp1 - ob->y);
    ob->state = ST(0x3188);
  }
}

}  // namespace

// 1080:0D44: also moves the K4 mines.
void T_Platform(objtype* ob) {
  xtry = static_cast<sword>(ob->xdir * 12 * static_cast<sword>(tics));
  ytry = static_cast<sword>(ob->ydir * 12 * static_cast<sword>(tics));

  if (ob->xdir == 1) {
    word newpos = static_cast<word>(ob->right + xtry);
    word newtile = static_cast<word>(newpos >> G_T_SHIFT);
    if (ob->tileright != newtile && InfoAt(newtile, ob->tiletop) == PLATFORMBLOCK) {
      ob->xdir = -1;
      xtry = static_cast<sword>(xtry - (newpos & 0xFF));
    }
  } else if (ob->xdir == -1) {
    word newpos = static_cast<word>(ob->left + xtry);
    word newtile = static_cast<word>(newpos >> G_T_SHIFT);
    if (ob->tileleft != newtile && InfoAt(newtile, ob->tiletop) == PLATFORMBLOCK) {
      ob->xdir = 1;
      xtry = static_cast<sword>(xtry + (0x100 - (newpos & 0xFF)));
    }
  } else if (ob->ydir == 1) {
    word newpos = static_cast<word>(ob->bottom + ytry);
    word newtile = static_cast<word>(newpos >> G_T_SHIFT);
    if (ob->tilebottom != newtile && InfoAt(ob->tileleft, newtile) == PLATFORMBLOCK) {
      if (InfoAt(ob->tileleft, static_cast<word>(newtile - 2)) == PLATFORMBLOCK) {
        ytry = 0;
        ob->needtoreact = true;
      } else {
        ob->ydir = -1;
        ytry = static_cast<sword>(ytry - (newpos & 0xFF));
      }
    }
  } else if (ob->ydir == -1) {
    word newpos = static_cast<word>(ob->top + ytry);
    word newtile = static_cast<word>(newpos >> G_T_SHIFT);
    if (ob->tiletop != newtile && InfoAt(ob->tileleft, newtile) == PLATFORMBLOCK) {
      if (InfoAt(ob->tileleft, static_cast<word>(newtile + 2)) == PLATFORMBLOCK) {
        ytry = 0;
        ob->needtoreact = true;
      } else {
        ob->ydir = 1;
        ytry = static_cast<sword>(ytry + (0x100 - (newpos & 0xFF)));
      }
    }
  }
}

// 1080:0006
void SpawnWormMouth(word tilex, word tiley) {
  GetNewObj(false);
  newobj->obclass = wormmouthobj;
  newobj->active = ac_yes;
  newobj->priority = 0;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>((tiley << G_T_SHIFT) + 0x8F);
  newobj->xdir = US_RndT() < 0x80 ? 1 : -1;
  newobj->ydir = 1;
  NewState(newobj, ST(0x26DE));
}

// 1080:01C2
void SpawnCloud(word tilex, word tiley) {
  GetNewObj(false);
  newobj->obclass = thundercloudobj;
  newobj->active = ac_yes;
  newobj->priority = 2;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>(tiley << G_T_SHIFT);
  newobj->xdir = newobj->ydir = 1;
  NewState(newobj, ST(0x28A0));
}

// 1080:039B
void SpawnBerkeloid(word tilex, word tiley) {
  GetNewObj(false);
  newobj->obclass = berkeloidobj;
  newobj->active = ac_yes;
  newobj->priority = 2;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>((tiley << G_T_SHIFT) - 2 * TILEGLOBAL);
  newobj->xdir = US_RndT() < 0x80 ? 1 : -1;
  newobj->ydir = 1;
  newobj->temp2 = 8;
  NewState(newobj, ST(0x2AF8));
}

// 1080:06B2
void SpawnInchworm(word tilex, word tiley) {
  GetNewObj(false);
  newobj->obclass = inchwormobj;
  newobj->active = ac_yes;
  newobj->priority = 2;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>(tiley << G_T_SHIFT);
  newobj->xdir = US_RndT() < 0x80 ? 1 : -1;
  newobj->ydir = 1;
  NewState(newobj, ST(0x2E9A));
  newobj->ticcount = static_cast<sword>(US_RndT() / 32);
}

// 1080:072C
void SpawnFoot(word tilex, word tiley) {
  GetNewObj(false);
  newobj->obclass = footobj;
  newobj->active = ac_yes;
  newobj->priority = 0;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>((tiley - 3) << G_T_SHIFT);
  NewState(newobj, ST(0x2EF4));
}

// 1080:08EA
void SpawnBounder(word tilex, word tiley) {
  GetNewObj(false);
  newobj->obclass = bounderobj;
  newobj->active = ac_yes;
  newobj->priority = 0;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>((tiley << G_T_SHIFT) - 8 * PIXGLOBAL);
  newobj->ydir = 1;
  newobj->xdir = 0;
  NewState(newobj, ST(0x2F12));
}

// 1080:0AC8
void SpawnLick(word tilex, word tiley) {
  GetNewObj(false);
  newobj->obclass = lickobj;
  newobj->active = ac_yes;
  newobj->priority = 2;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>(tiley << G_T_SHIFT);
  newobj->xdir = US_RndT() < 0x80 ? 1 : -1;
  newobj->ydir = 1;
  newobj->nothink = static_cast<word>(US_RndT() / 64);
  NewState(newobj, ST(0x3002));
}

// 1080:0CAB
void SpawnPlatform(word tilex, word tiley, sword dir) {
  GetNewObj(false);
  newobj->obclass = platformobj;
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
  NewState(newobj, ST(0x316A));
}

// 1080:10FC
void SpawnDropPlatform(word tilex, word tiley) {
  GetNewObj(false);
  newobj->obclass = platformobj;
  newobj->active = ac_allways;
  newobj->priority = 0;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>(tiley << G_T_SHIFT);
  newobj->temp1 = static_cast<sword>(newobj->y);
  newobj->xdir = 0;
  newobj->ydir = 1;
  newobj->needtoclip = cl_noclip;
  NewState(newobj, ST(0x3188));
}

CK_FUNC(0x1080, 0x0071, T_WormLookRight);
CK_FUNC(0x1080, 0x0091, T_WormLook);
CK_FUNC(0x1080, 0x00B4, T_WormLookLeft);
CK_FUNC(0x1080, 0x00D4, T_Worm);
CK_FUNC(0x1080, 0x015D, C_Worm);
CK_FUNC(0x1080, 0x017C, C_WormKill);
CK_FUNC(0x1080, 0x020C, T_Cloud);
CK_FUNC(0x1080, 0x0278, T_CloudAlign);
CK_FUNC(0x1080, 0x02E1, R_Cloud);
CK_FUNC(0x1080, 0x032A, T_CloudShoot);
CK_FUNC(0x1080, 0x037D, C_CloudSleep);
CK_FUNC(0x1080, 0x040B, T_Berkeloid);
CK_FUNC(0x1080, 0x04A4, T_BerkThrow);
CK_FUNC(0x1080, 0x051C, T_BerkThrowDone);
CK_FUNC(0x1080, 0x0530, C_Berkeloid);
CK_FUNC(0x1080, 0x055E, R_Fire);
CK_FUNC(0x1080, 0x05B5, R_Berkeloid);
CK_FUNC(0x1080, 0x0610, R_BerkeloidWalk);
CK_FUNC(0x1080, 0x0770, T_Inchworm);
CK_FUNC(0x1080, 0x0793, C_Inchworm);
CK_FUNC(0x1080, 0x08D9, C_Foot);
CK_FUNC(0x1080, 0x0938, C_Bounder);
CK_FUNC(0x1080, 0x097E, R_Bounder);
CK_FUNC(0x1080, 0x0B42, T_Lick);
CK_FUNC(0x1080, 0x0BFD, C_Lick);
CK_FUNC(0x1080, 0x0C23, C_LickKill);
CK_FUNC(0x1080, 0x0C71, R_Lick);
CK_FUNC(0x1080, 0x0D44, T_Platform);
CK_FUNC(0x1080, 0x0F56, R_Platform);
CK_FUNC(0x1080, 0x114F, T_DropPlatSit);
CK_FUNC(0x1080, 0x1182, T_DropPlatFall);
CK_FUNC(0x1080, 0x11FB, T_DropPlatRise);
