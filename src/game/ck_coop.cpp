// Port: two-player co-op. Billy (player 1) and his twin brother Tom (player 2)
// play the same level; see the co-op section of ck_def.h for the rules.
#include <cstdio>
#include <vector>

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
  objtype target;
  if (!CameraTarget(&target)) return;
  if (mapon != 0 && mapon != 17)
    ScrollCamera(&target);
  else
    WorldScrollScreen(&target);
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
