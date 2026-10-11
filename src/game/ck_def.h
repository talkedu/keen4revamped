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

struct gametype {
  sword worldx, worldy;               // 7A1A
  boolean leveldone[GAMELEVELS];      // 7A1E
  std::int32_t score, nextextra;      // 7A50 7A54
  sword ammo, drops;                  // 7A58 7A5A
  boolean wetsuit;                    // 7A5C
  sword rescued;                      // 7A5E
  sword keys[4];                      // 7A60
  sword mapon;                        // 7A68
  sword lives;                        // 7A6A
  sword difficulty;                   // 7A6C
  objtype* riding;                    // 7A6E
};

extern gametype gamestate;
extern exittype playstate;

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

extern ControlInfo c;
extern boolean jumpbutton, jumpheld, pogobutton, pogoheld, firebutton, fireheld, upheld;
extern sword oldfirecount;
extern boolean showscorebox, jerk, oldshooting, QuietFX;
extern byte firescan;
extern byte gamepadflag;
extern boolean GravisGamepad;
extern word GravisMap[4];
extern boolean godmode, jumpcheat, singlestep, debugok, storedemo;
extern boolean joypad;  // DGROUP 7A18
extern sword extravbls;
extern sword invincible;
extern boolean lockscroll;  // DGROUP A7DA: camera does not follow Keen
extern boolean scorescreenkludge;
extern word originxtilemax, originytilemax;
extern word inactivateleft, inactivateright, inactivatetop, inactivatebottom;
extern sword lastlevel;

void InitObjArray();
sword GetNewObj(boolean usedummy);
void RemoveObj(objtype* ob);
void GivePoints(word points);
void PollControls();
void StopMusic();
void StartMusic(sword level);
void PlayLoop();
void CenterActor(objtype* ob);
void StatusWindow();

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
void CheckHighScore(std::int32_t score, word completed);
void ShowHighScores();

// CK_TEXT
void HelpScreens();
void FinaleLayout();

// CK_KEEN / CK_KEEN2
void SpawnKeen(word tilex, word tiley, sword dir);
void KillKeen();
void HandleRiding(objtype* ob);
void CheckInTiles(objtype* ob);
void SpawnScore();
void UpdateScore(objtype* ob);
void SpawnWorldKeen(word tilex, word tiley);
void CheckEnterLevel(objtype* ob);
void CheckWorldInTiles(objtype* ob);
void SpawnFlag(word tilex, word tiley);
void SpawnThrowFlag(word tilex, word tiley);

void SpawnShot(word x, word y, sword dir);
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
