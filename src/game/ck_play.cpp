// CK_PLAY: the play loop, objects, controls, music and the status window
// (segment 06BD).
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "exe/keen4exe.h"
#include "game/ck_def.h"
#include "id/id_sys.h"

gametype gamestate;
exittype playstate;

objtype objarray[MAXACTORS + 1];
objtype* lastobj;
objtype* objfreelist;
objtype* player;
objtype* scoreobj;
objtype* newobj;
objtype dummyobj;
sword objectcount;

ControlInfo c;
boolean jumpbutton, jumpheld, pogobutton, pogoheld, firebutton, fireheld, upheld;
sword oldfirecount;
boolean showscorebox = true, jerk, oldshooting, QuietFX;
byte firescan = sc_Space;
byte gamepadflag;
boolean GravisGamepad;
word GravisMap[4];
boolean godmode, jumpcheat, singlestep, debugok, storedemo;
boolean joypad;
sword extravbls;
sword invincible;
boolean lockscroll;
boolean scorescreenkludge;
word originxtilemax, originytilemax;
word inactivateleft, inactivateright, inactivatetop, inactivatebottom;
sword lastlevel;
word centerlevel;

void SizeText(const char* text, word* width, word* height);  // CK_MAIN
void ScanInfoPlane();

namespace {

using keen4::exe::g_exe;

// Status window scrolling (CK_PLAY DGROUP A7F3..A7F7).
sword vislines;
boolean scrollup;
byte statusbuffer[SCREENWIDTH * SCREENHEIGHT];

char str[80];

void DebugMemory() {
  US_CenterWindow(16, 7);
  US_CPrint("Memory Usage");
  US_CPrint("------------");
  US_Print("Total     :");
  US_PrintUnsigned(640);
  US_Print("k\nFree      :");
  US_PrintUnsigned(640);
  US_Print("k\nWith purge:");
  US_PrintUnsigned(640);
  US_Print("k\n");
  VW_UpdateScreen();
  IN_Ack();
}

// 06BD:000B
void CountObjects() {
  word active = 0, inactive = 0;
  for (objtype* ob = player; ob; ob = ob->next) {
    if (ob->active)
      active++;
    else
      inactive++;
  }
  US_CenterWindow(18, 4);
  PrintY = static_cast<word>(PrintY + 7);
  US_Print("Active Objects :");
  US_PrintUnsigned(active);
  US_Print("\nInactive Objects:");
  US_PrintUnsigned(inactive);
  VW_UpdateScreen();
  IN_Ack();
}

// 06BD:07DC
boolean DebugKeys() {
  if (Keyboard[sc_B] && ingame) {
    US_CenterWindow(24, 3);
    PrintY = static_cast<word>(PrintY + 6);
    US_Print(" Border color (0-15):");
    VW_UpdateScreen();
    char buf[8];
    if (US_LineInput(px, py, buf, nullptr, true, 2, 0)) {
      int level = std::atoi(buf);
      if (level >= 0 && level <= 15) VW_ColorBorder(level);
    }
    return true;
  }
  if (Keyboard[sc_C] && ingame) {
    CountObjects();
    return true;
  }
  if (Keyboard[sc_D] && ingame) {
    if (DemoMode == demo_Off) {
      StartDemoRecord();
    } else if (DemoMode == demo_Record) {
      EndDemoRecord();
      playstate = ex_completed;
    }
    return true;
  }
  if (Keyboard[sc_E] && ingame) {
    if (tedlevel) TEDDeath();
    playstate = ex_completed;
  }
  if (Keyboard[sc_G] && ingame) {
    US_CenterWindow(12, 2);
    if (godmode)
      US_PrintCentered("God mode OFF");
    else
      US_PrintCentered("God mode ON");
    VW_UpdateScreen();
    IN_Ack();
    godmode ^= true;
    return true;
  }
  if (Keyboard[sc_I]) {
    US_CenterWindow(12, 3);
    US_PrintCentered("Free items!");
    for (int i = 0; i < 4; i++) gamestate.keys[i] = 99;
    gamestate.ammo = 99;
    gamestate.wetsuit = true;
    VW_UpdateScreen();
    IN_Ack();
    GivePoints(3000);
    return true;
  }
  if (Keyboard[sc_J]) {
    jumpcheat ^= true;
    US_CenterWindow(18, 3);
    if (jumpcheat)
      US_PrintCentered("Jump cheat ON");
    else
      US_PrintCentered("Jump cheat OFF");
    VW_UpdateScreen();
    IN_Ack();
    return true;
  }
  if (Keyboard[sc_M]) {
    DebugMemory();
    return true;
  }
  if (Keyboard[sc_N]) {
    US_CenterWindow(18, 3);
    if (player->needtoclip) {
      US_PrintCentered("No clipping ON");
      player->needtoclip = cl_noclip;
    } else {
      US_PrintCentered("No clipping OFF");
      player->needtoclip = cl_midclip;
    }
    VW_UpdateScreen();
    IN_Ack();
    return true;
  }
  if (Keyboard[sc_S] && ingame) {
    singlestep ^= true;
    US_CenterWindow(18, 3);
    if (singlestep)
      US_PrintCentered("Slow motion ON");
    else
      US_PrintCentered("Slow motion OFF");
    VW_UpdateScreen();
    IN_Ack();
    return true;
  }
  if (Keyboard[sc_V]) {
    US_CenterWindow(30, 3);
    PrintY = static_cast<word>(PrintY + 6);
    US_Print("  Add how many extra VBLs(0-8):");
    VW_UpdateScreen();
    char buf[8];
    if (US_LineInput(px, py, buf, nullptr, true, 2, 0)) {
      int level = std::atoi(buf);
      if (level >= 0 && level <= 8) extravbls = static_cast<sword>(level);
    }
    return true;
  }
  if (Keyboard[sc_W] && ingame) {
    US_CenterWindow(26, 3);
    PrintY = static_cast<word>(PrintY + 6);
    US_Print("  Warp to which level(1-18):");
    VW_UpdateScreen();
    char buf[8];
    if (US_LineInput(px, py, buf, nullptr, true, 2, 0)) {
      int level = std::atoi(buf);
      if (level > 0 && level <= 18) {
        gamestate.mapon = static_cast<sword>(level);
        playstate = ex_warped;
      }
    }
    return true;
  }
  if (Keyboard[sc_Z]) {
    gamestate.lives = 0;
    KillKeen();
    return false;
  }
  return false;
}

// 06BD:0C60: hold B, A and T.
void CheatKeys() {
  for (int i = sc_A; i <= sc_Z; i++) {
    if (i == sc_B || i == sc_A || i == sc_T) continue;
    if (Keyboard[i]) return;
  }
  US_CenterWindow(20, 7);
  PrintY = static_cast<word>(PrintY + 2);
  US_CPrint("Cheat Option!\n\nYou just got all\nthe keys, 99 shots,\nand an extra keen!");
  VW_UpdateScreen();
  IN_Ack();
  RF_ForceRefresh();
  gamestate.ammo = 99;
  gamestate.lives++;
  gamestate.keys[0] = gamestate.keys[1] = gamestate.keys[2] = gamestate.keys[3] = 1;
}

// 06BD:0FA2: right-aligned number in status window digits.
void DrawStatusNumber(int x, int y, int length, int blanktile, std::int32_t number) {
  char buffer[16];
  std::snprintf(buffer, sizeof buffer, "%ld", static_cast<long>(number));
  int len = static_cast<int>(std::strlen(buffer));
  int i;
  for (i = length; i > len; i--) {
    VWB_DrawTile8(x, y, blanktile);
    x += 8;
  }
  while (i > 0) {
    VWB_DrawTile8(x, y, blanktile + buffer[len - i] - '/');
    i--;
    x += 8;
  }
}

// 06BD:1021
void DrawStatusWindow() {
  int x = 64, y = 16, w = 184, h = 144;
  VWB_DrawTile8(x, y, 54);
  VWB_DrawTile8(x, y + h, 60);
  int i;
  for (i = x + 8; i <= x + w - 8; i += 8) {
    VWB_DrawTile8(i, y, 55);
    VWB_DrawTile8(i, y + h, 61);
  }
  VWB_DrawTile8(i, y, 56);
  VWB_DrawTile8(i, y + h, 62);
  for (i = y + 8; i <= y + h - 8; i += 8) {
    VWB_DrawTile8(x, i, 57);
    VWB_DrawTile8(x + w, i, 59);
  }
  VWB_Bar(72, 24, 176, 136, 7);

  PrintY = 28;
  WindowX = 80;
  WindowW = 160;
  US_CPrint("LOCATION");
  VWB_Bar(79, 38, 162, 20, 15);
  std::strncpy(str, LevelName(gamestate.mapon), sizeof str - 1);
  word tw, th;
  SizeText(str, &tw, &th);
  PrintY = static_cast<word>(static_cast<word>(20 - th) / 2 + 40 - 2);
  US_CPrint(str);

  PrintY = 61;
  WindowX = 80;
  WindowW = 64;
  US_CPrint("SCORE");
  VWB_Bar(79, 71, 66, 10, 0);
  DrawStatusNumber(80, 72, 8, 41, gamestate.score);

  PrintY = 61;
  WindowX = 176;
  WindowW = 64;
  US_CPrint("EXTRA");
  VWB_Bar(175, 71, 66, 10, 0);
  DrawStatusNumber(176, 72, 8, 41, gamestate.nextextra);

  PrintY = 85;
  WindowX = 80;
  WindowW = 64;
  US_CPrint("RESCUED");
  VWB_Bar(79, 95, 66, 10, 0);
  for (i = 0; i < gamestate.rescued; i++) VWB_DrawTile8(80 + i * 8, 96, 40);

  PrintY = 85;
  WindowX = 176;
  WindowW = 64;
  US_CPrint("LEVEL");
  VWB_Bar(175, 95, 66, 10, 15);
  PrintY = 96;
  WindowX = 176;
  WindowW = 64;
  switch (gamestate.difficulty) {
    case gd_Easy: US_CPrint("Easy"); break;
    case gd_Normal: US_CPrint("Normal"); break;
    case gd_Hard: US_CPrint("Hard"); break;
    default: break;
  }

  PrintX = 80;
  PrintY = 112;
  US_Print("KEYS");
  VWB_Bar(119, 111, 34, 10, 0);
  for (i = 0; i < 4; i++)
    if (gamestate.keys[i]) VWB_DrawTile8(120 + i * 8, 112, 36 + i);

  PrintX = 176;
  PrintY = 112;
  US_Print("AMMO");
  VWB_Bar(215, 111, 26, 10, 0);
  DrawStatusNumber(216, 112, 3, 41, gamestate.ammo);

  PrintX = 80;
  PrintY = 128;
  US_Print("KEENS");
  VWB_Bar(127, 127, 18, 10, 0);
  DrawStatusNumber(128, 128, 2, 41, gamestate.lives);

  PrintX = 176;
  PrintY = 128;
  US_Print("DROPS");
  VWB_Bar(224, 127, 16, 10, 0);
  DrawStatusNumber(224, 128, 2, 41, gamestate.drops);

  VWB_Bar(79, 143, 66, 10, 15);
  PrintY = 144;
  WindowX = 80;
  WindowW = 64;
  if (gamestate.wetsuit)
    US_CPrint("Wetsuit");
  else
    US_CPrint("???");

  for (i = 0; i < 10; i++) {
    VWB_DrawTile8(160 + i * 8, 140, 72 + i);
    VWB_DrawTile8(160 + i * 8, 148, 82 + i);
  }
}

// Copies `rows` rows of the 192-pixel-wide status column (x 64..255).
void CopyStatusRows(int srcrow, int destrow, int rows, int x = 64, int width = 192) {
  for (int r = 0; r < rows; r++) {
    int sy = srcrow + r, dy = destrow + r;
    if (sy < 0 || sy >= SCREENHEIGHT || dy < 0 || dy >= SCREENHEIGHT) continue;
    std::memcpy(vw_framebuffer + dy * SCREENWIDTH + x, statusbuffer + sy * SCREENWIDTH + x,
                static_cast<std::size_t>(width));
  }
}

// 06BD:1523: refresh hook that slides the status window down from the top.
// The window was drawn into statusbuffer at rows 16..160.
void ScrollStatusWindow() {
  int source, dest, rows;
  if (vislines > 152) {
    rows = vislines - 152;
    CopyStatusRows(0, 0, rows);
    VW_ClipDrawMPic(17, -(16 - rows), 123);
    source = 16;
    dest = rows;
    rows = 152;
  } else {
    source = 168 - vislines;
    dest = 0;
    rows = vislines;
  }
  if (rows > 0) CopyStatusRows(source, dest, rows);

  if (!scrollup) {
    rows = vislines - 72;
    if (rows > 0) CopyStatusRows(0, 0, rows, 40, 24);
  }
  if (vislines >= 72) VW_ClipDrawMPic(5, vislines - 168, 122);
  VW_UpdateScreen();
}

// 06BD:1983
void WorldScrollScreen(objtype* ob) {
  if (lockscroll) return;
  sword xscroll, yscroll;
  if (ob->left < static_cast<word>(originxglobal + 9 * TILEGLOBAL))
    xscroll = static_cast<sword>(ob->left - (originxglobal + 9 * TILEGLOBAL));
  else if (ob->right > static_cast<word>(originxglobal + 12 * TILEGLOBAL))
    xscroll = static_cast<sword>(ob->right + 16 - (originxglobal + 12 * TILEGLOBAL));
  else
    xscroll = 0;

  if (ob->top < static_cast<word>(originyglobal + 5 * TILEGLOBAL))
    yscroll = static_cast<sword>(ob->top - (originyglobal + 5 * TILEGLOBAL));
  else if (ob->bottom > static_cast<word>(originyglobal + 7 * TILEGLOBAL))
    yscroll = static_cast<sword>(ob->bottom - (originyglobal + 7 * TILEGLOBAL));
  else
    yscroll = 0;

  if (!xscroll && !yscroll) return;

  if (xscroll >= 0x100)
    xscroll = 0xFF;
  else if (xscroll <= -0x100)
    xscroll = -0xFF;
  if (yscroll >= 0x100)
    yscroll = 0xFF;
  else if (yscroll <= -0x100)
    yscroll = -0xFF;

  RF_Scroll(xscroll, yscroll);
  originxtilemax = static_cast<word>(originxtile + PORTTILESWIDE - 1);
  originytilemax = static_cast<word>(originytile + PORTTILESHIGH - 1);
  inactivateleft = static_cast<word>(originxtile - 4);
  if (static_cast<sword>(inactivateleft) < 0) inactivateleft = 0;
  inactivateright = static_cast<word>(originxtilemax + 4);
  if (static_cast<sword>(inactivateright) < 0) inactivateright = 0;
  inactivatetop = static_cast<word>(originytile - 4);
  if (static_cast<sword>(inactivatetop) < 0) inactivatetop = 0;
  inactivatebottom = static_cast<word>(originytilemax + 4);
  if (static_cast<sword>(inactivatebottom) < 0) inactivatebottom = 0;
}

// Keen states the camera treats specially (DGROUP addresses).
constexpr word s_keenlookup2 = 0x0C20;
constexpr word s_keenlookdown2 = 0x0C7A;
constexpr word s_keenpoledown = 0x12EC;
constexpr word s_keenonpole_a = 0x1328;
constexpr word s_keenonpole_b = 0x1346;
constexpr word s_keenonpole_c = 0x1364;
constexpr word s_keenonpole_d = 0x1382;

// 06BD:1ACD
void ScrollScreen(objtype* ob) {
  if (lockscroll) return;

  // walked off the edge of the map
  if (ob->left < originxmin || ob->right > static_cast<word>(originxmax + 20 * TILEGLOBAL)) {
    playstate = ex_completed;
    return;
  }

  // fell off the bottom of the map
  if (ob->bottom > static_cast<word>(originymax + 13 * TILEGLOBAL)) {
    ob->y = static_cast<word>(ob->y - (ob->bottom - (originymax + 13 * TILEGLOBAL)));
    SD_PlaySound(0x14);  // SND_PLUMMET
    godmode = false;
    KillKeen();
    return;
  }

  sword xscroll = 0, yscroll = 0;
  if (ob->x < static_cast<word>(originxglobal + 9 * TILEGLOBAL))
    xscroll = static_cast<sword>(ob->x - (originxglobal + 9 * TILEGLOBAL));
  else if (ob->x > static_cast<word>(originxglobal + 12 * TILEGLOBAL))
    xscroll = static_cast<sword>(ob->x - (originxglobal + 12 * TILEGLOBAL));

  sword pix;
  if (ob->state == ST(s_keenlookup2)) {
    if (centerlevel + tics > 167)
      pix = static_cast<sword>(167 - centerlevel);
    else
      pix = static_cast<sword>(tics);
    centerlevel = static_cast<word>(centerlevel + pix);
    yscroll = static_cast<sword>(-(pix << 4));
  } else if (ob->state == ST(s_keenlookdown2)) {
    if (static_cast<word>(centerlevel - tics) < 33)
      pix = static_cast<sword>(centerlevel - 33);
    else
      pix = static_cast<sword>(tics);
    centerlevel = static_cast<word>(centerlevel - pix);
    yscroll = static_cast<sword>(pix << 4);
  }

  if (ob->hitnorth || !ob->needtoclip || ob->state == ST(s_keenpoledown)) {
    if (ob->state != ST(s_keenonpole_a) && ob->state != ST(s_keenonpole_b) &&
        ob->state != ST(s_keenonpole_c) && ob->state != ST(s_keenonpole_d)) {
      yscroll = static_cast<sword>(yscroll + ob->ymove);
      word bottom = static_cast<word>(originyglobal + yscroll + (centerlevel << 4));
      if (ob->bottom != bottom) {
        sword delta;
        if (ob->bottom < bottom)
          delta = static_cast<sword>(bottom - ob->bottom);
        else
          delta = static_cast<sword>(ob->bottom - bottom);
        sword speed = static_cast<sword>(static_cast<sword>(delta << 4) >> 7);
        if (speed > 48) speed = 48;
        speed = static_cast<sword>(speed * tics);
        if (speed < 16) {
          if (delta < 16)
            speed = delta;
          else
            speed = 16;
        }
        if (ob->bottom < bottom)
          yscroll = static_cast<sword>(yscroll - speed);
        else
          yscroll = static_cast<sword>(yscroll + speed);
      }
    }
  } else {
    centerlevel = 140;
  }

  // keep Keen on screen vertically
  sword delta = static_cast<sword>(ob->bottom - 2 * TILEGLOBAL - (originyglobal + yscroll));
  if (delta < 0) yscroll = static_cast<sword>(yscroll + delta);
  delta = static_cast<sword>(ob->bottom + 2 * TILEGLOBAL - (originyglobal + yscroll + 200 * PIXGLOBAL));
  if (delta > 0) yscroll = static_cast<sword>(yscroll + delta);

  if (!xscroll && !yscroll) return;

  if (xscroll >= 0x100)
    xscroll = 0xFF;
  else if (xscroll <= -0x100)
    xscroll = -0xFF;
  if (yscroll >= 0x100)
    yscroll = 0xFF;
  else if (yscroll <= -0x100)
    yscroll = -0xFF;

  RF_Scroll(xscroll, yscroll);
  originxtilemax = static_cast<word>(originxtile + PORTTILESWIDE - 1);
  originytilemax = static_cast<word>(originytile + PORTTILESHIGH - 1);
  inactivateleft = static_cast<word>(originxtile - 4);
  if (static_cast<sword>(inactivateleft) < 0) inactivateleft = 0;
  inactivateright = static_cast<word>(originxtilemax + 4);
  if (static_cast<sword>(inactivateright) < 0) inactivateright = 0;
  inactivatetop = static_cast<word>(originytile - 4);
  if (static_cast<sword>(inactivatetop) < 0) inactivatetop = 0;
  inactivatebottom = static_cast<word>(originytilemax + 4);
  if (static_cast<sword>(inactivatebottom) < 0) inactivatebottom = 0;
}

// 06BD:0CD5
void CheckKeys() {
  if (screenfaded) return;  // don't do anything with a faded screen

  // Enter: status window
  if (Keyboard[sc_Enter]) {
    StatusWindow();
    IN_ClearKeysDown();
    RF_ForceRefresh();
    lasttimecount = TimeCount;
  }

  // pause key
  if (Paused) {
    SD_MusicOff();
    US_CenterWindow(8, 3);
    US_PrintCentered("PAUSED");
    VW_UpdateScreen();
    IN_Ack();
    RF_ForceRefresh();
    Paused = false;
    SD_MusicOn();
  }

  // F1: help
  if (LastScan == sc_F1) {
    StopMusic();
    HelpScreens();
    StartMusic(gamestate.mapon);
    if (showscorebox) scoreobj->temp2 = scoreobj->temp1 = scoreobj->temp3 = scoreobj->temp4 = -1;
    RF_ForceRefresh();
  }

  if (!storedemo) {
    // F2-F7 or Escape: control panel
    if ((LastScan >= sc_F2 && LastScan <= sc_F7) || LastScan == sc_Escape) {
      StopMusic();
      US_ControlPanel();
      RF_FixOfs();
      StartMusic(gamestate.mapon);
      if (!showscorebox && scoreobj->sprite) RF_RemoveSprite(&scoreobj->sprite);
      if (showscorebox) scoreobj->temp2 = scoreobj->temp1 = scoreobj->temp3 = scoreobj->temp4 = -1;
      IN_ClearKeysDown();
      if (restartgame) {
        playstate = ex_resetgame;
      } else if (!loadedgame) {
        RF_ForceRefresh();
      }
      if (abortgame) {
        abortgame = false;
        playstate = ex_abortgame;
      }
      if (loadedgame) playstate = ex_loadedgame;
      lasttimecount = TimeCount;
    }

    // F9: the "boss key" showed a fake DOS prompt; the port just pauses.
    if (LastScan == sc_F9) {
      SD_MusicOff();
      IN_ClearKeysDown();
      while (LastScan != sc_Escape) SYS_Idle();
      RF_ForceRefresh();
      IN_ClearKeysDown();
      lasttimecount = TimeCount;
      SD_MusicOn();
    }
  }

  // B-A-T cheat
  if (Keyboard[sc_B] && Keyboard[sc_A] && Keyboard[sc_T]) CheatKeys();

  // F10 + key: debug keys
  if (debugok && Keyboard[sc_F10]) {
    if (DebugKeys()) {
      RF_ForceRefresh();
      lasttimecount = TimeCount;
    }
  }

  // Ctrl-S toggles sound in the store demo build
  if (storedemo && Keyboard[sc_Control] && LastScan == sc_S) {
    if (SoundMode != sdm_Off) {
      SD_SetSoundMode(sdm_Off);
      SD_SetMusicMode(smm_Off);
    } else {
      if (AdLibPresent) {
        SD_SetSoundMode(sdm_AdLib);
        QuietFX = false;
        SD_SetMusicMode(smm_AdLib);
      } else {
        SD_SetSoundMode(sdm_PC);
        SD_SetMusicMode(smm_Off);
      }
      CA_LoadAllSounds();
    }
  }

  // Ctrl-Q quits
  if (Keyboard[sc_Control] && LastScan == sc_Q) {
    IN_ClearKeysDown();
    Quit(nullptr);
  }
}

}  // namespace

