// K4_SPEC: level setup from the info plane, the dialogs with Lindsey and the
// council members, and Keen in the wetsuit (segment 0E8F).
#include <cstring>

#include "exe/keen4exe.h"
#include "game/ck_def.h"

extern sword xtry, ytry;

namespace {

using keen4::exe::g_exe;

constexpr int SND_KEENBUBBLE = 36, SND_LINDSEY = 45;

constexpr int NUMLUMPS = 37;
boolean lumpneeded[NUMLUMPS];  // DGROUP CB3B

// Sprite chunk ranges per actor type (DGROUP 1860 / 18AA).
word lumpstart(int lump) { return g_exe.Word(static_cast<std::uint16_t>(0x1860 + lump * 2)); }
word lumpend(int lump) { return g_exe.Word(static_cast<std::uint16_t>(0x18AA + lump * 2)); }
// Lump needed by each bonus item type (DGROUP 19BC).
word bonuslump(int type) { return g_exe.Word(static_cast<std::uint16_t>(0x19BC + type * 2)); }

enum {
  KEENLUMP = 1, WORLDKEENLUMP = 10, SLUGLUMP = 11, MUSHROOMLUMP = 12, LINDSEYLUMP = 14, INCHWORMLUMP = 15,
  EATERLUMP = 16, COUNCILLUMP = 17, EGGLUMP = 18, MIMROCKLUMP = 19, DOPEFISHLUMP = 20, SCHOOLFISHLUMP = 21,
  ARACHNUTLUMP = 22, SKYPESTLUMP = 23, WORMOUTHLUMP = 24, LICKLUMP = 25, PLATFORMLUMP = 26, BOUNDERLUMP = 27,
  CLOUDLUMP = 28, BERKELOIDLUMP = 29, DARTLUMP = 31, SCUBAKEENLUMP = 32, SPRITELUMP = 33, MINELUMP = 34,
  LUMP35 = 35, EGGBIRDLUMP = 36
};

// Picture chunks used by the dialogs.
constexpr int COUNCILPIC = 110, KEENTALK1PIC = 111, KEENTALK2PIC = 112, KEENMADPIC = 113, LINDSEYPIC = 114;

void MarkChunk(int chunk) { CA_MarkGrChunk(chunk); }

void MarkKeenSprites() {
  MarkChunk(253);  // score box
}

void MarkSmoke() {
  for (int i = 350; i <= 353; i++) MarkChunk(i);
}

const char* DgString(word ptrofs) { return g_exe.String(g_exe.Word(ptrofs)); }

// The far strings of the janitor's speech (segments 1F58..1F65 of the image).
const char* FarString(word seg) { return reinterpret_cast<const char*>(g_exe.At(seg, 0)); }

// Keen's talking window: his picture on the right.
void KeenTalkWindow(int pic) {
  US_CenterWindow(26, 8);
  VWB_DrawPic(WindowX + WindowW - 48, WindowY, pic);
  WindowW -= 48;
  PrintY += 12;
}

// The other party's window: their picture on the left.
void OtherTalkWindow(int pic) {
  US_CenterWindow(26, 8);
  VWB_DrawPic(WindowX, WindowY, pic);
  PrintY += 6;
  WindowW -= 48;
  WindowX += 48;
}

void WaitAck(int vbls) {
  VW_UpdateScreen();
  VW_WaitVBL(vbls);
  IN_ClearKeysDown();
  IN_Ack();
}

// 0E8F:0D43
void SpawnBubble(objtype* ob) {
  ob->temp4 = 0;
  GetNewObj(true);
  if (ob->xdir == -1)
    newobj->x = ob->x;
  else
    newobj->x = static_cast<word>(ob->x + 24 * PIXGLOBAL);
  newobj->y = ob->y;
  newobj->obclass = inertobj;
  newobj->priority = 3;
  newobj->active = ac_removable;
  newobj->needtoclip = cl_noclip;
  newobj->yspeed = -24;
  newobj->xspeed = 4;
  switch (US_RndT() / 64) {
    case 0: NewState(newobj, ST(0x1A64)); break;
    case 1: NewState(newobj, ST(0x1A82)); break;
    case 2: NewState(newobj, ST(0x1AA0)); break;
    case 3: NewState(newobj, ST(0x1ABE)); break;
  }
  SD_PlaySound(SND_KEENBUBBLE);
}

sword SwimAccel(sword speed, sword low) {
  if (speed > 12) return -3;
  if (speed > low) return -1;
  if (speed > -12) return 1;
  return 3;
}

// 0E8F:0E17
void KeenSwimThink(objtype* ob) {
  boolean wasleft = ob->xspeed < 0;
  boolean wasup = ob->yspeed < 4;

  ob->temp4 = static_cast<sword>(ob->temp4 + tics);
  if (ob->temp4 > 60) SpawnBubble(ob);

  if (jumpbutton && !jumpheld) {
    jumpheld = true;
    if (c.xaxis) ob->xspeed = static_cast<sword>(c.xaxis * 18);
    if (c.yaxis) ob->yspeed = static_cast<sword>(c.yaxis * 18);
    ob->state = ob->state->nextstate;
  }
  if (c.xaxis) ob->xdir = static_cast<sword>(c.xaxis);

  for (longword lt = lasttimecount - tics; lt < lasttimecount; lt++) {
    if ((lt & 7) == 0) {
      sword dx = static_cast<sword>(SwimAccel(ob->xspeed, 0) + c.xaxis * 2);
      ob->xspeed = static_cast<sword>(ob->xspeed + dx);
      if (c.xaxis == 0 && (ob->xspeed < 0) != wasleft) ob->xspeed = 0;

      sword dy = static_cast<sword>(SwimAccel(ob->yspeed, 4) + c.yaxis * 2);
      ob->yspeed = static_cast<sword>(ob->yspeed + dy);
      if (c.yaxis == 0 && ob->yspeed > 4 && wasup) ob->yspeed = 0;
    }
    xtry = static_cast<sword>(xtry + ob->xspeed);
    ytry = static_cast<sword>(ytry + ob->yspeed);
  }
}

// 0E8F:0FAE
void KeenSwimFastThink(objtype* ob) {
  ob->temp4 = static_cast<sword>(ob->temp4 + tics);
  if (ob->temp4 > 60) SpawnBubble(ob);

  if (jumpbutton && !jumpheld) {
    jumpheld = true;
    ob->xspeed = static_cast<sword>(c.xaxis * 18);
    if (c.yaxis) ob->yspeed = static_cast<sword>(c.yaxis * 18);
    if (ob->state == ST(0x1A28))
      ob->state = ST(0x1A46);
    else
      ob->state = ST(0x1A28);
  }

  xtry = static_cast<sword>(xtry + ob->xspeed * static_cast<sword>(tics));
  ytry = static_cast<sword>(ytry + ob->yspeed * static_cast<sword>(tics));
  if (xtry > 0)
    ob->xdir = 1;
  else if (xtry < 0)
    ob->xdir = -1;
  ytry = static_cast<sword>(ytry + tics * 4);
}

// 0E8F:1061
void KeenSwimContact(objtype*, objtype* hit) {
  switch (hit->obclass) {
    case bonusobj: {
      word type = static_cast<word>(hit->temp1);
      if (type > 11) break;
      SD_PlaySound(g_exe.Int(static_cast<std::uint16_t>(0x13DC + type * 2)));
      hit->obclass = inertobj;
      hit->priority = 3;
      hit->shapenum = g_exe.Word(static_cast<std::uint16_t>(0x140C + type * 2));
      GivePoints(g_exe.Word(static_cast<std::uint16_t>(0x13F4 + type * 2)));
      if (type < 4)
        gamestate.keys[type]++;
      else if (type == 10)
        gamestate.lives++;
      else if (type == 11)
        gamestate.ammo = static_cast<sword>(
            gamestate.ammo + g_exe.Int(static_cast<std::uint16_t>(0x13D4 + gamestate.difficulty * 2)));
      ChangeState(hit, ST(0x1F28));
      break;
    }
    case oracleobj:
      playstate = ex_rescued;
      break;
    default:
      break;
  }
}

// 0E8F:112F
void KeenSwimReact(objtype* ob) {
  if ((ob->hiteast && ob->xspeed < 0) || (ob->hitwest && ob->xspeed > 0)) ob->xspeed = 0;
  if ((ob->hitnorth && ob->yspeed > 0) || (ob->hitsouth && ob->yspeed < 0)) ob->yspeed = 0;
  RF_PlaceSprite(&ob->sprite, ob->x, ob->y, ob->shapenum, spritedraw, ob->priority);
}

// 0E8F:0CFE
void SpawnSwimKeen(word tilex, word tiley) {
  player->obclass = keenobj;
  player->active = ac_allways;
  player->priority = 1;
  player->x = static_cast<word>(tilex << G_T_SHIFT);
  player->y = static_cast<word>(tiley << G_T_SHIFT);
  player->xdir = player->ydir = 1;
  player->needtoclip = cl_fullclip;
  NewState(player, ST(0x19EC));
}

}  // namespace

