// ID_IN: input (segment 1460).
#include "id/id_in.h"

#include <cstdio>
#include <cstdlib>
#include <iterator>

#include "exe/keen4exe.h"
#include "id/id_sd.h"
#include "id/id_sys.h"
#include "platform/platform.h"

boolean Keyboard[128];
boolean Paused;
char LastASCII;
ScanCode LastScan;
KeyboardDef KbdDefs[2];
ControlType Controls[4];
Demo DemoMode;
byte* DemoBuffer;
word DemoOffset, DemoSize;
boolean MousePresent;
boolean JoysPresent[2];

namespace {

using keen4::exe::g_exe;

boolean IN_Started;
boolean CapsLock;
ScanCode CurCode, LastCode;
void (*INL_KeyHook)();

// Scancode to ASCII tables in far data segments 1F68, 1F70 and 1F78, and the
// direction table at DGROUP 3D28.
byte ASCIINames[128], ShiftNames[128], SpecialNames[128];
// The code indexes DGROUP 3D28 with (y*3+x)*2 for x, y in -1..1, so the
// nine entries start four words earlier.
sword DirTable[9];

// 1460:000B, the keyboard interrupt handler.
void INL_KeyService(byte k, bool special) {
  if (k & 0x80) {
    Keyboard[k & 0x7F] = false;
  } else {
    LastCode = CurCode;
    CurCode = LastScan = k;
    Keyboard[k] = true;

    char c;
    if (special) {
      c = static_cast<char>(SpecialNames[k]);
    } else {
      if (k == sc_CapsLock) CapsLock ^= true;
      if (Keyboard[sc_LShift] || Keyboard[sc_RShift]) {
        c = static_cast<char>(ShiftNames[k]);
        if (c >= 'A' && c <= 'Z' && CapsLock) c += 'a' - 'A';
      } else {
        c = static_cast<char>(ASCIINames[k]);
        if (c >= 'a' && c <= 'z' && CapsLock) c -= 'a' - 'A';
      }
    }
    if (c) LastASCII = c;
  }
  if (INL_KeyHook && !special) INL_KeyHook();
}

// Port: the gamepad. The newest connected pad is joystick 1. Controls[0]
// follows whichever device was used last: the pad when it connects or a button
// is pressed, the keyboard when a key is pressed or the pad goes away. While
// Keen is being played (PollControls runs) the pad drives him through
// IN_ReadControl; anywhere else (menus, help, dialogs) its buttons type the
// keys those screens wait for.
namespace pad {

using namespace keen4::platform;

constexpr float kStickDeadZone = 0.5f;
constexpr std::uint64_t kGameplayTics = 4;  // PollControls runs every frame
constexpr std::uint64_t kRepeatDelay = 18, kRepeatRate = 5;  // menu auto-repeat, in tics
constexpr std::uint32_t kDirections = kPadUp | kPadDown | kPadLeft | kPadRight;

struct KeyMap {
  std::uint32_t bit;
  ScanCode scan;
  bool extended;
};
constexpr KeyMap kMenuKeys[] = {
    {kPadUp, sc_UpArrow, true},     {kPadDown, sc_DownArrow, true}, {kPadLeft, sc_LeftArrow, true},
    {kPadRight, sc_RightArrow, true}, {kPadA, sc_Return, false},    {kPadB, sc_Escape, false},
    {kPadStart, sc_Escape, false},
};
constexpr KeyMap kGameKeys[] = {
    {kPadStart, sc_Escape, false},  // the control panel
    {kPadBack, sc_Enter, false},    // the status window
};

bool connected;
std::uint32_t held;      // buttons and stick directions in use
std::uint32_t ignored;   // held through a clear or a mode change; dropped until released
std::uint32_t typed;     // bits whose key is currently down
bool menu = true;
std::uint64_t lastGameplay;
bool sawGameplay;
std::uint64_t repeatAt;

std::uint32_t Bits(const GamepadState& s) {
  std::uint32_t b = s.buttons;
  if (s.x < -kStickDeadZone) b |= kPadLeft;
  if (s.x > kStickDeadZone) b |= kPadRight;
  if (s.y < -kStickDeadZone) b |= kPadUp;
  if (s.y > kStickDeadZone) b |= kPadDown;
  return b;
}

void Type(const KeyMap& key, bool down) {
  INL_KeyService(static_cast<byte>(key.scan | (down ? 0 : 0x80)), key.extended);
}

void ReleaseTyped() {
  for (const KeyMap& key : kMenuKeys)
    if (typed & key.bit) Type(key, false);
  for (const KeyMap& key : kGameKeys)
    if (typed & key.bit) Type(key, false);
  typed = 0;
}

void UseGamepad(const char* name) {
  if (Controls[0] != ctrl_Keyboard1) return;
  Controls[0] = ctrl_Joystick1;
  std::fprintf(stderr, "input: %s\n", name);
}

void UseKeyboard() {
  if (Controls[0] != ctrl_Joystick1 && Controls[0] != ctrl_Joystick2) return;
  Controls[0] = ctrl_Keyboard1;
  std::fprintf(stderr, "input: keyboard\n");
}

void Pump() {
  GamepadState s;
  Get().ReadGamepad(&s);
  std::uint32_t bits = s.connected ? Bits(s) : 0;
  if (s.connected != connected) {
    connected = s.connected;
    JoysPresent[0] = connected;
    ReleaseTyped();
    ignored = bits;  // whatever was held while plugging in does nothing yet
    if (connected) {
      std::fprintf(stderr, "input: %s connected\n", s.name);
      UseGamepad(s.name);
    } else {
      std::fprintf(stderr, "input: gamepad disconnected\n");
      UseKeyboard();
    }
  }

  std::uint64_t now = Get().Ticks70();
  bool nowMenu = !sawGameplay || now - lastGameplay > kGameplayTics;
  if (nowMenu != menu) {
    menu = nowMenu;
    ReleaseTyped();
    ignored |= bits;
  }
  ignored &= bits;
  bits &= ~ignored;
  if (bits & ~held) UseGamepad(s.name);

  const KeyMap* keys = menu ? kMenuKeys : kGameKeys;
  std::size_t count = menu ? std::size(kMenuKeys) : std::size(kGameKeys);
  for (std::size_t i = 0; i < count; i++) {
    const KeyMap& key = keys[i];
    bool down = bits & key.bit;
    if (down && !(typed & key.bit)) {
      // menu directions repeat like a held key
      if (key.bit & kDirections) repeatAt = now + kRepeatDelay;
      Type(key, true);
      typed |= key.bit;
    } else if (!down && (typed & key.bit)) {
      Type(key, false);
      typed &= ~key.bit;
    } else if (down && menu && (key.bit & kDirections) && now >= repeatAt) {
      repeatAt = now + kRepeatRate;
      Type(key, true);
    }
  }
  held = bits;
}

}  // namespace pad

}  // namespace

