// ID_US_1: user manager, general routines (segments 1931 and 1D02).
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "engine/datafiles.h"
#include "engine/lang.h"
#include "exe/keen4exe.h"
#include "id/id_ca.h"
#include "id/id_in.h"
#include "id/id_rf.h"
#include "id/id_sd.h"
#include "id/id_sys.h"
#include "id/id_us.h"
#include "id/id_vw.h"

boolean ingame, abortgame, loadedgame, NoWait;
boolean HighScoresDirty;
GameDiff restartgame;
word PrintX, PrintY;
word WindowX, WindowY, WindowW, WindowH;
boolean tedlevel;
sword tedlevelnum;
boolean (*USL_SaveGame)(std::FILE* file), (*USL_LoadGame)(std::FILE* file);
void (*USL_ResetGame)();
SaveGame Games[MaxSaveGames];
HighScore Scores[MaxScores];

// Options stored in CONFIG.CK4 that belong to the game (CK_MAIN / CK_PLAY).
extern boolean showscorebox, jerk, oldshooting, QuietFX;
extern byte gamepadflag;
extern boolean GravisGamepad;
extern word GravisMap[4];

namespace {

using keen4::exe::g_exe;

boolean US_Started;
int g_argc;
char** g_argv;

void (*USL_MeasureString)(const char*, word*, word*) = VW_MeasurePropString;
void (*USL_DrawString)(const char*) = VWB_DrawPropString;

// US_RndT state (far segment 1ED3: index word, then the 256-byte table).
word rndindex;
byte rndtable[256];

constexpr char kConfigName[] = "CONFIG.CK4";
constexpr char kConfigSig[4] = {'C', 'K', '4', '\0'};
constexpr word kConfigVersion = 4;

// 1931:0180
void USL_ReadConfig() {
  boolean gotit = false;
  boolean hadAdLib = false;
  SDMode sd = sdm_Off;
  SMMode sm = smm_Off;
  ControlType ctl = ctrl_Keyboard;

  std::FILE* f = std::fopen(keen4::files::WritePath(kConfigName).c_str(), "rb");
  if (f) {
    char sig[4];
    word version = 0;
    bool ok = std::fread(sig, 1, 4, f) == 4 && std::fread(&version, 2, 1, f) == 1 &&
              std::memcmp(sig, kConfigSig, 4) == 0 && version == kConfigVersion;
    if (ok) {
      sword w[3];
      ok = std::fread(Scores, sizeof Scores, 1, f) == 1 && std::fread(w, 2, 3, f) == 3 &&
           std::fread(&KbdDefs[0], 10, 1, f) == 1 && std::fread(&showscorebox, 2, 1, f) == 1 &&
           std::fread(&compatability, 2, 1, f) == 1 && std::fread(&QuietFX, 2, 1, f) == 1 &&
           std::fread(&hadAdLib, 2, 1, f) == 1 && std::fread(&jerk, 2, 1, f) == 1 &&
           std::fread(&oldshooting, 2, 1, f) == 1 && std::fread(&gamepadflag, 1, 1, f) == 1 &&
           std::fread(&GravisGamepad, 2, 1, f) == 1 && std::fread(GravisMap, 8, 1, f) == 1;
      sd = static_cast<SDMode>(w[0]);
      sm = static_cast<SMMode>(w[1]);
      ctl = static_cast<ControlType>(w[2]);
      gotit = ok;
      // Port: the language follows the original fields; older files lack it.
      byte language;
      if (ok && std::fread(&language, 1, 1, f) == 1)
        keen4::lang::SetFromConfig(static_cast<keen4::lang::Language>(language));
    }
    std::fclose(f);
  }
  if (!gotit) {
    sd = sdm_Off;
    sm = smm_Off;
    ctl = ctrl_Keyboard;
    showscorebox = true;
    oldshooting = false;
  }
  SD_Default(gotit && hadAdLib == AdLibPresent, sd, sm);
  IN_Default(gotit, ctl);
}

// 1931:0348
void USL_WriteConfig() {
  std::FILE* f = std::fopen(keen4::files::WritePath(kConfigName).c_str(), "wb");
  if (!f) return;
  word version = kConfigVersion;
  sword w[3] = {SoundMode, MusicMode, Controls[0]};
  std::fwrite(kConfigSig, 1, 4, f);
  std::fwrite(&version, 2, 1, f);
  std::fwrite(Scores, sizeof Scores, 1, f);
  std::fwrite(w, 2, 3, f);
  std::fwrite(&KbdDefs[0], 10, 1, f);
  std::fwrite(&showscorebox, 2, 1, f);
  std::fwrite(&compatability, 2, 1, f);
  std::fwrite(&QuietFX, 2, 1, f);
  std::fwrite(&AdLibPresent, 2, 1, f);
  std::fwrite(&jerk, 2, 1, f);
  std::fwrite(&oldshooting, 2, 1, f);
  std::fwrite(&gamepadflag, 1, 1, f);
  std::fwrite(&GravisGamepad, 2, 1, f);
  std::fwrite(GravisMap, 8, 1, f);
  byte language = static_cast<byte>(keen4::lang::Current());
  std::fwrite(&language, 1, 1, f);
  std::fclose(f);
}

// The original stores the address of PrintX (DGROUP A537) as a build check.
constexpr word kOldTest = 0xA537;

}  // namespace

