// CK_KEEN: Keen himself on the levels (segment 0B80).
#include "exe/keen4exe.h"
#include "game/ck_def.h"

extern sword xtry, ytry;
void SpawnShot(objtype* shooter, word x, word y, sword dir);  // CK_KEEN2
void ExplodeShot(objtype* shot);
void SpawnDropSplash(word tilex, word tiley);  // K4_ACT1
void OracleCouncilMember();  // K4_SPEC: the fake member in level 14
void LindseyDialog();        // K4_SPEC

namespace {

using keen4::exe::g_exe;

// Keen's states (DGROUP addresses).
constexpr word s_keenstand = 0x098C;
constexpr word s_keenpauselook = 0x09AA;
constexpr word s_keenwait1 = 0x09C8;
constexpr word s_keenmoon1 = 0x0A7C;
constexpr word s_keenread1 = 0x0AD6;
constexpr word s_keenstopread1 = 0x0BA8;
constexpr word s_keenlookup = 0x0C02;
constexpr word s_keenlookdown = 0x0C3E;
constexpr word s_keenlookdown4 = 0x0C98;
constexpr word s_keendead = 0x0CD4;
constexpr word s_keendie1 = 0x0CF2;
constexpr word s_keendie2 = 0x0D10;
constexpr word s_keensuitdie1 = 0x0D2E;
constexpr word s_keensuitdie2 = 0x0D4C;
constexpr word s_keenshoot1 = 0x0D6A;
constexpr word s_keenshootup1 = 0x0DA6;
constexpr word s_keenswitch = 0x0DE2;
constexpr word s_keenkey = 0x0E1E;
constexpr word s_keenlineup = 0x0E3C;
constexpr word s_keenenter1 = 0x0E5A;
constexpr word s_keenpole = 0x0EF0;
constexpr word s_keenclimb1 = 0x0F0E;
constexpr word s_keenslide1 = 0x0F68;
constexpr word s_keenpoleshoot1 = 0x0FE0;
constexpr word s_keenpoleshootup1 = 0x101C;
constexpr word s_keenpoleshootdown1 = 0x1058;
constexpr word s_keenwalk1 = 0x1094;
constexpr word s_keenwalk2 = 0x10B2;
constexpr word s_keenwalk3 = 0x10D0;
constexpr word s_keenwalk4 = 0x10EE;
constexpr word s_keenpogodown = 0x110C;
constexpr word s_keenpogo = 0x112A;
constexpr word s_keenjump1 = 0x1166;
constexpr word s_keenjump3 = 0x11A2;
constexpr word s_keenjump4 = 0x11C0;
constexpr word s_keenairshoot1 = 0x11DE;
constexpr word s_keenairshoot2 = 0x11FC;
constexpr word s_keenairshootup1 = 0x1238;
constexpr word s_keenairshootup2 = 0x1256;
constexpr word s_keenairshootdown1 = 0x1292;
constexpr word s_keenairshootdown2 = 0x12B0;
constexpr word s_keenholdon = 0x12EC;
constexpr word s_keenclimbup1 = 0x1328;
constexpr word s_door1 = 0x17EC;
constexpr word s_bonusrise = 0x1F28;

// Sounds.
constexpr int SND_WORLDWALK1 = 0, SND_WORLDWALK2 = 1, SND_JUMP = 2, SND_LAND = 3, SND_POGOBOUNCE = 7,
              SND_GETWATER = 10, SND_HELMETHIT = 15, SND_EXTRAKEEN = 17, SND_OPENDOOR = 18,
              SND_PLUMMET = 20, SND_USESWITCH = 21, SND_KEENDEAD = 23, SND_FOUNDMEMBER = 31;

// The CK_KEEN globals (DGROUP CAB5..CABB: moonok, leavepoletime, jumptime)
// are kept per player in PlayerState.

sword walkspeed(sword slope) { return g_exe.Int(static_cast<std::uint16_t>(0x13BE + (slope & 7) * 2)); }
sword polexspeed(sword xaxis) { return g_exe.Int(static_cast<std::uint16_t>(0x13D0 + xaxis * 2)); }
sword ammopickup(sword difficulty) { return g_exe.Int(static_cast<std::uint16_t>(0x13D4 + difficulty * 2)); }
sword bonussound(int type) { return g_exe.Int(static_cast<std::uint16_t>(0x13DC + type * 2)); }
word bonuspoints(int type) { return g_exe.Word(static_cast<std::uint16_t>(0x13F4 + type * 2)); }
word bonusshape(int type) { return g_exe.Word(static_cast<std::uint16_t>(0x140C + type * 2)); }

const word zeromap = 0;  // DGROUP 1424

inline word* FGSpot(word x, word y) { return mapsegs[1] + mapbwidthtable[y] / 2 + x; }
inline word* InfoSpot(word x, word y) { return mapsegs[2] + mapbwidthtable[y] / 2 + x; }

void KeenWalkThink(objtype* ob);
void KeenStandActions(objtype* ob);

// 0B80:0052
boolean CheckGrabPole(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  if (static_cast<std::int32_t>(lasttimecount) < ps.leavepoletime)
    ps.leavepoletime = 0;
  else if (static_cast<std::int32_t>(lasttimecount) - ps.leavepoletime < 19)
    return false;

  word* map;
  if (ps.c.yaxis == -1)
    map = FGSpot(0, static_cast<word>((ob->top + 6 * PIXGLOBAL) >> G_T_SHIFT));
  else
    map = FGSpot(0, static_cast<word>(ob->tilebottom + 1));
  map += ob->tilemidx;

  if ((TINF_IN(*map) & 0x7F) == 1) {
    ob->x = static_cast<word>(((ob->tilemidx - 1) << G_T_SHIFT) + 8 * PIXGLOBAL);
    xtry = 0;
    ytry = static_cast<sword>(ps.c.yaxis << 5);
    ob->needtoclip = cl_noclip;
    ob->state = ST(s_keenpole);
    return true;
  }
  return false;
}

// 0B80:0137: switches and doors are used by pressing up.
boolean CheckEnterHouse(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  word intile = TINF_IN(*FGSpot(ob->tilemidx, ob->tiletop));
  if (intile == 5 || intile == 6 || intile == 15) {
    word x = static_cast<word>((ob->tilemidx << G_T_SHIFT) - 4 * PIXGLOBAL);
    if (ob->x != x) {
      ob->temp1 = static_cast<sword>(x);
      ob->state = ST(s_keenlineup);
    } else {
      ob->state = ST(s_keenswitch);
    }
    ps.upheld = true;
    return true;
  }
  if (intile == 2 || intile == 32) {
    word x = static_cast<word>((ob->tilemidx << G_T_SHIFT) + 6 * PIXGLOBAL);
    word left = TINF_IN(*(FGSpot(ob->tilemidx, ob->tiletop) - 1));
    if (left == 2 || left == 32) x = static_cast<word>(x - TILEGLOBAL);
    if (ob->x != x) {
      ob->temp1 = static_cast<sword>(x);
      ob->state = ST(s_keenlineup);
    } else {
      ps.invincible = 110;
      ob->state = ST(s_keenenter1);
      ob->priority = 0;
    }
    ps.upheld = true;
    return true;
  }
  return false;
}

// 0B80:0234 and 0B80:024C: footsteps while walking into a door.
void KeenWalkSound1(objtype*) { SD_PlaySound(SND_WORLDWALK1); }
void KeenWalkSound2(objtype*) { SD_PlaySound(SND_WORLDWALK2); }

// 0B80:0265
void KeenStandActions(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  if (ps.c.xaxis) {
    ob->state = ST(s_keenwalk1);
    KeenWalkThink(ob);
    xtry = static_cast<sword>(static_cast<sword>(ob->xdir * ob->state->xmove * static_cast<sword>(tics)) / 4);
    return;
  }

  if (ps.firebutton && !ps.fireheld) {
    ps.fireheld = true;
    if (ps.c.yaxis == -1)
      ob->state = ST(s_keenshootup1);
    else
      ob->state = ST(s_keenshoot1);
    return;
  }

  if (ps.jumpbutton && !ps.jumpheld) {
    ps.jumpheld = true;
    SD_PlaySound(SND_JUMP);
    ob->xspeed = 0;
    ob->yspeed = -40;
    ytry = 0;
    ps.jumptime = 18;
    ob->state = ST(s_keenjump1);
    return;
  }

  if (ps.pogobutton && !ps.pogoheld) {
    ps.pogoheld = true;
    SD_PlaySound(SND_JUMP);
    ob->state = ST(s_keenpogodown);
    ob->xspeed = 0;
    ob->yspeed = -48;
    ytry = 0;
    ps.jumptime = 24;
    return;
  }

  switch (ps.c.yaxis) {
    case -1:
      if (CheckGrabPole(ob)) return;
      if (!ps.upheld && CheckEnterHouse(ob)) return;
      ob->state = ST(s_keenlookup);
      return;
    case 1:
      if (CheckGrabPole(ob)) return;
      ob->state = ST(s_keenlookdown);
      return;
    default:
      return;
  }
}

// 0B80:0395
void KeenStandThink(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  if (ps.c.dir != dir_None || ps.jumpbutton || ps.pogobutton || ps.firebutton) {
    ob->temp1 = ob->temp2 = 0;
    ob->state = ST(s_keenstand);
    KeenStandActions(ob);
    return;
  }

  if ((ob->hitnorth & ~7) != 24) ob->temp1 = static_cast<sword>(ob->temp1 + tics);

  switch (ob->temp2) {
    case 0:
      if (ob->temp1 > 200) {
        ob->temp2++;
        ob->state = ST(s_keenpauselook);
        ob->temp1 = 0;
      }
      break;
    case 1:
      if (ob->temp1 > 300) {
        ob->temp2++;
        ob->temp1 = 0;
        if (ps.moonok == 1) {
          ps.moonok = 2;
          ob->state = ST(s_keenmoon1);
        } else {
          ob->state = ST(s_keenwait1);
        }
      }
      break;
    case 2:
      if (ob->temp1 > 700) {
        ob->temp2++;
        ob->state = ST(s_keenread1);
        ob->temp1 = 0;
      }
      break;
  }
}

// 0B80:045C
void KeenReadThink(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  if (storedemo) {
    playstate = ex_abortgame;
    IN_ClearKeysDown();
  }
  if (ps.c.dir != dir_None || ps.jumpbutton || ps.pogobutton) {
    ob->temp1 = ob->temp2 = 0;
    ob->state = ST(s_keenstopread1);
  }
}

// 0B80:049A
void KeenLookUpThink(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  if (ps.c.yaxis != -1 || ps.c.xaxis || (ps.jumpbutton && !ps.jumpheld) || (ps.pogobutton && !ps.pogoheld) ||
      ps.firebutton) {
    ob->state = ST(s_keenstand);
    KeenStandActions(ob);
  }
}

// 0B80:04E2
void KeenLookDownThink(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  if (ps.jumpbutton && !ps.jumpheld && (ob->hitnorth & 7) == 1) {
    // drop down through a one-way platform
    ps.jumpheld = true;
    word* map = FGSpot(ob->tilemidx, ob->tilebottom);
    word tile = *map;
    if (TINF_W(tile) || TINF_E(tile) || TINF_S(tile)) return;
    map += mapwidth;
    tile = *map;
    if (TINF_W(tile) || TINF_E(tile) || TINF_S(tile)) return;

    sword ymove = static_cast<sword>((tics < 4 ? 4 : tics) << 4);
    if (ps.riding) ymove = static_cast<sword>(ymove + ps.riding->ymove);
    ob->bottom = static_cast<word>(ob->bottom + ymove);
    ps.riding = nullptr;
    ob->y = static_cast<word>(ob->y + ymove);
    xtry = ytry = 0;
    ob->state = ST(s_keenjump3);
    ob->xspeed = ob->yspeed = 0;
    SD_PlaySound(SND_PLUMMET);
    return;
  }

  if (ps.c.yaxis != 1 || ps.c.xaxis || (ps.jumpbutton && !ps.jumpheld) || (ps.pogobutton && !ps.pogoheld))
    ob->state = ST(s_keenlookdown4);
}

// 0B80:062C
void KeenWalkThink(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  if (!ps.c.xaxis) {
    ob->state = ST(s_keenstand);
    KeenStandActions(ob);
    return;
  }

  ob->xdir = ps.c.xaxis;

  switch (ps.c.yaxis) {
    case -1:
      if (CheckGrabPole(ob)) return;
      if (ps.upheld) return;
      if (CheckEnterHouse(ob)) return;
      break;
    case 1:
      if (CheckGrabPole(ob)) return;
      break;
    default:
      break;
  }

  if (ps.firebutton && !ps.fireheld) {
    ps.fireheld = true;
    if (ps.c.yaxis == -1)
      ob->state = ST(s_keenshootup1);
    else
      ob->state = ST(s_keenshoot1);
    return;
  }

  if (ps.jumpbutton && !ps.jumpheld) {
    ps.jumpheld = true;
    SD_PlaySound(SND_JUMP);
    ob->xspeed = static_cast<sword>(ob->xdir << 4);
    ob->yspeed = -40;
    ytry = xtry = 0;
    ps.jumptime = 18;
    ob->state = ST(s_keenjump1);
  }

  if (ps.pogobutton && !ps.pogoheld) {
    ps.pogoheld = true;
    ob->state = ST(s_keenpogodown);
    SD_PlaySound(SND_JUMP);
    ob->xspeed = static_cast<sword>(ob->xdir << 4);
    ob->yspeed = -48;
    xtry = 0;
    ps.jumptime = 24;
    return;
  }

  // walking adjusts for slopes
  xtry = static_cast<sword>(xtry + walkspeed(ob->hitnorth) * static_cast<sword>(tics));

  if (ob->state == ST(s_keenwalk1) && !ob->temp3) {
    SD_PlaySound(SND_WORLDWALK1);
    ob->temp3 = 1;
    return;
  }
  if (ob->state == ST(s_keenwalk3) && !ob->temp3) {
    SD_PlaySound(SND_WORLDWALK2);
    ob->temp3 = 1;
    return;
  }
  if (ob->state == ST(s_keenwalk2) || ob->state == ST(s_keenwalk4)) ob->temp3 = 0;
}

// 0B80:07D8
void KeenLineUpThink(objtype* ob) {
  sword delta = static_cast<sword>(ob->temp1 - ob->x);
  if (delta < 0) {
    xtry = static_cast<sword>(xtry - (tics << 4));
    if (xtry > delta) return;
  } else if (delta > 0) {
    xtry = static_cast<sword>(xtry + (tics << 4));
    if (xtry < delta) return;
  }
  xtry = delta;
  ob->temp1 = 0;
  if (!CheckEnterHouse(ob)) ob->state = ST(s_keenstand);
}

// 0B80:0842: the info plane under a door holds the destination tile.
void KeenEnterThink(objtype* ob) {
  word info = *InfoSpot(ob->tileleft, ob->tilebottom);
  ob->y = static_cast<word>(((info & 0xFF) << G_T_SHIFT) - TILEGLOBAL + 15);
  ob->x = static_cast<word>((info >> 8) << G_T_SHIFT);
  ob->priority = 1;
  ob->needtoclip = cl_noclip;
  ChangeState(ob, ob->state->nextstate);
  ob->needtoclip = cl_midclip;
  CenterActor(ob);
}

// 0B80:08B9
void KeenSwitchThink(objtype* ob) {
  word* spot = FGSpot(ob->tilemidx, ob->tiletop);
  word tile = *spot;
  word newtile = static_cast<word>(tile + static_cast<signed char>(tinf[MANIM + tile]));
  word info = *InfoSpot(ob->tilemidx, ob->tiletop);
  word destx = info >> 8;
  word desty = info & 0xFF;
  word intile = TINF_IN(tile);

  RF_MemToMap(&newtile, 1, ob->tilemidx, ob->tiletop, 1, 1);
  SD_PlaySound(SND_USESWITCH);

  if (intile == 15) {
    // toggle a bridge: animate every foreground tile along the two rows
    for (word y = desty; y < desty + 2; y++) {
      word startx = static_cast<word>(destx - (y != desty ? 1 : 0));
      word* map = FGSpot(startx, y);
      for (word x = startx; x < mapwidth; x++) {
        word t = *map++;
        sword manim = static_cast<signed char>(tinf[MANIM + t]);
        if (!manim) break;
        t = static_cast<word>(t + manim);
        RF_MemToMap(&t, 1, x, y, 1, 1);
      }
    }
  } else {
    // toggle a platform blocker in the info plane
    *InfoSpot(destx, desty) ^= 0x1F;
  }
}

// 0B80:0A34
void KeenKeyThink(objtype* ob) {
  word newtile = static_cast<word>(*FGSpot(ob->tilemidx, ob->tilebottom) + 0x12);
  word info = *InfoSpot(ob->tilemidx, ob->tilebottom);
  word destx = info >> 8;
  word desty = info & 0xFF;

  RF_MemToMap(&newtile, 1, ob->tilemidx, ob->tilebottom, 1, 1);
  SD_PlaySound(SND_OPENDOOR);
  GetNewObj(false);
  newobj->x = destx;
  newobj->y = desty;
  if (destx > mapwidth || destx < 2 || desty > mapheight || desty < 2)
    Quit("Keyholder points to a bad spot!");

  word* map = FGSpot(destx, desty);
  word doortile = *map;
  sword count = 1;
  map += mapwidth;
  while (*map == doortile) {
    count++;
    map += mapwidth;
  }
  newobj->temp1 = count;
  newobj->active = ac_allways;
  newobj->needtoclip = cl_noclip;
  newobj->obclass = inertobj;
  NewState(newobj, ST(s_door1));
}

// 0B80:0B5B
void KeenAirThink(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  if (jumpcheat && ps.jumpbutton) {
    ob->yspeed = -40;
    ps.jumptime = 18;
    ps.jumpheld = true;
  }

  if (ps.jumptime) {
    if (ps.jumptime <= tics) {
      ytry = static_cast<sword>(ob->yspeed * static_cast<sword>(ps.jumptime));
      ps.jumptime = 0;
    } else {
      ytry = static_cast<sword>(ob->yspeed * static_cast<sword>(tics));
      if (!jumpcheat) ps.jumptime = static_cast<word>(ps.jumptime - tics);
    }
    if (!ps.jumpbutton) ps.jumptime = 0;
    if (!ps.jumptime && ob->state->nextstate) ob->state = ob->state->nextstate;
  } else {
    if (gamestate.difficulty == gd_Easy)
      DoWeakGravity(ob);
    else
      DoGravity(ob);
    if (ob->yspeed > 0 && ob->state != ST(s_keenjump3) && ob->state != ST(s_keenjump4))
      ob->state = ob->state->nextstate;
  }

  if (ps.c.xaxis) {
    ob->xdir = ps.c.xaxis;
    AccelerateX(ob, static_cast<sword>(ps.c.xaxis * 2), 24);
  } else {
    FrictionX(ob);
  }

  if (ob->hitsouth == 17) {
    xtry = 0;
    ob->xspeed = 0;
  }

  if (ps.firebutton && !ps.fireheld) {
    ps.fireheld = true;
    switch (ps.c.yaxis) {
      case -1: ob->state = ST(s_keenairshootup1); return;
      case 0: ob->state = ST(s_keenairshoot1); return;
      case 1: ob->state = ST(s_keenairshootdown1); return;
    }
  }

  if (ps.pogobutton && !ps.pogoheld) {
    ps.pogoheld = true;
    ob->state = ST(s_keenpogo);
    ps.jumptime = 0;
    return;
  }

  if (ps.c.yaxis == -1) CheckGrabPole(ob);
}

// 0B80:0CC9
void KeenBounceThink(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  ob->yspeed = -48;
  ytry = static_cast<sword>(ob->yspeed * 6);
  ps.jumptime = 24;
  SD_PlaySound(SND_POGOBOUNCE);
}

// 0B80:0CF5
void KeenPogoThink(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  if (ps.jumptime) {
    if (ps.jumpbutton || static_cast<sword>(ps.jumptime) <= 9)
      DoTinyGravity(ob);
    else
      DoGravity(ob);
    if (ps.jumptime <= tics)
      ps.jumptime = 0;
    else
      ps.jumptime = static_cast<word>(ps.jumptime - tics);
    if (!ps.jumptime && ob->state->nextstate) ob->state = ob->state->nextstate;
  } else {
    if (gamestate.difficulty == gd_Easy)
      DoWeakGravity(ob);
    else
      DoGravity(ob);
  }

  if (ps.c.xaxis) {
    if (!ob->xspeed) ob->xdir = ps.c.xaxis;
    AccelerateX(ob, ps.c.xaxis, 24);
  } else {
    xtry = static_cast<sword>(xtry + ob->xspeed * static_cast<sword>(tics));
    if (ob->xspeed > 0)
      ob->xdir = 1;
    else if (ob->xspeed < 0)
      ob->xdir = -1;
  }

  if (ob->hitsouth == 17) {
    xtry = 0;
    ob->xspeed = 0;
  }

  if (ps.firebutton && !ps.fireheld) {
    ps.fireheld = true;
    switch (ps.c.yaxis) {
      case -1: ob->state = ST(s_keenairshootup1); return;
      case 0: ob->state = ST(s_keenairshoot1); return;
      case 1: ob->state = ST(s_keenairshootdown1); return;
    }
  }

  if (ps.pogobutton && !ps.pogoheld) {
    ps.pogoheld = true;
    ob->state = ST(s_keenjump3);
  }
}

// 0B80:0E2D: shooting and jumping off a pole.
void PoleActions(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  if (ps.c.xaxis) ob->xdir = ps.c.xaxis;

  if (ps.firebutton && !ps.fireheld) {
    ps.fireheld = true;
    switch (ps.c.yaxis) {
      case -1: ob->state = ST(s_keenpoleshootup1); break;
      case 0: ob->state = ST(s_keenpoleshoot1); break;
      case 1: ob->state = ST(s_keenpoleshootdown1); break;
    }
  }

  if (ps.jumpbutton && !ps.jumpheld) {
    ps.jumpheld = true;
    SD_PlaySound(SND_JUMP);
    ob->xspeed = polexspeed(ps.c.xaxis);
    ob->yspeed = -20;
    ob->needtoclip = cl_midclip;
    ps.jumptime = 10;
    ob->state = ST(s_keenjump1);
    ob->ydir = 1;
    ps.leavepoletime = static_cast<std::int32_t>(lasttimecount);
  }
}

void KeenSlideThink(objtype* ob);

// 0B80:0ED3
void KeenPoleThink(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  switch (ps.c.yaxis) {
    case -1:
      ob->state = ST(s_keenclimb1);
      ob->ydir = -1;
      return;
    case 1:
      ob->state = ST(s_keenslide1);
      ob->ydir = 1;
      KeenSlideThink(ob);
      return;
    default:
      break;
  }

  if (ps.c.xaxis) {
    // jumping off the pole onto a floor
    word tile = *FGSpot(ob->tilemidx, static_cast<word>(ob->tilebottom + 1));
    if (TINF_N(tile)) {
      ob->xspeed = 0;
      ob->yspeed = 0;
      ob->needtoclip = cl_midclip;
      ps.jumptime = 0;
      ob->state = ST(s_keenjump3);
      ob->ydir = 1;
      SD_PlaySound(SND_PLUMMET);
      return;
    }
  }
  PoleActions(ob);
}

// 0B80:0F89
void KeenClimbThink(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  word tile = *FGSpot(ob->tilemidx, ob->tiletop);
  if ((TINF_IN(tile) & 0x7F) != 1) {
    ytry = 0;
    ob->state = ST(s_keenpole);
    PoleActions(ob);
    return;
  }
  switch (ps.c.yaxis) {
    case 0:
      ob->state = ST(s_keenpole);
      ob->ydir = 0;
      break;
    case 1:
      ob->state = ST(s_keenslide1);
      ob->ydir = 1;
      KeenSlideThink(ob);
      break;
  }
  PoleActions(ob);
}

// 0B80:101C
void KeenSlideThink(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  word y = static_cast<word>((ob->bottom - 4 * PIXGLOBAL) >> G_T_SHIFT);
  word tile = *FGSpot(ob->tilemidx, y);
  if ((TINF_IN(tile) & 0x7F) != 1) {
    // off the bottom of the pole
    SD_PlaySound(SND_PLUMMET);
    ob->state = ST(s_keenjump3);
    ps.jumptime = 0;
    ob->xspeed = polexspeed(ps.c.xaxis);
    ob->yspeed = 0;
    ob->needtoclip = cl_midclip;
    ob->tilebottom--;
    return;
  }
  switch (ps.c.yaxis) {
    case -1:
      ob->state = ST(s_keenclimb1);
      ob->ydir = -1;
      break;
    case 0:
      ob->state = ST(s_keenpole);
      ob->ydir = 0;
      break;
  }
  PoleActions(ob);
}

// 0B80:10D1
void KeenDropThink(objtype* ob) { ob->needtoclip = cl_midclip; }

// 0B80:10E0: hanging on a ledge.
void KeenHoldThink(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  if (ps.c.yaxis == -1 || ob->xdir == ps.c.xaxis) {
    ob->state = ST(s_keenclimbup1);
    word tile;
    if (ob->xdir == 1)
      tile = *FGSpot(ob->tileright, static_cast<word>(ob->tiletop - 1));
    else
      tile = *FGSpot(ob->tileleft, static_cast<word>(ob->tiletop - 1));
    if (ob->xdir == 1)
      ytry = -256;
    else
      ytry = -128;
    if (!(TINF_IN(tile) & 0x80)) ob->priority = 3;
  } else if (ps.c.yaxis == 1 || (ps.c.xaxis && ob->xdir != ps.c.xaxis)) {
    ob->state = ST(s_keenjump3);
    ob->needtoclip = cl_midclip;
  }
}

// 0B80:1195
void KeenShootThink(objtype* ob) {
  if (ob->state == ST(s_keenshoot1)) {
    if (ob->xdir == 1)
      SpawnShot(ob, static_cast<word>(ob->x + 16 * PIXGLOBAL), static_cast<word>(ob->y + 4 * PIXGLOBAL), 2);
    else
      SpawnShot(ob, static_cast<word>(ob->x - 8 * PIXGLOBAL), static_cast<word>(ob->y + 4 * PIXGLOBAL), 6);
  }
  if (ob->state == ST(s_keenairshoot2)) {
    if (ob->xdir == 1)
      SpawnShot(ob, static_cast<word>(ob->x + 16 * PIXGLOBAL), static_cast<word>(ob->y + 2 * PIXGLOBAL), 2);
    else
      SpawnShot(ob, ob->x, static_cast<word>(ob->y + 2 * PIXGLOBAL), 6);
  }
  if (ob->state == ST(s_keenairshootdown2))
    SpawnShot(ob, static_cast<word>(ob->x + 8 * PIXGLOBAL), static_cast<word>(ob->y + 18 * PIXGLOBAL), 4);
  if (ob->state == ST(s_keenairshootup2))
    SpawnShot(ob, static_cast<word>(ob->x + 5 * PIXGLOBAL), static_cast<word>(ob->y - 10 * PIXGLOBAL), 0);
  if (ob->state == ST(s_keenshootup1))
    SpawnShot(ob, static_cast<word>(ob->x + 5 * PIXGLOBAL), static_cast<word>(ob->y - 10 * PIXGLOBAL), 0);
  if (ob->state == ST(s_keenpoleshoot1)) {
    if (ob->xdir == 1)
      SpawnShot(ob, static_cast<word>(ob->x + 16 * PIXGLOBAL), static_cast<word>(ob->y + 4 * PIXGLOBAL), 2);
    else
      SpawnShot(ob, static_cast<word>(ob->x - 8 * PIXGLOBAL), static_cast<word>(ob->y + 4 * PIXGLOBAL), 6);
  }
  if (ob->state == ST(s_keenpoleshootup1)) {
    if (ob->xdir == 1)
      SpawnShot(ob, static_cast<word>(ob->x + 6 * PIXGLOBAL), static_cast<word>(ob->y + 4 * PIXGLOBAL), 0);
    else
      SpawnShot(ob, static_cast<word>(ob->x + 12 * PIXGLOBAL), static_cast<word>(ob->y + 4 * PIXGLOBAL), 0);
  }
  if (ob->state == ST(s_keenpoleshootdown1)) {
    if (ob->xdir == 1)
      SpawnShot(ob, static_cast<word>(ob->x + 6 * PIXGLOBAL), static_cast<word>(ob->y + 24 * PIXGLOBAL), 4);
    else
      SpawnShot(ob, static_cast<word>(ob->x + 12 * PIXGLOBAL), static_cast<word>(ob->y + 24 * PIXGLOBAL), 4);
  }
}

// 0B80:134A..139F: pulling up onto a ledge.
void T_PullUp1(objtype* ob) {
  if (ob->xdir == 1)
    xtry = 8 * PIXGLOBAL;
  else
    ytry = -8 * PIXGLOBAL;
}

void T_PullUp2(objtype* ob) {
  if (ob->xdir == 1) {
    xtry = 8 * PIXGLOBAL;
    ytry = -8 * PIXGLOBAL;
  } else {
    xtry = -8 * PIXGLOBAL;
    ytry = -8 * PIXGLOBAL;
  }
}

void T_PullUp3(objtype*) { ytry = -8 * PIXGLOBAL; }

void T_PulledUp(objtype* ob) {
  ob->needtoclip = cl_midclip;
  ob->priority = 1;
  ytry = 8 * PIXGLOBAL;
}

// 0B80:13B9
void KeenDieThink(objtype* ob) {
  DoWeakGravity(ob);
  xtry = static_cast<sword>(ob->xspeed * static_cast<sword>(tics));
  if (!OnScreen(ob)) playstate = ex_died;
}

// 0B80:1775: items placed as foreground tiles.
void GetBonus(objtype* ob, word tilex, word tiley, int type) {
  RF_MemToMap(&zeromap, 1, tilex, tiley, 1, 1);
  SD_PlaySound(bonussound(type));
  GivePoints(ob, bonuspoints(type));
  if (type < 4)
    gamestate.keys[type]++;
  else if (type == 10)
    gamestate.lives++;
  else if (type == 11)
    PlayerOf(ob).ammo = static_cast<sword>(PlayerOf(ob).ammo + ammopickup(gamestate.difficulty));

  // the score that rises up
  GetNewObj(true);
  newobj->obclass = inertobj;
  newobj->priority = 3;
  newobj->x = static_cast<word>(tilex << G_T_SHIFT);
  newobj->y = static_cast<word>(tiley << G_T_SHIFT);
  newobj->ydir = -1;
  newobj->shapenum = bonusshape(type);
  newobj->temp2 = static_cast<sword>(newobj->shapenum);
  NewState(newobj, ST(s_bonusrise));
  newobj->needtoclip = cl_noclip;
}

// 0B80:183F
void GetDrop(objtype* ob, word tilex, word tiley) {
  PlayerState& ps = PlayerOf(ob);
  RF_MemToMap(&zeromap, 1, tilex, tiley, 1, 1);
  SD_PlaySound(SND_GETWATER);
  SpawnDropSplash(tilex, tiley);
  if (++ps.drops == 100) {
    ps.drops = 0;
    SD_PlaySound(SND_EXTRAKEEN);
    gamestate.lives++;
    GetNewObj(true);
    newobj->obclass = inertobj;
    newobj->priority = 3;
    newobj->x = static_cast<word>(tilex << G_T_SHIFT);
    newobj->y = static_cast<word>((tiley - 1) << G_T_SHIFT);
    newobj->ydir = -1;
    newobj->shapenum = 218;
    newobj->temp2 = 218;
    NewState(newobj, ST(s_bonusrise));
    newobj->needtoclip = cl_noclip;
  }
}

// 0B80:14A9
void KeenContact(objtype* ob, objtype* hit) {
  PlayerState& ps = PlayerOf(ob);
  switch (hit->obclass) {
    case bonusobj: {
      int type = hit->temp1;
      if (type > 11) return;
      SD_PlaySound(bonussound(type));
      hit->obclass = inertobj;
      hit->priority = 3;
      hit->shapenum = bonusshape(type);
      GivePoints(ob, bonuspoints(type));
      if (type < 4)
        gamestate.keys[type]++;
      else if (type == 10)
        gamestate.lives++;
      else if (type == 11)
        ps.ammo = static_cast<sword>(ps.ammo + ammopickup(gamestate.difficulty));
      ChangeState(hit, ST(s_bonusrise));
      break;
    }
    case oracleobj:
      if (!ob->hitnorth) break;
      if (mapon == 14) {
        OracleCouncilMember();
        RF_ForceRefresh();
        RemoveObj(hit);
      } else {
        SD_PlaySound(SND_FOUNDMEMBER);
        playstate = ex_rescued;
      }
      break;
    case stunnedobj:
      if (hit->temp4 != bounderobj) break;
      [[fallthrough]];
    case platformobj:
      if (!ps.riding) ClipToSpriteTop(ob, hit);
      break;
    case bounderobj:
      ClipToSprite(ob, hit, false);
      break;
    case lindseyobj:
      LindseyDialog();
      RemoveObj(hit);
      RF_ForceRefresh();
      break;
    case footobj:
      playstate = ex_foot;
      break;
    default:
      break;
  }
}

// 0B80:1632: contact while on a pole, climbing or hanging.
void KeenPosContact(objtype* ob, objtype* hit) {
  PlayerState& ps = PlayerOf(ob);
  switch (hit->obclass) {
    case platformobj:
      ob->needtoclip = cl_midclip;
      ChangeState(ob, ST(s_keenjump3));
      ob->yspeed = ob->xspeed = 0;
      ps.jumptime = 0;
      ClipToSpriteTop(ob, hit);
      break;
    case madmushroomobj:
    case arachnutobj:
    case berkeloidobj:
      KillKeen(ob);
      break;
    case bounderobj:
      ob->priority = 1;
      ob->needtoclip = cl_midclip;
      ChangeState(ob, ST(s_keenjump3));
      ob->yspeed = ob->xspeed = 0;
      ps.jumptime = 0;
      ClipToSprite(ob, hit, false);
      break;
    default:
      break;
  }
}

// 0B80:1A7B
void KeenSimpleReact(objtype* ob) {
  RF_PlaceSprite(&ob->sprite, ob->x, ob->y, ob->shapenum, spritedraw, static_cast<int>(ob->priority));
}

// 0B80:1AA2
void KeenStandReact(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  if (!ob->hitnorth) {
    // walked off an edge
    SD_PlaySound(SND_PLUMMET);
    ob->xspeed = static_cast<sword>(ob->xdir * 8);
    ob->yspeed = 0;
    ChangeState(ob, ST(s_keenjump3));
    ps.jumptime = 0;
  } else if ((ob->hitnorth & ~7) == 8) {
    KillKeen(ob);
  } else if (ob->hitnorth == 41) {
    xtry = static_cast<sword>(tics * 8);
    ytry = 0;
    ob->temp1 = 0;
    ClipToWalls(ob);
  } else if (ob->hitnorth == 49) {
    xtry = static_cast<sword>(tics * -8);
    ytry = 0;
    ob->temp1 = 0;
    ClipToWalls(ob);
  }
  RF_PlaceSprite(&ob->sprite, ob->x, ob->y, ob->shapenum, spritedraw, static_cast<int>(ob->priority));
}

// 0B80:1B5F
void KeenWalkReact(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  if (!ob->hitnorth) {
    SD_PlaySound(SND_PLUMMET);
    ob->xspeed = static_cast<sword>(ob->xdir * 8);
    ob->yspeed = 0;
    ChangeState(ob, ST(s_keenjump3));
    ps.jumptime = 0;
  } else if ((ob->hitnorth & ~7) == 8) {
    KillKeen(ob);
  } else if (ob->hitnorth == 41) {
    xtry = static_cast<sword>(tics * 8);
    ytry = 0;
    ClipToWalls(ob);
  } else if (ob->hitnorth == 49) {
    xtry = static_cast<sword>(tics * -8);
    ytry = 0;
    ClipToWalls(ob);
  } else if ((ob->hiteast && ob->xdir == -1) || (ob->hitwest && ob->xdir == 1)) {
    // walked into a wall
    ob->ticcount = 0;
    ob->state = ST(s_keenstand);
    statetype* stand = ST(s_keenstand);
    ob->shapenum = static_cast<word>(ob->xdir == 1 ? stand->rightshapenum : stand->leftshapenum);
  }
  RF_PlaceSprite(&ob->sprite, ob->x, ob->y, ob->shapenum, spritedraw, static_cast<int>(ob->priority));
}

// Shared head-bump handling of KeenAirReact and KeenPogoReact.
void HitCeiling(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  if (ob->hitsouth == 17) {
    // jumping up through a pole hole
    ob->y = static_cast<word>(ob->y - 2 * PIXGLOBAL);
    ob->top = static_cast<word>(ob->top - 2 * PIXGLOBAL);
    ob->xspeed = 0;
    ob->x = static_cast<word>((ob->tilemidx << G_T_SHIFT) - 2 * PIXGLOBAL);
  } else if (!jumpcheat) {
    SD_PlaySound(SND_HELMETHIT);
    if (ob->hitsouth > 1) {
      ob->yspeed = static_cast<sword>(ob->yspeed + 16);
      if (ob->yspeed < 0) ob->yspeed = 0;
    } else {
      ob->yspeed = 0;
    }
    ps.jumptime = 0;
  }
}

// 0B80:1C48
void KeenAirReact(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  if ((ob->hiteast && ob->xdir == -1) || (ob->hitwest && ob->xdir == 1)) ob->xspeed = 0;

  if (ob->hitsouth) HitCeiling(ob);

  if (ob->hitnorth) {
    ob->ymove = 0;
    if ((ob->hitnorth & ~7) == 8) {
      KillKeen(ob);
    } else if (!(ob->hitnorth == 25 && ps.jumptime)) {
      ob->temp1 = ob->temp2 = 0;
      if (ob->state == ST(s_keenairshoot1))
        ChangeState(ob, ST(s_keenshoot1));
      else if (ob->state == ST(s_keenairshootup1))
        ChangeState(ob, ST(s_keenshootup1));
      else if (ps.c.xaxis)
        ChangeState(ob, ST(s_keenwalk1));
      else
        ChangeState(ob, ST(s_keenstand));
      SD_PlaySound(SND_LAND);
    }
  } else if (ob->ymove > 0) {
    // grab a ledge
    word oldtop = static_cast<word>(ob->top - ob->ymove);
    word graby = static_cast<word>(((ob->top - 4 * PIXGLOBAL) & 0xFF00) + 4 * PIXGLOBAL);
    word ty = static_cast<word>((graby >> G_T_SHIFT) - 1);
    if (oldtop < graby && ob->top >= graby) {
      if (ps.c.xaxis == -1) {
        word* map = FGSpot(ob->tileleft, ty);
        if (ob->hiteast) map--;
        word tile = *map;
        if (!TINF_E(tile) && !TINF_W(tile) && !TINF_N(tile) && !TINF_S(tile) &&
            TINF_E(map[mapwidth]) && TINF_N(map[mapwidth])) {
          ob->xdir = -1;
          ob->needtoclip = cl_noclip;
          ob->x = static_cast<word>((ob->x & 0xFF00) + 8 * PIXGLOBAL);
          ob->y = static_cast<word>(graby - 4 * PIXGLOBAL);
          ob->yspeed = ob->ymove = 0;
          ChangeState(ob, ST(s_keenholdon));
        }
      } else if (ps.c.xaxis == 1) {
        word* map = FGSpot(ob->tileright, ty);
        if (ob->hitwest) map++;
        word tile = *map;
        if (!TINF_E(tile) && !TINF_W(tile) && !TINF_N(tile) && !TINF_S(tile) &&
            TINF_W(map[mapwidth]) && TINF_N(map[mapwidth])) {
          ob->xdir = 1;
          ob->needtoclip = cl_noclip;
          ob->x = static_cast<word>((ob->x & 0xFF00) + 16 * PIXGLOBAL);
          ob->y = static_cast<word>(graby - 4 * PIXGLOBAL);
          ob->yspeed = ob->ymove = 0;
          ChangeState(ob, ST(s_keenholdon));
        }
      }
    }
  }

  RF_PlaceSprite(&ob->sprite, ob->x, ob->y, ob->shapenum, spritedraw, static_cast<int>(ob->priority));
}

// 0B80:1F81
void KeenPogoReact(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  if ((ob->hiteast && ob->xdir == -1) || (ob->hitwest && ob->xdir == 1)) ob->xspeed = 0;

  if (ob->hitsouth) HitCeiling(ob);

  if (ob->hitnorth) {
    ob->ymove = 0;
    if ((ob->hitnorth & ~7) == 8) {
      KillKeen(ob);
    } else if (!(ob->hitnorth == 25 && ps.jumptime)) {
      ob->yspeed = -48;
      ps.jumptime = 24;
      SD_PlaySound(SND_POGOBOUNCE);
      ChangeState(ob, ST(s_keenpogo));
    }
  }

  RF_PlaceSprite(&ob->sprite, ob->x, ob->y, ob->shapenum, spritedraw, static_cast<int>(ob->priority));
}

// 0B80:2071: sliding down onto the floor at the bottom of a pole.
void KeenPoleReact(objtype* ob) {
  word tile = *FGSpot(ob->tilemidx, ob->tilebottom);
  if (TINF_N(tile) == 1) {
    word delta = static_cast<word>((ob->bottom & 0xFF) + 1);
    ob->y = static_cast<word>(ob->y - delta);
    ob->bottom = static_cast<word>(ob->bottom - delta);
    ob->tilebottom--;
    ob->needtoclip = cl_midclip;
    ChangeState(ob, ST(s_keenlookdown));
  }
  RF_PlaceSprite(&ob->sprite, ob->x, ob->y, ob->shapenum, spritedraw, static_cast<int>(ob->priority));
}

}  // namespace