void IN_PumpEvents() {
  keen4::platform::KeyEvent ev;
  while (keen4::platform::Get().PollKey(&ev)) {
    if (ev.down) pad::UseKeyboard();
    if (ev.scancode == 0xE1) {
      // The Pause key sends E1 1D 45 E1 9D C5 on press only.
      if (ev.down) Paused = true;
      continue;
    }
    byte k = static_cast<byte>((ev.scancode & 0x7F) | (ev.down ? 0 : 0x80));
    INL_KeyService(k, ev.extended);
  }
  pad::Pump();
}

void IN_NoteGameplay() {
  if (DemoMode == demo_Playback) return;  // the title demos leave the pad on the menus
  pad::lastGameplay = keen4::platform::Get().Ticks70();
  pad::sawGameplay = true;
}

boolean IN_GamepadActive() {
  return pad::connected && (Controls[0] == ctrl_Joystick1 || Controls[0] == ctrl_Joystick2);
}

word IN_JoyButtons() {
  if (!pad::connected) return 0;
  using namespace keen4::platform;
  word b = 0;
  if (pad::held & kPadA) b |= 1;
  if (pad::held & kPadB) b |= 2;
  if (pad::held & kPadX) b |= 4;
  if (pad::held & kPadY) b |= 8;
  return b;
}

int IN_GamepadCount() { return keen4::platform::Get().GamepadCount(); }

std::uint32_t IN_GamepadId(int index) {
  keen4::platform::GamepadState s;
  keen4::platform::Get().ReadGamepadAt(index, &s);
  return s.connected ? s.id : 0;
}

