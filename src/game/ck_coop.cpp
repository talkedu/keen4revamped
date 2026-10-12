// Port: two-player co-op. Billy (player 1) and his twin brother Tom (player 2)
// play the same level; see the co-op section of ck_def.h for the rules.
#include <algorithm>
#include <cstdio>
#include <vector>

#include "engine/lang.h"
#include "game/ck_def.h"

namespace {

// Keen's states the camera and the respawn look at (DGROUP addresses).
constexpr word s_keenstand = 0x098C;
constexpr word s_keenwalk1 = 0x1094;
constexpr word s_keenwalk2 = 0x10B2;
constexpr word s_keenwalk3 = 0x10D0;
constexpr word s_keenwalk4 = 0x10EE;
constexpr word s_keenlookup2 = 0x0C20;
constexpr word s_keenlookdown2 = 0x0C7A;
constexpr word s_keenpoledown = 0x12EC;
constexpr word s_keendie1 = 0x0CF2;
constexpr word s_keendie2 = 0x0D10;
constexpr word s_keensuitdie1 = 0x0D2E;
constexpr word s_keensuitdie2 = 0x0D4C;

constexpr int SCOREBOXSPR = 253;

// Tom's colours: a red helmet (Keen's is green), a green shirt (magenta) and
// gray pants (blue). EGA colour indices, as Keen's sprites use them.
constexpr byte kTomColours[16] = {
    0,   // black
    8,   // dark blue pants -> dark gray
    4,   // dark green helmet -> dark red
    3,   //
    4,   // red shoes stay red
    2,   // dark magenta shirt -> dark green
    6,   //
    7,   //
    8,   //
    7,   // light blue pants -> light gray
    12,  // light green helmet -> light red
    11,  //
    12,  //
    10,  // light magenta shirt -> light green
    14,  //
    15,  //
};

std::vector<byte> tomscorebox;
int peakactors;
word flickerframe;

// Co-op events go to stderr; the co-op test reads them.
void Log(const char* event, int player) {
  std::fprintf(stderr, "co-op: tic %lu player %d %s, %d lives\n", static_cast<unsigned long>(TimeCount), player + 1,
               event, gamestate.lives);
}

boolean Dying(const objtype* ob) {
  return ob->state == ST(s_keendie1) || ob->state == ST(s_keendie2) || ob->state == ST(s_keensuitdie1) ||
         ob->state == ST(s_keensuitdie2) || ob->obclass != keenobj;
}

void Hide(PlayerState& ps, CoopStatus status) {
  ps.status = status;
  ps.riding = nullptr;
  ps.flicker = 0;
  RF_RemoveSprite(&ps.ob->sprite);
}

// Brings a Keen back where his brother last stood on solid ground.
void Respawn(int i) {
  PlayerState& ps = players[i];
  const PlayerState& other = players[1 - i];
  objtype* ob = ps.ob;
  gamestate.lives--;
  ps.status = coop_playing;
  ps.flicker = COOP_FLICKERTICS;
  ps.invincible = 0;
  ps.jumptime = 0;
  ps.riding = nullptr;
  ps.jumpheld = ps.pogoheld = ps.fireheld = ps.upheld = true;  // nothing happens until released
  ob->obclass = keenobj;
  ob->active = ac_allways;
  ob->priority = 1;
  ob->needtoclip = cl_midclip;
  ob->x = other.groundx;
  ob->y = other.groundy;
  ob->xdir = other.ob->xdir ? other.ob->xdir : 1;
  ob->ydir = 1;
  ob->xspeed = ob->yspeed = 0;
  ob->temp1 = ob->temp2 = ob->temp3 = ob->temp4 = 0;
  NewState(ob, ST(s_keenstand));
  ps.groundx = ob->x;
  ps.groundy = ob->y;
  Log("is back", i);
}

boolean OnSolidGround(const objtype* ob) {
  if (mapon == 0 || mapon == 17) return true;  // the world map and swimming
  if (!ob->hitnorth || ob->hitnorth == 25 || (ob->hitnorth & ~7) == 8) return false;
  return ob->state == ST(s_keenstand) || ob->state == ST(s_keenwalk1) || ob->state == ST(s_keenwalk2) ||
         ob->state == ST(s_keenwalk3) || ob->state == ST(s_keenwalk4);
}

// What the camera follows: the Keen in play, or a stand-in halfway between
// both. Returns false when no Keen is in play.
boolean CameraTarget(objtype* target) {
  int inplay[MAXPLAYERS], count = 0;
  for (int i = 0; i < numplayers; i++)
    if (CoopInPlay(i)) inplay[count++] = i;
  if (!count) return false;
  const objtype* a = players[inplay[0]].ob;
  *target = *a;
  if (count == 1) return true;
  const objtype* b = players[inplay[1]].ob;
  auto mid = [](word p, word q) { return static_cast<word>((static_cast<std::uint32_t>(p) + q) / 2); };
  target->x = mid(a->x, b->x);
  target->y = mid(a->y, b->y);
  target->left = mid(a->left, b->left);
  target->right = mid(a->right, b->right);
  target->top = mid(a->top, b->top);
  target->bottom = mid(a->bottom, b->bottom);
  target->ymove = static_cast<sword>((a->ymove + b->ymove) / 2);
  target->needtoclip = cl_midclip;
  // Follows the ground only when both stand on something.
  auto grounded = [](const objtype* o) { return o->hitnorth || !o->needtoclip || o->state == ST(s_keenpoledown); };
  target->hitnorth = static_cast<sword>(grounded(a) && grounded(b) ? 1 : 0);
  // Looking up or down moves the view if one does and the other doesn't look the other way.
  statetype* up = ST(s_keenlookup2);
  statetype* down = ST(s_keenlookdown2);
  if ((a->state == up || b->state == up) && a->state != down && b->state != down)
    target->state = up;
  else if ((a->state == down || b->state == down) && a->state != up && b->state != up)
    target->state = down;
  else
    target->state = ST(s_keenstand);
  return true;
}

CoopSplit split;

CoopBox BoxOf(const objtype* ob) {
  return CoopBox{ob->left >> G_P_SHIFT, ob->top >> G_P_SHIFT, ob->right >> G_P_SHIFT, ob->bottom >> G_P_SHIFT};
}

// The current camera follows `ob` by the original rules.
void Follow(objtype* ob) {
  if (mapon != 0 && mapon != 17)
    ScrollCamera(ob);
  else
    WorldScrollScreen(ob);
}

// The size of camera i's view on a split screen.
void ViewSize(int i, int* width, int* height) {
  const int size = CoopSplitSize(split);
  if (split.mode == coop_stacked) {
    *width = SCREENWIDTH;
    *height = i ? SCREENHEIGHT - size : size;
  } else {
    *width = i ? SCREENWIDTH - size : size;
    *height = SCREENHEIGHT;
  }
}

// Moves the part of camera i's 320x200 view that its half shows so that the
// Keen stays in its middle, as the camera itself keeps him in the middle of
// the whole view.
void FollowWindow(int i, const objtype* ob) {
  Camera& cam = cameras[i];
  int w, h;
  ViewSize(i, &w, &h);
  const int kx = (ob->left + ob->right) / 2 / PIXGLOBAL - cam.originxglobal / PIXGLOBAL;
  const int ky = ob->bottom / PIXGLOBAL - cam.originyglobal / PIXGLOBAL;
  if (kx - cam.windowx < w / 2 - 24) cam.windowx = kx - (w / 2 - 24);
  if (kx - cam.windowx > w / 2 + 24) cam.windowx = kx - (w / 2 + 24);
  if (ky - cam.windowy < h / 2) cam.windowy = ky - h / 2;
  if (ky - cam.windowy > h * 4 / 5) cam.windowy = ky - h * 4 / 5;
  cam.windowx = std::clamp(cam.windowx, 0, SCREENWIDTH - w);
  cam.windowy = std::clamp(cam.windowy, 0, SCREENHEIGHT - h);
}

void SetSplitViews() {
  const int size = CoopSplitSize(split);
  RF_View views[2];
  for (int i = 0; i < 2; i++) {
    Camera& cam = cameras[i];
    ViewSize(i, &cam.width, &cam.height);
    cam.x = split.mode == coop_sidebyside && i ? size : 0;
    cam.y = split.mode == coop_stacked && i ? size : 0;
    cam.windowx = std::clamp(cam.windowx, 0, SCREENWIDTH - cam.width);
    cam.windowy = std::clamp(cam.windowy, 0, SCREENHEIGHT - cam.height);
    views[i] = RF_View{cam.originxglobal, cam.originyglobal, cam.windowx, cam.windowy,
                       cam.x,             cam.y,             cam.width,   cam.height};
  }
  RF_SetViews(views, 2);
}

// The screen splits: both cameras start where the shared one is, each
// showing the half its Keen is in (player 1 left or on top); a Keen out of
// the whole view (came out of a door) gets his camera centred on him.
void StartSplit() {
  std::fprintf(stderr, "co-op: tic %lu screen splits %s\n", static_cast<unsigned long>(TimeCount),
               split.mode == coop_stacked ? "stacked" : "side by side");
  StoreCamera();
  cameras[1] = cameras[0];
  numcameras = 2;
  const int size = CoopSplitSize(split);
  cameras[0].windowx = cameras[0].windowy = 0;
  cameras[1].windowx = split.mode == coop_sidebyside ? size : 0;
  cameras[1].windowy = split.mode == coop_stacked ? size : 0;
  for (int i = 0; i < 2; i++) {
    const objtype* ob = players[i].ob;
    const Camera& cam = cameras[i];
    const bool inview = ob->right > cam.originxglobal &&
                        ob->left < static_cast<word>(cam.originxglobal + SCREENWIDTH * PIXGLOBAL) &&
                        ob->bottom > cam.originyglobal &&
                        ob->top < static_cast<word>(cam.originyglobal + SCREENHEIGHT * PIXGLOBAL);
    if (inview) continue;
    SelectCamera(i);
    CenterActor(players[i].ob);
    StoreCamera();
    SelectCamera(0);
  }
  for (int i = 0; i < 2; i++) FollowWindow(i, players[i].ob);
}

// Back to one view, camera `keep`'s.
void EndSplit(int keep) {
  std::fprintf(stderr, "co-op: tic %lu screen is one view\n", static_cast<unsigned long>(TimeCount));
  StoreCamera();
  if (keep != 0) cameras[0] = cameras[keep];
  curcamera = 0;
  LoadCamera();
  numcameras = 1;
  split = CoopSplit{};
  RF_SetViews(nullptr, 0);
}

// The overlay: who is out, and for how long.
void DrawBadges() {
  int row = 0;
  for (int i = 0; i < numplayers; i++) {
    const PlayerState& ps = players[i];
    char text[64];
    const char* name = i ? "TOM" : "BILLY";
    if (ps.status == coop_respawning) {
      std::int32_t left = static_cast<std::int32_t>(ps.respawntime - lasttimecount);
      std::snprintf(text, sizeof text, "%s %ld", name, static_cast<long>((left + 69) / 70));
    } else if (ps.status == coop_atexit) {
      std::snprintf(text, sizeof text, "%s: %s", name, keen4::lang::Tr("WAITING"));
    } else {
      continue;
    }
    word w, h;
    VW_MeasurePropString(text, &w, &h);
    int x = (SCREENWIDTH - w) / 2, y = 4 + row * (h + 4);
    VWB_Bar(x - 3, y - 1, w + 6, h + 2, 0);
    px = static_cast<sword>(x);
    py = static_cast<sword>(y);
    byte color = fontcolor;
    fontcolor = 15;
    VWB_DrawPropString(text);
    fontcolor = color;
    row++;
  }
}

}  // namespace