// 0B80:000E
void SpawnKeen(word tilex, word tiley, sword dir) {
  player->obclass = keenobj;
  player->active = ac_allways;
  player->priority = 1;
  player->x = static_cast<word>(tilex << G_T_SHIFT);
  player->y = static_cast<word>((tiley << G_T_SHIFT) - 241);
  player->xdir = dir;
  player->ydir = 1;
  NewState(player, ST(s_keenstand));
}

// 0B80:13E9
void KillKeen(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  if (ps.invincible || godmode) return;
  if (ob->state == ST(s_keendead)) return;

  ps.moonok = 0;
  ps.invincible = 30;
  lockscroll = true;
  ob->needtoclip = cl_noclip;
  ob->priority = 3;
  if (mapon == 17) {
    if (US_RndT() < 0x80)
      ChangeState(ob, ST(s_keensuitdie1));
    else
      ChangeState(ob, ST(s_keensuitdie2));
  } else {
    if (US_RndT() < 0x80)
      ChangeState(ob, ST(s_keendie1));
    else
      ChangeState(ob, ST(s_keendie2));
  }
  SD_PlaySound(SND_KEENDEAD);
  ob->yspeed = -40;
  ob->xspeed = 16;
}

// 0B80:16CD
void HandleRiding(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  objtype* plat = ps.riding;
  if (ob->right < plat->left || ob->left > plat->right) {
    ps.riding = nullptr;
    return;
  }
  if (ob->ymove < 0) {
    ps.riding = nullptr;
    if (plat->ymove < 0) {
      xtry = 0;
      ytry = plat->ymove;
      PushObj(ob);
    }
    return;
  }
  xtry = plat->xmove;
  ytry = static_cast<sword>(plat->top - ob->bottom - 16);
  PushObj(ob);

  if (compatability)
    ob->x = static_cast<word>((ob->x & 0xFF80) | (plat->x & 0x7F));
  else
    ob->x = static_cast<word>(ob->x | (plat->x & 0x1F));

  if (ob->hitsouth) {
    ps.riding = nullptr;
    return;
  }
  ob->hitnorth = 25;
}

