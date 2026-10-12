#pragma once

// ID_SD: sound (segment 1874).
//
// The original drives the PC speaker and an AdLib card from a timer
// interrupt: music at 560 Hz, sound effects at 140 Hz, TimeCount at 70 Hz.
// The port runs the same services from the audio callback, clocked by the
// sample count, and feeds an emulated OPL2 (Nuked OPL3) and a square-wave PC
// speaker. TimeCount follows real time in the game thread (see ID_SYS).

#include "id/id_heads.h"

enum SDMode : sword { sdm_Off, sdm_PC, sdm_AdLib };
enum SMMode : sword { smm_Off, smm_AdLib };

inline constexpr int NUMSOUNDS = 52;
inline constexpr int STARTPCSOUNDS = 0;
inline constexpr int STARTADLIBSOUNDS = NUMSOUNDS;
inline constexpr int STARTDIGISOUNDS = 2 * NUMSOUNDS;
inline constexpr int STARTMUSIC = 3 * NUMSOUNDS;
inline constexpr int LASTMUSTRACK = 6;

// IMF song data as cached in audiosegs: byte length, then (reg, value, delay)
// records of four bytes.
struct MusicGroup {
  word length;
  word values[1];
};

extern boolean AdLibPresent;
extern SDMode SoundMode;
extern SMMode MusicMode;
extern longword TimeCount;

void SD_Startup();
void SD_Shutdown();
void SD_Default(boolean gotit, SDMode sd, SMMode sm);
boolean SD_SetSoundMode(SDMode mode);
boolean SD_SetMusicMode(SMMode mode);
void SD_SetUserHook(void (*hook)());
void SD_PlaySound(int sound);
word SD_SoundPlaying();
void SD_StopSound();
void SD_WaitSoundDone();
void SD_MusicOn();
void SD_MusicOff();
void SD_StartMusic(MusicGroup* music);
void SD_FadeOutMusic();
boolean SD_MusicPlaying();