// 06BD:16F1
void StatusWindow() {
  WindowX = 0;
  WindowW = 320;
  WindowY = 0;
  WindowH = 200;

  if (Keyboard[sc_A] && Keyboard[sc_2]) {
    US_CenterWindow(20, 2);
    PrintY = static_cast<word>(PrintY + 2);
    US_Print("Debug keys active");
    VW_UpdateScreen();
    IN_Ack();
    debugok = true;
  }

  RF_Refresh();
  RFL_InitAnimList();
  std::memcpy(statusbuffer, vw_framebuffer, sizeof statusbuffer);
  vw_screen = statusbuffer;
  DrawStatusWindow();
  vw_screen = vw_framebuffer;
  RF_Refresh();

  SD_PlaySound(0x22);  // SND_SHOWSTATUS
  vislines = 16;
  scrollup = false;
  RF_SetRefreshHook(ScrollStatusWindow);
  while (true) {
    RF_Refresh();
    if (vislines == 168) break;
    vislines = static_cast<sword>(vislines + tics * 8);
    if (vislines > 168) vislines = 168;
  }

  RF_Refresh();
  RF_SetRefreshHook(nullptr);
  IN_ClearKeysDown();
  IN_Ack();

  SD_PlaySound(0x23);  // SND_HIDESTATUS
  vislines = static_cast<sword>(vislines - 16);
  scrollup = true;
  RF_SetRefreshHook(ScrollStatusWindow);
  while (true) {
    RF_Refresh();
    if (vislines == 0) break;
    vislines = static_cast<sword>(vislines - tics * 8);
    if (vislines < 0) vislines = 0;
  }
  RF_SetRefreshHook(nullptr);

  scoreobj->x = 0;  // force scorebox to redraw if it's not there
}