// Actor spawners (K4_ACT1..3).
void SpawnMiragia(word tilex, word tiley);
void SpawnBonus(word tilex, word tiley, word type);
void SpawnCouncil(word tilex, word tiley);
void SpawnSlug(word tilex, word tiley);
void SpawnMadMushroom(word tilex, word tiley);
void SpawnEgg(word tilex, word tiley);
void SpawnEggbird(word tilex, word tiley);
void SpawnArachnut(word tilex, word tiley);
void SpawnSkypest(word tilex, word tiley);
void SpawnWormMouth(word tilex, word tiley);
void SpawnCloud(word tilex, word tiley);
void SpawnBerkeloid(word tilex, word tiley);
void SpawnInchworm(word tilex, word tiley);
void SpawnFoot(word tilex, word tiley);
void SpawnBounder(word tilex, word tiley);
void SpawnLick(word tilex, word tiley);
void SpawnPlatform(word tilex, word tiley, sword dir);
void SpawnDropPlatform(word tilex, word tiley);
void SpawnEater(word tilex, word tiley);
void SpawnMimrock(word tilex, word tiley);
void SpawnDopefish(word tilex, word tiley);
void SpawnSchoolfish(word tilex, word tiley);
void SpawnPixie(word tilex, word tiley);
void SpawnMine(word tilex, word tiley, sword dir);
void SpawnLindsey(word tilex, word tiley);
void SpawnDartShooter(word tilex, word tiley, sword dir);
void SpawnScuba(word tilex, word tiley);

