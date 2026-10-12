// ID_CA: cache manager (segment 12A6).
#include "id/id_ca.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "engine/datafiles.h"
#include "exe/keen4exe.h"
#include "id/id_sd.h"

byte* grsegs[NUMCHUNKS];
std::uint32_t grsize[NUMCHUNKS];
byte grneeded[NUMCHUNKS];
byte ca_levelbit = 1, ca_levelnum;

word* mapsegs[MAPPLANES];
maptype* mapheaderseg[NUMMAPS];
sword mapon = -1;

byte* audiosegs[NUMSNDCHUNKS];

pictabletype* pictable;
pictabletype* picmtable;
spritetabletype* spritetable;

const byte* tinf;

namespace {

using keen4::exe::g_exe;

// Whole data files, read once.
std::vector<byte> egagraph, gamemaps, audiofile;

// Huffman trees (255 nodes of two words each) copied out of DGROUP.
word grhuffman[255 * 2];
word audiohuffman[255 * 2];

const byte* grstarts;     // EGAHEAD: 3 bytes per chunk
const byte* audiostarts;  // AUDIOHED: 4 bytes per chunk
const byte* maphead;      // MAPHEAD: RLEW tag, 100 map offsets, tile info

word ReadWord(const byte* p) { return static_cast<word>(p[0] | (p[1] << 8)); }
std::int32_t ReadLong(const byte* p) {
  return static_cast<std::int32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<std::uint32_t>(p[3]) << 24));
}

// 12A6:000F
std::int32_t GRFILEPOS(int c) {
  const byte* p = grstarts + c * 3;
  std::int32_t value = p[0] | (p[1] << 8) | (p[2] << 16);
  if (value == 0xFFFFFF) value = -1;
  return value;
}

byte* Alloc(std::uint32_t size) {
  // Padding lets the bit readers run a few bytes past the end safely.
  return static_cast<byte*>(std::calloc(size + 16, 1));
}

void LoadDataFile(const char* name, std::vector<byte>* out, const char* error) {
  if (!keen4::files::ReadAll(name, out)) Quit(error);
  out->resize(out->size() + 16, 0);
}

void CopyHuffman(word* dest, std::uint16_t dgofs) {
  for (int i = 0; i < 255 * 2; i++) dest[i] = g_exe.Word(static_cast<std::uint16_t>(dgofs + i * 2));
}

template <typename T>
T* CacheStruct(int chunk, std::uint32_t expect) {
  CA_CacheGrChunk(chunk);
  if (!grsegs[chunk] || grsize[chunk] < expect) Quit("CAL_SetupGrFile: bad structure chunk");
  return reinterpret_cast<T*>(grsegs[chunk]);
}

// 12A6:0741
void CAL_SetupGrFile() {
  grstarts = g_exe.At(keen4::exe::kEgaHeadSeg, 0);
  CopyHuffman(grhuffman, keen4::exe::kEgaDictOfs);
  LoadDataFile("EGAGRAPH.CK4", &egagraph, "Cannot open EGAGRAPH.CK4!");
  pictable = CacheStruct<pictabletype>(0, NUMPICS * 4);
  picmtable = CacheStruct<pictabletype>(1, NUMPICM * 4);
  spritetable = CacheStruct<spritetabletype>(2, NUMSPRITES * 18);
}

// 12A6:08DD
void CAL_SetupMapFile() {
  maphead = g_exe.At(keen4::exe::kMapHeadSeg, 0);
  tinf = maphead + 0x192;
  LoadDataFile("GAMEMAPS.CK4", &gamemaps, "Can't open GAMEMAPS.CK4!");
}

// 12A6:0910
void CAL_SetupAudioFile() {
  CopyHuffman(audiohuffman, keen4::exe::kAudioDictOfs);
  audiostarts = g_exe.At(keen4::exe::kAudioHedSeg, 0);
  LoadDataFile("AUDIO.CK4", &audiofile, "Can't open AUDIO.CK4!");
}

// 12A6:0F05
void CAL_ExpandGrChunk(int chunk, const byte* source) {
  std::uint32_t expanded;
  if (chunk >= STARTTILE8 && chunk < STARTEXTERNS) {
    // Tiles are stored without a length prefix.
    if (chunk < STARTTILE8M)
      expanded = NUMTILE8 * 32;
    else if (chunk < STARTTILE16)
      expanded = NUMTILE8M * 40;
    else if (chunk < STARTTILE16M)
      expanded = 32 * 4;
    else
      expanded = 32 * 5;
  } else {
    expanded = static_cast<std::uint32_t>(ReadLong(source));
    source += 4;
  }
  // Sprites are kept unshifted: the software renderer positions them per pixel
  // and reproduces the original shift granularity itself (see ID_RF).
  grsegs[chunk] = Alloc(expanded);
  grsize[chunk] = expanded;
  CAL_HuffExpand(source, grsegs[chunk], expanded, grhuffman);
}

}  // namespace