// 06BD:18A5
void CenterActor(objtype* ob) {
  centerlevel = 140;
  word orgx = ob->x < 152 * PIXGLOBAL ? 0 : static_cast<word>(ob->x - 152 * PIXGLOBAL);
  word orgy;
  if (mapon == 0)
    orgy = ob->y < 80 * PIXGLOBAL ? 0 : static_cast<word>(ob->y - 80 * PIXGLOBAL);
  else
    orgy = ob->bottom < 140 * PIXGLOBAL ? 0 : static_cast<word>(ob->bottom - 140 * PIXGLOBAL);

  if (!scorescreenkludge) RF_NewPosition(orgx, orgy);

  originxtilemax = static_cast<word>(originxtile + PORTTILESWIDE - 1);
  originytilemax = static_cast<word>(originytile + PORTTILESHIGH - 1);
  inactivateleft = static_cast<word>(originxtile - 4);
  if (static_cast<sword>(inactivateleft) < 0) inactivateleft = 0;
  inactivateright = static_cast<word>(originxtilemax + 4);
  if (static_cast<sword>(inactivateright) < 0) inactivateright = 0;
  inactivatetop = static_cast<word>(originytile - 4);
  if (static_cast<sword>(inactivatetop) < 0) inactivatetop = 0;
  inactivatebottom = static_cast<word>(originytilemax + 4);
  if (static_cast<sword>(inactivatebottom) < 0) inactivatebottom = 0;
}

