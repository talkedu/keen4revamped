#pragma once

// Shared definitions for the ported game modules (CK_DEF.H in id's layout).
//
// Field order and sizes follow the original structures so that the ported
// code reads like the disassembly it came from. Offsets in comments are the
// original's (DGROUP addresses for globals, byte offsets for fields).

#include "id/id_ca.h"
#include "id/id_heads.h"
#include "id/id_in.h"
#include "id/id_rf.h"
#include "id/id_sd.h"
#include "id/id_us.h"
#include "id/id_vw.h"

#include "game/coop_split.h"

// ---------------------------------------------------------------------------
// Objects and states

enum classtype : sword {
  nothing,
  inertobj,
  keenobj,
  stunshotobj,
  bonusobj,
  slugobj,
  oracleobj,
  classtype7,
  eggobj,
  madmushroomobj,
  arachnutobj,
  skypestobj,
  wormmouthobj,
  thundercloudobj,
  berkeloidobj,
  bounderobj,
  inchwormobj,
  footobj,
  lickobj,
  mimrockobj,
  platformobj,
  dopefishobj,
  schoolfishobj,
  pixieobj,
  lindseyobj,
  lightningobj,
  treasureeaterobj,
  eggbirdobj,
  classtype28,
  classtype29,
  scubaobj,
  mshotobj,
  mineobj,
  stunnedobj,
  flagobj,
  NUMCLASSTYPES
};

enum activetype : sword { ac_no, ac_yes, ac_allways, ac_removable };
enum cliptype : sword { cl_noclip, cl_midclip, cl_fullclip };
enum progresstype : sword { step, slide, think, stepthink, slidethink };

struct objtype;
using thinkfunc = void (*)(objtype*);
using contactfunc = void (*)(objtype*, objtype*);

// 30 bytes in DGROUP. Built at startup from the player's exe (see ck_states).
struct statetype {
  sword leftshapenum, rightshapenum;
  progresstype progress;
  boolean skippable;
  boolean pushtofloor;
  sword tictime;
  sword xmove, ymove;
  thinkfunc think;
  contactfunc contact;
  thinkfunc react;
  statetype* nextstate;
  word dgofs;  // the original's DGROUP address of this state
};

// 0x4C bytes in the original objarray (DGROUP A807).
struct objtype {
  classtype obclass;   // 00
  activetype active;   // 02
  boolean needtoreact; // 04
  cliptype needtoclip; // 06
  word nothink;        // 08
  word x, y;           // 0A 0C
  sword xdir, ydir;    // 0E 10
  sword xmove, ymove;  // 12 14
  sword xspeed, yspeed;// 16 18
  sword ticcount;      // 1A
  statetype* state;    // 1C
  word shapenum;       // 1E
  word priority;       // 20
  word left, top, right, bottom, midx;              // 22..2A
  word tileleft, tiletop, tileright, tilebottom, tilemidx;  // 2C..34
  sword hitnorth, hiteast, hitsouth, hitwest;       // 36..3C
  sword temp1, temp2, temp3, temp4;                 // 3E..44
  void* sprite;        // 46
  objtype* next;       // 48
  objtype* prev;       // 4A

  // Port: the original keeps near pointers in some temp fields. A 64-bit
  // pointer does not fit, so those uses get their own slots.
  void* temp3sprite;   // second sprite (stunned stars, platform jets)
  void* temp2sprite;
  objtype* tempobj;    // object pointer kept in temp1/temp2 by some actors
};

// Looks up a state by its DGROUP address in the original (statetype 30 bytes).
statetype* ST(word dgofs);
void CK_InitStates();
void CK_RegisterFunc(word seg, word ofs, void* fn, const char* name);

struct FuncRegistrar {
  FuncRegistrar(word seg, word ofs, void* fn, const char* name) { CK_RegisterFunc(seg, ofs, fn, name); }
};
#define CK_FUNC(seg, ofs, fn) \
  static FuncRegistrar reg_##fn##_##ofs(seg, ofs, reinterpret_cast<void*>(fn), #fn)

// ---------------------------------------------------------------------------
// Game state (DGROUP 7A1A, 0x56 bytes)

