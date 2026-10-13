// keen4-extract: exports the original Keen 4 graphics as 1x PNG files, the
// reference grid for artists and the input of tools/keen4-upscale.
//
//   keen4-extract --data DIR --out DIR
//
// Writes tile16/NNNN.png (background tiles), tile16m/NNNN.png (foreground
// tiles with transparency), sprite/NNNN.png (sprite chunks with transparency),
// sprite2/NNNN.png (Keen's sprites in Tom's colours, for co-op), pic/NNNN.png
// (pictures), sheets/*.png (all tiles of a kind in one image, for upscaling in
// context) and index.txt describing the sheets and sprite strips.
// The output is derived from the game data: keep it out of the repository.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "engine/datafiles.h"
#include "engine/png_writer.h"
#include "exe/keen4exe.h"
#include "id/id_ca.h"
#include "render/art.h"

namespace fs = std::filesystem;
using keen4::render::AssetKind;

[[noreturn]] void Quit(const char* error) {
  if (error && *error) std::fprintf(stderr, "%s\n", error);
  std::exit(error && *error ? 1 : 0);
}

namespace {

constexpr int kSheetCols = 18;  // the tile sets are laid out 18 tiles wide

std::uint32_t ToArgb(std::uint8_t index) {
  return index >= 16 ? 0 : (0xFF000000u | keen4::render::EgaColor(index));
}

std::string Name(int n) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "%04d.png", n);
  return buf;
}

bool Export(AssetKind kind, int number, const fs::path& dir, std::vector<std::uint32_t>* argb, int* w, int* h) {
  std::vector<std::uint8_t> idx;
  if (!keen4::render::DecodeOriginal(kind, number, w, h, &idx)) return false;
  argb->resize(idx.size());
  for (std::size_t i = 0; i < idx.size(); i++) (*argb)[i] = ToArgb(idx[i]);
  return keen4::WritePngRgba((dir / Name(number)).string(), argb->data(), *w, *h);
}

// All 16x16 tiles of a kind in one sheet, kSheetCols wide, missing ones clear.
void Sheet(AssetKind kind, int count, const fs::path& path) {
  int rows = (count + kSheetCols - 1) / kSheetCols;
  int sw = kSheetCols * 16, sh = rows * 16;
  std::vector<std::uint32_t> sheet(static_cast<std::size_t>(sw) * sh, 0);
  for (int n = 0; n < count; n++) {
    int w, h;
    std::vector<std::uint8_t> idx;
    if (!keen4::render::DecodeOriginal(kind, n, &w, &h, &idx)) continue;
    int ox = (n % kSheetCols) * 16, oy = (n / kSheetCols) * 16;
    for (int y = 0; y < 16; y++)
      for (int x = 0; x < 16; x++)
        sheet[static_cast<std::size_t>(oy + y) * sw + ox + x] = ToArgb(idx[static_cast<std::size_t>(y) * 16 + x]);
  }
  keen4::WritePngRgba(path.string(), sheet.data(), sw, sh);
}

bool ExportPic(int chunk, bool masked, const fs::path& dir) {
  CA_CacheGrChunk(chunk);
  const std::uint8_t* data = grsegs[chunk];
  if (!data) return false;
  const pictabletype& pic = masked ? picmtable[chunk - STARTPICM] : pictable[chunk - STARTPICS];
  int wbytes = pic.width, h = pic.height, planesize = wbytes * h;
  const std::uint8_t* mask = masked ? data : nullptr;
  const std::uint8_t* planes = masked ? data + planesize : data;
  std::vector<std::uint32_t> argb(static_cast<std::size_t>(wbytes) * 8 * h);
  for (int y = 0; y < h; y++)
    for (int b = 0; b < wbytes; b++)
      for (int bit = 0; bit < 8; bit++) {
        int i = y * wbytes + b;
        std::uint8_t sel = static_cast<std::uint8_t>(0x80 >> bit);
        std::uint8_t c = 0;
        for (int p = 0; p < 4; p++)
          if (planes[p * planesize + i] & sel) c = static_cast<std::uint8_t>(c | (1 << p));
        bool clear = mask && (mask[i] & sel);
        argb[static_cast<std::size_t>(y) * wbytes * 8 + b * 8 + bit] = clear ? 0 : ToArgb(c);
      }
  return keen4::WritePngRgba((dir / Name(chunk)).string(), argb.data(), wbytes * 8, h);
}

}  // namespace

