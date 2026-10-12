#include "audio/remaster_audio.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "platform/platform.h"

namespace keen4::audio {

namespace {

class AudioLock {
 public:
  AudioLock() { platform::Get().LockAudio(); }
  ~AudioLock() { platform::Get().UnlockAudio(); }
};

float Gain(int percent) { return std::clamp(percent, 0, 100) / 100.0f; }

}  // namespace

void RemasterAudio::Load(const Options& options, int rate) {
  rate_ = rate;
  enabled_ = options.remastered;
  music_gain_ = Gain(options.music_volume);
  sfx_gain_ = Gain(options.sfx_volume);
  songs_.clear();
  sounds_.clear();
  if (options.dir.empty() || rate <= 0) return;
  std::filesystem::path dir(options.dir);
  std::ifstream in(dir / "manifest.txt");
  if (!in) {
    std::fprintf(stderr, "audio: no manifest.txt in %s\n", options.dir.c_str());
    return;
  }
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    std::istringstream ss(line);
    std::string kind, file, extra;
    int number;
    if (!(ss >> kind >> number >> file)) continue;
    double loop = 0;
    float volume = 1;
    while (ss >> extra) {
      if (extra == "loop")
        ss >> loop;
      else if (extra == "volume")
        ss >> volume;
    }
    std::string path = (dir / file).string();
    if (kind == "music") {
      // checked now so a typo shows at startup, decoded while it plays
      std::string error;
      if (!OpenAudioFile(path, &error)) {
        std::fprintf(stderr, "audio: cannot read %s for song %d (%s), using the original\n", path.c_str(), number,
                     error.c_str());
        continue;
      }
      songs_[number] = Song{path, loop, volume};
    } else if (kind == "sound") {
      Sound sound;
      std::string error;
      if (!DecodeAll(path, rate, &sound.stereo, &error)) {
        std::fprintf(stderr, "audio: cannot read %s for sound %d (%s), using the original\n", path.c_str(), number,
                     error.c_str());
        continue;
      }
      sound.volume = volume;
      sounds_[number] = std::move(sound);
    }
  }
  std::fprintf(stderr, "audio: %d songs and %d sounds from %s\n", songs(), sounds(), options.dir.c_str());
}

void RemasterAudio::OpenSong() {
  std::unique_ptr<Resampler> music;
  float volume = 1;
  auto found = songs_.find(song_);
  if (enabled_ && found != songs_.end()) {
    std::string error;
    if (auto file = OpenAudioFile(found->second.path, &error)) {
      auto loop = static_cast<std::uint64_t>(found->second.loop_seconds * file->rate());
      music = std::make_unique<Resampler>(std::move(file), rate_, true, loop);
      volume = found->second.volume;
    }
  }
  {
    AudioLock lock;
    std::swap(music_, music);
    music_volume_ = volume;
  }
  // the old stream closes here, outside the lock
}

void RemasterAudio::SetEnabled(bool on) {
  if (on == enabled_) return;
  enabled_ = on;
  if (!loaded()) return;
  OpenSong();  // the current song continues as the other version, from the start
  if (!on) StopSound();
}

void RemasterAudio::StartSong(int song) {
  song_ = song;
  paused_ = false;
  if (!loaded()) return;
  OpenSong();
  if (song >= 0)
    std::fprintf(stderr, "audio: song %d (%s)\n", song, enabled_ && HasSong(song) ? "remastered" : "original");
}

void RemasterAudio::PauseSong(bool paused) {
  AudioLock lock;
  paused_ = paused;
}

bool RemasterAudio::PlaySound(int sound) {
  auto found = sounds_.find(sound);
  AudioLock lock;
  sound_pos_ = 0;
  sound_ = enabled_ && found != sounds_.end() ? &found->second : nullptr;
  return sound_ != nullptr;
}

void RemasterAudio::StopSound() {
  AudioLock lock;
  sound_ = nullptr;
}

bool RemasterAudio::MixMusic(float* out, int frames) {
  if (!music_) return false;
  if (paused_) return true;  // the original is paused too
  scratch_.resize(static_cast<std::size_t>(frames) * 2);
  music_->Read(scratch_.data(), frames);
  float g = music_gain_ * music_volume_;
  for (int i = 0; i < frames * 2; i++) out[i] += scratch_[i] * g;
  return true;
}

bool RemasterAudio::MixSound(float* out, int frames) {
  if (!sound_) return false;
  float g = sfx_gain_ * sound_->volume;
  const std::vector<float>& s = sound_->stereo;
  for (int i = 0; i < frames && sound_pos_ < s.size(); i++, sound_pos_ += 2) {
    out[i * 2] += s[sound_pos_] * g;
    out[i * 2 + 1] += s[sound_pos_ + 1] * g;
  }
  return true;  // keeps the original muted until the next sound starts
}

RemasterAudio& Remaster() {
  static RemasterAudio remaster;
  return remaster;
}

Options& AudioOptions() {
  static Options options;
  return options;
}

}  // namespace keen4::audio
