// ID_US_2: the control panel (segment 1A89).
//
// The menu tree (cards of items, with their hotkeys, texts and command codes)
// lives in far data segments of the original program. It is read from the
// player's exe at startup into the same shape, so the code below can walk and
// modify it as the original does (enabling items, changing option texts).
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "engine/datafiles.h"
#include "engine/lang.h"
#include "engine/version.h"
#include "exe/keen4exe.h"
#include "id/id_ca.h"
#include "id/id_in.h"
#include "id/id_rf.h"
#include "id/id_sd.h"
#include "id/id_sys.h"
#include "id/id_us.h"
#include "id/id_vw.h"
#include "platform/platform.h"
#include "update/github.h"
#include "update/install.h"

// Game-side globals the panel edits (CK_PLAY / CK_MAIN).
extern boolean showscorebox, jerk, oldshooting, QuietFX, GravisGamepad;
extern word GravisMap[4];
extern byte firescan;
void TEDDeath();
void HelpScreens();

// Port: the original's mouse and joystick routines (ID_IN). There is no
// mouse; joystick 1 is an SDL gamepad (see IN_JoyButtons), which needs no
// calibration, and joystick 2 stays disabled.
struct CursorInfo {
  boolean button0, button1;
  sword x, y;
};
// The original read the hardware on every call, and the panel spins on these
// until a button is released, so each call pumps events (and waits a moment).
static void IN_ReadCursor(CursorInfo* info) {
  *info = CursorInfo{};
  SYS_Idle();
  if (!IN_GamepadActive()) return;
  word buttons = IN_JoyButtons();
  info->button0 = static_cast<boolean>(buttons & 1);
  info->button1 = static_cast<boolean>((buttons & 2) != 0);
}
static word IN_GetJoyButtonsDB(word joy) {
  SYS_Idle();
  return joy == 1 ? 0 : IN_JoyButtons();
}
static void IN_GetJoyAbs(word, word* x, word* y) { *x = *y = 0; }
static void IN_SetupJoy(word, word, word, word, word) {}

