#include "audio/audio_file.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "dr_libs/dr_flac.h"
#include "dr_libs/dr_wav.h"
#define STB_VORBIS_HEADER_ONLY
#include "stb/stb_vorbis.c"

namespace keen4::audio {

namespace {

constexpr int kChunk = 1024;

// Converts `frames` frames of `channels` interleaved samples to stereo in place
// (the buffer holds max(channels, 2) * frames floats).
void ToStereo(float* buf, int frames, int channels) {
  if (channels == 2) return;
  if (channels == 1) {
    for (int i = frames - 1; i >= 0; i--) buf[i * 2] = buf[i * 2 + 1] = buf[i];
    return;
  }
  for (int i = 0; i < frames; i++) {
    buf[i * 2] = buf[i * channels];
    buf[i * 2 + 1] = buf[i * channels + 1];
  }
}

class VorbisFile final : public AudioFile {
 public:
  explicit VorbisFile(stb_vorbis* v) : v_(v) { rate_ = static_cast<int>(stb_vorbis_get_info(v).sample_rate); }
  ~VorbisFile() override { stb_vorbis_close(v_); }
  int Read(float* out, int frames) override {
    return stb_vorbis_get_samples_float_interleaved(v_, 2, out, frames * 2);
  }
  bool Seek(std::uint64_t frame) override { return stb_vorbis_seek(v_, static_cast<unsigned>(frame)) != 0; }

 private:
  stb_vorbis* v_;
};

class FlacFile final : public AudioFile {
 public:
  explicit FlacFile(drflac* f) : f_(f) { rate_ = static_cast<int>(f->sampleRate); }
  ~FlacFile() override { drflac_close(f_); }
  int Read(float* out, int frames) override {
    int channels = f_->channels;
    if (channels <= 2) {
      auto n = static_cast<int>(drflac_read_pcm_frames_f32(f_, static_cast<drflac_uint64>(frames), out));
      ToStereo(out, n, channels);
      return n;
    }
    tmp_.resize(static_cast<std::size_t>(frames) * channels);
    auto n = static_cast<int>(drflac_read_pcm_frames_f32(f_, static_cast<drflac_uint64>(frames), tmp_.data()));
    ToStereo(tmp_.data(), n, channels);
    std::copy(tmp_.begin(), tmp_.begin() + n * 2, out);
    return n;
  }
  bool Seek(std::uint64_t frame) override { return drflac_seek_to_pcm_frame(f_, frame) != 0; }

 private:
  drflac* f_;
  std::vector<float> tmp_;
};

class WavFile final : public AudioFile {
 public:
  WavFile() = default;
  ~WavFile() override {
    if (open_) drwav_uninit(&wav_);
  }
  bool Open(const std::string& path) {
    open_ = drwav_init_file(&wav_, path.c_str(), nullptr) != 0;
    if (open_) rate_ = static_cast<int>(wav_.sampleRate);
    return open_ && wav_.channels > 0;
  }
  int Read(float* out, int frames) override {
    int channels = wav_.channels;
    tmp_.resize(static_cast<std::size_t>(frames) * std::max(channels, 2));
    auto n = static_cast<int>(drwav_read_pcm_frames_f32(&wav_, static_cast<drwav_uint64>(frames), tmp_.data()));
    ToStereo(tmp_.data(), n, channels);
    std::copy(tmp_.begin(), tmp_.begin() + n * 2, out);
    return n;
  }
  bool Seek(std::uint64_t frame) override { return drwav_seek_to_pcm_frame(&wav_, frame) != 0; }

 private:
  drwav wav_{};
  bool open_ = false;
  std::vector<float> tmp_;
};

}  // namespace

std::unique_ptr<AudioFile> OpenAudioFile(const std::string& path, std::string* error) {
  char magic[4] = {};
  if (std::FILE* f = std::fopen(path.c_str(), "rb")) {
    std::size_t n = std::fread(magic, 1, 4, f);
    std::fclose(f);
    if (n != 4) {
      *error = "too short";
      return nullptr;
    }
  } else {
    *error = "cannot open";
    return nullptr;
  }
  if (std::memcmp(magic, "OggS", 4) == 0) {
    int err = 0;
    stb_vorbis* v = stb_vorbis_open_filename(path.c_str(), &err, nullptr);
    if (!v) {
      *error = "not Ogg Vorbis (error " + std::to_string(err) + ")";
      return nullptr;
    }
    return std::make_unique<VorbisFile>(v);
  }
  if (std::memcmp(magic, "fLaC", 4) == 0) {
    drflac* f = drflac_open_file(path.c_str(), nullptr);
    if (!f) {
      *error = "bad FLAC file";
      return nullptr;
    }
    return std::make_unique<FlacFile>(f);
  }
  auto wav = std::make_unique<WavFile>();
  if (wav->Open(path)) return wav;
  *error = "not OGG, FLAC or WAV";
  return nullptr;
}

Resampler::Resampler(std::unique_ptr<AudioFile> file, int out_rate, bool loop, std::uint64_t loop_frame)
    : file_(std::move(file)),
      step_(static_cast<double>(file_->rate()) / out_rate),
      loop_(loop),
      loop_frame_(loop_frame),
      buf_(kChunk * 2) {
  if (!Next(a_))
    ended_ = true;
  else if (!Next(b_))
    eof_ = true;
}

bool Resampler::Next(float* frame) {
  if (buf_pos_ == buf_len_) {
    buf_pos_ = 0;
    buf_len_ = file_->Read(buf_.data(), kChunk);
    if (buf_len_ <= 0 && loop_ && file_->Seek(loop_frame_)) buf_len_ = file_->Read(buf_.data(), kChunk);
    if (buf_len_ <= 0) {
      buf_len_ = 0;
      frame[0] = frame[1] = 0;
      return false;
    }
  }
  frame[0] = buf_[buf_pos_ * 2];
  frame[1] = buf_[buf_pos_ * 2 + 1];
  buf_pos_++;
  return true;
}

bool Resampler::Read(float* out, int frames) {
  for (int i = 0; i < frames; i++) {
    if (ended_) {
      out[i * 2] = out[i * 2 + 1] = 0;
      continue;
    }
    while (pos_ >= 1) {
      pos_ -= 1;
      if (eof_) {
        ended_ = true;
        break;
      }
      a_[0] = b_[0];
      a_[1] = b_[1];
      if (!Next(b_)) eof_ = true;  // fade the last frame out to silence
    }
    if (ended_) {
      out[i * 2] = out[i * 2 + 1] = 0;
      continue;
    }
    float t = static_cast<float>(pos_);
    out[i * 2] = a_[0] + (b_[0] - a_[0]) * t;
    out[i * 2 + 1] = a_[1] + (b_[1] - a_[1]) * t;
    pos_ += step_;
  }
  return !ended_;
}

bool DecodeAll(const std::string& path, int out_rate, std::vector<float>* stereo, std::string* error) {
  auto file = OpenAudioFile(path, error);
  if (!file) return false;
  Resampler r(std::move(file), out_rate, false, 0);
  stereo->clear();
  float buf[kChunk * 2];
  bool more = true;
  while (more) {
    more = r.Read(buf, kChunk);
    stereo->insert(stereo->end(), buf, buf + kChunk * 2);
  }
  // drop the trailing silence the last block was padded with
  while (stereo->size() >= 2 && (*stereo)[stereo->size() - 1] == 0 && (*stereo)[stereo->size() - 2] == 0)
    stereo->resize(stereo->size() - 2);
  return true;
}

}  // namespace keen4::audio