boolean IN_ReadGamepad(std::uint32_t id, ControlInfo* info, word* buttons) {
  using namespace keen4::platform;
  *info = ControlInfo{};
  info->dir = DirTable[4];
  *buttons = 0;
  int count = Get().GamepadCount();
  for (int i = 0; i < count; i++) {
    GamepadState s;
    Get().ReadGamepadAt(i, &s);
    if (!s.connected || s.id != id) continue;
    std::uint32_t bits = pad::Bits(s);
    sword mx = 0, my = 0;
    if (bits & kPadUp)
      my = -1;
    else if (bits & kPadDown)
      my = 1;
    if (bits & kPadLeft)
      mx = -1;
    else if (bits & kPadRight)
      mx = 1;
    info->xaxis = mx;
    info->yaxis = my;
    info->x = static_cast<sword>(mx * 127);
    info->y = static_cast<sword>(my * 127);
    info->button0 = (bits & kPadA) != 0;
    info->button1 = (bits & kPadB) != 0;
    info->dir = DirTable[((my + 1) * 3) + (mx + 1)];
    if (bits & kPadA) *buttons |= 1;
    if (bits & kPadB) *buttons |= 2;
    if (bits & kPadX) *buttons |= 4;
    if (bits & kPadY) *buttons |= 8;
    if (bits & kPadBack) *buttons |= kJoyBack;
    if (bits & kPadStart) *buttons |= kJoyStart;
    return true;
  }
  return false;
}

// 1460:0794
void IN_Startup() {
  if (IN_Started) return;
  for (int i = 0; i < 128; i++) {
    ASCIINames[i] = *g_exe.At(0x1F68, static_cast<std::uint16_t>(i));
    ShiftNames[i] = *g_exe.At(0x1F70, static_cast<std::uint16_t>(i));
    SpecialNames[i] = *g_exe.At(0x1F78, static_cast<std::uint16_t>(i));
  }
  for (int i = 0; i < 9; i++) DirTable[i] = g_exe.Int(static_cast<std::uint16_t>(0x3D20 + i * 2));
  std::memcpy(KbdDefs, g_exe.DGroup(0x3BC2), sizeof KbdDefs);
  IN_ClearKeysDown();
  MousePresent = false;
  JoysPresent[0] = JoysPresent[1] = false;
  IN_Started = true;
}

// 1460:0826
void IN_Default(boolean gotit, ControlType in) {
  if (!gotit || (in == ctrl_Joystick1 && !JoysPresent[0]) ||
      (in == ctrl_Joystick2 && !JoysPresent[1]) || (in == ctrl_Mouse && !MousePresent))
    in = ctrl_Keyboard1;
  IN_SetControlType(0, in);
}

// 1460:0868
void IN_Shutdown() { IN_Started = false; }

// 1460:0896
void IN_SetKeyHook(void (*hook)()) { INL_KeyHook = hook; }

// 1460:08A8
void IN_ClearKeysDown() {
  LastScan = sc_None;
  LastASCII = key_None;
  for (auto& k : Keyboard) k = false;
  // Port: pad buttons held now count only once released and pressed again.
  pad::ignored |= pad::held;
}

// 1460:09BE
void IN_ReadControl(int player, ControlInfo* info) {
  boolean realdelta = false;
  sword dx = 0, dy = 0;
  sword mx = 0, my = 0;
  word buttons = 0;

  SYS_Poll();
  if (DemoMode == demo_Playback) {
    byte dbyte = DemoBuffer[DemoOffset + 1];
    my = static_cast<sword>((dbyte & 3) - 1);
    mx = static_cast<sword>(((dbyte >> 2) & 3) - 1);
    buttons = static_cast<word>((dbyte >> 4) & 3);
    if (!(--DemoBuffer[DemoOffset])) {
      DemoOffset += 2;
      if (DemoOffset >= DemoSize) DemoMode = demo_PlayDone;
    }
  } else if (DemoMode == demo_PlayDone) {
    Quit("Demo playback exceeded");
  } else {
    ControlType type = Controls[player];
    switch (type) {
      case ctrl_Keyboard1:
      case ctrl_Keyboard2: {
        const KeyboardDef* def = &KbdDefs[type - ctrl_Keyboard];
        if (Keyboard[def->upleft]) {
          mx = -1;
          my = -1;
        } else if (Keyboard[def->upright]) {
          mx = 1;
          my = -1;
        } else if (Keyboard[def->downleft]) {
          mx = -1;
          my = 1;
        } else if (Keyboard[def->downright]) {
          mx = 1;
          my = 1;
        }
        if (Keyboard[def->up])
          my = -1;
        else if (Keyboard[def->down])
          my = 1;
        if (Keyboard[def->left])
          mx = -1;
        else if (Keyboard[def->right])
          mx = 1;
        if (Keyboard[def->button0]) buttons += 1;
        if (Keyboard[def->button1]) buttons += 2;
        realdelta = false;
        break;
      }
      case ctrl_Joystick1:
      case ctrl_Joystick2: {
        // Port: an SDL gamepad, read as a digital stick like the original's
        // calibrated joystick. A is button 1 (jump), B is button 2 (pogo).
        using namespace keen4::platform;
        if (pad::held & kPadUp)
          my = -1;
        else if (pad::held & kPadDown)
          my = 1;
        if (pad::held & kPadLeft)
          mx = -1;
        else if (pad::held & kPadRight)
          mx = 1;
        if (pad::held & kPadA) buttons += 1;
        if (pad::held & kPadB) buttons += 2;
        realdelta = false;
        break;
      }
      default:
        // The mouse is not supported by the port.
        break;
    }
  }

  if (realdelta) {
    mx = dx < 0 ? -1 : dx > 0 ? 1 : 0;
    my = dy < 0 ? -1 : dy > 0 ? 1 : 0;
  } else {
    dx = static_cast<sword>(mx * 127);
    dy = static_cast<sword>(my * 127);
  }

  info->x = dx;
  info->xaxis = mx;
  info->y = dy;
  info->yaxis = my;
  info->button0 = static_cast<boolean>(buttons & 1);
  info->button1 = static_cast<boolean>(buttons & 2);
  info->dir = DirTable[((my + 1) * 3) + (mx + 1)];

  if (DemoMode == demo_Record) {
    byte dbyte = static_cast<byte>((buttons << 4) | ((mx + 1) << 2) | (my + 1));
    if (DemoBuffer[DemoOffset + 1] == dbyte && DemoBuffer[DemoOffset] < 255) {
      DemoBuffer[DemoOffset]++;
      return;
    }
    if (DemoOffset || DemoBuffer[DemoOffset]) DemoOffset += 2;
    if (DemoOffset >= DemoSize) Quit("Demo buffer overflow");
    DemoBuffer[DemoOffset] = 1;
    DemoBuffer[DemoOffset + 1] = dbyte;
  }
}