inline constexpr int GAMELEVELS = 25;

enum exittype : sword {
  ex_stillplaying, ex_died, ex_completed, ex_rescued, ex_warped, ex_resetgame,
  ex_loadedgame, ex_foot, ex_abortgame, ex_sandwich, ex_hook, ex_card, ex_molly,
  ex_portout, ex_fusebroke, ex_qedbroke, NUMEXITTYPES
};

// Port: the score, extra-life threshold, ammo, drops (7A50..7A5A) and the
// platform Keen rides (7A6E) belong to each player, see PlayerState.
struct gametype {
  sword worldx, worldy;               // 7A1A
  boolean leveldone[GAMELEVELS];      // 7A1E
  boolean wetsuit;                    // 7A5C
  sword rescued;                      // 7A5E
  sword keys[4];                      // 7A60
  sword mapon;                        // 7A68
  sword lives;                        // 7A6A
  sword difficulty;                   // 7A6C
};

extern gametype gamestate;
extern exittype playstate;

// ---------------------------------------------------------------------------
// Players (port)
//
// The original has one Keen and keeps what belongs to him in globals: the
// controls read each frame, his invincibility, the jump and pole timers, and
// in gamestate his score, ammo, drops and the platform he rides. Co-op has
// two Keens, so these live in one PlayerState per player. Single-player has
// one entry, players[0], whose Keen is `player`.

inline constexpr int MAXPLAYERS = 2;

struct PlayerState {
  objtype* ob;        // the player's Keen; players[0].ob is `player`
  objtype* scorebox;  // its score box; players[0].scorebox is `scoreobj`

  // Port: in co-op, the gamepad bound to this player (its IN_GamepadId), or 0
  // for the keyboard. Single-player reads Controls[0] as the original does.
  std::uint32_t pad;
  boolean padconnected;  // the bound pad answered at the last PollControls
  word padbuttons;       // its buttons then, as IN_ReadGamepad numbers them

  // Controls (CK_PLAY DGROUP c, jumpbutton..upheld, oldfirecount).
  ControlInfo c;
  boolean jumpbutton, jumpheld, pogobutton, pogoheld, firebutton, fireheld, upheld;
  sword oldfirecount;

  // CK_KEEN timers (DGROUP CAB5..CABB) and invincibility.
  word moonok;
  std::int32_t leavepoletime;
  word jumptime;
  sword invincible;

  // gamestate 7A50..7A5A and 7A6E.
  std::int32_t score, nextextra;
  sword ammo, drops;
  objtype* riding;

  // Co-op (ck_coop.cpp).
  sword status;          // CoopStatus
  longword respawntime;  // lasttimecount at which the respawn cool-down ends
  sword flicker;         // tics left of immunity after respawning
  word groundx, groundy; // where the Keen last stood on solid ground
  word lastpadbuttons;   // padbuttons of the frame before
};

// ---------------------------------------------------------------------------
// Co-op (port, ck_coop.cpp)
//
// Billy and his twin brother Tom play the same level. They share the lives,
// keys, the wetsuit and the rescued council members; score, drops and ammo
// are each their own. A Keen that dies sits out a cool-down and comes back
// on the last solid ground his brother stood on, flickering and immune for a
// while. A Keen that dies with no life left to come back with is out for the
// rest of the level while his brother plays on; the level fails when the last
// Keen still in play dies. A Keen who walks off the map waits there, hidden,
// for his brother.

enum CoopStatus : sword {
  coop_playing,     // in play (also while dying, until his death is over)
  coop_respawning,  // dead, waiting for the cool-down to end; hidden
  coop_atexit,      // walked off the map, waiting for his brother; hidden
  coop_out,         // dead with no life left, out until the level ends; hidden
};

inline constexpr sword COOP_RESPAWNTICS = 3 * 70;
inline constexpr sword COOP_FLICKERTICS = 2 * 70;

