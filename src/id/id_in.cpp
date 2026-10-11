// ID_IN: input (segment 1460).
#include "id/id_in.h"

#include <cstdlib>

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

}  // namespace

void IN_PumpEvents() {
  keen4::platform::KeyEvent ev;
  while (keen4::platform::Get().PollKey(&ev)) {
    if (ev.scancode == 0xE1) {
      // The Pause key sends E1 1D 45 E1 9D C5 on press only.
      if (ev.down) Paused = true;
      continue;
    }
    byte k = static_cast<byte>((ev.scancode & 0x7F) | (ev.down ? 0 : 0x80));
    INL_KeyService(k, ev.extended);
  }
}

// 1460:0794
void IN_Startup() {
  if (IN_Started) return;
  for (int i = 0; i < 128; i++) {
    ASCIINames[i] = *g_exe.At(0x1F68, static_cast<std::uint16_t>(i));
    ShiftNames[i] = *g_exe.At(0x1F70, static_cast<std::uint16_t>(i));
    SpecialNames[i] = *g_exe.At(0x1F78, static_cast<std::uint16_t>(i));
  }
  for (int i = 0; i < 9; i++) DirTable[i] = g_exe.Int(static_cast<std::uint16_t>(0x3D28 + i * 2));
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
      default:
        // Joysticks and the mouse are not supported by the port yet.
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
