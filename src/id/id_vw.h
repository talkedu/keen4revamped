#pragma once

// ID_VW: video (segments 1D06 ID_VW.C and 1E0A ID_VW_AE.ASM).
//
// The original draws into planar EGA memory with latches, pel panning and
// several pages. The port keeps one 8-bit framebuffer the size of the visible
// screen; every drawing routine takes coordinates on that screen and clips.
// Results are pixel-identical for everything the game draws, including the
// EGA quirks that show (sprite x granularity, text written in whole bytes).

#include "id/id_heads.h"

inline constexpr int SCREENWIDTH = 320;
inline constexpr int SCREENHEIGHT = 200;

// The visible frame, and where drawing goes (normally the same buffer; the
// status window is drawn off screen first).
extern byte vw_framebuffer[SCREENWIDTH * SCREENHEIGHT];
extern byte* vw_screen;

// Text state shared with ID_US (DGROUP 6020..6025, A550).
extern sword px, py;
extern byte pdrawmode, fontcolor;
extern sword fontnumber;

extern sword bordercolor;
extern boolean screenfaded;

// Screen offsets of the original page flipping. Game code reads and passes
// them around; the port only keeps them for that.
extern word bufferofs, displayofs, linewidth;

void VW_Startup();
void VW_Shutdown();
void VW_SetScreenMode(int grmode);
void VW_SetLineWidth(int width);
void VW_ClearVideo(int color);

void VW_ColorBorder(int color);
void VW_SetDefaultColors();
void VW_FadeOut();
void VW_FadeIn();
void VW_FadeUp();
void VW_FadeDown();
// Sets one EGA attribute (palette) register, values as in the original.
void VW_SetPaletteReg(int index, int egacolor);

// Shows the framebuffer.
void VW_UpdateScreen();
void VW_SetScreen(word crtc, word pelpan);
// Waits `number` vertical retraces (1/70 s each), showing the frame.
void VW_WaitVBL(int number);
// 1D06:0999. Copies the shown page into the draw page. The port draws into a
// single framebuffer that still holds the last frame, so there is nothing to do.
inline void VW_FixRefreshBuffer() {}

// x in bytes (8 pixels) for the plain VW_ forms, pixels for VWB_.
void VW_DrawTile8(int xbytes, int y, int tile);
void VWB_DrawTile8(int x, int y, int tile);
void VWB_DrawTile8M(int x, int y, int tile);
void VWB_DrawTile16(int x, int y, int tile);
void VWB_DrawTile16M(int x, int y, int tile);
void VW_DrawPic(int xbytes, int y, int chunknum);
void VWB_DrawPic(int x, int y, int chunknum);
void VW_DrawMPic(int xbytes, int y, int chunknum);
void VWB_DrawMPic(int x, int y, int chunknum);
void VW_ClipDrawMPic(int xbytes, int y, int chunknum);
void VW_DrawSprite(int x, int y, int chunknum);
void VWB_DrawSprite(int x, int y, int chunknum);
void VW_Plot(int x, int y, int color);
void VWB_Plot(int x, int y, int color);
void VW_Hlin(int xl, int xh, int y, int color);
void VWB_Hlin(int xl, int xh, int y, int color);
void VW_Vlin(int yl, int yh, int x, int color);
void VWB_Vlin(int yl, int yh, int x, int color);
void VW_Bar(int x, int y, int width, int height, int color);
void VWB_Bar(int x, int y, int width, int height, int color);

void VW_DrawPropString(const char* string);
void VWB_DrawPropString(const char* string);
void VW_MeasurePropString(const char* string, word* width, word* height);
void VW_MeasureMPropString(const char* string, word* width, word* height);

// Low-level block drawing used by ID_RF. `data` is planar EGA data: an
// optional mask plane followed by four colour planes of wbytes*height each.
void VWL_DrawPlanar(int x, int y, int wbytes, int height, const byte* data, bool masked,
                    int skiprows = 0);
// Port: when set, VWL_DrawPlanar draws colour c as vw_remap[c] (player 2's
// colours in co-op).
extern const byte* vw_remap;
// Draws the sprite's opaque pixels in colour 15 (the original "maskdraw").
void VWL_DrawInverseMask(int x, int y, int wbytes, int height, const byte* mask, int skiprows = 0);

// Graphics chunk accessors.
const byte* VW_Tile16(int tile);
const byte* VW_Tile16M(int tile);