boolean CoopHidden(const objtype* ob) {
  int i = PlayerIndex(ob);
  return i >= 0 && players[i].status != coop_playing;
}

boolean CoopInPlay(int player) {
  const PlayerState& ps = players[player];
  return ps.status == coop_playing && !Dying(ps.ob);
}

void CoopStartLevel() {
  RF_SetVariantRemap(1, kTomColours);
  RF_SetOverlayHook(DrawBadges);
  RF_SetViews(nullptr, 0);
  split = CoopSplit{};
  numcameras = 1;
  curcamera = 0;
  peakactors = 0;
  for (int i = 0; i < numplayers; i++) {
    PlayerState& ps = players[i];
    ps.status = coop_playing;
    ps.respawntime = 0;
    ps.flicker = 0;
    ps.groundx = ps.ob->x;
    ps.groundy = ps.ob->y;
    ps.lastpadbuttons = ps.padbuttons;
  }
}

void CoopSpawnTom(word tilex, word tiley, sword dir, int kind) {
  objtype* tom = players[1].ob;
  switch (kind) {
    case 0:
      // a tile behind Billy, if there is room
      SpawnKeenObj(tom, static_cast<word>(tilex - dir), tiley, dir);
      if (!CheckPosition(tom)) SpawnKeenObj(tom, tilex, tiley, dir);
      break;
    case 1:
      SpawnWorldKeenObj(tom, tilex, tiley);
      break;
    default:
      SpawnSwimKeenObj(tom, tilex, tiley);
      break;
  }
  SpawnScoreObj(players[1].scorebox);
}

