#pragma once

// ID_IN: keyboard, joystick, mouse and demo input (segment 1460).

#include "id/id_heads.h"

using ScanCode = byte;
inline constexpr ScanCode sc_None = 0, sc_Bad = 0xFF, sc_Return = 0x1C, sc_Enter = sc_Return,
                          sc_Escape = 0x01, sc_Space = 0x39, sc_BackSpace = 0x0E, sc_Tab = 0x0F,
                          sc_Alt = 0x38, sc_Control = 0x1D, sc_CapsLock = 0x3A, sc_LShift = 0x2A,
                          sc_RShift = 0x36, sc_UpArrow = 0x48, sc_DownArrow = 0x50,
                          sc_LeftArrow = 0x4B, sc_RightArrow = 0x4D, sc_Insert = 0x52,
                          sc_Delete = 0x53, sc_Home = 0x47, sc_End = 0x4F, sc_PgUp = 0x49,
                          sc_PgDn = 0x51, sc_F1 = 0x3B, sc_F2 = 0x3C, sc_F3 = 0x3D, sc_F4 = 0x3E,
                          sc_F5 = 0x3F, sc_F6 = 0x40, sc_F7 = 0x41, sc_F8 = 0x42, sc_F9 = 0x43,
                          sc_F10 = 0x44, sc_F11 = 0x57, sc_F12 = 0x58, sc_1 = 0x02, sc_2 = 0x03,
                          sc_3 = 0x04, sc_4 = 0x05, sc_5 = 0x06, sc_6 = 0x07, sc_7 = 0x08,
                          sc_8 = 0x09, sc_9 = 0x0A, sc_0 = 0x0B, sc_A = 0x1E, sc_B = 0x30,
                          sc_C = 0x2E, sc_D = 0x20, sc_E = 0x12, sc_F = 0x21, sc_G = 0x22,
                          sc_H = 0x23, sc_I = 0x17, sc_J = 0x24, sc_K = 0x25, sc_L = 0x26,
                          sc_M = 0x32, sc_N = 0x31, sc_O = 0x18, sc_P = 0x19, sc_Q = 0x10,
                          sc_R = 0x13, sc_S = 0x1F, sc_T = 0x14, sc_U = 0x16, sc_V = 0x2F,
                          sc_W = 0x11, sc_X = 0x2D, sc_Y = 0x15, sc_Z = 0x2C;

inline constexpr int key_None = 0;

enum Direction : sword {
  dir_North, dir_NorthEast, dir_East, dir_SouthEast, dir_South, dir_SouthWest, dir_West,
  dir_NorthWest, dir_None
};

enum ControlType : sword {
  ctrl_Keyboard, ctrl_Keyboard1 = ctrl_Keyboard, ctrl_Keyboard2, ctrl_Joystick,
  ctrl_Joystick1 = ctrl_Joystick, ctrl_Joystick2, ctrl_Mouse
};

enum Demo : sword { demo_Off, demo_Record, demo_Playback, demo_PlayDone };

// Layout as in DGROUP (size 0x0E).
struct ControlInfo {
  boolean button0, button1;
  sword x, y;
  sword xaxis, yaxis;  // -1, 0, 1
  sword dir;           // Direction
};

// Ten scancodes per keyboard player (DGROUP 3BC2).
struct KeyboardDef {
  byte button0, button1, upleft, up, upright, left, right, downleft, down, downright;
};

extern boolean Keyboard[128];
extern boolean Paused;
extern char LastASCII;
extern ScanCode LastScan;
extern KeyboardDef KbdDefs[2];
extern ControlType Controls[4];
extern Demo DemoMode;
extern byte* DemoBuffer;
extern word DemoOffset, DemoSize;
extern boolean MousePresent;
extern boolean JoysPresent[2];

void IN_Startup();
void IN_Shutdown();
void IN_Default(boolean gotit, ControlType in);
void IN_SetKeyHook(void (*hook)());
void IN_ClearKeysDown();
void IN_ReadControl(int player, ControlInfo* info);
void IN_SetControlType(int player, ControlType type);
boolean IN_StartDemoRecord(word bufsize);
void IN_StartDemoPlayback(byte* buffer, word bufsize);
void IN_StopDemo();
void IN_FreeDemoBuffer();
const char* IN_GetScanName(ScanCode scan);
ScanCode IN_WaitForKey();
char IN_WaitForASCII();
void IN_AckBack();
void IN_Ack();
boolean IN_IsUserInput();
boolean IN_UserInput(longword delay, boolean clear);

// Port: drains platform key events into the state above (the keyboard ISR),
// and reads the gamepad.
void IN_PumpEvents();

// Port: gamepad support. The newest connected SDL gamepad is joystick 1, and
// Controls[0] switches to it when it connects or is used, and back to the
// keyboard when a key is pressed or the pad is unplugged.
// The play loop calls this every frame; while it does, the pad steers Keen.
// Elsewhere its buttons type the keys menus wait for (stick and d-pad as
// arrows, A Return, B and Start Escape).
void IN_NoteGameplay();
// True while a gamepad is connected and Controls[0] is a joystick.
boolean IN_GamepadActive();
// Face buttons held, numbered like a four-button Gravis pad: bit 0 A, 1 B,
// 2 X, 3 Y.
word IN_JoyButtons();

// Port: every gamepad (co-op gives each player one; the functions above keep
// following the newest pad). Pads are listed oldest first; a pad's id stays
// the same while it is plugged in and is never 0.
int IN_GamepadCount();
std::uint32_t IN_GamepadId(int index);
// Buttons of IN_ReadGamepad beyond the four of IN_JoyButtons.
inline constexpr word kJoyBack = 16, kJoyStart = 32;
// Reads pad `id` as IN_ReadControl reads joystick 1 (digital stick or d-pad,
// A is button0, B button1) and its buttons as IN_JoyButtons numbers them, plus
// kJoyBack and kJoyStart. False, with everything zero, if it is not plugged in.
boolean IN_ReadGamepad(std::uint32_t id, ControlInfo* info, word* buttons);
// Port: stands for "the first gamepad the other player doesn't use" where a
// gamepad id is expected (co-op); 0 there means the keyboard.
inline constexpr std::uint32_t kAnyGamepad = 0xFFFFFFFFu;
