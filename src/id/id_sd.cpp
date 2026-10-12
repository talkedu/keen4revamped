// ID_SD: sound (segment 1874).
#include "id/id_sd.h"

#include <algorithm>
#include <vector>

#include "audio/remaster_audio.h"
#include "id/id_ca.h"
#include "id/id_sys.h"
#include "platform/platform.h"

extern "C" {
#include "opl3.h"
}

boolean AdLibPresent;
SDMode SoundMode;
SMMode MusicMode;
longword TimeCount;

namespace {

// Sound chunk header (SoundCommon) and AdLib instrument, as stored in AUDIO.CK4.
constexpr int kCommonSize = 6;     // longword length; word priority
constexpr int kInstrumentOfs = 6;  // 16 bytes
constexpr int kBlockOfs = 0x16;
constexpr int kAdLibDataOfs = 0x17;

boolean SD_Started;
void (*SoundUserHook)();

word SoundNumber, SoundPriority;
byte** SoundTable;  // &audiosegs[STARTPCSOUNDS or STARTADLIBSOUNDS]

// PC speaker (1874:007B..0144).
const byte* pcSound;
longword pcLengthLeft;
byte pcLastSample;
word pcSoundLookup[255];
word pcDivisor;  // 0 = speaker off

// AdLib sound effects (1874:01A0..0356).
const byte* alSound;
longword alLengthLeft;
byte alBlock;

// Music sequencer (1874:03DE).
boolean sqActive;
const word* sqHack;
const word* sqHackPtr;
word sqHackLen, sqHackSeqLen;
longword sqHackTime, alTimeCount;

// Output. Music and sound effects each get their own emulated OPL2, so they
// can have separate volumes and either can be replaced by a recording. The
// original keeps effects on channel 0 and music on channels 1-8, so the sum
// of the two chips is what one card would play.
opl3_chip oplMusic, oplFx;
std::vector<float> musicBuf, fxBuf;
int sampleRate;
int tickPhase;      // accumulates 560 Hz against the sample rate
int serviceCount;   // 560 Hz tick counter
double pcPhase;
bool audioRunning;
std::uint64_t lastServiceMillis;

constexpr byte modifiers[1] = {0};
constexpr byte carriers[1] = {3};

longword ReadLong(const byte* p) {
  return static_cast<longword>(p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<longword>(p[3]) << 24));
}
word ReadWord(const byte* p) { return static_cast<word>(p[0] | (p[1] << 8)); }

// 1874:015E, for sound effects.
void alOut(byte reg, byte value) { OPL3_WriteReg(&oplFx, reg, value); }
void alOutMusic(byte reg, byte value) { OPL3_WriteReg(&oplMusic, reg, value); }
void alOutBoth(byte reg, byte value) {
  alOut(reg, value);
  alOutMusic(reg, value);
}

void Lock() { keen4::platform::Get().LockAudio(); }
void Unlock() { keen4::platform::Get().UnlockAudio(); }

// 1874:00A9
void SDL_PCStopSound() {
  pcSound = nullptr;
  pcDivisor = 0;
}

// 1874:007B
void SDL_PCPlaySound(const byte* sound) {
  pcLastSample = 0xFF;
  pcLengthLeft = ReadLong(sound);
  pcSound = sound + kCommonSize;
}

// 1874:00C3, 140 Hz.
void SDL_PCService() {
  if (!pcSound) return;
  byte s = *pcSound++;
  if (s != pcLastSample) {
    pcLastSample = s;
    pcDivisor = s ? pcSoundLookup[s] : 0;
  }
  if (!--pcLengthLeft) {
    SDL_PCStopSound();
    SoundNumber = SoundPriority = 0;
  }
}

// 1874:0144
void SDL_ShutPC() {
  pcSound = nullptr;
  pcDivisor = 0;
}

// 1874:01A0
void SDL_ALStopSound() {
  alSound = nullptr;
  alOut(0xB0, 0);
}