int main(int argc, char** argv) {
  std::string data = ".", out = "extracted-art";
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--data" && i + 1 < argc)
      data = argv[++i];
    else if (a == "--out" && i + 1 < argc)
      out = argv[++i];
    else {
      std::fprintf(stderr, "usage: keen4-extract --data DIR --out DIR\n");
      return 2;
    }
  }

  keen4::files::SetDataDir(data);
  std::string exe = keen4::files::Find("KEEN4E.EXE"), error;
  if (exe.empty() || !keen4::exe::g_exe.Load(exe, &error)) {
    std::fprintf(stderr, "Could not load KEEN4E.EXE from %s %s\n", data.c_str(), error.c_str());
    return 1;
  }
  CA_Startup();

  fs::path root(out);
  for (const char* d : {"tile16", "tile16m", "sprite", "sprite2", "pic", "sheets"}) fs::create_directories(root / d);

  std::vector<std::uint32_t> argb;
  int w, h, count = 0;
  for (int n = 0; n < NUMTILE16; n++) count += Export(AssetKind::Tile16, n, root / "tile16", &argb, &w, &h);
  for (int n = 1; n < NUMTILE16M; n++) count += Export(AssetKind::Tile16M, n, root / "tile16m", &argb, &w, &h);

  std::ofstream index(root / "index.txt");
  index << "# keen4-extract index: sheets of 16x16 tiles and sprite strips\n";
  index << "sheet tile16 sheets/tile16.png " << kSheetCols << " " << NUMTILE16 << "\n";
  index << "sheet tile16m sheets/tile16m.png " << kSheetCols << " " << NUMTILE16M << "\n";
  Sheet(AssetKind::Tile16, NUMTILE16, root / "sheets" / "tile16.png");
  Sheet(AssetKind::Tile16M, NUMTILE16M, root / "sheets" / "tile16m.png");

  // Sprites: consecutive chunks of the same size are treated as one animation
  // strip, so the upscaler sees all frames of an animation together.
  // Keen's sprites also go out recoloured as Tom's (co-op), the indexed image
  // recoloured before any upscaling, in strip2 lines of their own.
  int strip_w = -1, strip_h = -1;
  std::vector<int> strip, strip2;
  auto flush = [&]() {
    if (!strip.empty()) {
      index << "strip";
      for (int c : strip) index << " " << c;
      index << "\n";
    }
    if (!strip2.empty()) {
      index << "strip2";
      for (int c : strip2) index << " " << c;
      index << "\n";
    }
    strip.clear();
    strip2.clear();
  };
  for (int n = STARTSPRITES; n < STARTSPRITES + NUMSPRITES; n++) {
    if (!Export(AssetKind::Sprite, n, root / "sprite", &argb, &w, &h)) continue;
    count++;
    if (w != strip_w || h != strip_h) flush();
    strip_w = w;
    strip_h = h;
    strip.push_back(n);
    if (keen4::render::IsKeenSprite(n) && Export(AssetKind::Sprite2, n, root / "sprite2", &argb, &w, &h)) {
      count++;
      strip2.push_back(n);
    }
  }
  flush();

  // Context patches: each tile with the 8 tiles around its first use in a
  // level, so the upscaler sees the neighbours the tile is actually drawn
  // next to and its edges come out matching them.
  int patches = 0;
  for (int plane = 0; plane < 2; plane++) {
    AssetKind kind = plane == 0 ? AssetKind::Tile16 : AssetKind::Tile16M;
    fs::path dir = root / "context" / (plane == 0 ? "tile16" : "tile16m");
    fs::create_directories(dir);
    std::vector<bool> done(plane == 0 ? NUMTILE16 : NUMTILE16M, false);
    for (int map = 0; map < NUMMAPS; map++) {
      if (!CA_MapExists(map)) continue;
      CA_CacheMap(map);
      const int mw = mapheaderseg[map]->width, mh = mapheaderseg[map]->height;
      const word* tiles = mapsegs[plane];
      for (int y = 1; y < mh - 1; y++) {
        for (int x = 1; x < mw - 1; x++) {
          int t = tiles[y * mw + x];
          if ((plane == 1 && t == 0) || t >= static_cast<int>(done.size()) || done[t]) continue;
          std::vector<std::uint32_t> patch(48 * 48, 0);
          for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
              int n = tiles[(y + dy) * mw + x + dx];
              int tw, th;
              std::vector<std::uint8_t> idx;
              if ((plane == 1 && n == 0) || !keen4::render::DecodeOriginal(kind, n, &tw, &th, &idx)) continue;
              for (int py = 0; py < 16; py++)
                for (int px = 0; px < 16; px++)
                  patch[static_cast<std::size_t>((dy + 1) * 16 + py) * 48 + (dx + 1) * 16 + px] =
                      ToArgb(idx[static_cast<std::size_t>(py) * 16 + px]);
            }
          }
          keen4::WritePngRgba((dir / Name(t)).string(), patch.data(), 48, 48);
          done[t] = true;
          patches++;
        }
      }
    }
  }
  index << "context 48\n";
  std::printf("Wrote %d tile context patches\n", patches);

  for (int n = STARTPICS; n < STARTPICS + NUMPICS; n++) count += ExportPic(n, false, root / "pic");
  for (int n = STARTPICM; n < STARTPICM + NUMPICM; n++) count += ExportPic(n, true, root / "pic");

  std::printf("Wrote %d images to %s\n", count, out.c_str());
  return 0;
}
