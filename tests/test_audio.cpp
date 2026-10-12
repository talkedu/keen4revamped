// Remastered audio: decoding, resampling, the song and sound manifest, and
// ID_SD mixing recordings in place of the OPL while the original sound code
// keeps its timing.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "audio/audio_file.h"
#include "audio/remaster_audio.h"
#include "id/id_ca.h"
#include "id/id_sd.h"
#include "platform/headless/headless_platform.h"

[[noreturn]] void Quit(const char* error) {
  std::printf("Quit: %s\n", error ? error : "");
  std::exit(1);
}

static int failures = 0;

#define CHECK(cond)                                              \
  do {                                                           \
    if (!(cond)) {                                               \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                \
    }                                                            \
  } while (0)

namespace {

namespace fs = std::filesystem;
using keen4::audio::OpenAudioFile;

// A 16-bit PCM WAV of a constant level per channel.
void WriteWav(const fs::path& path, int rate, int channels, int frames, std::int16_t left, std::int16_t right) {
  std::ofstream f(path, std::ios::binary);
  auto u32 = [&](std::uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
  auto u16 = [&](std::uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };
  std::uint32_t bytes = static_cast<std::uint32_t>(frames * channels * 2);
  f.write("RIFF", 4);
  u32(36 + bytes);
  f.write("WAVEfmt ", 8);
  u32(16);
  u16(1);
  u16(static_cast<std::uint16_t>(channels));
  u32(static_cast<std::uint32_t>(rate));
  u32(static_cast<std::uint32_t>(rate * channels * 2));
  u16(static_cast<std::uint16_t>(channels * 2));
  u16(16);
  f.write("data", 4);
  u32(bytes);
  for (int i = 0; i < frames; i++) {
    u16(static_cast<std::uint16_t>(left));
    if (channels == 2) u16(static_cast<std::uint16_t>(right));
  }
}

bool Near(float a, float b) { return std::fabs(a - b) < 0.002f; }

// Headless, but with an audio device the test pulls samples from by hand.
class TestPlatform : public keen4::platform::HeadlessPlatform {
 public:
  int StartAudio(keen4::platform::AudioCallback callback, void* user) override {
    callback_ = callback;
    user_ = user;
    return 48000;
  }
  std::vector<float> Pull(int frames) {
    std::vector<float> out(static_cast<std::size_t>(frames) * 2);
    callback_(out.data(), frames, user_);
    return out;
  }

 private:
  keen4::platform::AudioCallback callback_ = nullptr;
  void* user_ = nullptr;
};

}  // namespace

int main() {
  fs::path root = fs::temp_directory_path() / "keen4-audio-test";
  fs::remove_all(root);
  fs::create_directories(root);
  const float kLeft = 8192 / 32768.0f, kRight = -16384 / 32768.0f;
  WriteWav(root / "stereo.wav", 24000, 2, 2400, 8192, -16384);
  WriteWav(root / "mono.wav", 48000, 1, 480, 4096, 0);
  std::ofstream(root / "junk.ogg") << "OggS but not really";

  // Decoding: stereo stays stereo, mono is copied to both sides.
  {
    std::string error;
    auto f = OpenAudioFile((root / "stereo.wav").string(), &error);
    CHECK(f && f->rate() == 24000);
    std::vector<float> buf(5000 * 2);
    int n = f ? f->Read(buf.data(), 5000) : 0;
    CHECK(n == 2400 && Near(buf[0], kLeft) && Near(buf[1], kRight) && Near(buf[2399 * 2 + 1], kRight));
    auto m = OpenAudioFile((root / "mono.wav").string(), &error);
    n = m ? m->Read(buf.data(), 1000) : 0;
    CHECK(n == 480 && Near(buf[0], 4096 / 32768.0f) && Near(buf[1], 4096 / 32768.0f));
    CHECK(!OpenAudioFile((root / "junk.ogg").string(), &error) && !error.empty());
    CHECK(!OpenAudioFile((root / "missing.wav").string(), &error));
  }

  // Resampling 24 kHz to 48 kHz doubles the length; a loop never ends.
  {
    std::vector<float> all;
    std::string error;
    CHECK(keen4::audio::DecodeAll((root / "stereo.wav").string(), 48000, &all, &error));
    CHECK(all.size() / 2 >= 4798 && all.size() / 2 <= 4802);
    CHECK(Near(all[1000], kLeft) && Near(all[1001], kRight));

    keen4::audio::Resampler loop(OpenAudioFile((root / "stereo.wav").string(), &error), 48000, true, 100);
    std::vector<float> buf(20000 * 2);
    CHECK(loop.Read(buf.data(), 20000) && !loop.ended());
    CHECK(Near(buf[19999 * 2], kLeft));
  }

  std::ofstream(root / "manifest.txt") << "# remastered audio\n"
                                          "music 3 stereo.wav loop 0.01 volume 0.5\n"
                                          "music 4 missing.ogg\n"
                                          "sound 5 mono.wav\r\n"
                                          "sound 9 junk.ogg\n";
  TestPlatform platform;
  keen4::platform::Set(&platform);

  // The manifest: unreadable files are skipped, so the original plays instead.
  {
    keen4::audio::Options options;
    options.dir = root.string();
    options.music_volume = 50;
    keen4::audio::RemasterAudio r;
    r.Load(options, 48000);
    CHECK(r.HasSong(3) && !r.HasSong(4) && r.HasSound(5) && !r.HasSound(9));
    CHECK(Near(r.music_gain(), 0.5f) && Near(r.sfx_gain(), 1));

    std::vector<float> out(256 * 2, 0.0f);
    r.StartSong(3);
    CHECK(r.MixMusic(out.data(), 256) && Near(out[200], kLeft * 0.25f));
    r.StartSong(2);
    CHECK(!r.MixMusic(out.data(), 256));
    CHECK(r.PlaySound(5) && r.MixSound(out.data(), 256));
    CHECK(!r.PlaySound(6) && !r.MixSound(out.data(), 256));
    r.SetEnabled(false);
    r.StartSong(3);
    CHECK(!r.MixMusic(out.data(), 256) && !r.PlaySound(5));
  }

  // ID_SD: the recording replaces song 3 and sound 5, and the original sound
  // still runs underneath, so it ends exactly when it did before.
  {
    keen4::audio::AudioOptions().dir = root.string();
    SD_Startup();
    CHECK(keen4::audio::Remaster().loaded() && keen4::audio::Remaster().HasSong(3));
    SD_SetMusicMode(smm_AdLib);
    SD_SetSoundMode(sdm_AdLib);

    // song 3: one register write, then a long wait
    std::vector<std::uint16_t> song = {4, 0x0020, 1000};
    audiosegs[STARTMUSIC + 3] = reinterpret_cast<byte*>(song.data());
    SD_StartMusic(reinterpret_cast<MusicGroup*>(song.data()));
    std::vector<float> out = platform.Pull(512);
    CHECK(Near(out[400], kLeft * 0.5f) && Near(out[401], kRight * 0.5f));

    // AdLib sound 5: 10 notes at 140 Hz
    std::vector<byte> snd(0x17 + 10, 0);
    snd[0] = 10;          // length
    snd[4] = 1;           // priority
    snd[6 + 6] = 0xF0;    // instrument: modulator attack/decay, so it is valid
    snd[0x16] = 4;        // block
    for (int i = 0; i < 10; i++) snd[0x17 + i] = 0x80;
    audiosegs[STARTADLIBSOUNDS + 5] = snd.data();
    audiosegs[STARTMUSIC + 3] = nullptr;
    SD_PlaySound(5);
    CHECK(SD_SoundPlaying() == 5);
    out = platform.Pull(512);
    // music and the recorded effect together; the OPL effect is muted
    CHECK(Near(out[400], kLeft * 0.5f + 4096 / 32768.0f));
    platform.Pull(48000 * 10 / 140 + 1000);
    CHECK(SD_SoundPlaying() == 0);

    SD_MusicOff();
    out = platform.Pull(256);
    CHECK(Near(out[200], 0));
    audiosegs[STARTADLIBSOUNDS + 5] = nullptr;
    SD_Shutdown();
  }

  fs::remove_all(root);
  if (failures == 0) std::printf("all audio tests passed\n");
  return failures == 0 ? 0 : 1;
}
