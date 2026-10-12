#pragma once

// Decoding of remastered audio files: OGG Vorbis (stb_vorbis), FLAC (dr_flac)
// and WAV (dr_wav), always delivered as interleaved stereo float frames, and
// a streaming resampler to the output rate.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace keen4::audio {

// A decoded file, read in order, at its own sample rate.
class AudioFile {
 public:
  virtual ~AudioFile() = default;
  int rate() const { return rate_; }
  // Reads up to `frames` stereo frames into `out` (2 floats each); returns the
  // number read, 0 at the end.
  virtual int Read(float* out, int frames) = 0;
  virtual bool Seek(std::uint64_t frame) = 0;

 protected:
  int rate_ = 0;
};

// Opens an .ogg, .flac or .wav file (chosen by its contents). Returns null and
// sets `error` if it cannot be read.
std::unique_ptr<AudioFile> OpenAudioFile(const std::string& path, std::string* error);

// Plays an AudioFile at another sample rate with linear interpolation,
// optionally looping back to `loop_frame` (in the file's own frames).
class Resampler {
 public:
  Resampler(std::unique_ptr<AudioFile> file, int out_rate, bool loop, std::uint64_t loop_frame);
  // Fills `frames` stereo frames; after the end (without loop) fills silence.
  // Returns false once the file has ended.
  bool Read(float* out, int frames);
  bool ended() const { return ended_; }

 private:
  bool Next(float* frame);

  std::unique_ptr<AudioFile> file_;
  double step_;
  double pos_ = 0;
  float a_[2] = {}, b_[2] = {};
  bool loop_;
  std::uint64_t loop_frame_;
  bool eof_ = false;    // the file has no more frames after b_
  bool ended_ = false;  // and b_ has been played out
  std::vector<float> buf_;
  int buf_len_ = 0, buf_pos_ = 0;
};

// Decodes a whole file at `out_rate`, for short sound effects.
bool DecodeAll(const std::string& path, int out_rate, std::vector<float>* stereo, std::string* error);

}  // namespace keen4::audio
