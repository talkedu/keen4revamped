#include "game/coop_split.h"

namespace {
constexpr int kWidth = 320, kHeight = 200;  // the classic view
}  // namespace

void CoopSplitStep(CoopSplit* split, const CoopBox& a, const CoopBox& b, int tics) {
  const int spanx = (a.right > b.right ? a.right : b.right) - (a.left < b.left ? a.left : b.left);
  const int spany = (a.bottom > b.bottom ? a.bottom : b.bottom) - (a.top < b.top ? a.top : b.top);
  const int roomx = kWidth - 2 * COOP_SPLITMARGINX, roomy = kHeight - 2 * COOP_SPLITMARGINY;
  if (split->mode == coop_shared) {
    if (spanx > roomx || spany > roomy) {
      // across or up and down, whichever overflows more
      split->mode = spanx * roomy >= spany * roomx ? coop_sidebyside : coop_stacked;
      split->fittics = split->slidetics = 0;
    }
    return;
  }
  // a little closer than the split needs, so they don't flip back and forth
  const bool fits = spanx <= roomx - 16 && spany <= roomy - 16;
  if (!fits) {
    split->fittics = split->slidetics = 0;
    return;
  }
  if (split->slidetics) {
    split->slidetics += tics;
    if (split->slidetics >= COOP_SLIDETICS) *split = CoopSplit{};
    return;
  }
  split->fittics += tics;
  if (split->fittics >= COOP_MERGETICS) split->slidetics = 1;
}

int CoopSplitSize(const CoopSplit& split) {
  const int full = split.mode == coop_stacked ? kHeight : kWidth;
  const int half = full / 2;
  if (!split.slidetics) return half;
  int size = half + (full - half) * split.slidetics / COOP_SLIDETICS;
  return size < full ? size : full;
}
