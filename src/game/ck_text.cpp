// CK_TEXT: the help screens and the ending text, laid out from the text
// chunks in EGAGRAPH (segment 0915).
//
// Text format: pages start with ^P and the file ends with ^E. ^Gy,x,pic draws
// a picture the text flows around, ^Ty,x,pic,tics draws one after a delay,
// ^By,x,w,h clears a box, ^Cn sets the colour and ^Ly,x moves the pen.
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "engine/lang.h"
#include "exe/keen4exe.h"
#include "game/ck_def.h"
#include "id/id_sys.h"

namespace {

using keen4::exe::g_exe;

constexpr int BACKCOLOR = 4;
constexpr int WORDLIMIT = 80;
constexpr int FONTHEIGHT = 10;
constexpr int TOPMARGIN = 10;
constexpr int LEFTMARGIN = 10;
constexpr int RIGHTMARGIN = 10;
constexpr int PICMARGIN = 8;
constexpr int SPACEWIDTH = 7;
constexpr int SCREENPIXWIDTH = 320;
constexpr int SCREENMID = SCREENPIXWIDTH / 2;
constexpr int TEXTROWS = 18;

constexpr int H_HELPPIC = 6, H_HANDPIC = 45, H_FLASHARROW1PIC = 47, H_FLASHARROW2PIC = 48,
              H_TOPWINDOWPIC = 80, H_LEFTWINDOWPIC = 81, H_RIGHTWINDOWPIC = 82, H_BOTTOMINFOPIC = 83,
              H_BOTTOMWINDOWPIC = 84;
constexpr int T_ENDART1 = 4743;

// DGROUP C8A3..C8FF
sword helpmenupos;
boolean layoutdone;
sword picdelay, picnum, picy, picx;
sword rowon;
const char* text;
word rightmargin[TEXTROWS], leftmargin[TEXTROWS];
sword numpages, pagenum;

// Port: compared unsigned, so the accented letters of a translation
// (ISO-8859-1, above 0x7F) belong to words instead of counting as blanks.
bool IsBlank(char ch) { return static_cast<unsigned char>(ch) <= ' '; }

// 0915:000A
void RipToEOL() {
  while (*text++ != '\n') {
  }
}

// 0915:001D
sword ParseNumber() {
  char ch = *text;
  while (ch < '0' || ch > '9') ch = *++text;
  char num[80];
  char* p = num;
  do {
    *p++ = ch;
    ch = *++text;
  } while (ch >= '0' && ch <= '9');
  *p = 0;
  return static_cast<sword>(std::atoi(num));
}

// 0915:0083
void ParsePicCommand() {
  picy = ParseNumber();
  picx = ParseNumber();
  picnum = ParseNumber();
  RipToEOL();
}

// 0915:00A1
void ParseTimedCommand() {
  picy = ParseNumber();
  picx = ParseNumber();
  picnum = ParseNumber();
  picdelay = ParseNumber();
  RipToEOL();
}

// 0915:00C6
void TimedPicCommand() {
  ParseTimedCommand();
  VW_WaitVBL(1);  // shows the page as drawn so far
  TimeCount = 0;
  while (TimeCount < static_cast<longword>(picdelay)) SYS_Idle();
  VWB_DrawPic(picx & ~7, picy, picnum);
}

// 0915:0128
void HandleCommand() {
  text++;
  switch (std::toupper(static_cast<unsigned char>(*text))) {
    case 'B': {
      picy = ParseNumber();
      picx = ParseNumber();
      sword width = ParseNumber();
      sword height = ParseNumber();
      VWB_Bar(picx, picy, width, height, BACKCOLOR);
      RipToEOL();
      break;
    }
    case 'P':
    case 'E':
      layoutdone = true;
      text--;
      break;
    case 'C': {
      text++;
      int i = std::toupper(static_cast<unsigned char>(*text));
      if (i >= '0' && i <= '9')
        fontcolor = static_cast<byte>(i - '0');
      else if (i >= 'A' && i <= 'F')
        fontcolor = static_cast<byte>(i - 'A' + 10);
      fontcolor ^= BACKCOLOR;
      text++;
      break;
    }
    case 'L':
      py = ParseNumber();
      rowon = static_cast<sword>((py - TOPMARGIN) / FONTHEIGHT);
      py = static_cast<sword>(rowon * FONTHEIGHT + TOPMARGIN);
      px = ParseNumber();
      while (*text++ != '\n') {
      }
      break;
    case 'T':
      TimedPicCommand();
      break;
    case 'G': {
      ParsePicCommand();
      VWB_DrawPic(picx & ~7, picy, picnum);
      int picwidth = pictable[picnum - STARTPICS].width * 8;
      int picheight = pictable[picnum - STARTPICS].height;
      int picmid = picx + picwidth / 2;
      int margin = picmid > SCREENMID ? picx - PICMARGIN : picx + picwidth + PICMARGIN;
      int top = (picy - TOPMARGIN) / FONTHEIGHT;
      if (top < 0) top = 0;
      int bottom = (picy + picheight - TOPMARGIN) / FONTHEIGHT;
      if (bottom >= TEXTROWS) bottom = TEXTROWS - 1;
      for (int i = top; i <= bottom; i++) {
        if (picmid > SCREENMID)
          rightmargin[i] = static_cast<word>(margin);
        else
          leftmargin[i] = static_cast<word>(margin);
      }
      if (static_cast<word>(px) < leftmargin[rowon]) px = static_cast<sword>(leftmargin[rowon]);
      break;
    }
    default:
      break;
  }
}

// 0915:0357
void NewLine() {
  if (++rowon == TEXTROWS) {
    // overflowed the page: skip to the next page or the end
    layoutdone = true;
    for (;;) {
      if (*text == '^') {
        char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(text[1])));
        if (ch == 'E' || ch == 'P') {
          layoutdone = true;
          return;
        }
      }
      text++;
    }
  }
  px = static_cast<sword>(leftmargin[rowon]);
  py += FONTHEIGHT;
}