namespace {

using keen4::exe::g_exe;

struct UserItem;
using CustomFn = boolean (*)(int msg, UserItem* item);

enum { uii_Bad, uii_Button, uii_RadioButton, uii_Folder };
enum { ui_Pushed = 1, ui_Selected = 2, ui_Disabled = 4, ui_Separated = 8 };
enum { uc_None, uc_Return, uc_Abort, uc_Quit, uc_Loaded, uc_SEasy, uc_SNormal, uc_SHard };
constexpr word uc_Language = 0x100;  // port: the main menu's language item
constexpr word uc_Coop = 0x101;      // port: the main menu's co-op item
constexpr word uc_Update = 0x102;    // port: the main menu's update item
// Port: the VIDEO card's items, which cycle through their values.
constexpr word uc_Screen = 0x103, uc_Vsync = 0x104, uc_Graphics = 0x105, uc_Resolution = 0x106;
enum { uic_SetupCard, uic_DrawCard, uic_TouchupCard, uic_DrawIcon, uic_Draw, uic_Hit };

struct UserItemGroup;

// 18 bytes in the original.
struct UserItem {
  word type, flags;
  byte hotkey;
  const char* text;
  word comm;
  UserItemGroup* child;
  word x, y;
};

struct UserItemGroup {
  word x, y, picnum;
  byte hotkey;
  UserItem* items;
  CustomFn custom;
  word cursor;
};

constexpr int MaxCards = 7;
constexpr int CTL_X = 0x4A, CTL_Y = 0x30, CTL_W = 0xA0, CTL_H = 0x66;
constexpr int BackColor = 8, HiliteColor = 10, NohiliteColor = 2;
constexpr int CP_MENUSCREENPIC = 103, CP_JOYSTICKPIC = 102, CP_PADDLEWARPIC = 100, CP_MENUMASKPICM = 121;
constexpr int PADDLESPR = 124, BALLSPR = 125, BALL1PIXELTOTHERIGHTSPR = 126;
constexpr int SND_NOWAY = 14, SND_KEENPADDLE = 47, SND_BALLBOUNCE = 48, SND_COMPPADDLE = 49,
              SND_COMPSCORE = 50, SND_KEENSCORE = 51;

// ---------------------------------------------------------------------------
// The menu tree, loaded from the exe.

std::map<std::uint32_t, std::vector<UserItem>> itemarrays;
std::map<std::uint32_t, std::unique_ptr<UserItemGroup>> groups;
bool treeloaded;

CustomFn CustomAt(word seg, word ofs);

std::uint32_t Linear(word seg, word ofs) { return (static_cast<std::uint32_t>(seg) << 4) + ofs; }
word FarWord(std::uint32_t lin) {
  const byte* p = g_exe.Linear(lin);
  return static_cast<word>(p[0] | (p[1] << 8));
}

UserItemGroup* LoadGroup(word seg, word ofs);

UserItem* LoadItems(word seg, word ofs) {
  if (!seg && !ofs) return nullptr;
  std::uint32_t key = Linear(seg, ofs);
  auto found = itemarrays.find(key);
  if (found != itemarrays.end()) return found->second.data();
  std::vector<UserItem>& items = itemarrays[key];
  for (int i = 0;; i++) {
    std::uint32_t lin = key + i * 18;
    UserItem item{};
    item.type = FarWord(lin);
    items.push_back(item);
    if (!item.type) break;
  }
  for (std::size_t i = 0; i + 1 < items.size(); i++) {
    std::uint32_t lin = key + static_cast<std::uint32_t>(i) * 18;
    UserItem& item = items[i];
    item.flags = FarWord(lin + 2);
    item.hotkey = static_cast<byte>(FarWord(lin + 4));
    word text = FarWord(lin + 6);
    item.text = text ? g_exe.String(text) : nullptr;
    item.comm = FarWord(lin + 8);
    word childofs = FarWord(lin + 10), childseg = FarWord(lin + 12);
    item.x = FarWord(lin + 14);
    item.y = FarWord(lin + 16);
    if (childseg || childofs) item.child = LoadGroup(childseg, childofs);
  }
  return items.data();
}

UserItemGroup* LoadGroup(word seg, word ofs) {
  std::uint32_t key = Linear(seg, ofs);
  auto found = groups.find(key);
  if (found != groups.end()) return found->second.get();
  auto group = std::make_unique<UserItemGroup>();
  UserItemGroup* g = group.get();
  groups[key] = std::move(group);
  g->x = FarWord(key);
  g->y = FarWord(key + 2);
  g->picnum = FarWord(key + 4);
  g->hotkey = static_cast<byte>(FarWord(key + 6));
  g->custom = CustomAt(FarWord(key + 14), FarWord(key + 12));
  g->cursor = FarWord(key + 16);
  g->items = LoadItems(FarWord(key + 10), FarWord(key + 8));
  return g;
}

// Named cards and item arrays the code refers to directly.
UserItemGroup* rootgroup;      // 1FD8:00A2
UserItemGroup* soundgroup;     // 1F87:005A
UserItemGroup* musicgroup;     // 1F8E:0036
UserItemGroup* keymovegroup;   // 1FB1:00A2
UserItem* rootitems;           // 1FD8:0000
UserItem* soundmodeitems;      // 1F87:0000
UserItem* musicmodeitems;      // 1F8E:0000
UserItem* loadsaveitems;       // 1F99:0000
UserItem* optionsitems;        // 1FAA:0000
UserItem* keybuttonitems;      // 1FBD:0000
UserItem* configitems;         // 1FCC:0000
UserItemGroup* configgroup;    // the card configitems are on
// Port: the configure card's VIDEO and CONTROLS cards.
UserItemGroup videogroup, controlsgroup;
std::vector<UserItem> videoitems, controlitems;
std::string videotext[4];

void LoadTree() {
  if (treeloaded) return;
  treeloaded = true;
  rootgroup = LoadGroup(0x1FD8, 0x00A2);
  soundgroup = LoadGroup(0x1F87, 0x005A);
  musicgroup = LoadGroup(0x1F8E, 0x0036);
  keymovegroup = LoadGroup(0x1FB1, 0x00A2);
  rootitems = LoadItems(0x1FD8, 0);
  soundmodeitems = LoadItems(0x1F87, 0);
  musicmodeitems = LoadItems(0x1F8E, 0);
  loadsaveitems = LoadItems(0x1F99, 0);
  optionsitems = LoadItems(0x1FAA, 0);
  keybuttonitems = LoadItems(0x1FBD, 0);
  configitems = LoadItems(0x1FCC, 0);

  // Port: a co-op item under NEW GAME. The list starts right under the
  // card's title and its items are seven pixels apart.
  std::vector<UserItem>& items = itemarrays[Linear(0x1FD8, 0)];
  UserItem coop{};
  coop.type = uii_Button;
  coop.comm = uc_Coop;
  coop.text = "CO-OP GAME";
  items.insert(items.begin() + 1, coop);
  rootgroup->items = rootitems = items.data();
  rootgroup->y = 0;

  // Port: the configure card becomes VIDEO, SOUND, MUSIC, OPTIONS, CONTROLS,
  // then LANGUAGE and CHECK FOR UPDATES after a gap. The four USE items and
  // the CONTROL: line (ConfigCustom) move to the CONTROLS card.
  std::vector<UserItem>& config = itemarrays[Linear(0x1FCC, 0)];
  for (auto& [key, group] : groups) {
    if (group->items == configitems) configgroup = group.get();
  }
  controlitems.assign(config.begin() + 3, config.begin() + 7);
  controlitems.push_back(UserItem{});
  controlitems[0].flags &= static_cast<word>(~ui_Separated);
  controlsgroup = *configgroup;
  controlsgroup.picnum = 0;
  controlsgroup.items = controlitems.data();
  controlsgroup.cursor = 0;

  auto button = [](word comm) {
    UserItem item{};
    item.type = uii_Button;
    item.comm = comm;
    return item;
  };
  videoitems = {button(uc_Screen), button(uc_Vsync), button(uc_Graphics), button(uc_Resolution), UserItem{}};
  videogroup = *configgroup;
  videogroup.picnum = 0;
  videogroup.custom = nullptr;
  videogroup.items = videoitems.data();
  videogroup.cursor = 0;

  UserItem video{};
  video.type = uii_Folder;
  video.text = "VIDEO";
  video.child = &videogroup;
  UserItem controls{};
  controls.type = uii_Folder;
  controls.text = "CONTROLS";
  controls.child = &controlsgroup;
  UserItem language = button(uc_Language);
  language.flags = ui_Separated;
  language.text = keen4::lang::MenuLabel();
  UserItem update = button(uc_Update);
  update.text = "CHECK FOR UPDATES";
  config = {video, config[0], config[1], config[2], controls, language, update, UserItem{}};
  configgroup->items = configitems = config.data();
  configgroup->custom = nullptr;
}

// The root items the original code refers to by number (the co-op item comes
// after NEW GAME).
UserItem& RootItem(int original) { return rootitems[original >= 1 ? original + 1 : original]; }

// Item rows are 8 pixels apart; the root card's are 7.
int RowHeight(const UserItemGroup* card) { return card == rootgroup ? 7 : 8; }

// Port: the VIDEO and CONTROLS cards have no title picture, so their title is
// drawn as text.
const char* CardTitle(const UserItemGroup* card) {
  if (card == &videogroup) return "VIDEO";
  if (card == &controlsgroup) return "CONTROLS";
  return nullptr;
}

// Port: the VIDEO card's items show their current values, such as
// "VSYNC: ON", already in the chosen language. RESOLUTION is only for the
// upscaled view.
void SetVideoText() {
  using keen4::lang::Tr;
  keen4::platform::VideoSettings video = keen4::platform::Get().GetVideo();
  auto row = [](int i, const char* label, const std::string& value) {
    videotext[i] = std::string(Tr(label)) + ": " + value;
    videoitems[i].text = videotext[i].c_str();
  };
  row(0, "SCREEN", Tr(video.fullscreen ? "FULL SCREEN" : "WINDOW"));
  row(1, "VSYNC", Tr(video.vsync ? "ON" : "OFF"));
  row(2, "GRAPHICS", Tr(video.upscaled ? "UPSCALED" : "ORIGINAL"));
  std::string size = Tr("AUTO");
  if (video.width > 0) size = std::to_string(video.width) + "X" + std::to_string(video.height);
  row(3, "RESOLUTION", size);
  if (video.upscaled)
    videoitems[3].flags &= static_cast<word>(~ui_Disabled);
  else
    videoitems[3].flags |= ui_Disabled;
}

// Port: the next value of a VIDEO item, applied at once.
void ChangeVideo(word comm) {
  keen4::platform::IPlatform& platform = keen4::platform::Get();
  keen4::platform::VideoSettings video = platform.GetVideo();
  switch (comm) {
    case uc_Screen: video.fullscreen = !video.fullscreen; break;
    case uc_Vsync: video.vsync = !video.vsync; break;
    case uc_Graphics: video.upscaled = !video.upscaled; break;
    case uc_Resolution: {
      // AUTO, then each size that fits the display, then AUTO again
      std::vector<keen4::platform::Resolution> sizes = platform.Resolutions();
      std::size_t next = 0;
      for (std::size_t i = 0; i < sizes.size(); i++) {
        if (sizes[i].width == video.width && sizes[i].height == video.height) next = i + 1;
      }
      if (video.width == 0) next = 0;
      if (next < sizes.size()) {
        video.width = sizes[next].width;
        video.height = sizes[next].height;
      } else {
        video.width = video.height = 0;
      }
      break;
    }
    default: break;
  }
  platform.SetVideo(video);
  SetVideoText();
}

// ---------------------------------------------------------------------------
// State (DGROUP 78D6..78E2, E814..E834)

word lastcomm;                    // 78D6
const char* footerstrings[3];     // 78D8 (bottom line), 78DA (right), 78DC (left)
boolean ctlpaneldone;             // 78DE
boolean QuitToDos;                // 78E0
boolean GameIsDirty;              // 78E2
boolean coopchosen;               // port: the difficulty card was opened from CO-OP GAME
UserItemGroup* topcard;           // E814
UserItemGroup* cardstack[MaxCards];
word cstackptr;                   // E834

const char* S(word ofs) { return g_exe.String(ofs); }

// Key bindings the keyboard cards edit (DGROUP 474C).
byte* KeyMap(int i) {
  switch (i) {
    case 0: return &KbdDefs[0].button0;
    case 1: return &KbdDefs[0].button1;
    case 2: return &firescan;
    case 3: return &KbdDefs[0].upleft;
    case 4: return &KbdDefs[0].up;
    case 5: return &KbdDefs[0].upright;
    case 6: return &KbdDefs[0].right;
    case 7: return &KbdDefs[0].downright;
    case 8: return &KbdDefs[0].down;
    case 9: return &KbdDefs[0].downleft;
    default: return &KbdDefs[0].left;
  }
}

// Port: everything the panel shows goes through these, so they translate it.
void Measure(const char* s, word* w, word* h) { VW_MeasurePropString(s ? keen4::lang::Tr(s) : "", w, h); }
void DrawString(const char* s) { VWB_DrawPropString(s ? keen4::lang::Tr(s) : ""); }

word ItemIndex(const UserItem* item, const UserItem* base) { return static_cast<word>(item - base); }

// 1A89:0005
void InitCtlStack() {
  cstackptr = 0;
  topcard = cardstack[0] = rootgroup;
}

// 1A89:0024
void PopCard() {
  if (!cstackptr) return;
  topcard = cardstack[--cstackptr];
}

// 1A89:004B
void PushCard(UserItemGroup* card) {
  if (cstackptr == MaxCards - 1) return;
  topcard = cardstack[++cstackptr] = card;
}

// 1A89:0078
void DrawItemIcon(UserItem* item) {
  if (topcard->custom && topcard->custom(uic_DrawIcon, item)) return;
  word flags = item->flags;
  int tile;
  if (flags & ui_Disabled)
    tile = (flags & ui_Selected) ? 5 : 4;
  else if (item->type == uii_RadioButton && !(flags & ui_Pushed))
    tile = (flags & ui_Selected) ? 3 : 2;
  else
    tile = (flags & ui_Selected) ? 1 : 0;
  VWB_DrawTile8(item->x, item->y, tile + 0x5C);
}

// 1A89:0110
void DrawItem(UserItem* item) {
  if (topcard->custom && topcard->custom(uic_Draw, item)) return;
  VWB_Bar(CTL_X + 1, item->y, CTL_W - 1, RowHeight(topcard), BackColor);
  DrawItemIcon(item);
  if ((item->flags & ui_Selected) && !(item->flags & ui_Disabled))
    fontcolor = NohiliteColor;
  else
    fontcolor = HiliteColor;
  px = static_cast<sword>(item->x + 8);
  py = static_cast<sword>(item->y + 1);
  DrawString(item->text);
  fontcolor = 15;
}

// 1A89:01A6
void DrawCtlPanelFooter() {
  int top = 0x85;
  word w, h;
  fontcolor = HiliteColor;
  px = CTL_X + 4;
  py = static_cast<sword>(top + 2);
  DrawString(footerstrings[2]);
  Measure(footerstrings[1], &w, &h);
  px = static_cast<sword>(0xE6 - w);
  DrawString(footerstrings[1]);
  Measure(footerstrings[0], &w, &h);
  px = static_cast<sword>(CTL_X + (CTL_W - w) / 2);
  py = static_cast<sword>(py + h + 1);
  DrawString(footerstrings[0]);
  fontcolor = 0;
  VWB_Hlin(0x4D, 0xE7, top, HiliteColor);
}

// Port: the game's version, on the right of the main menu's title line.
void DrawVersion() {
  std::string text = std::string("V") + keen4::VersionNumber();
  word w, h;
  Measure(text.c_str(), &w, &h);
  int x = 0xE6 - w;
  VWB_Bar(x - 3, 0x37 - 3, w + 5, h, BackColor);
  fontcolor = HiliteColor;
  px = static_cast<sword>(x);
  py = 0x37 - 3;
  DrawString(text.c_str());
  fontcolor = 15;
}

// 1A89:024C
void DrawCtlPanelContents() {
  if (topcard->custom && topcard->custom(uic_DrawCard, nullptr)) return;
  if (topcard->picnum) {
    VWB_Hlin(0x4D, 0xE7, 0x37, HiliteColor);
    VWB_DrawPic(0x50, 0x30, topcard->picnum);
    if (topcard == rootgroup) DrawVersion();
  } else if (const char* title = CardTitle(topcard)) {
    // like the title pictures: a short line, the title, then a line to the
    // right edge, in the large font
    VWB_Hlin(0x4D, 0xE7, 0x37, HiliteColor);
    fontnumber = 0;
    word w, h;
    Measure(title, &w, &h);
    px = 0x54;
    py = static_cast<sword>(0x38 - h / 2);
    VWB_Bar(px - 2, py, w + 4, h, BackColor);
    fontcolor = HiliteColor;
    DrawString(title);
    fontcolor = 15;
    fontnumber = 1;
  }
  if (topcard == &videogroup) SetVideoText();  // F11 or Alt+Enter may have changed them
  DrawCtlPanelFooter();
  if (!topcard->items) return;

  int x = topcard->x + CTL_X;
  if (x % 8) x += 8 - x % 8;
  int y = topcard->y + 0x3C;
  for (UserItem* item = topcard->items; item->type; item++) {
    if (item->flags & ui_Separated) y += 8;
    item->x = static_cast<word>(x);
    item->y = static_cast<word>(y);
    DrawItem(item);
    y += RowHeight(topcard);
  }
  if (topcard->custom) topcard->custom(uic_TouchupCard, nullptr);
}

// 1A89:035E
void DrawCtlPanel() {
  if (topcard->items || topcard->picnum) {
    VWB_DrawPic(0, 0, CP_MENUSCREENPIC);
    DrawCtlPanelContents();
  }
  VW_UpdateScreen();
}

// 1A89:0391
void DrawPanel(word w, word h, word* x, word* y) {
  VWB_DrawMPic(CTL_X, CTL_Y, CP_MENUMASKPICM);
  *x = static_cast<word>(CTL_X + (CTL_W - w) / 2);
  *y = static_cast<word>(CTL_Y + (CTL_H - h) / 2);
  VWB_Bar(*x, *y, w + 1, h + 1, BackColor);
  VWB_Hlin(*x - 1, *x + w + 1, *y - 1, HiliteColor);
  VWB_Hlin(*x - 1, *x + w + 1, *y + h + 1, HiliteColor);
  VWB_Vlin(*y - 1, *y + h + 1, *x - 1, HiliteColor);
  VWB_Vlin(*y - 1, *y + h + 1, *x + w + 1, HiliteColor);
}

// 1A89:045A
void ShowLoadSave(const char* title, const char* name) {
  char str[40];
  std::snprintf(str, sizeof str, "%s%s%s", S(0x48FF), name, S(0x4901));
  word w1, w2, h;
  Measure(title, &w1, &h);
  Measure(str, &w2, &h);
  word w = static_cast<word>((w1 > w2 ? w1 : w2) + 6);
  word x, y;
  DrawPanel(w, static_cast<word>(h * 2 + 2), &x, &y);
  py = static_cast<sword>(y + 2);
  px = static_cast<sword>(x + (w - w1) / 2);
  DrawString(title);
  py = static_cast<sword>(py + h);
  px = static_cast<sword>(x + (w - w2) / 2);
  DrawString(str);
  VW_UpdateScreen();
}

// 1A89:0536. Returns true if the player pressed Y.
boolean CtlDialog(const char* s1, const char* s2, const char* s3) {
  word w1, w2, w3, h;
  Measure(s1, &w1, &h);
  Measure(s2, &w2, &h);
  if (s3)
    Measure(s3, &w3, &h);
  else
    w3 = 0;
  word w = w1 > w2 ? (w1 > w3 ? w1 : w3) : (w2 > w3 ? w2 : w3);
  w = static_cast<word>(w + 7);
  word lh = h;
  h = static_cast<word>(h * (s3 ? 5 : 4));
  word x, y;
  DrawPanel(w, h, &x, &y);

  fontcolor = NohiliteColor;
  px = static_cast<sword>(x + (w - w1) / 2);
  py = static_cast<sword>(y + lh + 1);
  DrawString(s1);
  py = static_cast<sword>(py + lh * 2 - 1);
  VWB_Hlin(x + 3, x + w - 3, py, HiliteColor);
  py += 2;
  fontcolor = HiliteColor;
  px = static_cast<sword>(x + (w - w2) / 2);
  DrawString(s2);
  py = static_cast<sword>(py + lh);
  if (s3) {
    px = static_cast<sword>(x + (w - w3) / 2);
    DrawString(s3);
  }
  VW_UpdateScreen();

  IN_ClearKeysDown();
  CursorInfo cursor;
  ScanCode c;
  do {
    SYS_Idle();
    IN_ReadCursor(&cursor);
    if (cursor.button0)
      c = sc_Y;
    else if (cursor.button1)
      c = sc_Escape;
    else
      c = LastScan;
  } while (c == sc_None);
  do {
    IN_ReadCursor(&cursor);
  } while (cursor.button0 || cursor.button1);

  IN_ClearKeysDown();
  DrawCtlPanel();
  return c == sc_Y;
}

// Port: before a co-op game, each player picks a device: player 1 a gamepad
// (A) or the keyboard (any key), player 2 another gamepad. Escape backs out.
void DrawCoopSetup(const char* title, const char* line1, const char* line2) {
  const char* lines[4] = {title, line1, line2, S(0x4922)};  // "ESC TO BACK OUT"
  word w = 0, h = 0, lw;
  for (const char* s : lines) {
    Measure(s, &lw, &h);
    if (lw > w) w = lw;
  }
  w = static_cast<word>(w + 7);
  word x, y;
  DrawPanel(w, static_cast<word>(h * 6), &x, &y);
  py = static_cast<sword>(y + h);
  for (int i = 0; i < 4; i++) {
    fontcolor = i == 0 ? NohiliteColor : HiliteColor;
    Measure(lines[i], &lw, &h);
    px = static_cast<sword>(x + (w - lw) / 2);
    DrawString(lines[i]);
    py = static_cast<sword>(py + h + (i == 0 || i == 2 ? h / 2 + 1 : 0));
  }
  fontcolor = 15;
  VW_UpdateScreen();
}

boolean CoopInputSetup() {
  std::uint32_t chosen[2] = {0, 0};
  for (int p = 0; p < 2; p++) {
    if (p == 0)
      DrawCoopSetup("PLAYER 1: BILLY", "PRESS A ON A GAMEPAD", "OR ANY KEY");
    else
      DrawCoopSetup("PLAYER 2: TOM", "PRESS A ON ANOTHER GAMEPAD",
                    chosen[0] ? "OR ANY KEY" : "");  // the keyboard, if player 1 took a pad
    IN_ClearKeysDown();
    // a button already down (the one that picked the difficulty) counts once released
    std::vector<std::uint32_t> held;
    for (int i = 0; i < IN_GamepadCount(); i++) held.push_back(IN_GamepadId(i));
    boolean done = false;
    while (!done) {
      SYS_Idle();
      for (int i = 0; i < IN_GamepadCount() && !done; i++) {
        std::uint32_t id = IN_GamepadId(i);
        ControlInfo info;
        word buttons;
        IN_ReadGamepad(id, &info, &buttons);
        bool washeld = false;
        for (std::size_t k = 0; k < held.size(); k++) {
          if (held[k] != id) continue;
          washeld = true;
          if (!(buttons & 1)) held.erase(held.begin() + static_cast<std::ptrdiff_t>(k));
          break;
        }
        if (washeld || !(buttons & 1) || (p == 1 && id == chosen[0])) continue;
        chosen[p] = id;
        done = true;
      }
      if (done) break;
      if (LastScan == sc_Escape) {
        IN_ClearKeysDown();
        return false;
      }
      // the keyboard, for player 1, or for player 2 if player 1 took a pad
      if (LastScan && (p == 0 || chosen[0])) {
        chosen[p] = 0;
        done = true;
      }
      LastScan = sc_None;
    }
    SD_PlaySound(SND_KEENPADDLE);
  }
  restartpads[0] = chosen[0];
  restartpads[1] = chosen[1];
  IN_ClearKeysDown();
  return true;
}

// ---------------------------------------------------------------------------
// Port: CHECK FOR UPDATES. Signs in to GitHub (the repository is private),
// compares the latest release with this version, downloads it with a
// progress bar and restarts the game on the new files. See src/update/.

namespace update = keen4::update;
using keen4::lang::Tr;

// Text that fits a dialog: longer error messages are cut.
std::string Fit(std::string s) {
  word w, h;
  for (Measure(s.c_str(), &w, &h); w > CTL_W - 14 && s.size() > 3; Measure(s.c_str(), &w, &h))
    s.erase(s.size() - 4) += "...";
  return s;
}

std::string Upper(std::string s) {
  for (char& c : s) {
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  }
  return s;
}

// A panel with a title, more lines and, when `permille` is 0..1000, a
// progress bar above the last line.
void DrawUpdatePanel(const std::vector<std::string>& lines, int permille = -1) {
  constexpr word kBarW = 120, kBarH = 7;
  word w = 0, h = 0, lw;
  for (const std::string& s : lines) {
    Measure(s.c_str(), &lw, &h);
    if (lw > w) w = lw;
  }
  if (permille >= 0 && w < kBarW) w = kBarW;
  w = static_cast<word>(w + 8);
  word total = static_cast<word>(h * lines.size() + h / 2 + 3 + (permille >= 0 ? kBarH + 3 : 0));
  word x, y;
  // the menu behind, so a smaller panel does not show the last one's edges
  VWB_DrawPic(0, 0, CP_MENUSCREENPIC);
  DrawCtlPanelContents();
  DrawPanel(w, total, &x, &y);
  py = static_cast<sword>(y + 2);
  for (std::size_t i = 0; i < lines.size(); i++) {
    if (permille >= 0 && i + 1 == lines.size()) {
      int bx = x + (w - kBarW) / 2, by = py;
      VWB_Hlin(bx, bx + kBarW - 1, by, HiliteColor);
      VWB_Hlin(bx, bx + kBarW - 1, by + kBarH - 1, HiliteColor);
      VWB_Vlin(by, by + kBarH - 1, bx, HiliteColor);
      VWB_Vlin(by, by + kBarH - 1, bx + kBarW - 1, HiliteColor);
      int fill = (kBarW - 4) * (permille > 1000 ? 1000 : permille) / 1000;
      if (fill > 0) VWB_Bar(bx + 2, by + 2, fill, kBarH - 4, NohiliteColor);
      py = static_cast<sword>(py + kBarH + 3);
    }
    fontcolor = i == 0 ? NohiliteColor : HiliteColor;
    Measure(lines[i].c_str(), &lw, &h);
    px = static_cast<sword>(x + (w - lw) / 2);
    DrawString(lines[i].c_str());
    py = static_cast<sword>(py + h + (i == 0 ? h / 2 + 1 : 0));
  }
  fontcolor = 15;
}

// Runs `work` on another thread while the panel keeps drawing and watches
// for Escape (or B), which sets `cancel`. Returns false if the player canceled.
bool RunInBackground(const std::function<void()>& work, const std::function<void()>& draw,
                     std::atomic<bool>* cancel) {
  std::atomic<bool> done{false};
  std::thread worker([&] {
    work();
    done = true;
  });
  IN_ClearKeysDown();
  auto last = std::chrono::steady_clock::now() - std::chrono::seconds(1);
  bool canceled = false;
  while (!done) {
    SYS_Idle();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    CursorInfo cursor;
    IN_ReadCursor(&cursor);
    if (cancel && !canceled && (LastScan == sc_Escape || cursor.button1)) {
      canceled = true;
      *cancel = true;
    }
    auto now = std::chrono::steady_clock::now();
    if (now - last >= std::chrono::milliseconds(100)) {
      last = now;
      draw();
      VW_UpdateScreen();
    }
  }
  worker.join();
  IN_ClearKeysDown();
  return !canceled;
}

void UpdateFailed(const std::string& error) {
  std::fprintf(stderr, "Update failed: %s\n", error.c_str());
  CtlDialog("UPDATE FAILED", Fit(error).c_str(), S(0x49F6));  // "PRESS ANY KEY"
}

// The OAuth device flow: GitHub gives a code, the player enters it at
// github.com/login/device (opened in the browser), the game polls for the token.
bool SignIn(std::string* token) {
  std::atomic<bool> cancel{false};
  update::DeviceCode code;
  std::string error;
  bool ok = false;
  if (!RunInBackground([&] { ok = update::RequestDeviceCode(&code, &error); },
                       [] { DrawUpdatePanel({Tr("CHECK FOR UPDATES"), Tr("CONNECTING TO GITHUB"), S(0x4922)}); },
                       &cancel))
    return false;
  if (!ok) {
    UpdateFailed(error);
    return false;
  }
  keen4::platform::Get().SetClipboardText(code.user_code.c_str());
  keen4::platform::Get().OpenUrl(code.verification_uri.c_str());
  std::string where = code.verification_uri;
  if (where.compare(0, 8, "https://") == 0) where.erase(0, 8);
  std::string open = std::string(Tr("OPEN")) + " " + where;

  update::Poll result = update::Poll::Pending;
  RunInBackground(
      [&] {
        using clock = std::chrono::steady_clock;
        auto deadline = clock::now() + std::chrono::seconds(code.expires_in);
        int interval = code.interval > 0 ? code.interval : 5;
        while (!cancel) {
          auto wake = clock::now() + std::chrono::seconds(interval);
          while (!cancel && clock::now() < wake) std::this_thread::sleep_for(std::chrono::milliseconds(50));
          if (cancel) return;
          result = update::PollForToken(code, token, &error);
          if (result == update::Poll::SlowDown) interval += 5;
          if (result != update::Poll::Pending && result != update::Poll::SlowDown) return;
          if (clock::now() > deadline) {
            result = update::Poll::Expired;
            return;
          }
        }
      },
      [&] { DrawUpdatePanel({Tr("SIGN IN TO GITHUB"), open, Tr("AND ENTER THIS CODE:"), code.user_code, S(0x4922)}); },
      &cancel);
  if (cancel) return false;
  switch (result) {
    case update::Poll::Token:
      update::SaveToken(*token);
      return true;
    case update::Poll::Denied:
      CtlDialog("SIGN-IN CANCELED", "ON GITHUB", S(0x49F6));
      return false;
    case update::Poll::Expired:
      CtlDialog("THE CODE EXPIRED", "PLEASE TRY AGAIN", S(0x49F6));
      return false;
    default:
      UpdateFailed(error);
      return false;
  }
}

void CheckForUpdates() {
  if (update::ClientId().empty()) {
    CtlDialog("UPDATES ARE OFF", "IN THIS BUILD", S(0x49F6));
    return;
  }
  std::string error;
  if (!keen4::http::Available(&error)) {
    UpdateFailed(error);
    return;
  }
  std::string token = update::LoadToken();
  if (token.empty() && !SignIn(&token)) return;

  // the latest release; a token GitHub no longer takes means signing in again
  std::atomic<bool> cancel{false};
  update::Release release;
  update::Check check = update::Check::Failed;
  for (int attempt = 0;; attempt++) {
    if (!RunInBackground([&] { check = update::FetchLatestRelease(token, &release, &error); },
                         [] { DrawUpdatePanel({Tr("CHECK FOR UPDATES"), Tr("CONNECTING TO GITHUB"), S(0x4922)}); },
                         &cancel))
      return;
    if (check != update::Check::SignIn) break;
    update::ForgetToken();
    token.clear();
    if (attempt > 0 || !SignIn(&token)) return;
  }
  switch (check) {
    case update::Check::Ok:
      break;
    case update::Check::NoAccess:
      // A GitHub App's token sees only the repositories the app is installed
      // on, so installing it fixes this without signing in again. Otherwise
      // the next try can sign in with another account.
      if (update::IsGitHubApp()) {
        CtlDialog("NO ACCESS TO THE GAME", "INSTALL THE GITHUB APP", "ON THE GAME'S REPOSITORY");
      } else {
        update::ForgetToken();
        CtlDialog("NO ACCESS TO THE GAME", "ON THIS GITHUB ACCOUNT", S(0x49F6));
      }
      return;
    case update::Check::NoRelease:
      CtlDialog("NO RELEASES YET", "ON GITHUB", S(0x49F6));
      return;
    default:
      UpdateFailed(error);
      return;
  }

  std::string version = Upper(release.tag);
  if (update::CompareVersions(release.tag, keen4::VersionNumber()) <= 0) {
    std::string latest = std::string("V") + keen4::VersionNumber() + " " + Tr("IS THE LATEST");
    CtlDialog("NO UPDATE AVAILABLE", latest.c_str(), S(0x49F6));
    return;
  }
  const update::Asset* asset = update::PickAsset(release, update::PlatformAssetSuffix());
  if (!asset) {
    UpdateFailed(release.tag + " has no download for this system");
    return;
  }
  std::string offer = std::string(Tr("NEW VERSION:")) + " " + version;
  if (!CtlDialog(offer.c_str(), "PRESS Y TO UPDATE", S(0x4922))) return;

  // download
  namespace fs = std::filesystem;
  fs::path staging = keen4::files::WritePath("update");
  std::error_code ec;
  fs::remove_all(staging, ec);
  fs::create_directories(staging, ec);
  std::string archive = (staging / asset->name).string();
  std::atomic<std::uint64_t> done{0}, total{asset->size};
  bool ok = false, canceled = false;
  std::string title = std::string(Tr("DOWNLOADING")) + " " + version;
  RunInBackground(
      [&] {
        ok = update::DownloadAsset(
            token, *asset, archive,
            [&](std::uint64_t d, std::uint64_t t) {
              done = d;
              if (t) total = t;
              return !cancel;
            },
            &canceled, &error);
      },
      [&] {
        std::uint64_t d = done, t = total;
        char amount[48];
        std::snprintf(amount, sizeof amount, "%.1f / %.1f MB", d / 1048576.0, t / 1048576.0);
        DrawUpdatePanel({title, amount, S(0x4922)}, t ? static_cast<int>(d * 1000 / t) : 0);
      },
      &cancel);
  if (cancel || canceled || !ok) {
    fs::remove_all(staging, ec);
    if (!cancel && !canceled) UpdateFailed(error);
    return;
  }

  // unpack and put the files in place (on Windows maybe with a UAC prompt)
  update::Apply applied = update::Apply::Failed;
  std::string installing = std::string(Tr("INSTALLING")) + " " + version;
  RunInBackground(
      [&] {
        std::string root;
        if (update::Extract(archive, (staging / "files").string(), &root, &error))
          applied = update::ApplyUpdate(root, &error);
      },
      [&] { DrawUpdatePanel({installing, Tr("PLEASE WAIT")}); }, nullptr);
  fs::remove_all(staging, ec);
  if (applied == update::Apply::Declined) {
    CtlDialog("UPDATE CANCELED", "WINDOWS DID NOT ALLOW IT", S(0x49F6));
    return;
  }
  if (applied != update::Apply::Done) {
    UpdateFailed(error);
    return;
  }
  std::string updated = std::string(Tr("UPDATED TO")) + " " + version;
  CtlDialog(updated.c_str(), S(0x49F6), "TO RESTART THE GAME");  // "PRESS ANY KEY"
  update::RelaunchOnExit();
  lastcomm = uc_Quit;
  ctlpaneldone = true;
}

// 1A89:06E8
boolean ConfirmComm(word comm) {
  if (!comm) Quit(S(0x4903));  // "USL_ConfirmComm() - empty comm"

  boolean confirm = true;
  boolean dialog = false;
  const char *s1 = nullptr, *s2 = nullptr, *s3 = S(0x4922);  // "ESC TO BACK OUT"
  switch (comm) {
    case uc_Abort:
      s1 = S(0x4932);  // "REALLY END CURRENT GAME?"
      s2 = S(0x494B);  // "PRESS Y TO END IT"
      if (ingame && GameIsDirty) dialog = true;
      break;
    case uc_Quit:
      s1 = S(0x495D);  // "REALLY QUIT?"
      s2 = S(0x496A);  // "PRESS Y TO QUIT"
      dialog = true;
      break;
    case uc_Loaded:
      s1 = S(0x497A);  // "YOU'RE IN A GAME"
      s2 = S(0x498B);  // "PRESS Y TO LOAD GAME"
      if (ingame && GameIsDirty) dialog = true;
      break;
    case uc_SEasy:
    case uc_SNormal:
    case uc_SHard:
      s1 = S(0x49A0);  // "YOU'RE IN A GAME"
      s2 = S(0x49B1);  // "PRESS Y FOR NEW GAME"
      if (ingame && GameIsDirty) dialog = true;
      break;
    default:
      break;
  }

  confirm = dialog ? CtlDialog(s1, s2, s3) : true;
  // Port: a co-op game first asks each player for a device.
  if (confirm && coopchosen && comm >= uc_SEasy && comm <= uc_SHard && !CoopInputSetup()) {
    confirm = false;
    DrawCtlPanel();
  }
  if (confirm) {
    lastcomm = comm;
    ctlpaneldone = true;
  }
  return confirm;
}

// 1A89:07C4
void HandleError(int num) {
  char buf[64];
  using keen4::lang::Tr;
  std::strcpy(buf, Tr(S(0x49C6)));  // "Error: "
  if (num < 0)
    std::strcat(buf, Tr(S(0x49CE)));  // "Unknown"
  else if (num == ENOSPC || num == 8)
    std::strcat(buf, Tr(S(0x49D6)));  // "Disk is Full"
  else if (num == 11)
    std::strcat(buf, Tr(S(0x49E3)));  // "File is Incomplete"
  else
    std::strncat(buf, std::strerror(num), sizeof buf - std::strlen(buf) - 1);
  CtlDialog(buf, S(0x49F6), nullptr);  // "PRESS ANY KEY"
  VW_UpdateScreen();
  IN_ClearKeysDown();
  IN_Ack();
  VW_UpdateScreen();
}

// 1A89:086B
void SetOptionsText() {
  optionsitems[0].text = S(showscorebox ? 0x4A04 : 0x4A13);
  optionsitems[1].text = S(oldshooting ? 0x4A23 : 0x4A3A);
  optionsitems[2].text = S(jerk ? 0x4A52 : 0x4A68);
  optionsitems[3].text = S(compatability ? 0x4A7F : 0x4A97);

  keybuttonitems[2].flags &= static_cast<word>(~ui_Disabled);
  if (oldshooting) keybuttonitems[2].flags |= ui_Disabled;

  // the Gravis item, on the CONTROLS card
  UserItem& gravis = controlitems[3];
  gravis.flags |= ui_Disabled;
  if (Controls[0] == ctrl_Joystick1 || Controls[0] == ctrl_Joystick2)
    gravis.flags &= static_cast<word>(~ui_Disabled);
  gravis.text = S(GravisGamepad ? 0x4AB0 : 0x4AC8);
}

boolean ToggleOption(int msg, boolean* flag, word ontext, word offtext, word anykey) {
  if (msg != uic_SetupCard) return false;
  *flag ^= 1;
  CtlDialog(S(*flag ? ontext : offtext), S(anykey), nullptr);
  SetOptionsText();
  return true;
}

// 1A89:0926, 095F, 0998, 09D1
boolean ScoreCustom(int msg, UserItem*) { return ToggleOption(msg, &showscorebox, 0x4AE1, 0x4AF2, 0x4B04); }
boolean JerkCustom(int msg, UserItem*) { return ToggleOption(msg, &jerk, 0x4B12, 0x4B2B, 0x4B45); }
boolean CompCustom(int msg, UserItem*) { return ToggleOption(msg, &compatability, 0x4B53, 0x4B6D, 0x4B88); }
boolean TwoCustom(int msg, UserItem*) { return ToggleOption(msg, &oldshooting, 0x4B96, 0x4BAF, 0x4BC9); }

// 1A89:0A0A
boolean ConfigCustom(int msg, UserItem*) {
  if (msg == uic_TouchupCard) {
    const char* s = S(0x4C07);  // "CONTROL: "
    word w, h, w2;
    Measure(s, &w, &h);
    const char* name = S(g_exe.Word(static_cast<std::uint16_t>(0x4762 + Controls[0] * 2)));
    Measure(name, &w2, &h);
    py = static_cast<sword>(0x82 - h);  // port: above the footer line
    px = static_cast<sword>(CTL_X + (CTL_W - (w + w2)) / 2);
    fontcolor = HiliteColor;
    DrawString(s);
    DrawString(name);
  }
  return false;
}

// 1A89:0A8E: waits for the new key of a binding.
void KeySCustom(UserItem* item, word index) {
  boolean on = false;
  longword lasttime = 0;
  // Port: also clears the pad, so the A that picked this item does not cancel it.
  IN_ClearKeysDown();
  fontcolor = NohiliteColor;
  ScanCode scan;
  for (;;) {
    if (TimeCount >= lasttime) {
      on ^= 1;
      VWB_Bar(item->x + 0x5A, item->y, 0x28, 8, fontcolor ^ BackColor);
      VWB_Bar(item->x + 0x5B, item->y + 1, 0x26, 6, BackColor);
      if (on) VWB_DrawTile8(item->x + 0x6A, item->y, 0x64);
      VW_UpdateScreen();
      lasttime = TimeCount + 35;
    }
    CursorInfo cursor;
    IN_ReadCursor(&cursor);
    while (cursor.button0 || cursor.button1) {
      IN_ReadCursor(&cursor);
      LastScan = sc_Escape;
    }
    if (LastScan == sc_LShift) LastScan = sc_None;
    scan = LastScan;
    if (scan) break;
    SYS_Idle();
  }

  if (scan != sc_Escape) {
    boolean used = false;
    for (word i = 0; i < 11; i++) {
      if (i != index && *KeyMap(i) == scan) {
        used = true;
        break;
      }
    }
    if (used)
      CtlDialog(S(0x4C11), S(0x4C22), nullptr);  // "Key already used"
    else
      *KeyMap(index) = scan;
  }
  IN_ClearKeysDown();
}

// 1A89:0BF1
boolean KeyCustom(int msg, UserItem*) {
  if (msg == uic_SetupCard) {
    Controls[0] = ctrl_Keyboard;
    GravisGamepad = false;
    SetOptionsText();
  }
  return false;
}

// 1A89:0C0E: the movement and button cards.
boolean KeySCardCustom(int msg, UserItem* item) {
  boolean result = false;
  word which = 0;
  if (item) {
    if (topcard == keymovegroup)
      which = static_cast<word>(item - keymovegroup->items + 3);
    else
      which = static_cast<word>(item - keybuttonitems);
  }
  switch (msg) {
    case uic_SetupCard:
      Controls[0] = ctrl_Keyboard;
      break;
    case uic_Draw:
      VWB_Bar(CTL_X + 1, item->y, CTL_W - 1, 8, BackColor);
      DrawItemIcon(item);
      fontcolor = (item->flags & ui_Selected) ? NohiliteColor : HiliteColor;
      px = static_cast<sword>(item->x + 8);
      py = static_cast<sword>(item->y + 1);
      DrawString(item->text);
      VWB_Bar(item->x + 0x5A, item->y, 0x28, 8, fontcolor ^ BackColor);
      VWB_Bar(item->x + 0x5B, item->y + 1, 0x26, 6, BackColor);
      px = static_cast<sword>(item->x + 0x60);
      py = static_cast<sword>(item->y + 1);
      DrawString(IN_GetScanName(*KeyMap(which)));
      result = true;
      break;
    case uic_Hit:
      KeySCardCustom(uic_Draw, item);
      KeySCustom(item, which);
      DrawCtlPanel();
      result = true;
      break;
    default:
      break;
  }
  return result;
}

// 1A89:0D8B
void CJDraw(const char* s1, const char* s2) {
  word w, h;
  Measure(s1, &w, &h);
  px = static_cast<sword>(CTL_X + (CTL_W - w) / 2);
  py = 0x74;
  VWB_Bar(CTL_X + 1, py, CTL_W - 2, h * 2, BackColor);
  fontcolor = NohiliteColor;
  DrawString(s1);
  py = static_cast<sword>(py + h);
  Measure(s2, &w, &h);
  px = static_cast<sword>(CTL_X + (CTL_W - w) / 2);
  DrawString(s2);
}

// 1A89:0E21
boolean CJGet(word joy, word button, word x, word y, word* xaxis, word* yaxis) {
  while (IN_GetJoyButtonsDB(joy)) {
    SYS_Idle();
    if (LastScan == sc_Escape) return false;
  }
  boolean on = false;
  longword lasttime = 0;
  while (!(IN_GetJoyButtonsDB(joy) & (1 << button))) {
    if (TimeCount >= lasttime) {
      on ^= 1;
      lasttime = TimeCount + 35;
      VWB_DrawTile8(x, y, on + 0x5C);
      VW_UpdateScreen();
    }
    if (LastScan == sc_Escape) return false;
    SYS_Idle();
  }
  IN_GetJoyAbs(joy, xaxis, yaxis);
  return true;
}

// 1A89:0ED6
boolean ConfigJoystick(word joy) {
  // Port: an SDL gamepad comes calibrated.
  if (joy < 2 && JoysPresent[joy]) return true;
  footerstrings[2] = footerstrings[1] = S(0x4C30);
  footerstrings[0] = S(0x4C31);  // "ESC to back out"
  DrawCtlPanel();
  word x = 0x86, y = 0x43;
  VWB_DrawPic(x, y, CP_JOYSTICKPIC);

  CJDraw(S(0x4C41), S(0x4C5D));  // "Move Joystick to upper left", "and press button #1"
  VWB_DrawTile8(x + 0x18, y + 8, 0x62);
  VWB_DrawTile8(x + 8, y + 8, 0x5D);
  VWB_DrawTile8(x + 8, y + 0x18, 0x5C);
  VW_UpdateScreen();
  word minx, miny, maxx, maxy;
  if (!CJGet(joy, 0, x + 8, y + 8, &minx, &miny)) return false;

  CJDraw(S(0x4C71), S(0x4C8E));  // "Move Joystick to lower right", "and press button #2"
  VWB_DrawTile8(x + 0x18, y + 8, 0x43);
  VWB_DrawTile8(x + 0x28, y + 0x18, 0x63);
  VWB_DrawTile8(x + 8, y + 8, 0x5C);
  VWB_DrawTile8(x + 8, y + 0x18, 0x5D);
  VW_UpdateScreen();
  if (!CJGet(joy, 1, x + 8, y + 0x18, &maxx, &maxy)) return false;

  while (IN_GetJoyButtonsDB(joy)) SYS_Idle();
  if (minx != maxx && miny != maxy) {
    IN_SetupJoy(joy, minx, maxx, miny, maxy);
    return true;
  }
  return false;
}

// 1A89:1075, 10B1
boolean JoyCustom(int msg, word joy, ControlType type, word text, word anykey) {
  if (msg != uic_SetupCard) return false;
  if (ConfigJoystick(joy)) {
    Controls[0] = type;
    CtlDialog(S(text), S(anykey), nullptr);
    SetOptionsText();
  }
  return true;
}
boolean Joy1Custom(int msg, UserItem*) { return JoyCustom(msg, 0, ctrl_Joystick1, 0x4CA2, 0x4CB4); }
boolean Joy2Custom(int msg, UserItem*) { return JoyCustom(msg, 1, ctrl_Joystick2, 0x4CC2, 0x4CD4); }

// 1A89:10EE
void GravisDraw(const char* s1, const char* s2, word count) {
  VWB_Bar(CTL_X + 1, 0x40, CTL_W - 2, 0x44, BackColor);
  px = 0x52;
  py = 0x40;
  DrawString(S(0x4D0E));  // "Make sure that the button"
  px = 0x52;
  py = 0x48;
  DrawString(S(0x4D28));  // "switch is set for 4 buttons"
  for (word button = 0; button < 4; button++) {
    px = 0x52;
    py = static_cast<sword>(button * 8 + 0x58);
    DrawString(S(g_exe.Word(static_cast<std::uint16_t>(0x476C + button * 2))));
    DrawString(S(0x4D44));  // ":"
    const char* label = S(0x4D46);  // "?"
    for (word i = 0; i < count; i++) {
      if (GravisMap[i] == button) label = S(g_exe.Word(static_cast<std::uint16_t>(0x4774 + i * 2)));
    }
    px = 0x82;
    DrawString(label);
  }
  word w, h, w2;
  Measure(s1, &w, &h);
  Measure(s2, &w2, &h);
  px = static_cast<sword>(CTL_X + (CTL_W - w - w2) / 2);
  py = 0x7C;
  DrawString(s1);
  DrawString(s2);
  VW_UpdateScreen();
}

// 1A89:1216
boolean GravisGetButton(word index, const char* name) {
  GravisDraw(S(0x4D48), name, index);  // "PRESS BUTTON FOR "
  for (;;) {
    // Port: the pad is read before Escape, since B types Escape in menus, and
    // every pass waits a moment so the pad state can change.
    SYS_Idle();
    word buttons = IN_GetJoyButtonsDB(2);
    if (!buttons && LastScan == sc_Escape) {
      Keyboard[sc_Escape] = false;
      LastScan = sc_None;
      return false;
    }
    if (!buttons) continue;
    word button = 0;
    for (word i = 0; i < 4; i++) {
      if (buttons & (1 << i)) button = static_cast<word>(button + i + 1);
    }
    if (!button || button >= 5) continue;
    button--;
    bool used = false;
    for (word i = 0; i < index; i++) {
      if (GravisMap[i] == button) used = true;
    }
    if (used) continue;
    GravisMap[index] = button;
    IN_ClearKeysDown();  // port: drop the key the button typed, and wait for its release
    return true;
  }
}

// 1A89:12C8
boolean GravisCustom(int msg, UserItem*) {
  if (msg != uic_SetupCard) return false;
  if (GravisGamepad) {
    GravisGamepad = false;
  } else {
    footerstrings[2] = footerstrings[1] = S(0x4D5A);
    footerstrings[0] = S(0x4D5B);  // "ESC to back out"
    DrawCtlPanel();
    fontcolor = NohiliteColor;
    px = py = 0x52;
    IN_ClearKeysDown();
    if (GravisGetButton(0, S(0x4D6B)) && GravisGetButton(1, S(0x4D70)) && GravisGetButton(2, S(0x4D75)) &&
        GravisGetButton(3, S(0x4D7A))) {
      GravisGamepad = true;
      GravisDraw(S(0x4D81), S(0x4D8F), 4);  // "PRESS ANY KEY"
      IN_Ack();
    } else {
      GravisGamepad = false;
    }
  }
  SetOptionsText();
  return true;
}

// 1A89:1390
void DrawFileIcon(UserItem* item) {
  item->y = static_cast<word>(topcard->y + 0x3C + ItemIndex(item, loadsaveitems) * 11);
  fontcolor = (item->flags & ui_Selected) ? NohiliteColor : HiliteColor;
  int color = fontcolor ^ BackColor;
  VWB_Hlin(item->x, item->x + 0x94, item->y, color);
  VWB_Hlin(item->x, item->x + 0x94, item->y + 9, color);
  VWB_Vlin(item->y, item->y + 9, item->x, color);
  VWB_Vlin(item->y, item->y + 9, item->x + 0x94, color);
}

// 1A89:146D
void DoLoadGame(UserItem* item) {
  if (!ConfirmComm(uc_Loaded)) return;
  word n = ItemIndex(item, loadsaveitems);
  SaveGame* game = &Games[n];
  ShowLoadSave(S(0x4D90), game->name);  // "Loading"

  int err = 0;
  std::string filename = keen4::files::WritePath(USL_GiveSaveName(n));
  std::FILE* file = std::fopen(filename.c_str(), "rb");
  if (file) {
    if (std::fread(game, sizeof *game, 1, file) == 1) {
      if (USL_LoadGame && !USL_LoadGame(file)) {
        err = 11;
        HandleError(err);
      }
    } else {
      err = 11;
      HandleError(err);
    }
    std::fclose(file);
  } else {
    err = errno ? errno : -1;
    HandleError(err);
  }

  if (err) {
    abortgame = true;
    lastcomm = uc_None;
    ctlpaneldone = false;
  } else {
    loadedgame = true;
    // port: a co-op game asks each player for a device again
    if (restartcoop && !CoopInputSetup()) {
      restartpads[0] = 0;
      restartpads[1] = kAnyGamepad;
    }
  }
  game->present = true;
  if (loadedgame) Paused = true;
  DrawCtlPanel();
}

// 1A89:1581
boolean LoadCustom(int msg, UserItem* item) {
  boolean result = false;
  switch (msg) {
    case uic_SetupCard:
      if (std::getenv("UID")) USL_CheckSavedGames();
      for (word i = 0; i < MaxSaveGames; i++) {
        if (Games[i].present)
          loadsaveitems[i].flags &= static_cast<word>(~ui_Disabled);
        else
          loadsaveitems[i].flags |= ui_Disabled;
      }
      break;
    case uic_DrawIcon:
      DrawFileIcon(item);
      result = true;
      break;
    case uic_Draw: {
      DrawFileIcon(item);
      VWB_Bar(item->x + 1, item->y + 2, 0x92, 7, BackColor);
      word i = ItemIndex(item, loadsaveitems);
      px = static_cast<sword>(item->x + (Games[i].present ? 2 : 0x3C));
      py = static_cast<sword>(item->y + 2);
      DrawString(Games[i].present ? Games[i].name : S(0x4D9C));  // "Empty"
      result = true;
      break;
    }
    case uic_Hit:
      DoLoadGame(item);
      result = true;
      break;
    default:
      break;
  }
  return result;
}

void SetupCard();

// 1A89:16DD
void DoSaveGame(UserItem* item) {
  footerstrings[2] = S(0x4DA2);  // "Type name"
  footerstrings[1] = S(0x4DAC);  // "Enter accepts"
  DrawCtlPanel();

  word n = ItemIndex(item, loadsaveitems);
  SaveGame* game = &Games[n];
  fontcolor = NohiliteColor;
  VWB_Bar(item->x + 1, item->y + 2, 0x92, 7, BackColor);
  game->oldtest = 0xA537;
  boolean ok = US_LineInput(item->x + 2, item->y + 2, game->name, game->present ? game->name : nullptr, true,
                            MaxGameName, 0x8A);
  if (!std::strlen(game->name)) std::strcpy(game->name, S(0x4DBA));  // "Untitled"

  if (ok) {
    ShowLoadSave(S(0x4DC3), game->name);  // "Saving"
    int err = 0;
    std::string filename = keen4::files::WritePath(USL_GiveSaveName(n));
    std::FILE* file = std::fopen(filename.c_str(), "wb");
    if (file) {
      if (std::fwrite(game, sizeof *game, 1, file) == 1) {
        if (USL_SaveGame) ok = USL_SaveGame(file);
        if (!ok) {
          err = 8;
          HandleError(err);
        }
      } else {
        err = 8;
        HandleError(err);
      }
      std::fclose(file);
    } else {
      err = 8;
      HandleError(err);
    }
    if (err) {
      std::remove(filename.c_str());
      ok = false;
    }
  }

  if (!game->present) game->present = ok;
  if (ok) GameIsDirty = false;
  SetupCard();
}

// 1A89:18A6
boolean SaveCustom(int msg, UserItem* item) {
  switch (msg) {
    case uic_SetupCard:
      if (std::getenv("UID")) USL_CheckSavedGames();
      for (word i = 0; i < MaxSaveGames; i++) loadsaveitems[i].flags &= static_cast<word>(~ui_Disabled);
      return false;
    case uic_Hit:
      DoSaveGame(item);
      return true;
    default:
      return LoadCustom(msg, item);
  }
}

// 1A89:191A
void DrawPongScore(word keen, word comp) {
  fontcolor = NohiliteColor;
  py = 0x34;
  PrintY = 0x34;
  px = 0x50;
  VWB_Bar(px, py, 0x2A, 6, BackColor);
  DrawString(S(0x4DCE));  // "KEEN:"
  PrintX = static_cast<word>(px);
  US_PrintUnsigned(keen);
  px = 0xB6;
  VWB_Bar(px, py, 0x32, 6, BackColor);
  DrawString(S(0x4DD4));  // "COMP:"
  PrintX = static_cast<word>(px);
  US_PrintUnsigned(comp);
}

// 1A89:19B1: Paddle War.
void PlayPong() {
  word compx = 0x94, keenx = 0x94;
  sword ballxv = 0, ballyv = 0;
  word ballx = 0, bally = 0;
  word oldballx = 0x4E, oldbally = 0x3E, oldcompx = 0x4E, oldkeenx = 0x4E;
  word keenscore = 0, compscore = 0;
  word ballpx = 0, ballpy = 0;
  DrawPongScore(0, 0);

  word lastcompupdate = 0;
  boolean done = false, rdraw = true, ball = false, lastscore = false, hit = false;
  sword speedup = 10;
  word hitx = 0;
  std::int32_t balltime = 0;
  longword lasttime = TimeCount;

  do {
    longword timediff;
    while ((timediff = TimeCount - lasttime) == 0) SYS_Idle();
    lasttime = TimeCount;
    if (timediff > 4) timediff = 4;

    while (timediff-- && !done && LastScan != sc_Escape) {
      CursorInfo cursor;
      IN_ReadCursor(&cursor);
      if ((cursor.x < 0 || Keyboard[sc_LeftArrow]) && keenx > 0x4E)
        keenx -= 2;
      else if ((cursor.x > 0 || Keyboard[sc_RightArrow]) && keenx < 0xDB)
        keenx += 2;

      if (rdraw) {
        ball = false;
        balltime = 70;
        speedup = 10;
        rdraw = false;
        VWB_Bar(oldballx, oldbally, 5, 5, BackColor);
      }

      if (ball && (lastcompupdate++ % 3)) {
        word x = static_cast<word>(ballx >> 2);
        if (!(x & 1)) x = static_cast<word>(x + (US_RndT() & 1));
        if (compx + 6 < x && compx < 0xDB)
          compx++;
        else if (compx + 6 > x && compx > 0x4E)
          compx--;
      }

      if (!ball && !--balltime) {
        ball = true;
        ballxv = static_cast<sword>(1 - US_RndT() % 3);
        ballyv = 3;
        if (lastscore) ballyv = static_cast<sword>(-ballyv);
        ballx = 0x264;
        bally = 0x18C;
      }

      if (!ball) continue;

      if (static_cast<word>(ballx + ballxv) >> 2 > 0xE4 || static_cast<word>(ballx + ballxv) >> 2 < 0x4E) {
        SD_PlaySound(SND_BALLBOUNCE);
        ballxv = static_cast<sword>(-ballxv);
      }
      ballx = static_cast<word>(ballx + ballxv);

      if (static_cast<word>(bally + ballyv) >> 2 > 0x89) {
        rdraw = true;
        lastscore = false;
        compscore++;
        SD_PlaySound(SND_COMPSCORE);
        DrawPongScore(keenscore, compscore);
        if (compscore == 21) {
          CtlDialog(S(0x4DDA), S(0x4DE4), nullptr);  // "You lost!"
          done = true;
          continue;
        }
      } else if (static_cast<word>(bally + ballyv) >> 2 < 0x3E) {
        rdraw = true;
        lastscore = true;
        keenscore++;
        SD_PlaySound(SND_KEENSCORE);
        DrawPongScore(keenscore, compscore);
        if (keenscore == 21) {
          CtlDialog(S(0x4DF2), S(0x4DFB), nullptr);  // "You won!"
          done = true;
          continue;
        }
      }
      bally = static_cast<word>(bally + ballyv);
      ballpx = static_cast<word>(ballx >> 2);
      ballpy = static_cast<word>(bally >> 2);

      if (rdraw) continue;
      if (ballyv < 0 && ballpy >= 0x42 && ballpy < 0x45 && compx - 5 <= ballpx && compx + 11 > ballpx) {
        hitx = compx;
        hit = true;
        SD_PlaySound(SND_COMPPADDLE);
      } else if (ballyv > 0 && ballpy >= 0x84 && ballpy < 0x87 && keenx - 5 <= ballpx && keenx + 11 > ballpx) {
        if ((ballyv >> 2) < 3 && !--speedup) {
          ballyv++;
          speedup = 10;
        }
        hitx = keenx;
        hit = true;
        SD_PlaySound(SND_KEENPADDLE);
      }
      if (hit) {
        ballyv = static_cast<sword>(-ballyv);
        ballxv = static_cast<sword>(((ballpx + 5 - hitx) >> 1) - 4);
        if (!ballxv) ballxv--;
        hit = false;
      }
    }

    if (ball) {
      VWB_Bar(oldballx, oldbally, 5, 5, BackColor);
      oldballx = ballpx;
      oldbally = ballpy;
      VWB_DrawSprite(ballpx, ballpy, (ballpx & 1) ? BALL1PIXELTOTHERIGHTSPR : BALLSPR);
    }
    VWB_Bar(oldcompx - 3, 0x42, 0x10, 3, BackColor);
    oldcompx = compx;
    VWB_DrawSprite(compx, 0x42, PADDLESPR);
    VWB_Bar(oldkeenx - 3, 0x87, 0x10, 3, BackColor);
    oldkeenx = keenx;
    VWB_DrawSprite(keenx, 0x87, PADDLESPR);
    VW_UpdateScreen();
  } while (LastScan != sc_Escape && !done);
  IN_ClearKeysDown();
}

// 1A89:1E34
boolean PaddleWarCustom(int msg, UserItem*) {
  if (msg != uic_SetupCard) return false;
  VWB_DrawPic(0, 0, CP_MENUSCREENPIC);
  VWB_DrawPic(0x82, 0x30, CP_PADDLEWARPIC);
  VWB_Hlin(0x4D, 0xE7, 0x3C, HiliteColor);
  VWB_Hlin(0x4D, 0xE7, 0x8F, HiliteColor);
  PlayPong();
  return true;
}

CustomFn CustomAt(word seg, word ofs) {
  if (!seg && !ofs) return nullptr;
  switch (ofs) {
    case 0x0926: return ScoreCustom;
    case 0x095F: return JerkCustom;
    case 0x0998: return CompCustom;
    case 0x09D1: return TwoCustom;
    case 0x0A0A: return ConfigCustom;
    case 0x0BF1: return KeyCustom;
    case 0x0C0E: return KeySCardCustom;
    case 0x1075: return Joy1Custom;
    case 0x10B1: return Joy2Custom;
    case 0x12C8: return GravisCustom;
    case 0x1581: return LoadCustom;
    case 0x18A6: return SaveCustom;
    case 0x1E34: return PaddleWarCustom;
    default: {
      char msg[64];
      std::snprintf(msg, sizeof msg, "Control panel routine %04X:%04X is not ported", seg, ofs);
      Quit(msg);
    }
  }
}

// 1A89:1E9E
void ClearFlags(UserItemGroup* group) {
  if (!group->items) return;
  for (UserItem* item = group->items; item->type; item++) {
    item->flags &= static_cast<word>(~(ui_Pushed | ui_Selected | ui_Disabled));
    if (item->child) ClearFlags(item->child);
  }
}

// 1A89:1EF6
int FindPushedItem(UserItemGroup* group) {
  int i = 0;
  for (UserItem* item = group->items; item->type; item++, i++) {
    if (item->flags & ui_Pushed) return i;
  }
  return -1;
}

// 1A89:1F37
void SelectItem(UserItemGroup* group, word index, boolean draw) {
  if (group->cursor != index) {
    UserItem* item = &group->items[group->cursor];
    item->flags &= static_cast<word>(~ui_Selected);
    if (draw) DrawItem(item);
  }
  group->cursor = index;
  group->items[index].flags |= ui_Selected;
  if (draw) DrawItem(&group->items[index]);
}

// 1A89:1FDA
void PushItem(UserItemGroup* group, word index, boolean draw) {
  SelectItem(group, index, draw);
  word i = 0;
  for (UserItem* item = group->items; item->type; item++, i++) {
    if (item->type != uii_RadioButton) continue;
    if (i == index) {
      item->flags |= ui_Pushed;
      if (draw) DrawItem(item);
    } else if (item->flags & ui_Pushed) {
      item->flags &= static_cast<word>(~ui_Pushed);
      if (draw) DrawItem(item);
    }
  }
}

// 1A89:2067
void NextItem() {
  if (topcard->items[topcard->cursor + 1].type) SelectItem(topcard, static_cast<word>(topcard->cursor + 1), true);
}

// 1A89:20A0
void PrevItem() {
  if (topcard->cursor) SelectItem(topcard, static_cast<word>(topcard->cursor - 1), true);
}

// 1A89:20CA
void SetupCard() {
  footerstrings[2] = S(0x4E09);  // "Arrows move"
  footerstrings[1] = S(0x4E15);  // "Enter selects"
  footerstrings[0] = S(cstackptr ? 0x4E23 : 0x4E33);  // "ESC to back out" / "ESC to quit"
  SelectItem(topcard, topcard->cursor, false);
  DrawCtlPanel();
}

// 1A89:2108
void DownLevel(UserItemGroup* card) {
  if (!card) Quit(S(0x4E3F));  // "USL_DownLevel() - nil card"
  PushCard(card);
  if (topcard->custom && topcard->custom(uic_SetupCard, nullptr)) PopCard();
  SetupCard();
}

// 1A89:2153
void UpLevel() {
  if (!cstackptr) {
    ConfirmComm(uc_Quit);
    return;
  }
  if (topcard->items) topcard->items[topcard->cursor].flags &= static_cast<word>(~ui_Selected);
  PopCard();
  SetupCard();
}

// 1A89:219F
void DoItem() {
  UserItem* item = &topcard->items[topcard->cursor];
  if (item->flags & ui_Disabled) {
    SD_PlaySound(SND_NOWAY);
    return;
  }
  if (item->comm == uc_Language) {
    keen4::lang::Next();
    item->text = keen4::lang::MenuLabel();
    SetVideoText();
    DrawCtlPanel();
    return;
  }
  if (item->comm >= uc_Screen && item->comm <= uc_Resolution) {
    ChangeVideo(item->comm);
    DrawCtlPanel();
    return;
  }
  if (item->comm == uc_Update) {
    CheckForUpdates();
    if (!ctlpaneldone) DrawCtlPanel();
    return;
  }
  if (item->comm == uc_Coop) {
    // the NEW GAME card, for two players; player 2 needs a gamepad
    if (!IN_GamepadCount()) {
      CtlDialog("CONNECT A GAMEPAD", S(0x49F6), nullptr);  // "PRESS ANY KEY"
      return;
    }
    coopchosen = true;
    DownLevel(rootitems[0].child);
    return;
  }
  if (topcard == rootgroup && item->type == uii_Folder) coopchosen = false;
  switch (item->type) {
    case uii_Button:
      if (!(topcard->custom && topcard->custom(uic_Hit, item))) ConfirmComm(item->comm);
      break;
    case uii_RadioButton:
      PushItem(topcard, topcard->cursor, true);
      break;
    case uii_Folder:
      DownLevel(item->child);
      break;
    default:
      break;
  }
}

// 1A89:2265
void SetControlValues() {
  word sd = static_cast<word>(SoundMode);
  if (sd == 2 && QuietFX) sd++;
  PushItem(soundgroup, sd, false);
  PushItem(musicgroup, static_cast<word>(MusicMode), false);
  if (!AdLibPresent) {
    soundmodeitems[2].flags |= ui_Disabled;
    soundmodeitems[3].flags |= ui_Disabled;
    musicmodeitems[1].flags |= ui_Disabled;
  }
  if (!JoysPresent[0]) controlitems[1].flags |= ui_Disabled;
  if (!JoysPresent[1]) controlitems[2].flags |= ui_Disabled;
  if (!JoysPresent[0] && !JoysPresent[1]) controlitems[3].flags |= ui_Disabled;
  SetVideoText();

  RootItem(4).text = S(ingame ? 0x4E5A : 0x4E69);  // "RETURN TO GAME" / "RETURN TO DEMO"
  if (!ingame) {
    RootItem(2).flags |= ui_Disabled;
    RootItem(5).flags |= ui_Disabled;
  }
  // updating restarts the game, so not in the middle of one
  for (UserItem* item = configitems; item->type; item++) {
    if (item->comm == uc_Update && ingame) item->flags |= ui_Disabled;
  }
  rootgroup->cursor = static_cast<word>(ingame ? &RootItem(4) - rootitems : 0);
  SetOptionsText();
}

// 1A89:2480
void GetControlValues() {
  int i = FindPushedItem(soundgroup);
  if (i == 3) {
    QuietFX = true;
    i--;
  } else {
    QuietFX = false;
  }
  if (i != SoundMode) SD_SetSoundMode(static_cast<SDMode>(i));
  i = FindPushedItem(musicgroup);
  if (i != MusicMode) SD_SetMusicMode(static_cast<SMMode>(i));
}

// 1A89:2426
void HandleComm(word comm) {
  switch (comm) {
    case uc_Return:
    case uc_Loaded:
      break;
    case uc_Abort:
      abortgame = true;
      break;
    case uc_Quit:
      QuitToDos = true;
      break;
    case uc_SEasy:
      restartgame = gd_Easy;
      restartcoop = coopchosen;
      break;
    case uc_SNormal:
      restartgame = gd_Normal;
      restartcoop = coopchosen;
      break;
    case uc_SHard:
      restartgame = gd_Hard;
      restartcoop = coopchosen;
      break;
    default:
      Quit(S(0x4E86));  // "USL_HandleComm() - unknown"
  }
}

// 1A89:2355
void SetUpCtlPanel() {
  LoadTree();
  CA_UpLevel();
  for (int i = 88; i <= 103; i++) CA_MarkGrChunk(i);
  for (int i = 124; i <= 128; i++) CA_MarkGrChunk(i);
  CA_MarkGrChunk(4);
  CA_MarkGrChunk(CP_MENUMASKPICM);
  CA_CacheMarks(S(0x4E78));  // "Control Panel"
  CA_LoadAllSounds();

  fontnumber = 1;  // DGROUP A550: the panel uses the small font
  US_SetPrintRoutines(VW_MeasurePropString, VWB_DrawPropString);
  fontcolor = 15;
  VW_ClearVideo(3);
  RF_FixOfs();

  lastcomm = uc_None;
  coopchosen = false;
  ClearFlags(rootgroup);
  SetControlValues();
  InitCtlStack();
  SetupCard();
  if (ingame) GameIsDirty = true;
  IN_ClearKeysDown();
}

// 1A89:24DB
void TearDownCtlPanel() {
  GetControlValues();
  if (lastcomm) HandleComm(lastcomm);
  fontnumber = 0;
  fontcolor = 15;
  if (restartgame && USL_ResetGame) {
    USL_ResetGame();
  } else if (QuitToDos) {
    if (tedlevel) {
      TEDDeath();
    } else {
      US_CenterWindow(20, 3);
      fontcolor = 3;
      US_PrintCentered(S(0x4EA1));  // "Quitting..."
      fontcolor = 15;
      VW_UpdateScreen();
      Quit(nullptr);
    }
  }
  IN_ClearKeysDown();
  SD_WaitSoundDone();
  VW_ClearVideo(3);
  CA_DownLevel();
  CA_LoadAllSounds();
}

}  // namespace

