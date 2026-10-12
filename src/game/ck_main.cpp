// CK_MAIN: startup, shutdown and the attract loop (segment 037D).
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "exe/keen4exe.h"
#include "game/ck_def.h"
#include "id/id_sys.h"

void InstallCacheBoxHooks();  // CK_GAME

namespace {

using keen4::exe::g_exe;

// Command-line words (DGROUP AA and AE).
const char* const jerkparms[] = {"JERK", nullptr};
const char* const difficultyparms[] = {"EASY", "NORMAL", "HARD", nullptr};

bool shuttingdown;

}  // namespace

// 037D:000C. Measures multi-line text with the current print routine.
void SizeText(const char* text, word* width, word* height) {
  *width = *height = 0;
  char line[80];
  char* p = line;
  char ch;
  while ((ch = *text++) != 0) {
    if (p < line + sizeof line - 1) *p++ = ch;
    if (ch == '\n' || !*text) {
      *p = 0;
      word w, h;
      VW_MeasurePropString(line, &w, &h);
      *height = static_cast<word>(*height + h);
      if (*width < w) *width = w;
      p = line;
    }
  }
}

// 037D:007F
void ShutdownId() {
  US_Shutdown();
  SD_Shutdown();
  IN_Shutdown();
  RF_Shutdown();
  VW_Shutdown();
  CA_Shutdown();
}

// 037D:00A7
void InitGame() {
  for (int i = 1; i < US_ArgCount(); i++) {
    if (US_CheckParm(US_Arg(i), jerkparms) == 0) jerk = true;
  }

  US_TextScreen();
  VW_Startup();
  RF_Startup();
  IN_Startup();
  SD_Startup();
  US_Startup();
  US_UpdateTextScreen();
  CA_Startup();
  US_Setup();

  US_SetLoadSaveHooks(LoadTheGame, SaveTheGame, ResetGame);
  InstallCacheBoxHooks();

  // the fonts, 8x8 tiles and the status window pictures stay loaded
  CA_ClearMarks();
  CA_MarkGrChunk(STARTFONT);
  CA_MarkGrChunk(STARTTILE8);
  CA_MarkGrChunk(STARTTILE8M);
  CA_MarkGrChunk(122);
  CA_MarkGrChunk(123);
  CA_CacheMarks(nullptr);

  fontcolor = 15;
  US_FinishTextScreen();
  VW_SetScreenMode(2);
  VW_ColorBorder(3);
  VW_ClearVideo(0);
}

// 037D:022F. The original shows its ending text screen on a normal exit.
[[noreturn]] void Quit(const char* error) {
  if (!shuttingdown) {
    shuttingdown = true;
    VW_ClearVideo(0);
    ShutdownId();
  }
  if (error && *error) {
    std::fprintf(stderr, "%s\n", error);
    std::exit(1);
  }
  std::exit(0);
}

// 037D:0329. Returning to the TED5 editor is not possible here.
void TEDDeath() {
  ShutdownId();
  std::exit(0);
}

// 037D:03CB
void DemoLoop() {
  if (tedlevel) {
    NewGame();
    // port: /TEDLEVEL n COOP plays the level in co-op, player 1 on the
    // keyboard and player 2 on the first gamepad
    if (US_ParmPresent("COOP")) numplayers = 2;
    CA_LoadAllSounds();
    gamestate.mapon = tedlevelnum;
    restartgame = gd_Normal;
    for (int i = 1; i < US_ArgCount(); i++) {
      int level = US_CheckParm(US_Arg(i), difficultyparms);
      if (level != -1) {
        restartgame = static_cast<GameDiff>(level + 1);
        break;
      }
    }
    GameLoop();
    TEDDeath();
  }

  int i = 0;
  playstate = ex_stillplaying;
  for (;;) {
    switch (i++) {
      case 0:
        if (compatability)
          ShowTitle();
        else
          Terminator();
        break;
      case 1:
        RunDemo(0);
        break;
      case 2:
        StarWars();
        break;
      case 3:
        RunDemo(1);
        break;
      case 4:
        ShowHighScores();
        break;
      case 5:
        RunDemo(2);
        break;
      case 6:
        i = 0;
        RunDemo(3);
        break;
    }

    while (playstate == ex_resetgame || playstate == ex_loadedgame) {
      GameLoop();
      ShowHighScores();
      if (playstate == ex_resetgame || playstate == ex_loadedgame) continue;
      ShowTitle();
    }
  }
}

// 037D:06AB, after the port's own setup (see src/main.cpp).
void CK_Main() {
  if (US_ParmPresent("DEMO")) storedemo = true;
  if (US_ParmPresent("JOYPAD")) joypad = true;
  InitGame();
  if (NoWait || tedlevel) debugok = true;
  DemoLoop();
  Quit("Demo loop exited???");
}
