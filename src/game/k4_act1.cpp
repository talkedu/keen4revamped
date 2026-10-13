// K4_ACT1: Miragia, bonus items, council members, slugs, mushrooms, eggs and
// eggbirds, arachnuts and skypests (segment 0FA8).
#include "exe/keen4exe.h"
#include "game/ck_def.h"

extern sword xtry, ytry;
void ExplodeShot(objtype* shot);

namespace {

using keen4::exe::g_exe;

constexpr int SND_MINEEXPLODE = 5, SND_BOUNCE2 = 16, SND_SQUISH = 22, SND_BOUNCE1 = 28,
              SND_SLUGPOO = 46;

// 0FA8:002E: Miragia appears while Keen stands away from it.
void T_MiragiaCheck(objtype* ob) {
  for (int i = 0; i < numplayers; i++) {
    const objtype* p = players[i].ob;
    if (p->tileright >= ob->x && p->tileleft <= ob->x + 5 && p->tilebottom >= ob->y && p->tiletop <= ob->y + 4) {
      ob->state = ST(0x1ECE);
      return;
    }
  }
  RF_MapToMap(8, 60, ob->x, ob->y, 6, 4);
}

// 0FA8:008B..017B: copy one frame of the Miragia animation onto the map.
void T_Miragia1(objtype* ob) { RF_MapToMap(14, 60, ob->x, ob->y, 6, 4); }
void T_Miragia2(objtype* ob) { RF_MapToMap(20, 60, ob->x, ob->y, 6, 4); }
void T_Miragia3(objtype* ob) { RF_MapToMap(26, 60, ob->x, ob->y, 6, 4); }
void T_Miragia4(objtype* ob) { RF_MapToMap(20, 60, ob->x, ob->y, 6, 4); }
void T_Miragia5(objtype* ob) { RF_MapToMap(14, 60, ob->x, ob->y, 6, 4); }
void T_Miragia6(objtype* ob) { RF_MapToMap(8, 60, ob->x, ob->y, 6, 4); }
void T_Miragia7(objtype* ob) { RF_MapToMap(2, 60, ob->x, ob->y, 6, 4); }

// 0FA8:024E
void T_Bonus(objtype* ob) {
  if (++ob->shapenum == static_cast<word>(ob->temp3)) ob->shapenum = static_cast<word>(ob->temp2);
}

// 0FA8:02D4
void T_Council(objtype* ob) {
  word rnd = static_cast<word>(US_RndT());
  if (static_cast<word>(tics * 8) > rnd) ob->state = ST(0x1FF4);
}

// 0FA8:0369
void T_Slug(objtype* ob) {
  if (US_RndT() < 16) {
    if (ob->x < NearestPlayer(ob)->x)
      ob->xdir = 1;
    else
      ob->xdir = -1;
    ob->state = ST(0x204E);
    SD_PlaySound(SND_SLUGPOO);
  }
}

// 0FA8:03A6
void T_SlugSlime(objtype* ob) {
  GetNewObj(true);
  newobj->obclass = inertobj;
  newobj->active = ac_removable;
  newobj->priority = 0;
  newobj->x = ob->x;
  newobj->y = static_cast<word>(ob->bottom - 8 * PIXGLOBAL);
  NewState(newobj, ST(0x20A8));
}

// 0FA8:03EA
void C_Slug(objtype* ob, objtype* hit) {
  if (hit->obclass == keenobj) {
    KillKeen(hit);
  } else if (hit->obclass == stunshotobj) {
    if (US_RndT() < 0x80)
      StunObj(ob, hit, ST(0x206C));
    else
      StunObj(ob, hit, ST(0x208A));
    ob->yspeed = -24;
    ob->xspeed = static_cast<sword>(ob->xdir << 3);
  }
}

// 0FA8:0493
void T_Mushroom(objtype* ob) {
  if (NearestPlayer(ob)->x < ob->x)
    ob->xdir = -1;
  else
    ob->xdir = 1;
  DoWeakGravity(ob);
}

// 0FA8:04BE
void C_Mushroom(objtype*, objtype* hit) {
  if (hit->obclass == stunshotobj)
    ExplodeShot(hit);
  else if (hit->obclass == keenobj)
    KillKeen(hit);
}

// 0FA8:04EC
void R_Mushroom(objtype* ob) {
  if (ob->hitsouth) ob->yspeed = 0;
  if (ob->hitnorth) {
    ob->yspeed = 0;
    if (++ob->temp1 == 3) {
      ob->temp1 = 0;
      ob->yspeed = -68;
      SD_PlaySound(SND_BOUNCE2);
    } else {
      SD_PlaySound(SND_BOUNCE1);
      ob->yspeed = -40;
    }
  }
  R_Draw(ob);
}

// 0FA8:05AB
void T_EggHatch(objtype* ob) { ob->obclass = eggbirdobj; }

void SpawnBirdFrom(objtype* ob) {
  GetNewObj(true);
  newobj->obclass = eggbirdobj;
  newobj->active = ac_yes;
  newobj->x = ob->x;
  newobj->y = static_cast<word>(ob->y - 8 * PIXGLOBAL);
  if (newobj->x < NearestPlayer(ob)->x)
    newobj->xdir = 1;
  else
    newobj->xdir = -1;
  newobj->ydir = 1;
  NewState(newobj, ST(0x21B6));
}

void SpawnShell(objtype* ob, sword xspeed, sword yspeed, word state) {
  GetNewObj(true);
  newobj->obclass = inertobj;
  newobj->active = ac_removable;
  newobj->x = ob->x;
  newobj->y = ob->y;
  newobj->xspeed = xspeed;
  newobj->yspeed = yspeed;
  NewState(newobj, ST(state));
}

// 0FA8:0627
void C_Egg(objtype* ob, objtype* hit) {
  if (hit->obclass != stunshotobj && hit->obclass != keenobj) return;
  if (hit->obclass == stunshotobj) ExplodeShot(hit);
  ob->obclass = inertobj;
  ob->active = ac_removable;
  ChangeState(ob, ST(0x213E));
  SpawnBirdFrom(ob);
  SpawnShell(ob, -28, -40, 0x215C);
  SpawnShell(ob, 28, -40, 0x217A);
  SpawnShell(ob, 0, -56, 0x2198);
}

// 0FA8:0779
void T_EggbirdWalk(objtype* ob) {
  objtype* target = NearestPlayer(ob);
  if (ob->x < target->x)
    ob->xdir = 1;
  else
    ob->xdir = -1;
  if (ob->bottom >= static_cast<word>(target->bottom + 3 * TILEGLOBAL) && target->hitnorth &&
      StatePositionOk(ob, ST(0x224C))) {
    ob->state = ST(0x224C);
    ob->needtoclip = cl_fullclip;
    ob->yspeed = -8;
    ob->temp1 = 0;
  }
}

// 0FA8:07D7
void T_EggbirdFly(objtype* ob) {
  objtype* target = NearestPlayer(ob);
  if (!ob->temp1) {
    if (ob->x < target->x)
      ob->xdir = 1;
    else
      ob->xdir = -1;
  }
  AccelerateXv(ob, ob->xdir, 16);
  if (ob->y < target->y)
    AccelerateY(ob, 1, 16);
  else
    AccelerateY(ob, -1, 16);
}

// 0FA8:0840
void C_EggbirdWalk(objtype* ob, objtype* hit) {
  if (hit->obclass == keenobj) {
    KillKeen(hit);
  } else if (hit->obclass == stunshotobj) {
    ob->xspeed = 0;
    ob->needtoclip = cl_midclip;
    StunObj(ob, hit, ST(0x22E2));
  }
}

// 0FA8:087A
void C_EggbirdFly(objtype* ob, objtype* hit) {
  if (hit->obclass == stunshotobj) {
    ob->xspeed = 0;
    StunObj(ob, hit, ST(0x22E2));
  }
}

// 0FA8:08A1
void R_EggbirdWalk(objtype* ob) {
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
  }
  if (!ob->hitnorth) {
    ob->yspeed = -16;
    ob->needtoclip = cl_fullclip;
    ChangeState(ob, ST(0x224C));
  }
  R_Draw(ob);
}

