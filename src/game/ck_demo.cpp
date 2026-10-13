// CK_DEMO: title screen, demo playback, high scores (segment 03ED).
//
// Not ported yet: the scrolling "Terminator" intro (03ED:0FCF) and the
// "Star Wars" story crawl (03ED:18B8). Both reprogram the EGA (split screen,
// pel panning, per-line palette) and need their own rendering path; until
// then the title picture stands in for the intro and the story is skipped.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "exe/keen4exe.h"
#include "game/ck_def.h"

void HelpScreens();  // CK_TEXT

namespace {

using keen4::exe::g_exe;

constexpr int TITLEPICPIC = 109;
constexpr int DEMO0 = 4746;
constexpr int HIGHSCORELEVEL = 19;

std::vector<byte> demobuffer;

// 03ED:000B: a key pressed during the attract loop opens the menu.
void CheckLastScan() {
  if (LastScan == sc_None) return;
  if (storedemo) {
    playstate = ex_resetgame;
    restartgame = gd_Normal;
    IN_ClearKeysDown();
    NewGame();
    return;
  }
  if (LastScan == sc_F1) {
    HelpScreens();
    return;
  }
  US_ControlPanel();
  if (restartgame)
    playstate = ex_resetgame;
  else if (loadedgame)
    playstate = ex_loadedgame;
}

}  // namespace

// 03ED:19CA
void ShowTitle() {
  CA_CacheGrChunk(TITLEPICPIC);
  VW_DrawPic(0, 0, TITLEPICPIC);
  VW_UpdateScreen();
  IN_UserInput(6 * 70, false);
  CA_ClearMarks();
  CheckLastScan();
}

// 03ED:0FCF (stand-in, see the top of the file)
void Terminator() { ShowTitle(); }

// 03ED:18B8 (stand-in, see the top of the file)
void StarWars() { CheckLastScan(); }

// 03ED:1A31
void RunDemo(int num) {
  NewGame();
  int chunk = DEMO0 + num;
  CA_CacheGrChunk(chunk);
  const byte* data = grsegs[chunk];
  gamestate.mapon = static_cast<sword>(data[0] | (data[1] << 8));
  word size = static_cast<word>(data[2] | (data[3] << 8));
  demobuffer.assign(data + 4, data + 4 + size);
  IN_StartDemoPlayback(demobuffer.data(), size);
  SetupGameLevel(true);
  if (scorescreenkludge) DrawHighScores();
  PlayLoop();
  IN_StopDemo();
  VW_FixRefreshBuffer();
  CA_ClearMarks();
  CheckLastScan();
}

// 03ED:1B13: the table is drawn onto the level's background.
void DrawHighScores() {
  RF_NewPosition(0, 0);
  byte* saved = vw_screen;
  vw_screen = RF_MasterScreen();
  for (word i = 0; i < MaxScores; i++) {
    const HighScore& s = Scores[i];
    PrintY = static_cast<word>(i * 16 + 0x33);
    PrintX = 0x18;
    US_Print(s.name);

    PrintX = 0x98;
    for (word j = 0; j < s.completed; j++) {
      VWB_DrawTile8(PrintX, PrintY + 1, 0x47);  // rescued council member
      PrintX += 8;
    }

    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "%ld", static_cast<long>(s.score));
    for (char* p = buffer; *p; p++) *p = static_cast<char>(*p + 0x51);  // digits of the score font
    word w, h;
    VW_MeasurePropString(buffer, &w, &h);
    PrintX = static_cast<word>(0x128 - w);
    US_Print(buffer);
  }
  fontcolor = 15;
  vw_screen = saved;
}

// 03ED:1BFE
void CheckHighScore(std::int32_t score, word completed, const char* name) {
  HighScore myscore{};
  std::strcpy(myscore.name, g_exe.String(0x0242));
  myscore.score = score;
  myscore.completed = completed;

  int n = -1;
  word i;
  for (i = 0; i < MaxScores; i++) {
    if (Scores[i].score < score || (Scores[i].score == score && Scores[i].completed < completed)) {
      for (word j = MaxScores - 1; j > i; j--) Scores[j] = Scores[j - 1];
      Scores[i] = myscore;
      n = i;
      HighScoresDirty = true;
      break;
    }
  }

  if (n != -1) {
    scorescreenkludge = true;
    gamestate.mapon = HIGHSCORELEVEL;
    SetupGameLevel(true);
    DrawHighScores();
    RF_Refresh();
    RF_Refresh();
    PrintY = static_cast<word>(i * 16 + 0x33);
    PrintX = 0x18;
    US_LineInput(PrintX, PrintY, Scores[n].name, name, true, MaxHighName, 0x70);
    scorescreenkludge = false;
  }
}

// 03ED:1D55
void ShowHighScores() {
  scorescreenkludge = true;
  IN_ClearKeysDown();
  RunDemo(4);
  scorescreenkludge = false;
}
