// ID_VW: video (segments 1D06 and 1E0A).
#include "id/id_vw.h"

#include <map>
#include <utility>
#include <vector>

#include "exe/keen4exe.h"
#include "id/id_ca.h"
#include "id/id_sys.h"
#include "platform/platform.h"

byte vw_framebuffer[SCREENWIDTH * SCREENHEIGHT];
byte* vw_screen = vw_framebuffer;

sword px, py;
byte pdrawmode = 0x18, fontcolor = 15;  // DGROUP 6024: text is XORed by default
sword fontnumber;

sword bordercolor;
boolean screenfaded;

word bufferofs, displayofs, linewidth = 64;

namespace {

// Current EGA attribute registers (palette) and the fade tables of the
// original (DGROUP 4EB4: seven 17-byte palettes, the 17th byte is the border).
byte attribute_regs[16];
byte fade_colors[7][17];

constexpr std::uint32_t kRgbi[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

// In the 200-line EGA modes a palette register is read as RGBI:
// bits 0-2 blue/green/red, bit 4 intensity.
std::uint32_t EgaToRgb(byte value) {
  return kRgbi[(value & 7) | ((value & 0x10) ? 8 : 0)];
}

void SetPalette(const byte* colors) {
  for (int i = 0; i < 16; i++) attribute_regs[i] = colors[i];
}

inline void PutPixel(int x, int y, int color) {
  if (static_cast<unsigned>(x) < SCREENWIDTH && static_cast<unsigned>(y) < SCREENHEIGHT)
    vw_screen[y * SCREENWIDTH + x] = static_cast<byte>(color);
}

}  // namespace

void VWL_DrawPlanar(int x, int y, int wbytes, int height, const byte* data, bool masked,
                    int skiprows) {
  const int planesize = wbytes * height;
  const byte* mask = masked ? data : nullptr;
  const byte* colors = masked ? data + planesize : data;
  for (int row = skiprows; row < height; row++) {
    int sy = y + row - skiprows;
    if (static_cast<unsigned>(sy) >= SCREENHEIGHT) continue;
    byte* dest = vw_screen + sy * SCREENWIDTH;
    for (int b = 0; b < wbytes; b++) {
      int idx = row * wbytes + b;
      byte m = mask ? mask[idx] : 0;
      if (m == 0xFF) continue;
      byte p0 = colors[idx], p1 = colors[planesize + idx];
      byte p2 = colors[2 * planesize + idx], p3 = colors[3 * planesize + idx];
      int sx0 = x + b * 8;
      for (int bit = 0; bit < 8; bit++) {
        byte sel = static_cast<byte>(0x80 >> bit);
        if (m & sel) continue;
        int sx = sx0 + bit;
        if (static_cast<unsigned>(sx) >= SCREENWIDTH) continue;
        dest[sx] = static_cast<byte>(((p0 & sel) ? 1 : 0) | ((p1 & sel) ? 2 : 0) |
                                     ((p2 & sel) ? 4 : 0) | ((p3 & sel) ? 8 : 0));
      }
    }
  }
}

void VWL_DrawInverseMask(int x, int y, int wbytes, int height, const byte* mask, int skiprows) {
  for (int row = skiprows; row < height; row++) {
    int sy = y + row - skiprows;
    if (static_cast<unsigned>(sy) >= SCREENHEIGHT) continue;
    for (int b = 0; b < wbytes; b++) {
      byte m = static_cast<byte>(~mask[row * wbytes + b]);
      for (int bit = 0; bit < 8; bit++) {
        if (m & (0x80 >> bit)) {
          int sx = x + b * 8 + bit;
          if (static_cast<unsigned>(sx) < SCREENWIDTH) vw_screen[sy * SCREENWIDTH + sx] |= 15;
        }
      }
    }
  }
}

const byte* VW_Tile16(int tile) { return grsegs[STARTTILE16 + tile]; }
const byte* VW_Tile16M(int tile) { return grsegs[STARTTILE16M + tile]; }

// 1D06:0005
void VW_Startup() {
  for (int i = 0; i < 7; i++)
    for (int j = 0; j < 17; j++) fade_colors[i][j] = keen4::exe::g_exe.Byte(static_cast<std::uint16_t>(0x4EB4 + i * 17 + j));
  SetPalette(fade_colors[3]);
}

// 1D06:0090
void VW_Shutdown() {}

// 1D06:00AC
void VW_SetScreenMode(int) { VW_SetLineWidth(64); }

// 1D06:02DD
void VW_SetLineWidth(int width) { linewidth = static_cast<word>(width); }

// 1D06:030D
void VW_ClearVideo(int color) {
  std::memset(vw_screen, color & 15, SCREENWIDTH * SCREENHEIGHT);
}

// 1D06:00E9
void VW_ColorBorder(int color) { bordercolor = static_cast<sword>(color); }

// 1D06:0152
void VW_SetDefaultColors() {
  fade_colors[3][16] = static_cast<byte>(bordercolor);
  SetPalette(fade_colors[3]);
  screenfaded = false;
}

void VW_SetPaletteReg(int index, int egacolor) { attribute_regs[index & 15] = static_cast<byte>(egacolor); }

// 1D06:016F
void VW_FadeOut() {
  for (int i = 3; i >= 0; i--) {
    fade_colors[i][16] = static_cast<byte>(bordercolor);
    SetPalette(fade_colors[i]);
    VW_WaitVBL(6);
  }
  screenfaded = true;
}

// 1D06:01C2
void VW_FadeIn() {
  for (int i = 0; i < 4; i++) {
    fade_colors[i][16] = static_cast<byte>(bordercolor);
    SetPalette(fade_colors[i]);
    VW_WaitVBL(6);
  }
  screenfaded = false;
}

// 1D06:0215
void VW_FadeUp() {
  for (int i = 3; i < 6; i++) {
    fade_colors[i][16] = static_cast<byte>(bordercolor);
    SetPalette(fade_colors[i]);
    VW_WaitVBL(6);
  }
  screenfaded = true;
}

// 1D06:0269
void VW_FadeDown() {
  for (int i = 5; i > 2; i--) {
    fade_colors[i][16] = static_cast<byte>(bordercolor);
    SetPalette(fade_colors[i]);
    VW_WaitVBL(6);
  }
  screenfaded = false;
}

void VW_UpdateScreen() {
  std::uint32_t palette[16];
  for (int i = 0; i < 16; i++) palette[i] = EgaToRgb(attribute_regs[i]);
  keen4::platform::Get().Present(vw_framebuffer, SCREENWIDTH, SCREENHEIGHT, palette);
  SYS_Poll();
}

// 1E0A:07DA
void VW_SetScreen(word crtc, word) {
  displayofs = crtc;
  VW_UpdateScreen();
}

// 1E0A:0AD3
void VW_WaitVBL(int number) {
  VW_UpdateScreen();
  SYS_WaitTics(number);
}

// 1E0A:00AF
void VW_DrawTile8(int xbytes, int y, int tile) {
  const byte* src = grsegs[STARTTILE8];
  if (!src) return;
  VWL_DrawPlanar(xbytes * 8, y, 1, 8, src + tile * 32, false);
}

// 1D06:0ABB
void VWB_DrawTile8(int x, int y, int tile) { VW_DrawTile8(x / 8, y, tile); }

// 1D06:0B0E
void VWB_DrawTile8M(int x, int y, int tile) {
  const byte* src = grsegs[STARTTILE8M];
  if (!src) return;
  VWL_DrawPlanar((x / 8) * 8, y, 1, 8, src + tile * 40, true);
}

// 1D06:0B8B
void VWB_DrawTile16(int x, int y, int tile) {
  const byte* src = grsegs[STARTTILE16 + tile];
  if (src) VWL_DrawPlanar((x / 8) * 8, y, 2, 16, src, false);
}

// 1D06:0BF9
void VWB_DrawTile16M(int x, int y, int tile) {
  const byte* src = grsegs[STARTTILE16M + tile];
  if (src) VWL_DrawPlanar((x / 8) * 8, y, 2, 16, src, true);
}

// 1D06:035C
void VW_DrawPic(int xbytes, int y, int chunknum) {
  const pictabletype& pic = pictable[chunknum - STARTPICS];
  if (grsegs[chunknum]) VWL_DrawPlanar(xbytes * 8, y, pic.width, pic.height, grsegs[chunknum], false);
}

// 1D06:0C79
void VWB_DrawPic(int x, int y, int chunknum) { VW_DrawPic(x / 8, y, chunknum); }

// 1D06:03C1
void VW_DrawMPic(int xbytes, int y, int chunknum) {
  const pictabletype& pic = picmtable[chunknum - STARTPICM];
  if (grsegs[chunknum]) VWL_DrawPlanar(xbytes * 8, y, pic.width, pic.height, grsegs[chunknum], true);
}

// 1D06:0D29
void VWB_DrawMPic(int x, int y, int chunknum) { VW_DrawMPic(x / 8, y, chunknum); }

// 1D06:0432. The original clips to the 216 lines of the scrolling buffer.
void VW_ClipDrawMPic(int xbytes, int y, int chunknum) { VW_DrawMPic(xbytes, y, chunknum); }

namespace {

// 1D06:04D1 and 1D06:0E75. Sprites can only start every 8/shifts pixels.
void DrawSpriteAt(int x, int y, int chunknum) {
  const spritetabletype& spr = spritetable[chunknum - STARTSPRITES];
  const byte* data = grsegs[chunknum];
  if (!data) return;
  y += spr.orgy >> 4;
  x += spr.orgx >> 4;
  int shift = (x & 7) / 2;
  int bytex = x >= 0 ? x / 8 : (x + 1) / 8;
  int step = spr.shifts == 4 ? 2 : spr.shifts == 2 ? 4 : 8;
  int sx = bytex * 8 + (shift * 2 / step) * step;
  VWL_DrawPlanar(sx, y, spr.width, spr.height, data, true);
}

}  // namespace

void VW_DrawSprite(int x, int y, int chunknum) { DrawSpriteAt(x, y, chunknum); }
void VWB_DrawSprite(int x, int y, int chunknum) { DrawSpriteAt(x, y, chunknum); }

// 1E0A:0000
void VW_Plot(int x, int y, int color) { PutPixel(x, y, color); }
void VWB_Plot(int x, int y, int color) { PutPixel(x, y, color); }

// 1D06:057D
void VW_Hlin(int xl, int xh, int y, int color) {
  for (int x = xl; x <= xh; x++) PutPixel(x, y, color);
}
void VWB_Hlin(int xl, int xh, int y, int color) { VW_Hlin(xl, xh, y, color); }

// 1E0A:004F
void VW_Vlin(int yl, int yh, int x, int color) {
  for (int y = yl; y <= yh; y++) PutPixel(x, y, color);
}
void VWB_Vlin(int yl, int yh, int x, int color) { VW_Vlin(yl, yh, x, color); }

// 1D06:064E
void VW_Bar(int x, int y, int width, int height, int color) {
  for (int j = 0; j < height; j++)
    for (int i = 0; i < width; i++) PutPixel(x + i, y + j, color);
}
void VWB_Bar(int x, int y, int width, int height, int color) { VW_Bar(x, y, width, height, color); }

namespace {

const byte* Font(int number) { return grsegs[STARTFONT + number]; }

word FontHeight(const byte* font) { return static_cast<word>(font[0] | (font[1] << 8)); }
word FontLocation(const byte* font, int c) {
  return static_cast<word>(font[2 + c * 2] | (font[3 + c * 2] << 8));
}
byte FontWidth(const byte* font, int c) { return font[0x202 + c]; }

bool FontPixel(const byte* font, int c, int x, int y) {
  int w = FontWidth(font, c);
  if (x < 0 || x >= w || y < 0 || y >= FontHeight(font)) return false;
  return font[FontLocation(font, c) + y * ((w + 7) / 8) + x / 8] & (0x80 >> (x & 7));
}

// Port: the original fonts stop at ASCII, so the accented letters of
// ISO-8859-1 (0xC0-0xFF) that translations use are drawn from the plain
// letter plus a mark. Marks above may rise kAccentRows over the first row,
// where capitals have no room of their own.
constexpr int kAccentRows = 2;

enum Mark { kNone, kGrave, kAcute, kCircumflex, kTilde, kDiaeresis, kCedilla, kRing };

struct MarkShape {
  int width, height;
  const char* rows;
};
constexpr MarkShape kMarks[] = {
    {0, 0, ""},
    {4, 2, "##..""..##"},  // grave
    {4, 2, "..##""##.."},  // acute
    {4, 2, ".##.""#..#"},  // circumflex
    {5, 2, ".##.#""#.##."},  // tilde
    {5, 1, "##.##"},  // diaeresis
    {3, 2, ".##""##."},  // cedilla, below the letter
    {3, 2, ".#.""#.#"},  // ring
};

struct Latin1Letter {
  char base;
  Mark mark;
};
// 0xC0-0xFF. Letters without a plain form (Æ, Ð, Ø, Þ, ß...) are left out.
constexpr Latin1Letter kLatin1[64] = {
    {'A', kGrave}, {'A', kAcute}, {'A', kCircumflex}, {'A', kTilde}, {'A', kDiaeresis}, {'A', kRing},
    {0, kNone},    {'C', kCedilla}, {'E', kGrave}, {'E', kAcute}, {'E', kCircumflex}, {'E', kDiaeresis},
    {'I', kGrave}, {'I', kAcute}, {'I', kCircumflex}, {'I', kDiaeresis}, {0, kNone}, {'N', kTilde},
    {'O', kGrave}, {'O', kAcute}, {'O', kCircumflex}, {'O', kTilde}, {'O', kDiaeresis}, {0, kNone},
    {0, kNone},    {'U', kGrave}, {'U', kAcute}, {'U', kCircumflex}, {'U', kDiaeresis}, {'Y', kAcute},
    {0, kNone},    {0, kNone},
    {'a', kGrave}, {'a', kAcute}, {'a', kCircumflex}, {'a', kTilde}, {'a', kDiaeresis}, {'a', kRing},
    {0, kNone},    {'c', kCedilla}, {'e', kGrave}, {'e', kAcute}, {'e', kCircumflex}, {'e', kDiaeresis},
    {'i', kGrave}, {'i', kAcute}, {'i', kCircumflex}, {'i', kDiaeresis}, {0, kNone}, {'n', kTilde},
    {'o', kGrave}, {'o', kAcute}, {'o', kCircumflex}, {'o', kTilde}, {'o', kDiaeresis}, {0, kNone},
    {0, kNone},    {'u', kGrave}, {'u', kAcute}, {'u', kCircumflex}, {'u', kDiaeresis}, {'y', kAcute},
    {0, kNone},    {'y', kDiaeresis},
};

struct AccentedGlyph {
  int width = 0, height = 0;
  std::vector<byte> bits;  // (kAccentRows + height) rows of `width` pixels
  bool Pixel(int x, int y) const {
    if (x < 0 || x >= width || y < -kAccentRows || y >= height) return false;
    return bits[(y + kAccentRows) * width + x];
  }
  void Set(int x, int y) {
    if (x >= 0 && x < width && y >= -kAccentRows && y < height) bits[(y + kAccentRows) * width + x] = 1;
  }
};

AccentedGlyph BuildAccented(const byte* font, int c) {
  AccentedGlyph g;
  const Latin1Letter letter = kLatin1[c - 0xC0];
  if (!letter.base) return g;
  int base = static_cast<byte>(letter.base);
  // A font without lowercase letters (the big one) uses the capital.
  if (!FontWidth(font, base) && base >= 'a') base -= 'a' - 'A';
  g.width = FontWidth(font, base);
  g.height = FontHeight(font);
  if (!g.width) return g;
  g.bits.assign(static_cast<std::size_t>(kAccentRows + g.height) * g.width, 0);

  // A mark replaces the dot of i; its stem starts where the x-height does.
  int stemtop = -1;
  if (base == 'i' && letter.mark != kCedilla) {
    for (int y = 0; y < g.height && stemtop < 0; y++)
      for (int x = 0; x < FontWidth(font, 'x'); x++)
        if (FontPixel(font, 'x', x, y)) stemtop = y;
  }
  int left = g.width, right = -1, top = g.height, bottom = -1;
  for (int y = 0; y < g.height; y++) {
    if (y < stemtop) continue;
    for (int x = 0; x < g.width; x++) {
      if (!FontPixel(font, base, x, y)) continue;
      g.Set(x, y);
      if (x < left) left = x;
      if (x > right) right = x;
      if (y < top) top = y;
      if (y > bottom) bottom = y;
    }
  }
  if (right < 0) return g;

  const MarkShape& m = kMarks[letter.mark];
  int x0 = (left + right + 1 - m.width) / 2;
  if (x0 + m.width > g.width) x0 = g.width - m.width;
  if (x0 < 0) x0 = 0;
  int y0;
  if (letter.mark == kCedilla) {
    y0 = bottom + 1;
  } else {
    // one blank row between the mark and the letter when there is room
    y0 = top - 1 - m.height;
    if (y0 < -kAccentRows) y0 = top - m.height;
    if (y0 < -kAccentRows) y0 = -kAccentRows;
  }
  for (int y = 0; y < m.height; y++)
    for (int x = 0; x < m.width; x++)
      if (m.rows[y * m.width + x] == '#') g.Set(x0 + x, y0 + y);
  return g;
}

const AccentedGlyph& Accented(const byte* font, int c) {
  static std::map<std::pair<const byte*, int>, AccentedGlyph> cache;
  auto key = std::make_pair(font, c);
  auto found = cache.find(key);
  if (found == cache.end()) found = cache.emplace(key, BuildAccented(font, c)).first;
  return found->second;
}

int GlyphWidth(const byte* font, int c) {
  int w = static_cast<signed char>(FontWidth(font, c));
  if (!w && c >= 0xC0) w = Accented(font, c).width;
  return w;
}

// 1D06:074D
void VWL_MeasureString(const char* string, word* width, word* height, const byte* font) {
  *height = FontHeight(font);
  *width = 0;
  for (; *string; string++) *width = static_cast<word>(*width + GlyphWidth(font, static_cast<byte>(*string)));
}

}  // namespace

// 1D06:078D
void VW_MeasurePropString(const char* string, word* width, word* height) {
  const byte* font = Font(fontnumber);
  if (!font) {
    *width = *height = 0;
    return;
  }
  VWL_MeasureString(string, width, height, font);
}

// 1D06:07B2 (masked fonts: Keen 4 has none).
void VW_MeasureMPropString(const char* string, word* width, word* height) {
  VW_MeasurePropString(string, width, height);
}

// 1E0A:09B9. The string is assembled bit by bit starting at px&7 and written
// in whole bytes: in the planes selected by fontcolor every pixel of those
// bytes becomes the glyph bit (replace) or is XORed with it (pdrawmode 0x18).
// Port: accents of translated letters may stand above the font's first row;
// those rows only touch the accent's own pixels.
void VW_DrawPropString(const char* string) {
  const byte* font = Font(fontnumber);
  if (!font) return;
  const int height = FontHeight(font);
  const int startbit = px & 7;
  int bit = startbit;
  // Glyph bits in a row-major buffer of up to 50 bytes (400 pixels).
  static byte buffer[kAccentRows + 64][50 * 8];
  int maxheight = height < 64 ? height : 64;
  for (int y = 0; y < kAccentRows + maxheight; y++) std::memset(buffer[y], 0, sizeof buffer[y]);
  int top = 0;
  for (const char* s = string; *s; s++) {
    int c = static_cast<byte>(*s);
    int w = FontWidth(font, c);
    if (!w && c >= 0xC0) {
      const AccentedGlyph& g = Accented(font, c);
      for (int y = 0; y < kAccentRows + maxheight; y++) {
        for (int x = 0; x < g.width && bit + x < 50 * 8; x++) {
          if (!g.Pixel(x, y - kAccentRows)) continue;
          buffer[y][bit + x] = 1;
          if (y - kAccentRows < top) top = y - kAccentRows;
        }
      }
      bit += g.width;
      continue;
    }
    int rowbytes = (w + 7) / 8;
    const byte* glyph = font + FontLocation(font, c);
    for (int y = 0; y < maxheight; y++) {
      for (int x = 0; x < w; x++) {
        if (bit + x >= 50 * 8) break;
        if (glyph[y * rowbytes + x / 8] & (0x80 >> (x & 7))) buffer[kAccentRows + y][bit + x] = 1;
      }
    }
    bit += w;
  }
  int bytes = bit / 8 + ((bit & 7) ? 1 : 0);
  int x0 = px & ~7;
  for (int y = top; y < maxheight; y++) {
    int sy = py + y;
    if (static_cast<unsigned>(sy) >= SCREENHEIGHT) continue;
    const byte* row = buffer[kAccentRows + y];
    for (int x = 0; x < bytes * 8; x++) {
      int sx = x0 + x;
      if (static_cast<unsigned>(sx) >= SCREENWIDTH) continue;
      if (y < 0 && !row[x]) continue;
      byte& pixel = vw_screen[sy * SCREENWIDTH + sx];
      byte g = row[x] ? fontcolor : 0;
      if (pdrawmode == 0x18)
        pixel ^= g;
      else
        pixel = static_cast<byte>((pixel & ~fontcolor) | g);
    }
  }
  // The original advances px by the bit count from the start of the first
  // byte, so a string that does not start on a byte boundary moves px further.
  px = static_cast<sword>(px + bit);
}

// 1D06:0E2C
void VWB_DrawPropString(const char* string) { VW_DrawPropString(string); }