// 1A89:2582
void US_ControlPanel() {
  if (LastScan < sc_F1 || LastScan > sc_F10) IN_ClearKeysDown();

  SetUpCtlPanel();
  DrawCtlPanel();

  sword ydelta = 0;
  ctlpaneldone = false;
  boolean blink = true, resetitem = true;
  longword lasttime = 0;
  while (!ctlpaneldone) {
    UserItem* item = &topcard->items[topcard->cursor];
    if (resetitem) {
      lasttime = TimeCount + 35;
      resetitem = false;
    }
    if (TimeCount >= lasttime) {
      blink ^= 1;
      resetitem = true;
      if (!blink) item->flags &= static_cast<word>(~ui_Selected);
      DrawItemIcon(item);
      item->flags |= ui_Selected;
    }
    VW_UpdateScreen();

    if (LastScan) {
      switch (LastScan) {
        case sc_UpArrow:
          PrevItem();
          resetitem = true;
          break;
        case sc_DownArrow:
          NextItem();
          resetitem = true;
          break;
        case sc_Return:
          DoItem();
          resetitem = true;
          break;
        case sc_Escape:
          UpLevel();
          resetitem = true;
          break;
        case sc_F1:
          HelpScreens();
          DrawCtlPanel();
          resetitem = true;
          break;
        default:
          break;
      }
      if (!resetitem && (LastScan == KbdDefs[0].button0 || LastScan == KbdDefs[0].button1)) {
        DoItem();
        resetitem = true;
      }
      if (!resetitem) {
        word i = 0;
        for (UserItem* it = topcard->items; it->type; it++, i++) {
          if (it->hotkey == LastScan) {
            SelectItem(topcard, i, true);
            resetitem = true;
            break;
          }
        }
      }
      IN_ClearKeysDown();
    } else {
      // The original also moves the cursor with the mouse (none here).
      CursorInfo cursor;
      IN_ReadCursor(&cursor);
      ydelta = static_cast<sword>(ydelta + cursor.y);
      if (ydelta < -40) {
        ydelta += 40;
        PrevItem();
        resetitem = true;
      } else if (ydelta > 40) {
        ydelta -= 40;
        NextItem();
        resetitem = true;
      }
      SYS_Idle();
    }
  }

  TearDownCtlPanel();
}

// Not used by Keen 4.
void US_DisplayHighScores(int) {}
boolean US_UpdateCursor() { return false; }
