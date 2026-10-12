#pragma once

// ID_US: user manager (segments 1931 ID_US_1.C, 1A89 ID_US_2.C, 1D02 ID_US_A.ASM).

#include <cstdio>

#include "id/id_heads.h"

inline constexpr int MaxX = 320;
inline constexpr int MaxY = 200;
inline constexpr int MaxHighName = 57;
inline constexpr int MaxScores = 8;
inline constexpr int MaxGameName = 32;
inline constexpr int MaxSaveGames = 6;
inline constexpr int MaxString = 128;

// 64 bytes, as in CONFIG.CK4.
#pragma pack(push, 1)
struct HighScore {
  char name[MaxHighName + 1];
  std::int32_t score;
  word completed;
};

// 42 bytes, as at the start of SAVEGAMx.CK4.
struct SaveGame {
  char signature[4];
  word oldtest;  // a DGROUP address in the original, used as a build check
  boolean present;
  char name[MaxGameName + 1];
  byte pad;
};
#pragma pack(pop)

struct WindowRec {
  sword x, y, w, h, px, py;
};

enum GameDiff : sword { gd_Continue, gd_Easy, gd_Normal, gd_Hard };

extern boolean ingame, abortgame, loadedgame, NoWait;
extern boolean HighScoresDirty;  // DGROUP A539
extern GameDiff restartgame;
// Port: the new game restartgame asks for is co-op, with these gamepads for
// players 1 and 2 (0: the keyboard).
extern boolean restartcoop;
extern std::uint32_t restartpads[2];
extern word PrintX, PrintY;
extern word WindowX, WindowY, WindowW, WindowH;
extern boolean tedlevel;
extern sword tedlevelnum;
extern boolean (*USL_SaveGame)(std::FILE* file), (*USL_LoadGame)(std::FILE* file);
extern void (*USL_ResetGame)();
extern SaveGame Games[MaxSaveGames];
extern HighScore Scores[MaxScores];

void US_Startup();
void US_Setup();
void US_Shutdown();
void US_InitRndT(boolean randomize);
int US_RndT();
void US_SetLoadSaveHooks(boolean (*load)(std::FILE*), boolean (*save)(std::FILE*), void (*reset)());
const char* USL_GiveSaveName(word game);
void US_SetPrintRoutines(void (*measure)(const char*, word*, word*), void (*print)(const char*));
void US_TextScreen();
void US_UpdateTextScreen();
void US_FinishTextScreen();
void US_DrawWindow(word x, word y, word w, word h);
void US_CenterWindow(word w, word h);
void US_SaveWindow(WindowRec* win);
void US_RestoreWindow(WindowRec* win);
void US_ClearWindow();
void US_PrintCentered(const char* s);
void US_CPrint(const char* s);
void US_CPrintLine(const char* s);
void US_Print(const char* s);
void US_PrintUnsigned(longword n);
void US_PrintSigned(std::int32_t n);
boolean US_LineInput(int x, int y, char* buf, const char* def, boolean escok, int maxchars,
                     int maxwidth);
// 1931:05FF: index of `parm` in the NULL-terminated `strings` (case-insensitive,
// leading non-letters skipped), or -1.
int US_CheckParm(const char* parm, const char* const* strings);
// 1931:06B5: true if any command-line argument matches `arg`.
boolean US_ParmPresent(const char* arg);

// Port: command line, set by main().
void USL_CheckSavedGames();
void US_SetArgs(int argc, char** argv);
int US_ArgCount();
const char* US_Arg(int i);

// ID_US_2
void US_ControlPanel();
void US_DisplayHighScores(int which);
boolean US_UpdateCursor();