void CoopKeenGone(objtype* ob) {
  int i = PlayerIndex(ob);
  if (i < 0 || players[i].status != coop_playing) return;
  // The level fails when the last Keen in play dies, or with no life left
  // to bring him back.
  if (!CoopInPlay(1 - i) || gamestate.lives <= 0) {
    Log("died and the level fails", i);
    playstate = ex_died;
    return;
  }
  Hide(players[i], coop_respawning);
  players[i].respawntime = lasttimecount + COOP_RESPAWNTICS;
  Log("died", i);
}

void CoopReachedExit(objtype* ob) {
  int i = PlayerIndex(ob);
  if (i < 0 || players[i].status != coop_playing) return;
  Hide(players[i], coop_atexit);
  Log("reached the exit", i);
  if (players[1 - i].status == coop_atexit) playstate = ex_completed;
}

void CoopFrame() {
  flickerframe++;
  for (int i = 0; i < numplayers; i++) {
    PlayerState& ps = players[i];
    objtype* ob = ps.ob;
    switch (ps.status) {
      case coop_respawning:
        if (static_cast<std::int32_t>(lasttimecount - ps.respawntime) >= 0) Respawn(i);
        break;
      case coop_playing:
        if (!Dying(ob) && OnSolidGround(ob)) {
          ps.groundx = ob->x;
          ps.groundy = ob->y;
        }
        if (ps.flicker) {
          ps.flicker = static_cast<sword>(ps.flicker - tics);
          if (ps.flicker < 0) ps.flicker = 0;
          // drawn every other frame while immune
          if (ps.flicker && (flickerframe & 1))
            RF_RemoveSprite(&ob->sprite);
          else if (!ob->sprite)
            RF_PlaceSprite(&ob->sprite, ob->x, ob->y, ob->shapenum, spritedraw, static_cast<int>(ob->priority));
        }
        break;
      default:
        break;
    }
  }
  if (players[1].ob->sprite) RF_SetSpriteVariant(players[1].ob->sprite, 1);

  int actors = 0;
  for (objtype* ob = player; ob; ob = ob->next) actors++;
  if (actors > peakactors) peakactors = actors;
}