// 0915:03BC
void HandleCtrls() {
  char ch = *text++;
  if (ch == '\n') NewLine();
}

// 0915:03DE
void HandleWord() {
  char word_[WORDLIMIT];
  word_[0] = *text++;
  int wordindex = 1;
  while (!IsBlank(*text)) {
    word_[wordindex] = *text++;
    if (++wordindex == WORDLIMIT) Quit(g_exe.String(0x0720));  // "PageLayout: Word limit exceeded"
  }
  word_[wordindex] = 0;

  word wwidth, wheight;
  VW_MeasurePropString(word_, &wwidth, &wheight);
  while (rightmargin[rowon] < static_cast<word>(px + wwidth)) {
    NewLine();
    if (layoutdone) return;
  }

  word newpos = static_cast<word>(px + wwidth);
  VWB_DrawPropString(word_);
  px = static_cast<sword>(newpos);

  while (*text == ' ') {
    px += SPACEWIDTH;
    text++;
  }
}

// 0915:0492
void PageLayout(boolean shownumber) {
  byte oldfontcolor = fontcolor;
  fontcolor = 10;

  VWB_Bar(0, 0, 320, 200, BACKCOLOR);
  VWB_DrawPic(0, 0, H_TOPWINDOWPIC);
  VWB_DrawPic(0, 8, H_LEFTWINDOWPIC);
  VWB_DrawPic(312, 8, H_RIGHTWINDOWPIC);
  if (shownumber)
    VWB_DrawPic(8, 176, H_BOTTOMINFOPIC);
  else
    VWB_DrawPic(8, 192, H_BOTTOMWINDOWPIC);

  for (int i = 0; i < TEXTROWS; i++) {
    leftmargin[i] = LEFTMARGIN;
    rightmargin[i] = SCREENPIXWIDTH - RIGHTMARGIN;
  }
  px = LEFTMARGIN;
  py = TOPMARGIN;
  rowon = 0;
  layoutdone = false;

  while (IsBlank(*text)) text++;
  if (*text != '^' || std::toupper(static_cast<unsigned char>(*++text)) != 'P')
    Quit(g_exe.String(0x0740));  // "PageLayout: Text not headed with ^P"
  while (*text++ != '\n') {
  }

  do {
    char ch = *text;
    if (ch == '^')
      HandleCommand();
    else if (IsBlank(ch))
      HandleCtrls();
    else
      HandleWord();
  } while (!layoutdone);

  ++pagenum;

  if (shownumber) {
    char str[40];
    std::snprintf(str, sizeof str, "%s%d%s%d", keen4::lang::Tr(g_exe.String(0x0764)), pagenum,
                  keen4::lang::Tr(g_exe.String(0x0768)), numpages);
    fontcolor = 8;
    py = 186;
    px = 218;
    VWB_DrawPropString(str);
  }

  fontcolor = oldfontcolor;
}