// 0E8F:0003
void ScanInfoPlane() {
  InitObjArray();
  std::memset(lumpneeded, 0, sizeof(lumpneeded));

  const word* info = mapsegs[2];
  const sword diff = gamestate.difficulty;
  for (word y = 0; y < mapheight; y++) {
    for (word x = 0; x < mapwidth; x++) {
      word tile = *info++;
      if (!tile) continue;
      switch (tile) {
        case 1:
          SpawnKeen(x, y, 1);
          SpawnScore();
          lumpneeded[KEENLUMP] = true;
          MarkKeenSprites();
          break;
        case 2:
          SpawnKeen(x, y, -1);
          SpawnScore();
          lumpneeded[KEENLUMP] = true;
          MarkKeenSprites();
          break;
        case 3:
          SpawnWorldKeen(x, y);
          SpawnScore();
          lumpneeded[WORLDKEENLUMP] = true;
          MarkKeenSprites();
          break;
        case 4:
          SpawnCouncil(x, y);
          lumpneeded[COUNCILLUMP] = true;
          break;

        case 50:
          if (diff < 3) break;
          [[fallthrough]];
        case 49:
          if (diff < 2) break;
          [[fallthrough]];
        case 5:
          SpawnBerkeloid(x, y);
          lumpneeded[BERKELOIDLUMP] = true;
          break;

        case 6:
          SpawnLindsey(x, y);
          lumpneeded[LINDSEYLUMP] = true;
          break;

        case 52:
          if (diff < 3) break;
          [[fallthrough]];
        case 51:
          if (diff < 2) break;
          [[fallthrough]];
        case 7:
          SpawnWormMouth(x, y);
          lumpneeded[WORMOUTHLUMP] = true;
          break;

        case 46:
          if (diff < 3) break;
          [[fallthrough]];
        case 45:
          if (diff < 2) break;
          [[fallthrough]];
        case 8:
          SpawnSkypest(x, y);
          lumpneeded[SKYPESTLUMP] = true;
          break;

        case 9:
          SpawnCloud(x, y);
          lumpneeded[CLOUDLUMP] = true;
          break;
        case 10:
          SpawnFoot(x, y);
          lumpneeded[INCHWORMLUMP] = true;
          MarkSmoke();
          break;
        case 11:
          SpawnInchworm(x, y);
          lumpneeded[INCHWORMLUMP] = true;
          MarkSmoke();
          break;
        case 12:
          SpawnBounder(x, y);
          lumpneeded[BOUNDERLUMP] = true;
          break;
        case 13:
          SpawnEgg(x, y);
          lumpneeded[EGGLUMP] = true;
          lumpneeded[EGGBIRDLUMP] = true;
          break;

        case 48:
          if (diff < 3) break;
          [[fallthrough]];
        case 47:
          if (diff < 2) break;
          [[fallthrough]];
        case 14:
          SpawnLick(x, y);
          lumpneeded[LICKLUMP] = true;
          break;

        case 88:
          if (diff < 3) break;
          [[fallthrough]];
        case 87:
          if (diff < 2) break;
          [[fallthrough]];
        case 15:
          SpawnDopefish(x, y);
          lumpneeded[DOPEFISHLUMP] = true;
          break;

        case 16:
          SpawnSchoolfish(x, y);
          lumpneeded[SCHOOLFISHLUMP] = true;
          break;

        case 24:
          if (diff < 3) break;
          [[fallthrough]];
        case 23:
          if (diff < 2) break;
          [[fallthrough]];
        case 17:
          SpawnPixie(x, y);
          lumpneeded[SPRITELUMP] = true;
          break;

        case 18:
          SpawnEater(x, y);
          lumpneeded[EATERLUMP] = true;
          break;
        case 19:
          SpawnMimrock(x, y);
          lumpneeded[MIMROCKLUMP] = true;
          break;

        case 74:
          if (diff < 3) break;
          [[fallthrough]];
        case 73:
          if (diff < 2) break;
          [[fallthrough]];
        case 20:
          SpawnArachnut(x, y);
          lumpneeded[ARACHNUTLUMP] = true;
          break;

        case 21:
          SpawnMadMushroom(x, y);
          lumpneeded[MUSHROOMLUMP] = true;
          break;

        case 44:
          if (diff < 3) break;
          [[fallthrough]];
        case 43:
          if (diff < 2) break;
          [[fallthrough]];
        case 22:
          SpawnSlug(x, y);
          lumpneeded[SLUGLUMP] = true;
          break;

        case 25:
          RF_SetScrollBlock(x, y, true);
          break;
        case 26:
          RF_SetScrollBlock(x, y, false);
          break;
        case 27:
        case 28:
        case 29:
        case 30:
          SpawnPlatform(x, y, static_cast<sword>(tile - 27));
          lumpneeded[PLATFORMLUMP] = true;
          break;
        case 32:
          SpawnDropPlatform(x, y);
          lumpneeded[PLATFORMLUMP] = true;
          break;
        case 33:
          SpawnMiragia(x, y);
          break;
        case 34:
          if (gamestate.ammo < 5) {
            SpawnBonus(x, y, 11);
            lumpneeded[bonuslump(11)] = true;
          }
          break;
        case 35:
          SpawnScuba(x, y);
          MarkChunk(430);
          break;
        case 42:
          SpawnSwimKeen(x, y);
          SpawnScore();
          lumpneeded[SCUBAKEENLUMP] = true;
          for (int i = 219; i <= 226; i++) MarkChunk(i);
          MarkKeenSprites();
          break;

        case 53:
        case 54:
        case 55:
        case 56:
          SpawnDartShooter(x, y, static_cast<sword>(tile - 53));
          lumpneeded[DARTLUMP] = true;
          break;
        case 79:
        case 80:
        case 81:
        case 82:
          if (diff < 2) break;
          SpawnDartShooter(x, y, static_cast<sword>(tile - 79));
          lumpneeded[DARTLUMP] = true;
          break;
        case 83:
        case 84:
        case 85:
        case 86:
          if (diff < 3) break;
          SpawnDartShooter(x, y, static_cast<sword>(tile - 83));
          lumpneeded[DARTLUMP] = true;
          break;

        case 57: case 58: case 59: case 60: case 61: case 62:
        case 63: case 64: case 65: case 66: case 67: case 68:
          SpawnBonus(x, y, static_cast<word>(tile - 57));
          lumpneeded[bonuslump(tile - 57)] = true;
          break;

        case 69:
        case 70:
        case 71:
        case 72:
          SpawnMine(x, y, static_cast<sword>(tile - 69));
          lumpneeded[MINELUMP] = true;
          break;

        case 75:
          lumpneeded[LUMP35] = true;
          break;

        case 78:
          if (diff < 3) break;
          [[fallthrough]];
        case 77:
          if (diff < 2) break;
          [[fallthrough]];
        case 76:
          SpawnEggbird(x, y);
          lumpneeded[EGGLUMP] = true;
          break;

        default:
          break;
      }
    }
  }

  for (objtype* ob = player; ob; ob = ob->next) {
    if (ob->active != ac_allways) ob->active = ac_no;
  }

  for (int i = 0; i < NUMLUMPS; i++) {
    if (lumpneeded[i]) {
      for (word j = lumpstart(i); j <= lumpend(i); j++) MarkChunk(j);
    }
  }
}