// True for a Keen that is hidden: respawning or waiting at the exit. Hidden
// Keens neither act nor touch anything.
boolean CoopHidden(const objtype* ob);
// True while the Keen is in play and not dying: the one the camera follows
// and the actors chase.
boolean CoopInPlay(int player);
// Level start: both Keens in play; for a loaded game, as they were saved,
// with the devices picked after loading.
void CoopStartLevel(boolean loaded);
// A player's co-op status and respawn tics left, for saving and loading.
void CoopSaveStatus(int player, sword* status, word* respawnleft);
void CoopLoadStatus(int player, sword status, word respawnleft);
// Pauses with "Reconnect gamepad" while a player's gamepad is unplugged,
// and gives him the next gamepad that is plugged in.
void CoopCheckPads();
// Spawns Tom and his score box next to Billy, who was just spawned at
// (tilex, tiley). `kind`: 0 on foot, 1 on the world map, 2 swimming.
void CoopSpawnTom(word tilex, word tiley, sword dir, int kind);
// A Keen's death is over (fell off the screen or the map, or was eaten).
void CoopKeenGone(objtype* ob);
// A Keen walked off the edge of the map.
void CoopReachedExit(objtype* ob);
// Every frame after the actors reacted: respawns, flicker, standing ground.
void CoopFrame();
// The camera, following the Keens in play (instead of ScrollScreen).
void CoopScroll();
// Centres the camera between the Keens in play (instead of CenterActor).
void CoopCenter();
// Tom's score box graphics: a copy of the score box sprite he writes into.
byte* CoopScoreBox();
// The player whose status window Enter or Back opens.
int CoopStatusPlayer();
// Prints how the level ended, the scores and the most actors it had at once
// (MAXACTORS is the limit).
void CoopEndLevel();
// F10 + 1/2 kills Billy/Tom, F10 + 3/4 sends him off the map (to test co-op).
void CoopDebugKeys();
// A Keen came out of a door: on a split screen his camera jumps there.
void CoopCenterOn(objtype* ob);
// Where player `who`'s score box goes (global units, before its 4-pixel
// margin) and the split-screen view it belongs to (-1 for all).
void CoopScoreBoxAt(int who, int boxwidth, word* x, word* y, int* view);

// The split screen: see game/coop_split.h.

extern PlayerState players[MAXPLAYERS];
extern int numplayers;  // 1, or 2 in co-op

// The PlayerState of a Keen object; players[0] for any object that is not one.
PlayerState& PlayerOf(objtype* ob);
// The player whose Keen is `ob`, or -1.
int PlayerIndex(const objtype* ob);
// The Keen an actor goes after: the nearest one in play. With one player
// this is always `player`, as in the original.
objtype* NearestPlayer(const objtype* ob);
// The Keen riding platform `plat`, or nullptr.
objtype* PlayerRiding(const objtype* plat);
// Sets up players[] for a new game: score 0, 5 shots, next extra Keen at 20000.
void ResetPlayers();

// ---------------------------------------------------------------------------
// Cameras (port)
//
// The original's view is ID_RF's origin plus what CK_PLAY derives from it: the
// last tile in view (originxtilemax/ytilemax), the bounds outside which actors
// go to sleep (inactivate*), the look up/down level and lockscroll. These
// globals are the current camera's. Co-op can have a second camera, made
// current with SelectCamera; actors wake, sleep and count as on screen by the
// union of all cameras. Single-player has one camera and never switches.

inline constexpr int MAXCAMERAS = 2;

struct Camera {
  word originxglobal, originyglobal;
  word originxtilemax, originytilemax;
  word inactivateleft, inactivateright, inactivatetop, inactivatebottom;
  word centerlevel;
  boolean lockscroll;
  // Port: on a split screen, the part of the camera's 320x200 view shown
  // (from windowx, windowy) and where on the screen (x, y, width, height).
  int windowx, windowy;
  int x, y, width, height;
};

extern Camera cameras[MAXCAMERAS];
extern int numcameras;  // 1, or 2 when co-op splits the screen
extern int curcamera;

// Stores the current camera's globals and makes camera `i` current.
void SelectCamera(int i);
// Stores the current camera's globals in cameras[curcamera], or loads them
// from there.
void StoreCamera();
void LoadCamera();