// 0915:0673
void BackPage() {
  pagenum--;
  do {
    text--;
  } while (!(*text == '^' && std::toupper(static_cast<unsigned char>(text[1])) == 'P'));
}

// 0915:06A1
void CacheLayoutGraphics() {
  const char* textstart = text;
  const char* bombpoint = text + 30000;
  pagenum = numpages = 0;
  CA_MarkGrChunk(H_TOPWINDOWPIC);
  CA_MarkGrChunk(H_LEFTWINDOWPIC);
  CA_MarkGrChunk(H_RIGHTWINDOWPIC);
  CA_MarkGrChunk(H_BOTTOMINFOPIC);
  CA_MarkGrChunk(H_BOTTOMWINDOWPIC);

  do {
    if (*text == '^') {
      char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(*++text)));
      if (ch == 'P') numpages++;
      if (ch == 'E') {
        CA_CacheMarks(nullptr);
        text = textstart;
        return;
      }
      if (ch == 'G') {
        ParsePicCommand();
        CA_MarkGrChunk(picnum);
      }
      if (ch == 'T') {
        ParseTimedCommand();
        CA_MarkGrChunk(picnum);
      }
    } else {
      text++;
    }
  } while (text < bombpoint);

  Quit(g_exe.String(0x076D));  // "CacheLayoutGraphics: No ^E to terminate file!"
}

// 0915:079C: returns the chosen topic, or -1 for Escape.
int HelpMenu() {
  VWB_Bar(0, 0, 320, 200, BACKCOLOR);
  CA_CacheGrChunk(H_HELPPIC);
  CA_CacheGrChunk(H_HANDPIC);
  CA_CacheGrChunk(H_TOPWINDOWPIC);
  CA_CacheGrChunk(H_LEFTWINDOWPIC);
  CA_CacheGrChunk(H_RIGHTWINDOWPIC);
  CA_CacheGrChunk(H_BOTTOMWINDOWPIC);
  VWB_DrawPic(0, 0, H_TOPWINDOWPIC);
  VWB_DrawPic(0, 8, H_LEFTWINDOWPIC);
  VWB_DrawPic(312, 8, H_RIGHTWINDOWPIC);
  VWB_DrawPic(8, 192, H_BOTTOMWINDOWPIC);
  VWB_DrawPic(96, 8, H_HELPPIC);

  sword ydelta = 0;
  IN_ClearKeysDown();
  for (;;) {
    if (helpmenupos < 0)
      helpmenupos = 0;
    else if (helpmenupos > 4)
      helpmenupos = 4;
    VWB_DrawPic(48, helpmenupos * 24 + 48, H_HANDPIC);
    VW_UpdateScreen();
    VWB_Bar(48, helpmenupos * 24 + 48, 39, 24, BACKCOLOR);

    ControlInfo control;
    IN_ReadControl(0, &control);
    // The original also follows the mouse (IN_ReadCursor); the port has none.
    if (LastScan) {
      ScanCode key = LastScan;
      IN_ClearKeysDown();
      switch (key) {
        case sc_UpArrow:
          helpmenupos--;
          break;
        case sc_DownArrow:
          helpmenupos++;
          break;
        case sc_Enter:
          VW_ClearVideo(BACKCOLOR);
          return helpmenupos;
        case sc_Escape:
          VW_ClearVideo(BACKCOLOR);
          return -1;
        default:
          break;
      }
    }
    if (control.button0 || control.button1) {
      VW_ClearVideo(BACKCOLOR);
      return helpmenupos;
    }
    if (ydelta < -40) {
      ydelta += 40;
      helpmenupos--;
    } else if (ydelta > 40) {
      ydelta -= 40;
      helpmenupos++;
    }
    SYS_Idle();
  }
}