// 0E8F:0645
void LindseyDialog() {
  SD_WaitSoundDone();
  StopMusic();
  CA_UpLevel();
  MarkChunk(LINDSEYPIC);
  MarkChunk(KEENTALK1PIC);
  MarkChunk(KEENTALK2PIC);
  CA_CacheMarks(nullptr);
  VW_FixRefreshBuffer();

  OtherTalkWindow(LINDSEYPIC);
  US_CPrint(g_exe.String(0x1BB5));  // "Princess Lindsey says:\n"
  US_CPrint(DgString(mapon == 7 ? 0x19D4 : 0x19D6));
  VW_UpdateScreen();
  SD_PlaySound(SND_LINDSEY);
  VW_WaitVBL(60);
  IN_ClearKeysDown();
  IN_Ack();

  KeenTalkWindow(KEENTALK1PIC);
  US_CPrint(DgString(mapon == 7 ? 0x19D8 : 0x19DA));
  WaitAck(30);

  VWB_DrawPic(WindowX + WindowW, WindowY, KEENTALK2PIC);
  WaitAck(30);

  CA_DownLevel();
  StartMusic(gamestate.mapon);

  // force the score box to redraw
  scoreobj->temp2 = -1;
  scoreobj->temp1 = -1;
  scoreobj->temp3 = -1;
  scoreobj->temp4 = -1;
}