// 1931:0490
void USL_CheckSavedGames() {
  for (word i = 0; i < MaxSaveGames; i++) {
    SaveGame* game = &Games[i];
    bool ok = false;
    std::FILE* f = std::fopen(keen4::files::WritePath(USL_GiveSaveName(i)).c_str(), "rb");
    if (f) {
      if (std::fread(game, sizeof *game, 1, f) == 1 && !std::strcmp(game->signature, "CK4") &&
          game->oldtest == kOldTest)
        ok = true;
      std::fclose(f);
    }
    if (ok) {
      game->present = true;
    } else {
      std::strcpy(game->signature, "CK4");
      game->present = false;
      std::strcpy(game->name, "Empty");
    }
  }
}

namespace {

// 1931:11AF
void USL_XORICursor(int x, int y, const char* s, word cursor) {
  char buf[MaxString];
  std::strcpy(buf, s);
  buf[cursor] = '\0';
  word w, h;
  USL_MeasureString(buf, &w, &h);
  px = static_cast<sword>(x + w - 1);
  py = static_cast<sword>(y);
  USL_DrawString("\x80");
}

// 1931:0DC2
void USL_PrintInCenter(const char* s, sword xl, sword yl, sword xh, sword yh) {
  s = keen4::lang::Tr(s);
  word w, h;
  USL_MeasureString(s, &w, &h);
  word rw = static_cast<word>(xh - xl);
  word rh = static_cast<word>(yh - yl);
  px = static_cast<sword>(xl + static_cast<word>(rw - w) / 2);
  py = static_cast<sword>(yl + static_cast<word>(rh - h) / 2);
  USL_DrawString(s);
}

}  // namespace

void US_SetArgs(int argc, char** argv) {
  g_argc = argc;
  g_argv = argv;
}

int US_ArgCount() { return g_argc; }
const char* US_Arg(int i) { return g_argv[i]; }

// 1D02:0002
void US_InitRndT(boolean randomize) {
  std::memcpy(rndtable, g_exe.At(0x1ED3, 2), sizeof rndtable);
  rndindex = randomize ? static_cast<word>(std::time(nullptr) & 0xFF) : 0;
}

// 1D02:002A
int US_RndT() {
  rndindex = static_cast<word>((rndindex + 1) & 0xFF);
  return rndtable[rndindex];
}

// 1931:0144
const char* USL_GiveSaveName(word game) {
  static char name[] = "SAVEGAMx.CK4";
  name[7] = static_cast<char>('0' + game);
  return name;
}