void CoopScroll() {
  // walking off the edge of the map or falling off its bottom, each Keen
  if (mapon != 0 && mapon != 17) {
    for (int i = 0; i < numplayers; i++)
      if (CoopInPlay(i)) LeftTheMap(players[i].ob);
  }
  if (playstate != ex_stillplaying) return;

  // one view or two
  const int before = split.mode;
  if (CoopInPlay(0) && CoopInPlay(1)) {
    CoopSplitStep(&split, BoxOf(players[0].ob), BoxOf(players[1].ob), tics);
    if (!before && split.mode) StartSplit();
    if (before && !split.mode) EndSplit(0);
  } else if (split.mode) {
    // a Keen out of play has no view: the other's takes the screen
    EndSplit(CoopInPlay(1) ? 1 : 0);
  }

  if (!split.mode) {
    objtype target;
    if (CameraTarget(&target)) Follow(&target);
    return;
  }

  // each half follows its own Keen; while the divider slides away, player
  // 1's half already follows both
  for (int i = 0; i < 2; i++) {
    SelectCamera(i);
    objtype target;
    objtype* follow = players[i].ob;
    if (i == 0 && split.slidetics && CameraTarget(&target)) follow = &target;
    Follow(follow);
    StoreCamera();
    FollowWindow(i, follow);
  }
  SelectCamera(0);
  SetSplitViews();
}

