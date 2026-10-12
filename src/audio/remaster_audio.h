#pragma once

// Remastered audio (milestone 7): new recordings that replace the original
// AdLib songs by song number and sound effects by sound number, listed in
// audio/manifest.txt (see docs/AUDIO.md). Anything not listed plays through
// the emulated OPL2 and PC speaker as before.
//
// ID_SD keeps running the original sound code in every case, so when a sound
// ends, and everything the game decides from that, is unchanged; this module
// only decides what is heard. Every call except Mix* comes from the game
// thread and takes the audio lock; Mix* run inside the audio callback.

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "audio/audio_file.h"

namespace keen4::audio {

struct Options {
  std::string dir;          // folder with manifest.txt, empty for none
  bool remastered = true;   // false: always the original sound
  int music_volume = 100;   // 0..100
  int sfx_volume = 100;     // 0..100
};

class RemasterAudio {
 public:
  // Reads `options.dir`/manifest.txt and decodes the effects at `rate`.
  void Load(const Options& options, int rate);
  bool loaded() const { return rate_ > 0; }

  // Switches between remastered and original sound while playing.
  void SetEnabled(bool on);
  bool enabled() const { return enabled_; }
  bool HasSong(int song) const { return songs_.count(song) != 0; }
  bool HasSound(int sound) const { return sounds_.count(sound) != 0; }
  int songs() const { return static_cast<int>(songs_.size()); }
  int sounds() const { return static_cast<int>(sounds_.size()); }

  float music_gain() const { return music_gain_; }
  float sfx_gain() const { return sfx_gain_; }

  // Game thread. Song -1 means a song with no number (silence for us).
  void StartSong(int song);
  void PauseSong(bool paused);
  // True if a recording now plays for `sound`; the original sound is muted
  // until another sound starts.
  bool PlaySound(int sound);
  void StopSound();

  // Audio thread: add the remastered music or effect for `frames` frames to
  // `out` (stereo). False if there is none, so the OPL output should be heard.
  bool MixMusic(float* out, int frames);
  bool MixSound(float* out, int frames);

 private:
  struct Song {
    std::string path;
    double loop_seconds = 0;
    float volume = 1;
  };
  struct Sound {
    std::vector<float> stereo;
    float volume = 1;
  };
  void OpenSong();

  int rate_ = 0;
  bool enabled_ = true;
  float music_gain_ = 1, sfx_gain_ = 1;
  std::map<int, Song> songs_;
  std::map<int, Sound> sounds_;

  int song_ = -1;
  bool paused_ = false;
  std::unique_ptr<Resampler> music_;
  float music_volume_ = 1;
  const Sound* sound_ = nullptr;
  std::size_t sound_pos_ = 0;
  std::vector<float> scratch_;
};

// The one instance ID_SD uses; the SDL backend toggles it with Shift+F11.
RemasterAudio& Remaster();
Options& AudioOptions();

}  // namespace keen4::audio