// 1931:0154
void US_SetLoadSaveHooks(boolean (*load)(std::FILE*), boolean (*save)(std::FILE*), void (*reset)()) {
  USL_LoadGame = load;
  USL_SaveGame = save;
  USL_ResetGame = reset;
}

// 1931:0547
void US_Startup() {
  if (US_Started) return;
  // The default high score table is initialised data at DGROUP 43C4.
  std::memcpy(Scores, g_exe.DGroup(0x43C4), sizeof Scores);
  US_InitRndT(true);
  USL_ReadConfig();
  for (int i = 1; i < g_argc; i++) {
    static const char* const parms[] = {"COMP", "NOCOMP", nullptr};
    switch (US_CheckParm(g_argv[i], parms)) {
      case 0: compatability = true; break;
      case 1: compatability = false; break;
      default: break;
    }
  }
  US_Started = true;
}

// 1931:05C1
void US_Setup() {
  USL_SaveGame = nullptr;
  USL_LoadGame = nullptr;
  USL_CheckSavedGames();
}

// 1931:05E2
void US_Shutdown() {
  if (!US_Started) return;
  USL_WriteConfig();
  US_Started = false;
}

// 1931:05FF
int US_CheckParm(const char* parm, const char* const* strings) {
  while (*parm && !std::isalpha(static_cast<unsigned char>(*parm))) parm++;
  for (int i = 0; strings[i] && *strings[i]; i++) {
    const char* s = strings[i];
    const char* p = parm;
    for (;;) {
      char cs = *s++;
      if (!cs) return i;
      char cp = *p++;
      if (std::toupper(static_cast<unsigned char>(cs)) != std::toupper(static_cast<unsigned char>(cp))) break;
    }
  }
  return -1;
}

// 1931:06B5
boolean US_ParmPresent(const char* arg) {
  const char* strings[2] = {arg, nullptr};
  for (int i = 1; i < g_argc; i++)
    if (US_CheckParm(g_argv[i], strings) != -1) return true;
  return false;
}

// 1931:079A. The DOS text-mode start screen is not shown; only its
// command-line parsing is kept.
void US_TextScreen() {
  static const char* const parms[] = {"TEDLEVEL", "NOWAIT", nullptr};
  for (int i = 1; i < g_argc; i++) {
    switch (US_CheckParm(g_argv[i], parms)) {
      case 0:
        if (i + 1 < g_argc) {
          tedlevelnum = static_cast<sword>(std::atoi(g_argv[i + 1]));
          tedlevel = true;
        }
        return;
      case 1:
        NoWait = true;
        return;
      default:
        break;
    }
  }
}

void US_UpdateTextScreen() {}
void US_FinishTextScreen() { IN_ClearKeysDown(); }

// 1931:0CDA
void US_SetPrintRoutines(void (*measure)(const char*, word*, word*), void (*print)(const char*)) {
  USL_MeasureString = measure;
  USL_DrawString = print;
}

// 1931:0CF9
void US_Print(const char* s) {
  s = keen4::lang::Tr(s);
  char buf[512];
  std::strncpy(buf, s, sizeof buf - 1);
  buf[sizeof buf - 1] = '\0';
  char* line = buf;
  while (*line) {
    char* se = line;
    while (*se && *se != '\n') se++;
    char c = *se;
    *se = '\0';
    word w, h;
    USL_MeasureString(line, &w, &h);
    px = static_cast<sword>(PrintX);
    py = static_cast<sword>(PrintY);
    USL_DrawString(line);
    if (c) {
      *se = c;
      line = se + 1;
      PrintX = WindowX;
      PrintY = static_cast<word>(PrintY + h);
    } else {
      line = se;
      PrintX = static_cast<word>(PrintX + w);
    }
  }
}