// 06BD:1D9E
void InitObjArray() {
  for (int i = 0; i < MAXACTORS; i++) {
    objarray[i].prev = &objarray[i + 1];
    objarray[i].next = nullptr;
  }
  // The original's last entry points one past the array; the spare entry
  // here ends the free list there.
  objarray[MAXACTORS].prev = nullptr;
  objfreelist = &objarray[0];
  lastobj = nullptr;
  objectcount = 0;

  // give the player and score the first free spots
  GetNewObj(false);
  player = newobj;
  GetNewObj(false);
  scoreobj = newobj;
}

// 06BD:1E11
sword GetNewObj(boolean usedummy) {
  if (!objfreelist) {
    if (usedummy) {
      newobj = &dummyobj;
      return -1;
    }
    Quit("GetNewObj: No free spots in objarray!");
  }
  newobj = objfreelist;
  objfreelist = newobj->prev;
  *newobj = objtype{};
  if (lastobj) lastobj->next = newobj;
  newobj->prev = lastobj;
  newobj->active = ac_yes;
  newobj->needtoclip = cl_midclip;
  lastobj = newobj;
  objectcount++;
  return 0;
}

// 06BD:1E8B
void RemoveObj(objtype* ob) {
  if (ob == player) Quit("RemoveObj: Tried to remove the player!");

  RF_RemoveSprite(&ob->sprite);
  if (ob->obclass == stunnedobj) RF_RemoveSprite(&ob->temp3sprite);

  if (ob == lastobj)
    lastobj = ob->prev;
  else
    ob->next->prev = ob->prev;
  ob->prev->next = ob->next;

  ob->prev = objfreelist;
  objfreelist = ob;
}

