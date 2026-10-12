#pragma once

// Port: when co-op splits the screen (see ck_coop.cpp).
//
// One view follows both Keens until either gets within a margin of its edges,
// that is until they are too far apart for one 320x200 view. Then the screen
// splits: side by side if they are further apart across, stacked if up and
// down, player 1 left or on top. Once both fit in one view again for half a
// second, the divider slides away. Only the Keens' boxes go in, so this is
// the same for the classic and the remaster renderer.

enum CoopSplitMode : int { coop_shared, coop_sidebyside, coop_stacked };

struct CoopSplit {
  int mode;       // CoopSplitMode
  int fittics;    // tics both have fitted in one view
  int slidetics;  // tics into sliding back into one view, 0 when not
};

struct CoopBox {
  int left, top, right, bottom;  // a Keen's hit box, in world pixels
};

inline constexpr int COOP_SPLITMARGINX = 32, COOP_SPLITMARGINY = 24;  // pixels from the view's edges
inline constexpr int COOP_MERGETICS = 35, COOP_SLIDETICS = 21;        // 0.5 s and 0.3 s

void CoopSplitStep(CoopSplit* split, const CoopBox& a, const CoopBox& b, int tics);
// Player 1's view width (side by side) or height (stacked), in classic pixels.
int CoopSplitSize(const CoopSplit& split);