// 0FA8:0945
void R_EggbirdLand(objtype* ob) {
  if (ob->hitnorth) ChangeState(ob, ST(0x21D4));
  R_Draw(ob);
}

// 0FA8:097F
void R_Chunk(objtype* ob) {
  if (ob->hitnorth) ob->xspeed = ob->yspeed = 0;
  R_Draw(ob);
}

// 0FA8:09B4
void R_EggbirdFly(objtype* ob) {
  if ((ob->hitsouth || ob->hitnorth) && !ob->temp1) ob->temp1++;
  if ((ob->hiteast && ob->xspeed < 0) || (ob->hitwest && ob->xspeed > 0)) {
    ob->xspeed = 0;
    ob->xdir = static_cast<sword>(-ob->xdir);
  }
  if (ob->hitnorth == 1 && static_cast<word>(NearestPlayer(ob)->bottom - ob->bottom) < 8 * PIXGLOBAL) {
    statetype* oldstate = ob->state;
    ob->needtoclip = cl_midclip;
    ChangeState(ob, ST(0x21D4));
    xtry = 0;
    ytry = 8 * PIXGLOBAL;
    PushObj(ob);
    if (!ob->hitnorth) {
      ob->needtoclip = cl_fullclip;
      ChangeState(ob, oldstate);
    }
    return;
  }
  if (!ob->hitsouth && !ob->hitnorth) ob->temp1 = 0;
  R_Draw(ob);
}