// 06BD:1EF2
void GivePoints(word points) {
  gamestate.score += points;
  if (!DemoMode && gamestate.score >= gamestate.nextextra) {
    SD_PlaySound(0x11);  // SND_EXTRAKEEN
    gamestate.lives++;
    gamestate.nextextra *= 2;
  }
}

// 06BD:1F47
void PollControls() {
  IN_ReadControl(0, &c);
  if (c.yaxis != -1) upheld = false;

  if (GravisGamepad && !DemoMode) {
    // Gravis gamepad buttons are not supported by the port.
    jumpbutton = c.button0;
    pogobutton = c.button1;
    firebutton = false;
    if (!jumpbutton) jumpheld = false;
    if (!pogobutton) pogoheld = false;
    if (!firebutton) fireheld = false;
  } else if (oldshooting || DemoMode) {
    if (c.button0 && c.button1) {
      firebutton = true;
      jumpbutton = jumpheld = pogobutton = pogoheld = false;
    } else {
      firebutton = fireheld = false;
      if (c.button0)
        jumpbutton = true;
      else
        jumpbutton = jumpheld = false;
      if (c.button1) {
        if (oldfirecount <= 8)
          oldfirecount = static_cast<sword>(oldfirecount + tics);
        else
          pogobutton = true;
      } else {
        if (oldfirecount)
          pogobutton = true;
        else
          pogobutton = pogoheld = false;
        oldfirecount = 0;
      }
    }
  } else {
    jumpbutton = c.button0;
    pogobutton = c.button1;
    firebutton = Keyboard[firescan];
    if (!jumpbutton) jumpheld = false;
    if (!pogobutton) pogoheld = false;
    if (!firebutton) fireheld = false;
  }
}