// 1460:0D28
void IN_SetControlType(int player, ControlType type) { Controls[player] = type; }

// 1460:0D39
boolean IN_StartDemoRecord(word bufsize) {
  if (!bufsize) return false;
  DemoBuffer = static_cast<byte*>(std::calloc(bufsize, 1));
  DemoMode = demo_Record;
  DemoSize = bufsize & ~1;
  DemoOffset = 0;
  DemoBuffer[0] = DemoBuffer[1] = 0;
  return true;
}

// 1460:0D81
void IN_StartDemoPlayback(byte* buffer, word bufsize) {
  DemoBuffer = buffer;
  DemoMode = demo_Playback;
  DemoSize = bufsize & ~1;
  DemoOffset = 0;
}

// 1460:0DA1
void IN_StopDemo() {
  if (DemoMode == demo_Record && DemoOffset) DemoOffset += 2;
  DemoMode = demo_Off;
}

// 1460:0DBF
void IN_FreeDemoBuffer() {
  // Playback buffers belong to the cache; only recorded ones are ours.
  DemoBuffer = nullptr;
}

// 1460:0DD7. Special keys are listed in far segment 1F80 with their name
// pointers at DGROUP 3CD8; everything else indexes the table at 3BD8.
const char* IN_GetScanName(ScanCode scan) {
  for (int i = 0;; i++) {
    byte s = *g_exe.At(0x1F80, static_cast<std::uint16_t>(i));
    if (!s) break;
    if (s == scan) return g_exe.String(g_exe.Word(static_cast<std::uint16_t>(0x3CD8 + i * 2)));
  }
  return g_exe.String(g_exe.Word(static_cast<std::uint16_t>(0x3BD8 + scan * 2)));
}

// 1460:0E1F
ScanCode IN_WaitForKey() {
  ScanCode result;
  while (!(result = LastScan)) SYS_Idle();
  LastScan = 0;
  return result;
}

// 1460:0E3D
char IN_WaitForASCII() {
  char result;
  while (!(result = LastASCII)) SYS_Idle();
  LastASCII = '\0';
  return result;
}

// 1460:0E5A
void IN_AckBack() {
  while (!LastScan) SYS_Idle();
  Keyboard[LastScan] = false;
  LastScan = sc_None;
}

// 1460:0EDF
void IN_Ack() {
  SYS_Poll();
  Keyboard[LastScan] = false;
  LastScan = sc_None;
  IN_AckBack();
}

// 1460:0F43
boolean IN_IsUserInput() {
  SYS_Poll();
  return LastScan != sc_None;
}

// 1460:0F92
boolean IN_UserInput(longword delay, boolean clear) {
  longword lasttime = TimeCount;
  do {
    if (IN_IsUserInput()) {
      if (clear) IN_AckBack();
      return true;
    }
    SYS_Idle();
  } while (TimeCount - lasttime < delay);
  return false;
}