// 0E8F:07D9: the janitor who looks like a council member.
void OracleCouncilMember() {
  SD_WaitSoundDone();
  CA_UpLevel();
  MarkChunk(COUNCILPIC);
  MarkChunk(KEENTALK1PIC);
  MarkChunk(KEENMADPIC);
  CA_CacheMarks(nullptr);
  VW_FixRefreshBuffer();
  StartMusic(-1);

  OtherTalkWindow(COUNCILPIC);
  US_CPrint(FarString(0x1F58));
  WaitAck(60);

  OtherTalkWindow(COUNCILPIC);
  US_CPrint(FarString(0x1F5E));
  WaitAck(60);

  KeenTalkWindow(KEENTALK1PIC);
  US_CPrint(FarString(0x1F63));
  WaitAck(60);

  OtherTalkWindow(COUNCILPIC);
  US_CPrint(FarString(0x1F65));
  WaitAck(60);

  US_CenterWindow(26, 8);
  VWB_DrawPic(WindowX + WindowW - 48, WindowY, KEENTALK1PIC);
  VWB_DrawPic(WindowX + WindowW - 40, WindowY + 24, KEENMADPIC);
  WaitAck(30);

  StopMusic();
  CA_DownLevel();
  StartMusic(gamestate.mapon);
}

// 0E8F:0A66
void CantSwim() {
  SD_WaitSoundDone();
  CA_UpLevel();
  CA_CacheGrChunk(KEENTALK1PIC);
  KeenTalkWindow(KEENTALK1PIC);
  US_CPrint(g_exe.String(0x1BCD));  // "I can't swim!"
  WaitAck(30);
  CA_DownLevel();
}

// 0E8F:0AE2
void GotScuba() {
  SD_WaitSoundDone();
  CA_UpLevel();
  MarkChunk(KEENTALK1PIC);
  MarkChunk(KEENTALK2PIC);
  CA_CacheMarks(nullptr);
  KeenTalkWindow(KEENTALK1PIC);
  US_CPrint(g_exe.String(0x1BDB));  // "Cool!  I can breathe under water now!"
  WaitAck(30);
  VWB_DrawPic(WindowX + WindowW, WindowY, KEENTALK2PIC);
  WaitAck(30);
  CA_DownLevel();
}

// 0E8F:0BA5
void RescuedMember() {
  SD_WaitSoundDone();
  CA_UpLevel();
  MarkChunk(COUNCILPIC);
  MarkChunk(KEENTALK1PIC);
  MarkChunk(KEENTALK2PIC);
  CA_CacheMarks(nullptr);
  StartMusic(-1);
  VW_FixRefreshBuffer();

  OtherTalkWindow(COUNCILPIC);
  // The member rescued in the underwater level speaks through bubbles.
  US_CPrint(g_exe.String(mapon == 17 ? 0x1D43 : 0x1DA7));
  WaitAck(60);

  KeenTalkWindow(KEENTALK1PIC);
  US_CPrint(DgString(static_cast<word>(0x19DC + gamestate.rescued * 2)));
  WaitAck(30);

  VWB_DrawPic(WindowX + WindowW, WindowY, KEENTALK2PIC);
  WaitAck(30);

  gamestate.rescued++;
  CA_DownLevel();
  StopMusic();
}

CK_FUNC(0x0E8F, 0x0E17, KeenSwimThink);
CK_FUNC(0x0E8F, 0x0FAE, KeenSwimFastThink);
CK_FUNC(0x0E8F, 0x1061, KeenSwimContact);
CK_FUNC(0x0E8F, 0x112F, KeenSwimReact);