// Tile bounds of one camera's view and of where its actors stay awake.
struct ViewBounds {
  word xtile, ytile, xtilemax, ytilemax;
  word inactivateleft, inactivateright, inactivatetop, inactivatebottom;
};
// Fills `out` (MAXCAMERAS entries) with every camera's bounds; returns how many.
int GetViewBounds(ViewBounds* out);

// ---------------------------------------------------------------------------
// CK_PLAY

inline constexpr int MAXACTORS = 100;

extern objtype objarray[MAXACTORS + 1];
extern objtype* lastobj;
extern objtype* objfreelist;
extern objtype* player;
extern objtype* scoreobj;
extern objtype* newobj;  // "new" in the original
extern objtype dummyobj;
extern sword objectcount;

extern ControlInfo c;  // scratch for the dialogs that read the controls (HandleDeath)
extern boolean showscorebox, jerk, oldshooting, QuietFX;
extern byte firescan;
extern byte gamepadflag;
extern boolean GravisGamepad;
extern word GravisMap[4];
extern boolean godmode, jumpcheat, singlestep, debugok, storedemo;
extern boolean joypad;  // DGROUP 7A18
extern sword extravbls;
extern boolean lockscroll;  // DGROUP A7DA: camera does not follow Keen
extern boolean scorescreenkludge;
extern word originxtilemax, originytilemax;
extern word inactivateleft, inactivateright, inactivatetop, inactivatebottom;
extern sword lastlevel;

void InitObjArray();
sword GetNewObj(boolean usedummy);
void RemoveObj(objtype* ob);
// Points for the player whose Keen is `who`; extra Keens go to the shared lives.
void GivePoints(objtype* who, word points);
void PollControls();
void StopMusic();
void StartMusic(sword level);
void PlayLoop();
void CenterActor(objtype* ob);
// The status window of player  (0 in single-player).
void StatusWindow(int who);
// Port: 06BD:1ACD ScrollScreen in two parts, LeftTheMap (walked off the edge or
// fell off the bottom) and ScrollCamera (the view follows Keen); co-op uses
// them separately.
void ScrollScreen(objtype* ob);
boolean LeftTheMap(objtype* ob);
void ScrollCamera(objtype* ob);
void WorldScrollScreen(objtype* ob);

// ---------------------------------------------------------------------------
// CK_PHYS

void CalcBounds(objtype* ob);
void ClipToEnds(objtype* ob);
void ClipToSides(objtype* ob);
boolean CheckPosition(objtype* ob);
boolean StatePositionOk(objtype* ob, statetype* state);
void ClipToWalls(objtype* ob);
void FullClipToWalls(objtype* ob);
void PushObj(objtype* ob);
void ClipToSpriteSide(objtype* push, objtype* solid);
void ClipToSpriteTop(objtype* push, objtype* solid);
void ClipToSprite(objtype* push, objtype* solid, boolean squish);
void DoActor(objtype* ob);
void StateMachine(objtype* ob);
void NewState(objtype* ob, statetype* state);
void ChangeState(objtype* ob, statetype* state);
boolean OnScreen(objtype* ob);
void DoGravity(objtype* ob);
void DoWeakGravity(objtype* ob);
void DoTinyGravity(objtype* ob);
void AccelerateX(objtype* ob, sword dir, sword maxspeed);
void AccelerateXv(objtype* ob, sword dir, sword maxspeed);
void AccelerateY(objtype* ob, sword dir, sword maxspeed);
void FrictionX(objtype* ob);
void FrictionY(objtype* ob);
void StunObj(objtype* ob, objtype* shot, statetype* stunstate);
void T_Projectile(objtype* ob);
void T_WeakProjectile(objtype* ob);
void ProjectileThink1(objtype* ob);
void T_Velocity(objtype* ob);
void SetReactThink(objtype* ob);
void T_Stunned(objtype* ob);
void C_Lethal(objtype* ob, objtype* hit);
void R_Draw(objtype* ob);
void R_Walk(objtype* ob);
void R_WalkNormal(objtype* ob);
void BadState(objtype* ob);
void R_Stunned(objtype* ob);