// 1931:0D72
void US_PrintUnsigned(longword n) {
  char buffer[32];
  std::snprintf(buffer, sizeof buffer, "%lu", static_cast<unsigned long>(n));
  US_Print(buffer);
}

// 1931:0D9A
void US_PrintSigned(std::int32_t n) {
  char buffer[32];
  std::snprintf(buffer, sizeof buffer, "%ld", static_cast<long>(n));
  US_Print(buffer);
}

// 1931:0E1F
void US_PrintCentered(const char* s) {
  USL_PrintInCenter(s, static_cast<sword>(WindowX), static_cast<sword>(WindowY),
                    static_cast<sword>(WindowX + WindowW), static_cast<sword>(WindowY + WindowH));
}

// 1931:0E60
void US_CPrintLine(const char* s) {
  s = keen4::lang::Tr(s);
  word w, h;
  USL_MeasureString(s, &w, &h);
  // Port: a translated line that is a little too wide is still drawn.
  if (w > WindowW && keen4::lang::Current() == keen4::lang::Language::English)
    Quit("US_CPrintLine() - String exceeds width");
  px = static_cast<sword>(WindowX + (WindowW - w) / 2);
  py = static_cast<sword>(PrintY);
  USL_DrawString(s);
  PrintY = static_cast<word>(PrintY + h);
}

// 1931:0EBD
void US_CPrint(const char* s) {
  s = keen4::lang::Tr(s);
  char buf[512];
  std::strncpy(buf, s, sizeof buf - 1);
  buf[sizeof buf - 1] = '\0';
  char* line = buf;
  while (*line) {
    char* se = line;
    while (*se && *se != '\n') se++;
    char c = *se;
    *se = '\0';
    US_CPrintLine(line);
    line = se;
    if (c) {
      *se = c;
      line++;
    }
  }
}

// 1931:0F02
void US_ClearWindow() {
  VWB_Bar(WindowX, WindowY, WindowW, WindowH, 15);
  PrintX = WindowX;
  PrintY = WindowY;
}

// 1931:0F2F
void US_DrawWindow(word x, word y, word w, word h) {
  WindowX = static_cast<word>(x * 8);
  WindowY = static_cast<word>(y * 8);
  WindowW = static_cast<word>(w * 8);
  WindowH = static_cast<word>(h * 8);
  PrintX = WindowX;
  PrintY = WindowY;

  int sx = (x - 1) * 8;
  int sy = (y - 1) * 8;
  int sw = (w + 1) * 8;
  int sh = (h + 1) * 8;

  US_ClearWindow();
  VWB_DrawTile8M(sx, sy, 0);
  VWB_DrawTile8M(sx, sy + sh, 6);
  int i;
  for (i = sx + 8; i <= sx + sw - 8; i += 8) {
    VWB_DrawTile8M(i, sy, 1);
    VWB_DrawTile8M(i, sy + sh, 7);
  }
  VWB_DrawTile8M(i, sy, 2);
  VWB_DrawTile8M(i, sy + sh, 8);
  for (i = sy + 8; i <= sy + sh - 8; i += 8) {
    VWB_DrawTile8M(sx, i, 3);
    VWB_DrawTile8M(sx + sw, i, 5);
  }
}

// 1931:1070
void US_CenterWindow(word w, word h) {
  US_DrawWindow(static_cast<word>(((MaxX / 8) - w) / 2), static_cast<word>(((MaxY / 8) - h) / 2), w, h);
}

// 1931:1155
void US_SaveWindow(WindowRec* win) {
  win->x = static_cast<sword>(WindowX);
  win->y = static_cast<sword>(WindowY);
  win->w = static_cast<sword>(WindowW);
  win->h = static_cast<sword>(WindowH);
  win->px = static_cast<sword>(PrintX);
  win->py = static_cast<sword>(PrintY);
}