// Text chunks of the help topics (DGROUP 0716).
int HelpChunk(int topic) { return g_exe.Word(static_cast<std::uint16_t>(0x0716 + topic * 2)); }

// Port: the article to lay out, in the chosen language when it has one.
const char* ArticleText(int chunk) {
  const char* translated = keen4::lang::Article(chunk);
  return translated ? translated : reinterpret_cast<const char*>(grsegs[chunk]);
}

}  // namespace

// 0915:09A5
void HelpScreens() {
  sword oldfontnumber = fontnumber;
  fontnumber = 0;
  CA_UpLevel();
  VW_ClearVideo(BACKCOLOR);
  RF_FixOfs();

  for (;;) {
    int n = HelpMenu();
    VW_ClearVideo(BACKCOLOR);
    if (n == -1) {
      CA_DownLevel();
      IN_ClearKeysDown();
      fontnumber = oldfontnumber;
      VW_ClearVideo(BACKCOLOR);
      RF_FixOfs();
      return;
    }

    int chunk = HelpChunk(n);
    CA_CacheGrChunk(chunk);
    text = ArticleText(chunk);
    CacheLayoutGraphics();

    boolean newpage = true;
    do {
      if (newpage) {
        newpage = false;
        PageLayout(true);
        VW_UpdateScreen();
      }
      LastScan = 0;
      while (!LastScan) SYS_Idle();
      switch (LastScan) {
        case sc_UpArrow:
        case sc_PgUp:
        case sc_LeftArrow:
          if (pagenum > 1) {
            BackPage();
            BackPage();
            newpage = true;
          }
          break;
        case sc_RightArrow:
        case sc_DownArrow:
        case sc_PgDn:
          if (pagenum < numpages) newpage = true;
          break;
        default:
          break;
      }
    } while (LastScan != sc_Escape);

    IN_ClearKeysDown();
  }
}

// 0915:0B2D
void FinaleLayout() {
  VW_ClearVideo(BACKCOLOR);
  RF_FixOfs();
  CA_UpLevel();
  CA_CacheGrChunk(H_FLASHARROW2PIC);
  CA_CacheGrChunk(H_FLASHARROW1PIC);
  CA_CacheGrChunk(T_ENDART1);
  text = ArticleText(T_ENDART1);
  CacheLayoutGraphics();
  StartMusic(7);

  while (pagenum < numpages) {
    PageLayout(false);
    IN_ClearKeysDown();
    VW_UpdateScreen();
    bool advance = false;
    while (!advance) {
      VWB_DrawPic(298, 184, H_FLASHARROW1PIC);
      for (int i = 0; i < 70 && !advance; i++) {
        if (IN_IsUserInput())
          advance = true;
        else
          VW_WaitVBL(1);
      }
      if (advance) break;
      VWB_DrawPic(298, 184, H_FLASHARROW2PIC);
      for (int i = 0; i < 70 && !advance; i++) {
        if (IN_IsUserInput())
          advance = true;
        else
          VW_WaitVBL(1);
      }
    }
  }

  StopMusic();
  CA_DownLevel();
  IN_ClearKeysDown();
  VW_ClearVideo(BACKCOLOR);
  RF_FixOfs();
}