// Tile info shortcuts (MAPHEAD tile info).
inline byte TINF_N(word tile) { return tinf[NORTHWALL + tile]; }
inline byte TINF_E(word tile) { return tinf[EASTWALL + tile]; }
inline byte TINF_S(word tile) { return tinf[SOUTHWALL + tile]; }
inline byte TINF_W(word tile) { return tinf[WESTWALL + tile]; }
inline byte TINF_IN(word tile) { return tinf[INTILE + tile]; }

// ---------------------------------------------------------------------------
// Module entry points used across files (filled in as modules are ported).

// CK_MAIN
void InitGame();
void ShutdownId();
void TEDDeath();
void DemoLoop();

// CK_GAME
void NewGame();
void ResetGame();
boolean SaveTheGame(std::FILE* file);
boolean LoadTheGame(std::FILE* file);
void SetupGameLevel(boolean loadnow);
void GameLoop();
void StartDemoRecord();
void EndDemoRecord();
void GameOver();
void HandleDeath();

// CK_DEMO
void Terminator();
void StarWars();
void ShowTitle();
void RunDemo(int num);
void DrawHighScores();
// Port: `name` starts the name entry (co-op: "Billy", "Tom").
void CheckHighScore(std::int32_t score, word completed, const char* name = nullptr);
// Port: CheckHighScore for every player.
void HighScores();
void ShowHighScores();

// CK_TEXT
void HelpScreens();
void FinaleLayout();

// CK_KEEN / CK_KEEN2
void SpawnKeen(word tilex, word tiley, sword dir);
// Port: SpawnKeen, SpawnWorldKeen and SpawnSwimKeen for any player's Keen.
void SpawnKeenObj(objtype* ob, word tilex, word tiley, sword dir);
void SpawnWorldKeenObj(objtype* ob, word tilex, word tiley);
void SpawnSwimKeenObj(objtype* ob, word tilex, word tiley);
void SpawnScoreObj(objtype* ob);
void KillKeen(objtype* ob);
void HandleRiding(objtype* ob);
void CheckInTiles(objtype* ob);
void SpawnScore();
void UpdateScore(objtype* ob);
void SpawnWorldKeen(word tilex, word tiley);
void CheckEnterLevel(objtype* ob);
void CheckWorldInTiles(objtype* ob);
void SpawnFlag(word tilex, word tiley);
void SpawnThrowFlag(word tilex, word tiley);

void SpawnShot(objtype* shooter, word x, word y, sword dir);
void ExplodeShot(objtype* shot);

// K4_SPEC
void ScanInfoPlane();
void LindseyDialog();
void OracleCouncilMember();
void CantSwim();
void GotScuba();
void RescuedMember();

// K4_ACT1..3
void SpawnMiragia(word tilex, word tiley);
void SpawnBonus(word tilex, word tiley, word type);
void SpawnDropSplash(word tilex, word tiley);
void SpawnCouncil(word tilex, word tiley);
void SpawnSlug(word tilex, word tiley);
void SpawnMadMushroom(word tilex, word tiley);
void SpawnEgg(word tilex, word tiley);
void SpawnEggbird(word tilex, word tiley);
void SpawnArachnut(word tilex, word tiley);
void SpawnSkypest(word tilex, word tiley);
void SpawnWormMouth(word tilex, word tiley);
void SpawnCloud(word tilex, word tiley);
void SpawnBerkeloid(word tilex, word tiley);
void SpawnInchworm(word tilex, word tiley);
void SpawnFoot(word tilex, word tiley);
void SpawnBounder(word tilex, word tiley);
void SpawnLick(word tilex, word tiley);
void SpawnPlatform(word tilex, word tiley, sword dir);
void SpawnDropPlatform(word tilex, word tiley);
void T_Platform(objtype* ob);
void SpawnEater(word tilex, word tiley);
void SpawnMimrock(word tilex, word tiley);
void SpawnDopefish(word tilex, word tiley);
void SpawnSchoolfish(word tilex, word tiley);
void SpawnPixie(word tilex, word tiley);
void SpawnMine(word tilex, word tiley, sword dir);
void SpawnLindsey(word tilex, word tiley);
void SpawnDartShooter(word tilex, word tiley, sword dir);
void SpawnScuba(word tilex, word tiley);

// String tables in the exe.
const char* LevelName(int level);
const char* LevelEnter(int level);