// 0FA8:0AF9
void T_Arachnut(objtype* ob) {
  if (ob->x > NearestPlayer(ob)->x)
    ob->xdir = -1;
  else
    ob->xdir = 1;
}

// 0FA8:0B1C
void C_Arachnut(objtype* ob, objtype* hit) {
  if (hit->obclass == stunshotobj)
    StunObj(ob, hit, ST(0x23F0));
  else if (hit->obclass == keenobj)
    KillKeen(hit);
}

// 0FA8:0B48
void C_ArachnutStunned(objtype* ob, objtype* hit) {
  if (hit->obclass == stunshotobj) StunObj(ob, hit, ST(0x23F0));
}

// 0FA8:0BE7
void T_SkypestFly(objtype* ob) {
  if (static_cast<word>(US_RndT()) < tics) ob->xdir = static_cast<sword>(-ob->xdir);
  if (ob->ydir == -1 && static_cast<word>(US_RndT()) < tics) ob->ydir = 1;
  if (ob->ydir == 1 && static_cast<word>(US_RndT()) < static_cast<word>(tics * 2))
    ob->ydir = static_cast<sword>(-ob->ydir);
  AccelerateX(ob, ob->xdir, 20);
  AccelerateY(ob, ob->ydir, 20);
}

// 0FA8:0C57
void C_Skypest(objtype* ob, objtype* hit) {
  if (hit->obclass == keenobj) KillKeen(hit);
  if (hit->obclass == stunshotobj) {
    if (hit->xdir == 1)
      ob->xspeed = 20;
    else if (hit->xdir == -1)
      ob->xspeed = -20;
    else if (hit->ydir == 1)
      ob->yspeed = 20;
    else if (hit->ydir == -1)
      ob->yspeed = -20;
    ExplodeShot(hit);
  }
}

// 0FA8:0CB0: only a pogoing Keen can squash a skypest.
void C_SquashablePest(objtype* ob, objtype* hit) {
  if (hit->state == ST(0x110C) || hit->state == ST(0x112A) || hit->state == ST(0x1148)) {
    ChangeState(ob, ST(0x24C2));
    SD_PlaySound(SND_SQUISH);
    ob->obclass = inertobj;
  }
}