// 1874:01C1
void SDL_AlSetFXInst(const byte* inst) {
  byte m = modifiers[0], c = carriers[0];
  alOut(static_cast<byte>(m + 0x20), inst[0]);
  alOut(static_cast<byte>(m + 0x40), inst[2]);
  alOut(static_cast<byte>(m + 0x60), inst[4]);
  alOut(static_cast<byte>(m + 0x80), inst[6]);
  alOut(static_cast<byte>(m + 0xE0), inst[8]);
  alOut(static_cast<byte>(c + 0x20), inst[1]);
  alOut(static_cast<byte>(c + 0x40), inst[3]);
  alOut(static_cast<byte>(c + 0x60), inst[5]);
  alOut(static_cast<byte>(c + 0x80), inst[7]);
  alOut(static_cast<byte>(c + 0xE0), inst[9]);
}

// 1874:02D7
void SDL_ALPlaySound(const byte* sound) {
  SDL_ALStopSound();
  alLengthLeft = ReadLong(sound);
  alSound = sound + kAdLibDataOfs;
  alBlock = static_cast<byte>(((sound[kBlockOfs] & 7) << 2) | 0x20);
  const byte* inst = sound + kInstrumentOfs;
  if (!(inst[6] | inst[7])) Quit("SDL_ALPlaySound() - Bad instrument");
  SDL_AlSetFXInst(inst);
}

// 1874:0356, 140 Hz.
void SDL_ALSoundService() {
  if (!alSound) return;
  byte s = *alSound++;
  if (!s) {
    alOut(0xB0, 0);
  } else {
    alOut(0xA0, s);
    alOut(0xB0, alBlock);
  }
  if (!--alLengthLeft) {
    alSound = nullptr;
    alOut(0xB0, 0);
    SoundNumber = SoundPriority = 0;
  }
}

// 1874:03DE, 560 Hz.
void SDL_ALService() {
  if (!sqActive) return;
  while (sqHackLen && sqHackTime <= alTimeCount) {
    word w = *sqHackPtr++;
    sqHackTime = alTimeCount + *sqHackPtr++;
    alOutMusic(static_cast<byte>(w & 0xFF), static_cast<byte>(w >> 8));
    sqHackLen = static_cast<word>(sqHackLen - 4);
  }
  alTimeCount++;
  if (!sqHackLen) {
    sqHackPtr = sqHack;
    sqHackLen = sqHackSeqLen;
    alTimeCount = sqHackTime = 0;
  }
}

// 1874:0491
void SDL_ShutAL() {
  alOutBoth(0xBD, 0);
  alOutBoth(0xB0, 0);
  alSound = nullptr;
}

// 1874:04CB
void SDL_CleanAL() {
  alOutBoth(0xBD, 0);
  for (int i = 1; i < 0xF5; i++) alOutBoth(static_cast<byte>(i), 0);
}

// 1874:0504
void SDL_StartAL() { alOutBoth(0xBD, 0); }

// 1874:06D7
void SDL_ShutDevice() {
  switch (SoundMode) {
    case sdm_PC: SDL_ShutPC(); break;
    case sdm_AdLib: SDL_ShutAL(); break;
    default: break;
  }
  SoundMode = sdm_Off;
}

// 1874:0712
void SDL_StartDevice() {
  if (SoundMode == sdm_AdLib) SDL_StartAL();
  SoundNumber = SoundPriority = 0;
}

// One tick of the 560 Hz timer interrupt (1874:05F5) minus TimeCount.
void SDL_Tick() {
  if (MusicMode == smm_AdLib) SDL_ALService();
  if (++serviceCount & 3) return;
  switch (SoundMode) {
    case sdm_PC: SDL_PCService(); break;
    case sdm_AdLib: SDL_ALSoundService(); break;
    default: break;
  }
}