bool CA_ReadFile(const char* filename, void* dest, std::uint32_t maxsize) {
  std::FILE* f = std::fopen(keen4::files::WritePath(filename).c_str(), "rb");
  if (!f) return false;
  std::size_t n = std::fread(dest, 1, maxsize, f);
  std::fclose(f);
  return n > 0;
}

bool CA_WriteFile(const char* filename, const void* source, std::uint32_t length) {
  std::FILE* f = std::fopen(keen4::files::WritePath(filename).c_str(), "wb");
  if (!f) return false;
  bool ok = std::fwrite(source, 1, length, f) == length;
  std::fclose(f);
  return ok;
}

// 12A6:02CC. Bits are read least significant first; node 254 is the root.
void CAL_HuffExpand(const byte* source, byte* dest, std::uint32_t length, const word* hufftable) {
  const word* headptr = hufftable + 254 * 2;
  const word* huffptr = headptr;
  byte val = *source++;
  byte mask = 1;
  std::uint32_t written = 0;
  while (written < length) {
    word nodeval = (val & mask) ? huffptr[1] : huffptr[0];
    mask = static_cast<byte>(mask << 1);
    if (!mask) {
      val = *source++;
      mask = 1;
    }
    if (nodeval < 256) {
      dest[written++] = static_cast<byte>(nodeval);
      huffptr = headptr;
    } else {
      huffptr = hufftable + (nodeval - 256) * 2;
    }
  }
}

// 12A6:03F5. `length` is the expanded size in bytes.
void CAL_CarmackExpand(const word* source, word* dest, word length) {
  constexpr word NEARTAG = 0xA7, FARTAG = 0xA8;
  const byte* in = reinterpret_cast<const byte*>(source);
  word* start = dest;
  word words = static_cast<word>(length / 2);
  while (words) {
    word ch = ReadWord(in);
    in += 2;
    word chhigh = static_cast<word>(ch >> 8);
    if (chhigh == NEARTAG || chhigh == FARTAG) {
      word count = ch & 0xFF;
      if (!count) {
        // Escaped tag byte.
        ch = static_cast<word>(ch | *in++);
        *dest++ = ch;
        words--;
      } else {
        const word* copyptr;
        if (chhigh == NEARTAG) {
          word offset = *in++;
          copyptr = dest - offset;
        } else {
          word offset = ReadWord(in);
          in += 2;
          copyptr = start + offset;
        }
        words = static_cast<word>(words - count);
        while (count--) *dest++ = *copyptr++;
      }
    } else {
      *dest++ = ch;
      words--;
    }
  }
}

// 12A6:0685
void CA_RLEWexpand(const word* source, word* dest, std::uint32_t length, word rlewtag) {
  word* end = dest + length / 2;
  while (dest < end) {
    word value = *source++;
    if (value != rlewtag) {
      *dest++ = value;
    } else {
      word count = *source++;
      value = *source++;
      for (word i = 0; i < count && dest < end; i++) *dest++ = value;
    }
  }
}

// 12A6:0535. Returns the compressed size in bytes.
std::uint32_t CA_RLEWCompress(const word* source, std::uint32_t length, word* dest, word rlewtag) {
  const word* start = dest;
  const word* end = source + (length + 1) / 2;
  do {
    word count = 1;
    word value = *source++;
    while (source < end && *source == value) {
      count++;
      source++;
    }
    if (count > 3 || value == rlewtag) {
      *dest++ = rlewtag;
      *dest++ = count;
      *dest++ = value;
    } else {
      for (word i = 1; i <= count; i++) *dest++ = value;
    }
  } while (source < end);
  return static_cast<std::uint32_t>((dest - start) * 2);
}

// 12A6:0954
void CA_Startup() {
  CAL_SetupMapFile();
  CAL_SetupGrFile();
  CAL_SetupAudioFile();
  mapon = -1;
  ca_levelbit = 1;
  ca_levelnum = 0;
}

// 12A6:0999
void CA_Shutdown() {
  egagraph.clear();
  gamemaps.clear();
  audiofile.clear();
}

// 12A6:09C2
void CA_CacheAudioChunk(int chunk) {
  if (audiosegs[chunk]) return;
  std::int32_t pos = ReadLong(audiostarts + chunk * 4);
  std::int32_t next = ReadLong(audiostarts + (chunk + 1) * 4);
  if (next - pos <= 4 || static_cast<std::size_t>(pos) + 4 > audiofile.size()) return;
  const byte* source = audiofile.data() + pos;
  std::uint32_t expanded = static_cast<std::uint32_t>(ReadLong(source));
  audiosegs[chunk] = Alloc(expanded);
  CAL_HuffExpand(source + 4, audiosegs[chunk], expanded, audiohuffman);
}