// 1931:1182
void US_RestoreWindow(WindowRec* win) {
  WindowX = static_cast<word>(win->x);
  WindowY = static_cast<word>(win->y);
  WindowW = static_cast<word>(win->w);
  WindowH = static_cast<word>(win->h);
  PrintX = static_cast<word>(win->px);
  PrintY = static_cast<word>(win->py);
}

// 1931:1205
boolean US_LineInput(int x, int y, char* buf, const char* def, boolean escok, int maxchars,
                     int maxwidth) {
  char s[MaxString], olds[MaxString];
  if (def)
    std::strcpy(s, def);
  else
    *s = '\0';
  *olds = '\0';
  word cursor = static_cast<word>(std::strlen(s));
  boolean cursormoved = true, redraw = true;
  boolean cursorvis = false, done = false, result = false;
  longword lasttime = TimeCount;
  LastASCII = key_None;
  LastScan = sc_None;

  while (!done) {
    if (cursorvis) USL_XORICursor(x, y, s, cursor);

    SYS_Idle();
    ScanCode sc = LastScan;
    LastScan = sc_None;
    char c = LastASCII;
    LastASCII = key_None;

    switch (sc) {
      case sc_LeftArrow:
        if (cursor) cursor--;
        c = key_None;
        cursormoved = true;
        break;
      case sc_RightArrow:
        if (s[cursor]) cursor++;
        c = key_None;
        cursormoved = true;
        break;
      case sc_Home:
        cursor = 0;
        c = key_None;
        cursormoved = true;
        break;
      case sc_End:
        cursor = static_cast<word>(std::strlen(s));
        c = key_None;
        cursormoved = true;
        break;
      case sc_Return:
        std::strcpy(buf, s);
        done = true;
        result = true;
        c = key_None;
        break;
      case sc_Escape:
        if (escok) {
          done = true;
          result = false;
        }
        c = key_None;
        break;
      case sc_BackSpace:
        if (cursor) {
          std::memmove(s + cursor - 1, s + cursor, std::strlen(s + cursor) + 1);
          cursor--;
          redraw = true;
        }
        c = key_None;
        cursormoved = true;
        break;
      case sc_Delete:
        if (s[cursor]) {
          std::memmove(s + cursor, s + cursor + 1, std::strlen(s + cursor + 1) + 1);
          redraw = true;
        }
        c = key_None;
        cursormoved = true;
        break;
      case 0x4C:  // keypad 5
      case sc_UpArrow:
      case sc_DownArrow:
      case sc_PgUp:
      case sc_PgDn:
        c = key_None;
        break;
      default:
        break;
    }

    if (c) {
      word len = static_cast<word>(std::strlen(s));
      word w, h;
      USL_MeasureString(s, &w, &h);
      if (c >= ' ' && c <= '~' && len < MaxString - 1 && (!maxchars || len < maxchars) &&
          (!maxwidth || w < maxwidth)) {
        for (word i = static_cast<word>(len + 1); i > cursor; i--) s[i] = s[i - 1];
        s[cursor++] = c;
        redraw = true;
      }
    }

    if (redraw) {
      px = static_cast<sword>(x);
      py = static_cast<sword>(y);
      USL_DrawString(olds);
      std::strcpy(olds, s);
      px = static_cast<sword>(x);
      py = static_cast<sword>(y);
      USL_DrawString(s);
      redraw = false;
    }

    if (cursormoved) {
      cursorvis = false;
      lasttime = TimeCount - 70;
      cursormoved = false;
    }
    if (TimeCount - lasttime > 35) {
      lasttime = TimeCount;
      cursorvis ^= true;
    }
    if (cursorvis) USL_XORICursor(x, y, s, cursor);
    VW_UpdateScreen();
  }

  if (cursorvis) USL_XORICursor(x, y, s, cursor);
  if (!result) {
    px = static_cast<sword>(x);
    py = static_cast<sword>(y);
    USL_DrawString(olds);
  }
  VW_UpdateScreen();
  IN_ClearKeysDown();
  return result;
}