// Fills `count` stereo frames: the original music and effects, or the
// remastered recordings that replace them, each at its own volume.
void AudioCallback(float* frames, int count, void*) {
  musicBuf.resize(static_cast<std::size_t>(count));
  fxBuf.resize(static_cast<std::size_t>(count));
  for (int i = 0; i < count; i++) {
    tickPhase += 560;
    while (tickPhase >= sampleRate) {
      tickPhase -= sampleRate;
      SDL_Tick();
    }
    std::int16_t buf[2];
    OPL3_GenerateResampled(&oplMusic, buf);
    musicBuf[i] = buf[0] / 32768.0f;
    OPL3_GenerateResampled(&oplFx, buf);
    float fx = buf[0];
    if (pcDivisor) {
      double freq = 1193180.0 / pcDivisor;
      pcPhase += freq / sampleRate;
      if (pcPhase >= 1.0) pcPhase -= static_cast<int>(pcPhase);
      fx += pcPhase < 0.5 ? 4000 : -4000;
    }
    fxBuf[i] = fx / 32768.0f;
  }
  std::fill(frames, frames + count * 2, 0.0f);
  keen4::audio::RemasterAudio& remaster = keen4::audio::Remaster();
  if (!remaster.MixMusic(frames, count)) {
    for (int i = 0; i < count; i++) {
      frames[i * 2] += musicBuf[i] * remaster.music_gain();
      frames[i * 2 + 1] += musicBuf[i] * remaster.music_gain();
    }
  }
  if (!remaster.MixSound(frames, count)) {
    for (int i = 0; i < count; i++) {
      frames[i * 2] += fxBuf[i] * remaster.sfx_gain();
      frames[i * 2 + 1] += fxBuf[i] * remaster.sfx_gain();
    }
  }
  for (int i = 0; i < count * 2; i++) frames[i] = std::clamp(frames[i], -1.0f, 1.0f);
}

}  // namespace

// Without an audio device the services are clocked from SYS_Poll so that
// sounds still end and SD_WaitSoundDone returns.
void SD_ServiceWithoutAudio() {
  if (audioRunning || !SD_Started) return;
  std::uint64_t now = keen4::platform::Get().Millis();
  std::uint64_t ticks = (now - lastServiceMillis) * 560 / 1000;
  if (!ticks) return;
  lastServiceMillis += ticks * 1000 / 560;
  if (ticks > 560) ticks = 560;
  while (ticks--) SDL_Tick();
}

// 1874:074B
boolean SD_SetSoundMode(SDMode mode) {
  boolean result = true;
  word tableoffset = 0;
  SD_StopSound();
  switch (mode) {
    case sdm_Off: break;
    case sdm_PC: tableoffset = STARTPCSOUNDS; break;
    case sdm_AdLib:
      if (!AdLibPresent) {
        result = false;
        break;
      }
      tableoffset = STARTADLIBSOUNDS;
      break;
    default: result = false; break;
  }
  if (result && mode != SoundMode) {
    Lock();
    SDL_ShutDevice();
    SoundMode = mode;
    SoundTable = &audiosegs[tableoffset];
    SDL_StartDevice();
    Unlock();
  }
  return result;
}

// 1874:07D0
boolean SD_SetMusicMode(SMMode mode) {
  boolean result = false;
  SD_FadeOutMusic();
  switch (mode) {
    case smm_Off: result = true; break;
    case smm_AdLib:
      if (AdLibPresent) result = true;
      break;
  }
  if (result) MusicMode = mode;
  return result;
}

// 1874:0826
void SD_Startup() {
  if (SD_Started) return;
  OPL3_Reset(&oplMusic, 49716);
  OPL3_Reset(&oplFx, 49716);
  SoundUserHook = nullptr;
  TimeCount = 0;
  alTimeCount = 0;
  SoundMode = sdm_Off;
  MusicMode = smm_Off;
  for (int i = 0; i < 255; i++) pcSoundLookup[i] = static_cast<word>(i * 60);

  sampleRate = keen4::platform::Get().StartAudio(AudioCallback, nullptr);
  audioRunning = sampleRate > 0;
  if (audioRunning) {
    OPL3_Reset(&oplMusic, static_cast<std::uint32_t>(sampleRate));
    OPL3_Reset(&oplFx, static_cast<std::uint32_t>(sampleRate));
    keen4::audio::Remaster().Load(keen4::audio::AudioOptions(), sampleRate);
  }
  // The emulated card is always there; without an audio device it is silent.
  AdLibPresent = true;
  SDL_CleanAL();
  lastServiceMillis = keen4::platform::Get().Millis();
  SD_Started = true;
}