// 0FA8:0CF1
void T_PestRest(objtype* ob) {
  ob->ydir = -1;
  ob->yspeed = -16;
  ytry = -144;
}

// 0FA8:0D0B
void R_Pest(objtype* ob) {
  if (ob->hitsouth) {
    ob->yspeed = 8;
    ob->ydir = 1;
  }
  if (ob->hitnorth && !ob->hiteast && !ob->hitwest) {
    ob->y = static_cast<word>(ob->y + 8 * PIXGLOBAL);
    ChangeState(ob, ST(0x24E0));
  }
  if (ob->hitwest) {
    ob->xspeed = 0;
    ob->xdir = -1;
  }
  if (ob->hiteast) {
    ob->xspeed = 0;
    ob->xdir = 1;
  }
  R_Draw(ob);
}

}  // namespace

// 0FA8:0000
void SpawnMiragia(word tilex, word tiley) {
  GetNewObj(false);
  newobj->obclass = inertobj;
  newobj->active = ac_allways;
  newobj->x = tilex;
  newobj->y = tiley;
  newobj->state = ST(0x1DFC);
}

// 0FA8:01A3
void SpawnBonus(word tilex, word tiley, word type) {
  GetNewObj(false);
  newobj->needtoclip = cl_noclip;
  newobj->priority = 2;
  newobj->obclass = bonusobj;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>(tiley << G_T_SHIFT);
  newobj->ydir = -1;
  newobj->temp1 = static_cast<sword>(type);
  newobj->shapenum = g_exe.Word(static_cast<std::uint16_t>(0x1FA0 + type * 2));
  newobj->temp2 = static_cast<sword>(newobj->shapenum);
  newobj->temp3 = static_cast<sword>(newobj->temp2 + 2);
  NewState(newobj, ST(0x1EEC));
}

// 0FA8:020C
void SpawnDropSplash(word tilex, word tiley) {
  GetNewObj(true);
  newobj->needtoclip = cl_noclip;
  newobj->priority = 3;
  newobj->obclass = inertobj;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>(tiley << G_T_SHIFT);
  NewState(newobj, ST(0x1F46));
}

// 0FA8:0269
void SpawnCouncil(word tilex, word tiley) {
  GetNewObj(false);
  newobj->obclass = oracleobj;
  newobj->active = ac_yes;
  newobj->priority = 0;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>((tiley << G_T_SHIFT) - 369);
  newobj->xdir = US_RndT() < 0x80 ? 1 : -1;
  newobj->ydir = 1;
  NewState(newobj, ST(0x1FB8));
}

// 0FA8:02FE
void SpawnSlug(word tilex, word tiley) {
  GetNewObj(false);
  newobj->obclass = slugobj;
  newobj->active = ac_yes;
  newobj->priority = 2;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>((tiley << G_T_SHIFT) - 113);
  newobj->xdir = US_RndT() < 0x80 ? 1 : -1;
  newobj->ydir = 1;
  NewState(newobj, ST(0x2012));
}

// 0FA8:0445
void SpawnMadMushroom(word tilex, word tiley) {
  GetNewObj(false);
  newobj->obclass = madmushroomobj;
  newobj->active = ac_yes;
  newobj->priority = 0;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>((tiley << G_T_SHIFT) - 241);
  newobj->xdir = 1;
  newobj->ydir = 1;
  NewState(newobj, ST(0x20E4));
}

// 0FA8:055D
void SpawnEgg(word tilex, word tiley) {
  GetNewObj(false);
  newobj->obclass = eggobj;
  newobj->active = ac_yes;
  newobj->priority = 2;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>((tiley << G_T_SHIFT) - 113);
  newobj->xdir = 1;
  newobj->ydir = 1;
  NewState(newobj, ST(0x2120));
}