// 12A6:0B28
void CA_LoadAllSounds() {
  int start;
  switch (SoundMode) {
    case sdm_PC: start = 0; break;
    case sdm_AdLib: start = NUMSOUNDS; break;
    default: return;
  }
  for (int i = 0; i < NUMSOUNDS; i++) CA_CacheAudioChunk(start + i);
}

// 12A6:14CD
void CA_UpLevel() {
  if (ca_levelnum == 7) Quit("CA_UpLevel: Up past level 7!");
  ca_levelbit = static_cast<byte>(ca_levelbit << 1);
  ca_levelnum++;
}

// 12A6:14ED
void CA_DownLevel() {
  if (!ca_levelnum) Quit("CA_DownLevel: Down past level 0!");
  ca_levelbit = static_cast<byte>(ca_levelbit >> 1);
  ca_levelnum--;
  CA_CacheMarks(nullptr);
}

// 12A6:151A
void CA_ClearMarks() {
  for (int i = 0; i < NUMCHUNKS; i++) grneeded[i] &= static_cast<byte>(~ca_levelbit);
}

// 12A6:153C
void CA_ClearAllMarks() {
  std::memset(grneeded, 0, sizeof grneeded);
  ca_levelbit = 1;
  ca_levelnum = 0;
}

void CA_SetGrPurge() {}
void CA_SetAllPurge() { CA_ClearMarks(); }

// 12A6:1117
void CA_CacheGrChunk(int chunk) {
  grneeded[chunk] |= ca_levelbit;
  if (grsegs[chunk]) return;
  std::int32_t pos = GRFILEPOS(chunk);
  if (pos < 0) return;  // sparse chunk
  CAL_ExpandGrChunk(chunk, egagraph.data() + pos);
}

// 12A6:1260
// Port: whether GAMEMAPS has a map with this number (for tools).
bool CA_MapExists(int mapnum) {
  return mapnum >= 0 && mapnum < NUMMAPS && ReadLong(maphead + 2 + mapnum * 4) > 0;
}

void CA_CacheMap(int mapnum) {
  for (int plane = 0; plane < MAPPLANES; plane++) {
    std::free(mapsegs[plane]);
    mapsegs[plane] = nullptr;
  }
  mapon = static_cast<sword>(mapnum);

  if (!mapheaderseg[mapnum]) {
    std::int32_t pos = ReadLong(maphead + 2 + mapnum * 4);
    if (pos <= 0) Quit("CA_CacheMap: Tried to load a non existent map!");
    mapheaderseg[mapnum] = static_cast<maptype*>(std::calloc(1, sizeof(maptype)));
    std::memcpy(mapheaderseg[mapnum], gamemaps.data() + pos, 0x26);
  }

  const maptype* header = mapheaderseg[mapnum];
  std::uint32_t size = static_cast<std::uint32_t>(header->width) * header->height * 2;
  word rlewtag = ReadWord(maphead);
  for (int plane = 0; plane < MAPPLANES; plane++) {
    std::int32_t pos = header->planestart[plane];
    if (!header->planelength[plane]) continue;
    mapsegs[plane] = reinterpret_cast<word*>(Alloc(size));
    const byte* source = gamemaps.data() + pos;
    word expanded = ReadWord(source);
    std::vector<word> buffer(expanded / 2 + 8);
    std::vector<word> packed((header->planelength[plane] + 1) / 2 + 8);
    std::memcpy(packed.data(), source + 2, header->planelength[plane] - 2u);
    CAL_CarmackExpand(packed.data(), buffer.data(), expanded);
    CA_RLEWexpand(buffer.data() + 1, mapsegs[plane], size, rlewtag);
  }
}

void (*drawcachebox)(const char* title, word numcache);
void (*updatecachebox)();
void (*finishcachebox)();

// 12A6:184B. With a title, the original shows a loading dialog while it reads
// the chunks that were purged since the last level. Nothing is ever purged
// here, so a titled call counts every marked chunk, as a DOS machine would
// have had to reload most of them, and the dialog still appears.
void CA_CacheMarks(const char* title) {
  const bool dialog = title != nullptr;
  word numcache = 0;
  for (int i = 0; i < NUMCHUNKS; i++) {
    if ((grneeded[i] & ca_levelbit) && (dialog || !grsegs[i])) numcache++;
  }
  if (!numcache) return;

  if (dialog && drawcachebox) drawcachebox(title, numcache);
  for (int i = 0; i < NUMCHUNKS; i++) {
    if (grneeded[i] & ca_levelbit) {
      if (dialog && updatecachebox) updatecachebox();
      if (!grsegs[i]) CA_CacheGrChunk(i);
    }
  }
  if (dialog && finishcachebox) finishcachebox();
}