// 0B80:18E9
void CheckInTiles(objtype* ob) {
  PlayerState& ps = PlayerOf(ob);
  if (ps.moonok == 1) ps.moonok = 0;

  word* map = FGSpot(ob->tileleft, ob->tiletop);
  word rowdelta = static_cast<word>(mapwidth - (ob->tileright - ob->tileleft + 1));
  for (word y = ob->tiletop; ob->tilebottom >= y; y++, map += rowdelta) {
    for (word x = ob->tileleft; ob->tileright >= x; x++, map++) {
      word intile = TINF_IN(*map) & 0x7F;
      if (!intile) continue;
      switch (intile) {
        case 3:
          KillKeen(ob);
          break;
        case 4:
          GetDrop(ob, x, y);
          break;
        case 7:
        case 8:
        case 9:
        case 10: {
          if (ob->tilebottom != y || !ob->hitnorth || ob->state == ST(s_keenkey) ||
              !gamestate.keys[intile - 7])
            return;
          word tx = static_cast<word>((x << G_T_SHIFT) - 4 * PIXGLOBAL);
          if (ob->x != tx) {
            ob->temp1 = static_cast<sword>(tx);
            ob->state = ST(s_keenlineup);
            return;
          }
          gamestate.keys[intile - 7]--;
          ChangeState(ob, ST(s_keenkey));
          break;
        }
        case 16:
          if (!ps.moonok) ps.moonok = 1;
          break;
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
          GetBonus(ob, x, y, static_cast<int>(intile - 21 + 4));
          break;
        default:
          break;
      }
    }
  }
}