// 06BD:2092
void StopMusic() { SD_MusicOff(); }

// 06BD:20C7. DGROUP 0396: song per level, -1 for none.
void StartMusic(sword level) {
  if ((level >= 20 && level != -1)) Quit("StartMusic() - bad level number");
  SD_MusicOff();
  sword song = level == -1 ? 5 : g_exe.Int(static_cast<std::uint16_t>(0x0396 + level * 2));
  if (song == -1 || MusicMode != smm_AdLib) return;
  CA_CacheAudioChunk(STARTMUSIC + song);
  if (audiosegs[STARTMUSIC + song])
    SD_StartMusic(reinterpret_cast<MusicGroup*>(audiosegs[STARTMUSIC + song]));
}

// 06BD:21C6
void PlayLoop() {
  StartMusic(gamestate.mapon);
  jumpheld = upheld = pogoheld = fireheld = false;
  ingame = true;
  playstate = ex_stillplaying;
  oldfirecount = 0;
  lockscroll = false;
  invincible = 0;
  CenterActor(player);

  if (DemoMode)
    US_InitRndT(false);
  else
    US_InitRndT(true);

  tics = 3;
  lasttimecount = 3;
  TimeCount = 3;

  do {
    PollControls();

    // go through state changes and propose movements
    for (objtype* obj = player; obj; obj = obj->next) {
      if (!obj->active && obj->tileright >= static_cast<word>(originxtile - 1) &&
          obj->tileleft <= static_cast<word>(originxtilemax + 1) &&
          obj->tiletop <= static_cast<word>(originytilemax + 1) &&
          obj->tilebottom >= static_cast<word>(originytile - 1)) {
        obj->needtoreact = true;
        obj->active = ac_yes;
      }
      if (obj->active) {
        if (obj->tileright < inactivateleft || obj->tileleft > inactivateright ||
            obj->tiletop > inactivatebottom || obj->tilebottom < inactivatetop) {
          if (obj->active == ac_removable) {
            RemoveObj(obj);
            continue;
          }
          if (obj->active != ac_allways) {
            if (static_cast<word>(US_RndT()) < static_cast<word>(tics * 2) || screenfaded || loadedgame) {
              RF_RemoveSprite(&obj->sprite);
              if (obj->obclass == stunnedobj) RF_RemoveSprite(&obj->temp3sprite);
              obj->active = ac_no;
              continue;
            }
          }
        }
        DoActor(obj);
      }
    }

    if (gamestate.riding) HandleRiding(player);

    // check for and handle collisions between objects
    for (objtype* obj = player; obj; obj = obj->next) {
      if (!obj->active) continue;
      for (objtype* check = obj->next; check; check = check->next) {
        if (!check->active) continue;
        if (obj->right > check->left && obj->left < check->right && obj->top < check->bottom &&
            obj->bottom > check->top) {
          if (obj->state->contact) obj->state->contact(obj, check);
          if (check->state->contact) check->state->contact(check, obj);
          if (obj->obclass == nothing) break;
        }
      }
    }

    // check intiles
    if (mapon)
      CheckInTiles(player);
    else
      CheckWorldInTiles(player);

    // react to whatever happened, and post sprites to the refresh manager
    for (objtype* obj = player; obj; obj = obj->next) {
      if (!obj->active) continue;
      if (obj->tilebottom >= static_cast<word>(mapheight - 1)) {
        if (obj->obclass == keenobj)
          playstate = ex_died;
        else
          RemoveObj(obj);
        continue;
      }
      if (obj->needtoreact && obj->state->react) {
        obj->needtoreact = false;
        obj->state->react(obj);
      }
    }

    // scroll the screen and update the score box
    if (mapon != 0 && mapon != 17)
      ScrollScreen(player);
    else
      WorldScrollScreen(player);
    UpdateScore(scoreobj);
    if (loadedgame) loadedgame = false;

    RF_Refresh();

    if (invincible) {
      invincible = static_cast<sword>(invincible - tics);
      if (invincible < 0) invincible = 0;
    }

    if (singlestep) {
      VW_WaitVBL(14);
      lasttimecount = TimeCount;
    }
    if (extravbls) VW_WaitVBL(extravbls);

    // handle user inputs
    if (DemoMode == demo_Playback) {
      if (!screenfaded && IN_IsUserInput()) {
        playstate = ex_completed;
        if (LastScan != sc_F1) LastScan = sc_Space;
      }
    } else if (DemoMode == demo_PlayDone) {
      playstate = ex_completed;
    } else {
      CheckKeys();
    }

    // E-N-D cheat
    if (Keyboard[sc_E] && Keyboard[sc_N] && Keyboard[sc_D]) {
      gamestate.rescued = 7;
      playstate = ex_rescued;
    }
  } while (playstate == ex_stillplaying);

  ingame = false;
}