// 0FA8:05B9
void SpawnEggbird(word tilex, word tiley) {
  GetNewObj(true);
  newobj->obclass = eggbirdobj;
  newobj->active = ac_yes;
  newobj->priority = 2;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>((tiley << G_T_SHIFT) - 241);
  if (newobj->x < NearestPlayer(newobj)->x)
    newobj->xdir = 1;
  else
    newobj->xdir = -1;
  newobj->ydir = 1;
  NewState(newobj, ST(0x21B6));
}

// 0FA8:0A8E
void SpawnArachnut(word tilex, word tiley) {
  GetNewObj(false);
  newobj->obclass = arachnutobj;
  newobj->active = ac_yes;
  newobj->priority = 0;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>((tiley << G_T_SHIFT) - 369);
  newobj->xdir = US_RndT() < 0x80 ? 1 : -1;
  newobj->ydir = 1;
  NewState(newobj, ST(0x2378));
}

// 0FA8:0B67
void SpawnSkypest(word tilex, word tiley) {
  GetNewObj(false);
  newobj->obclass = skypestobj;
  newobj->active = ac_yes;
  newobj->priority = 0;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>(tiley << G_T_SHIFT);
  newobj->xdir = US_RndT() < 0x80 ? 1 : -1;
  newobj->ydir = US_RndT() < 0x80 ? 1 : -1;
  NewState(newobj, ST(0x2486));
}

CK_FUNC(0x0FA8, 0x002E, T_MiragiaCheck);
CK_FUNC(0x0FA8, 0x008B, T_Miragia1);
CK_FUNC(0x0FA8, 0x00B3, T_Miragia2);
CK_FUNC(0x0FA8, 0x00DB, T_Miragia3);
CK_FUNC(0x0FA8, 0x0103, T_Miragia4);
CK_FUNC(0x0FA8, 0x012B, T_Miragia5);
CK_FUNC(0x0FA8, 0x0153, T_Miragia6);
CK_FUNC(0x0FA8, 0x017B, T_Miragia7);
CK_FUNC(0x0FA8, 0x024E, T_Bonus);
CK_FUNC(0x0FA8, 0x02D4, T_Council);
CK_FUNC(0x0FA8, 0x0369, T_Slug);
CK_FUNC(0x0FA8, 0x03A6, T_SlugSlime);
CK_FUNC(0x0FA8, 0x03EA, C_Slug);
CK_FUNC(0x0FA8, 0x0493, T_Mushroom);
CK_FUNC(0x0FA8, 0x04BE, C_Mushroom);
CK_FUNC(0x0FA8, 0x04EC, R_Mushroom);
CK_FUNC(0x0FA8, 0x05AB, T_EggHatch);
CK_FUNC(0x0FA8, 0x0627, C_Egg);
CK_FUNC(0x0FA8, 0x0779, T_EggbirdWalk);
CK_FUNC(0x0FA8, 0x07D7, T_EggbirdFly);
CK_FUNC(0x0FA8, 0x0840, C_EggbirdWalk);
CK_FUNC(0x0FA8, 0x087A, C_EggbirdFly);
CK_FUNC(0x0FA8, 0x08A1, R_EggbirdWalk);
CK_FUNC(0x0FA8, 0x0945, R_EggbirdLand);
CK_FUNC(0x0FA8, 0x097F, R_Chunk);
CK_FUNC(0x0FA8, 0x09B4, R_EggbirdFly);
CK_FUNC(0x0FA8, 0x0AF9, T_Arachnut);
CK_FUNC(0x0FA8, 0x0B1C, C_Arachnut);
CK_FUNC(0x0FA8, 0x0B48, C_ArachnutStunned);
CK_FUNC(0x0FA8, 0x0BE7, T_SkypestFly);
CK_FUNC(0x0FA8, 0x0C57, C_Skypest);
CK_FUNC(0x0FA8, 0x0CB0, C_SquashablePest);
CK_FUNC(0x0FA8, 0x0CF1, T_PestRest);
CK_FUNC(0x0FA8, 0x0D0B, R_Pest);