// 1874:0913
void SD_Default(boolean gotit, SDMode sd, SMMode sm) {
  boolean gotsd = gotit, gotsm = gotit;
  if (gotsd && sd == sdm_AdLib) gotsd = AdLibPresent;
  if (!gotsd) sd = AdLibPresent ? sdm_AdLib : sdm_PC;
  if (sd != SoundMode) SD_SetSoundMode(sd);
  if (gotsm && sm == smm_AdLib) gotsm = AdLibPresent;
  if (!gotsm) sm = AdLibPresent ? smm_AdLib : smm_Off;
  if (sm != MusicMode) SD_SetMusicMode(sm);
}

// 1874:099F
void SD_Shutdown() {
  if (!SD_Started) return;
  SD_MusicOff();
  Lock();
  SDL_ShutDevice();
  SDL_CleanAL();
  Unlock();
  SD_Started = false;
}

// 1874:09DF
void SD_SetUserHook(void (*hook)()) { SoundUserHook = hook; }

// 1874:09F1
void SD_PlaySound(int sound) {
  if (SoundMode == sdm_Off) return;
  const byte* s = SoundTable[sound];
  if (!s) Quit("SD_PlaySound() - Uncached sound");
  if (!ReadLong(s)) Quit("SD_PlaySound() - Zero length sound");
  word priority = ReadWord(s + 4);
  if (priority < SoundPriority) return;
  Lock();
  switch (SoundMode) {
    case sdm_PC: SDL_PCPlaySound(s); break;
    case sdm_AdLib: SDL_ALPlaySound(s); break;
    default: break;
  }
  SoundNumber = static_cast<word>(sound);
  SoundPriority = priority;
  Unlock();
  keen4::audio::Remaster().PlaySound(sound);
}

// 1874:0A90
word SD_SoundPlaying() {
  boolean result = false;
  switch (SoundMode) {
    case sdm_PC: result = pcSound != nullptr; break;
    case sdm_AdLib: result = alSound != nullptr; break;
    default: break;
  }
  return result ? SoundNumber : 0;
}

// 1874:0AD7
void SD_StopSound() {
  Lock();
  switch (SoundMode) {
    case sdm_PC: SDL_PCStopSound(); break;
    case sdm_AdLib: SDL_ALStopSound(); break;
    default: break;
  }
  SoundPriority = 0;
  SoundNumber = 0;
  Unlock();
  keen4::audio::Remaster().StopSound();
}

// 1874:0AFD
void SD_WaitSoundDone() {
  while (SD_SoundPlaying()) SYS_Idle();
}

// 1874:0B0A
void SD_MusicOn() {
  sqActive = true;
  keen4::audio::Remaster().PauseSong(false);
}

// 1874:0B15
void SD_MusicOff() {
  if (MusicMode == smm_AdLib) {
    Lock();
    alOutMusic(0xBD, 0);
    for (int i = 0; i < 10; i++) alOutMusic(static_cast<byte>(0xB1 + i), 0);
    Unlock();
  }
  sqActive = false;
  keen4::audio::Remaster().PauseSong(true);
}

// 1874:0B62
void SD_StartMusic(MusicGroup* music) {
  SD_MusicOff();
  Lock();
  if (MusicMode == smm_AdLib) {
    sqHackPtr = sqHack = music->values;
    sqHackLen = sqHackSeqLen = music->length;
    sqHackTime = 0;
    alTimeCount = 0;
  }
  Unlock();
  if (MusicMode == smm_AdLib) {
    // the remastered version is chosen by song number
    int song = -1;
    for (int i = 0; i < NUMSNDCHUNKS - STARTMUSIC; i++)
      if (audiosegs[STARTMUSIC + i] == reinterpret_cast<byte*>(music)) song = i;
    keen4::audio::Remaster().StartSong(song);
    SD_MusicOn();
  }
}

// 1874:0BB4
void SD_FadeOutMusic() {
  if (MusicMode == smm_AdLib) SD_MusicOff();
}

// 1874:0BC7
boolean SD_MusicPlaying() { return false; }