void CoopCenterOn(objtype* ob) {
  int i = PlayerIndex(ob);
  if (numcameras < 2 || i < 0) return;
  SelectCamera(i);
  CenterActor(ob);
  StoreCamera();
  SelectCamera(0);
}

void CoopScoreBoxAt(int who, int boxwidth, word* x, word* y, int* view) {
  if (numcameras > 1) {
    StoreCamera();
    const Camera& cam = cameras[who];
    *x = static_cast<word>(cam.originxglobal + cam.windowx * PIXGLOBAL);
    *y = static_cast<word>(cam.originyglobal + cam.windowy * PIXGLOBAL);
    *view = who;
    return;
  }
  // one view: Billy's box top left, Tom's top right
  *x = static_cast<word>(originxglobal + (who ? (SCREENWIDTH - 8 - boxwidth) * PIXGLOBAL : 0));
  *y = originyglobal;
  *view = -1;
}


void CoopCenter() {
  objtype target;
  if (CameraTarget(&target))
    CenterActor(&target);
  else
    CenterActor(player);
}

byte* CoopScoreBox() {
  if (tomscorebox.empty()) {
    CA_CacheGrChunk(SCOREBOXSPR);
    const byte* original = grsegs[SCOREBOXSPR];
    tomscorebox.assign(original, original + grsize[SCOREBOXSPR]);
  }
  return tomscorebox.data();
}

int CoopStatusPlayer() {
  // Enter on the keyboard, or Back on the newest pad (which types Enter)
  if (IN_GamepadActive()) {
    int pads = IN_GamepadCount();
    std::uint32_t newest = pads ? IN_GamepadId(pads - 1) : 0;
    for (int i = 0; i < numplayers; i++)
      if (players[i].pad && players[i].pad == newest) return i;
  }
  for (int i = 0; i < numplayers; i++)
    if (!players[i].pad) return i;
  return 0;
}

void CoopEndLevel() {
  RF_SetOverlayHook(nullptr);
  RF_SetViews(nullptr, 0);
  if (numcameras > 1) EndSplit(0);
  std::fprintf(stderr, "co-op: tic %lu map %d ends (playstate %d), scores %ld %ld, lives %d, at most %d of %d actors\n",
               static_cast<unsigned long>(TimeCount), mapon, static_cast<int>(playstate),
               static_cast<long>(players[0].score), static_cast<long>(players[1].score), gamestate.lives, peakactors,
               MAXACTORS);
}

void CoopDebugKeys() {
  // F10 + 1/2: Billy/Tom dies; F10 + 3/4: Billy/Tom walks off the map
  for (int i = 0; i < numplayers; i++) {
    if (Keyboard[sc_1 + i] && CoopInPlay(i)) KillKeen(players[i].ob);
    if (Keyboard[sc_3 + i] && CoopInPlay(i) && mapon) CoopReachedExit(players[i].ob);
  }
}
