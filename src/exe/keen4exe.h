#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Access to the player's own KEEN4E.EXE (v1.4 EGA).
//
// The original program keeps its data headers (EGAHEAD, EGADICT, MAPHEAD,
// AUDIOHED, AUDIODICT) and all initialised data (actor state tables, text,
// random table) inside the executable. We never ship any of that: at startup
// the exe is unpacked in memory (it is LZEXE 0.91 compressed) and the port
// reads what it needs from the load image, addressed exactly as the original
// code addresses it (segment:offset in the unrelocated image, or an offset in
// DGROUP).
namespace keen4::exe {

// Segment values in the unrelocated load image (see docs/decomp/MODULES.md).
inline constexpr std::uint16_t kDGroupSeg = 0x2EE7;
inline constexpr std::uint16_t kAudioHedSeg = 0x20DF;
inline constexpr std::uint16_t kEgaHeadSeg = 0x2108;
inline constexpr std::uint16_t kMapHeadSeg = 0x2483;
inline constexpr std::uint16_t kEgaDictOfs = 0x6A86;    // in DGROUP
inline constexpr std::uint16_t kAudioDictOfs = 0x6686;  // in DGROUP

class Image {
 public:
  // Reads and unpacks the executable. Returns false and fills *error on failure.
  bool Load(const std::string& path, std::string* error);
  // Same, from bytes already in memory (used by tests).
  bool LoadFromMemory(const std::vector<std::uint8_t>& file, std::string* error);

  bool loaded() const { return !image_.empty(); }
  std::size_t size() const { return image_.size(); }
  const std::uint8_t* Linear(std::uint32_t linear) const { return image_.data() + linear; }
  const std::uint8_t* At(std::uint16_t seg, std::uint16_t ofs) const {
    return Linear((static_cast<std::uint32_t>(seg) << 4) + ofs);
  }
  const std::uint8_t* DGroup(std::uint16_t ofs) const { return At(kDGroupSeg, ofs); }

  std::uint8_t Byte(std::uint16_t ofs) const { return *DGroup(ofs); }
  std::uint16_t Word(std::uint16_t ofs) const {
    const std::uint8_t* p = DGroup(ofs);
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
  }
  std::int16_t Int(std::uint16_t ofs) const { return static_cast<std::int16_t>(Word(ofs)); }
  // A NUL-terminated string in DGROUP.
  const char* String(std::uint16_t ofs) const { return reinterpret_cast<const char*>(DGroup(ofs)); }

 private:
  std::vector<std::uint8_t> image_;
};

// Unpacks an LZEXE 0.91 executable into its load image (no MZ header).
// Returns an empty vector if the file is not LZEXE 0.91.
std::vector<std::uint8_t> UnpackLzexe(const std::vector<std::uint8_t>& file);

// The program-wide instance, loaded once at startup.
extern Image g_exe;

}  // namespace keen4::exe