CK_FUNC(0x0B80, 0x0234, KeenWalkSound1);
CK_FUNC(0x0B80, 0x024C, KeenWalkSound2);
CK_FUNC(0x0B80, 0x0395, KeenStandThink);
CK_FUNC(0x0B80, 0x045C, KeenReadThink);
CK_FUNC(0x0B80, 0x049A, KeenLookUpThink);
CK_FUNC(0x0B80, 0x04E2, KeenLookDownThink);
CK_FUNC(0x0B80, 0x062C, KeenWalkThink);
CK_FUNC(0x0B80, 0x07D8, KeenLineUpThink);
CK_FUNC(0x0B80, 0x0842, KeenEnterThink);
CK_FUNC(0x0B80, 0x08B9, KeenSwitchThink);
CK_FUNC(0x0B80, 0x0A34, KeenKeyThink);
CK_FUNC(0x0B80, 0x0B5B, KeenAirThink);
CK_FUNC(0x0B80, 0x0CC9, KeenBounceThink);
CK_FUNC(0x0B80, 0x0CF5, KeenPogoThink);
CK_FUNC(0x0B80, 0x0ED3, KeenPoleThink);
CK_FUNC(0x0B80, 0x0F89, KeenClimbThink);
CK_FUNC(0x0B80, 0x101C, KeenSlideThink);
CK_FUNC(0x0B80, 0x10D1, KeenDropThink);
CK_FUNC(0x0B80, 0x10E0, KeenHoldThink);
CK_FUNC(0x0B80, 0x1195, KeenShootThink);
CK_FUNC(0x0B80, 0x134A, T_PullUp1);
CK_FUNC(0x0B80, 0x1369, T_PullUp2);
CK_FUNC(0x0B80, 0x1394, T_PullUp3);
CK_FUNC(0x0B80, 0x139F, T_PulledUp);
CK_FUNC(0x0B80, 0x13B9, KeenDieThink);
CK_FUNC(0x0B80, 0x14A9, KeenContact);
CK_FUNC(0x0B80, 0x1632, KeenPosContact);
CK_FUNC(0x0B80, 0x1A7B, KeenSimpleReact);
CK_FUNC(0x0B80, 0x1AA2, KeenStandReact);
CK_FUNC(0x0B80, 0x1B5F, KeenWalkReact);
CK_FUNC(0x0B80, 0x1C48, KeenAirReact);
CK_FUNC(0x0B80, 0x1F81, KeenPogoReact);
CK_FUNC(0x0B80, 0x2071, KeenPoleReact);
