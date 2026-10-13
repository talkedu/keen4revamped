// CK_GAME: new game, save/load, level setup, the loading dialog, demo
// recording, death and the game loop (segment 05C4).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "engine/datafiles.h"
#include "engine/lang.h"
#include "exe/keen4exe.h"
#include "game/ck_def.h"

void SizeText(const char* text, word* width, word* height);  // CK_MAIN
void TEDDeath();
void SpawnFlag(word tilex, word tiley);
void SpawnThrowFlag(word tilex, word tiley);

extern sword lastlevel;

namespace {

using keen4::exe::g_exe;

constexpr int SND_LEVELDONE = 13;
constexpr word RLEWTAG = 0xABCD;
constexpr word OBJARRAY_DGOFS = 0xA807;  // objarray's address in the original
constexpr int OBJSIZE = 0x4C;
constexpr int GAMESTATESIZE = 0x56;

char str[80];  // DGROUP 7A88

// Loading dialog (DGROUP A7B6..A7C0).
std::int32_t cachebox_frame;
std::int32_t cachebox_chunks, cachebox_count;
word fadecount;  // A7C4

// ---------------------------------------------------------------------------
// Saved games keep the original's byte layout so the files match.

void PutWord(byte* p, word v) {
  p[0] = static_cast<byte>(v);
  p[1] = static_cast<byte>(v >> 8);
}
word GetWord(const byte* p) { return static_cast<word>(p[0] | (p[1] << 8)); }

word ObjAddress(const objtype* ob) {
  if (!ob || ob < objarray || ob >= objarray + MAXACTORS) return 0;
  return static_cast<word>(OBJARRAY_DGOFS + (ob - objarray) * OBJSIZE);
}
objtype* ObjFromAddress(word addr) {
  if (addr < OBJARRAY_DGOFS) return nullptr;
  word idx = static_cast<word>((addr - OBJARRAY_DGOFS) / OBJSIZE);
  if (idx >= MAXACTORS || (addr - OBJARRAY_DGOFS) % OBJSIZE) return nullptr;
  return &objarray[idx];
}

void PackGameState(byte* p) {
  std::memset(p, 0, GAMESTATESIZE);
  PutWord(p + 0x00, static_cast<word>(gamestate.worldx));
  PutWord(p + 0x02, static_cast<word>(gamestate.worldy));
  for (int i = 0; i < GAMELEVELS; i++) PutWord(p + 0x04 + i * 2, static_cast<word>(gamestate.leveldone[i]));
  const PlayerState& ps = players[0];
  PutWord(p + 0x36, static_cast<word>(ps.score));
  PutWord(p + 0x38, static_cast<word>(ps.score >> 16));
  PutWord(p + 0x3A, static_cast<word>(ps.nextextra));
  PutWord(p + 0x3C, static_cast<word>(ps.nextextra >> 16));
  PutWord(p + 0x3E, static_cast<word>(ps.ammo));
  PutWord(p + 0x40, static_cast<word>(ps.drops));
  PutWord(p + 0x42, static_cast<word>(gamestate.wetsuit));
  PutWord(p + 0x44, static_cast<word>(gamestate.rescued));
  for (int i = 0; i < 4; i++) PutWord(p + 0x46 + i * 2, static_cast<word>(gamestate.keys[i]));
  PutWord(p + 0x4E, static_cast<word>(gamestate.mapon));
  PutWord(p + 0x50, static_cast<word>(gamestate.lives));
  PutWord(p + 0x52, static_cast<word>(gamestate.difficulty));
  PutWord(p + 0x54, ObjAddress(ps.riding));
}

void UnpackGameState(const byte* p) {
  gamestate.worldx = static_cast<sword>(GetWord(p + 0x00));
  gamestate.worldy = static_cast<sword>(GetWord(p + 0x02));
  for (int i = 0; i < GAMELEVELS; i++) gamestate.leveldone[i] = static_cast<boolean>(GetWord(p + 0x04 + i * 2));
  PlayerState& ps = players[0];
  ps.score = static_cast<std::int32_t>(GetWord(p + 0x36) | (GetWord(p + 0x38) << 16));
  ps.nextextra = static_cast<std::int32_t>(GetWord(p + 0x3A) | (GetWord(p + 0x3C) << 16));
  ps.ammo = static_cast<sword>(GetWord(p + 0x3E));
  ps.drops = static_cast<sword>(GetWord(p + 0x40));
  gamestate.wetsuit = static_cast<boolean>(GetWord(p + 0x42));
  gamestate.rescued = static_cast<sword>(GetWord(p + 0x44));
  for (int i = 0; i < 4; i++) gamestate.keys[i] = static_cast<sword>(GetWord(p + 0x46 + i * 2));
  gamestate.mapon = static_cast<sword>(GetWord(p + 0x4E));
  gamestate.lives = static_cast<sword>(GetWord(p + 0x50));
  gamestate.difficulty = static_cast<sword>(GetWord(p + 0x52));
  ps.riding = nullptr;
}

// `next` is the object written after it (0 for the last).
void PackObj(const objtype* ob, const objtype* next, byte* p) {
  const word fields[] = {
      static_cast<word>(ob->obclass), static_cast<word>(ob->active), static_cast<word>(ob->needtoreact),
      static_cast<word>(ob->needtoclip), ob->nothink, ob->x, ob->y, static_cast<word>(ob->xdir),
      static_cast<word>(ob->ydir), static_cast<word>(ob->xmove), static_cast<word>(ob->ymove),
      static_cast<word>(ob->xspeed), static_cast<word>(ob->yspeed), static_cast<word>(ob->ticcount),
      static_cast<word>(ob->state ? ob->state->dgofs : 0), ob->shapenum, ob->priority, ob->left, ob->top,
      ob->right, ob->bottom, ob->midx, ob->tileleft, ob->tiletop, ob->tileright, ob->tilebottom,
      ob->tilemidx, static_cast<word>(ob->hitnorth), static_cast<word>(ob->hiteast),
      static_cast<word>(ob->hitsouth), static_cast<word>(ob->hitwest), static_cast<word>(ob->temp1),
      static_cast<word>(ob->temp2), static_cast<word>(ob->temp3),
      static_cast<word>(ob->tempobj ? ObjAddress(ob->tempobj) : ob->temp4),
      static_cast<word>(ob->sprite ? 1 : 0), ObjAddress(next), ObjAddress(ob->prev)};
  static_assert(sizeof(fields) / sizeof(fields[0]) * 2 == OBJSIZE, "objtype is 0x4C bytes");
  for (int i = 0; i < OBJSIZE / 2; i++) PutWord(p + i * 2, fields[i]);
}

// Fills everything but the list links. Returns the saved "next" field.
word UnpackObj(objtype* ob, const byte* p) {
  auto w = [p](int ofs) { return GetWord(p + ofs); };
  auto s = [p](int ofs) { return static_cast<sword>(GetWord(p + ofs)); };
  ob->obclass = static_cast<classtype>(s(0x00));
  ob->active = static_cast<activetype>(s(0x02));
  ob->needtoreact = s(0x04);
  ob->needtoclip = static_cast<cliptype>(s(0x06));
  ob->nothink = w(0x08);
  ob->x = w(0x0A);
  ob->y = w(0x0C);
  ob->xdir = s(0x0E);
  ob->ydir = s(0x10);
  ob->xmove = s(0x12);
  ob->ymove = s(0x14);
  ob->xspeed = s(0x16);
  ob->yspeed = s(0x18);
  ob->ticcount = s(0x1A);
  ob->state = w(0x1C) ? ST(w(0x1C)) : nullptr;
  ob->shapenum = w(0x1E);
  ob->priority = w(0x20);
  ob->left = w(0x22);
  ob->top = w(0x24);
  ob->right = w(0x26);
  ob->bottom = w(0x28);
  ob->midx = w(0x2A);
  ob->tileleft = w(0x2C);
  ob->tiletop = w(0x2E);
  ob->tileright = w(0x30);
  ob->tilebottom = w(0x32);
  ob->tilemidx = w(0x34);
  ob->hitnorth = s(0x36);
  ob->hiteast = s(0x38);
  ob->hitsouth = s(0x3A);
  ob->hitwest = s(0x3C);
  ob->temp1 = s(0x3E);
  ob->temp2 = s(0x40);
  ob->temp3 = s(0x42);
  ob->temp4 = s(0x44);
  ob->sprite = nullptr;
  ob->temp2sprite = ob->temp3sprite = nullptr;
  ob->tempobj = ob->obclass == dopefishobj ? ObjFromAddress(w(0x44)) : nullptr;
  return w(0x48);
}

bool WriteBytes(std::FILE* f, const void* data, std::size_t size) {
  return std::fwrite(data, 1, size, f) == size;
}
bool ReadBytes(std::FILE* f, void* data, std::size_t size) {
  return std::fread(data, 1, size, f) == size;
}

// 05C4:0006: lets the level graphics be purged before the finale.
void FreeGraphics() {}

// 05C4:0093
void GameOverScreen() {
  VW_FixRefreshBuffer();
  US_CenterWindow(16, 3);
  US_PrintCentered(g_exe.String(0x024E));  // "Game Over!"
  VW_UpdateScreen();
  IN_ClearKeysDown();
  IN_UserInput(4 * 70, false);
}

// 05C4:0484: marks finished levels on the world map.
void PatchWorldMap() {
  word* info = mapsegs[2];
  word ofs = 0;
  for (word y = 0; y < mapheight; y++) {
    for (word x = 0; x < mapwidth; x++, info++, ofs++) {
      word tile = *info;
      word level = static_cast<word>(tile & 0xFF);
      if (level >= 1 && level <= 17 && gamestate.leveldone[level]) {
        word flag = static_cast<word>(tile >> 8);
        *info = 0;
        if (flag == 0xD0) {
          mapsegs[1][ofs] = 0;  // remove the fence around the level
        } else if (flag == 0xF0) {
          if (lastlevel == static_cast<sword>(level))
            SpawnThrowFlag(x, y);
          else
            SpawnFlag(x, y);
        }
      }
    }
  }
}

// 05C4:0561
void FadeAndUnhookHook() {
  if (++fadecount == 2) {
    VW_FadeIn();
    RF_SetRefreshHook(nullptr);
    lasttimecount = TimeCount;
  }
}

// 05C4:0541
void FadeAndUnhook() {
  VW_FadeOut();
  fadecount = 0;
  RF_SetRefreshHook(&FadeAndUnhookHook);
}

// 05C4:066D. The loading dialog: Keen's picture and the level's entry text.
void DialogDraw(const char* title, word numcache) {
  cachebox_frame = 0;
  for (int i = 0; i < 6; i++) {
    CA_CacheGrChunk(115 + i);
    grneeded[115 + i] &= static_cast<byte>(~ca_levelbit);
  }
  US_CenterWindow(26, 8);
  if (grsegs[115])
    VWB_DrawPic(WindowX, WindowY, 115);
  else
    cachebox_frame = 5;
  grneeded[115] &= static_cast<byte>(~ca_levelbit);
  WindowW -= 48;
  WindowX += 48;
  word width, height;
  SizeText(title, &width, &height);
  PrintY = static_cast<word>(PrintY + (WindowH - height) / 2 - 4);
  US_CPrint(title);
  VW_UpdateScreen();

  cachebox_chunks = cachebox_count = numcache / 6;
  if (!cachebox_count && !cachebox_frame) {
    cachebox_frame = 5;
    if (grsegs[120]) VWB_DrawPic(WindowX - 24, WindowY + 40, 120);
    VW_UpdateScreen();
  }
}

// 05C4:07EA
void DialogUpdate() {
  if (--cachebox_count == 0 && cachebox_frame <= 4) {
    cachebox_count = cachebox_chunks;
    if (grsegs[116 + cachebox_frame]) VWB_DrawPic(WindowX - 24, WindowY + 40, 116 + cachebox_frame);
    VW_UpdateScreen();
    cachebox_frame++;
  }
}

// 05C4:085B
void DialogFinish() {}

// 05C4:09F2
void HandleDeathImpl() {
  std::strncpy(str, LevelName(gamestate.mapon), sizeof str - 1);
  word w, h;
  SizeText(str, &w, &h);
  std::memset(gamestate.keys, 0, sizeof gamestate.keys);
  if (--gamestate.lives < 0) return;

  VW_FixRefreshBuffer();
  US_CenterWindow(20, 8);
  PrintY += 3;
  US_CPrint(g_exe.String(0x02FF));  // "You didn't make it past"
  word y1 = static_cast<word>(PrintY + 22);
  if (h < 15) PrintY += 4;
  US_CPrint(str);
  PrintY = static_cast<word>(y1 + 2);
  US_CPrint(g_exe.String(0x0317));  // "Try Again"
  PrintY += 4;
  word y2 = static_cast<word>(PrintY - 2);
  US_CPrint(g_exe.String(0x0321));  // "Exit to Shadowlands"

  IN_ClearKeysDown();
  boolean selection = false;
  for (;;) {
    word y = selection ? y2 : y1;
    for (int pass = 0; pass < 2; pass++) {
      int color = pass ? 15 : (((TimeCount >> 4) & 1) ? 12 : 1);
      int xl = WindowX + 4, xh = WindowX + WindowW - 4;
      VWB_Hlin(xl, xh, y, color);
      VWB_Hlin(xl, xh, y + 1, color);
      VWB_Hlin(xl, xh, y + 12, color);
      VWB_Hlin(xl, xh, y + 13, color);
      VWB_Vlin(y + 1, y + 11, WindowX + 4, color);
      VWB_Vlin(y + 1, y + 11, WindowX + 5, color);
      VWB_Vlin(y + 1, y + 11, WindowX + WindowW - 4, color);
      VWB_Vlin(y + 1, y + 11, WindowX + WindowW - 5, color);
      if (!pass) VW_UpdateScreen();
    }

    if (LastScan == sc_Escape) {
      gamestate.mapon = 0;
      IN_ClearKeysDown();
      return;
    }
    IN_ReadControl(0, &c);
    if (c.button0 || c.button1 || LastScan == sc_Return || LastScan == sc_Space) {
      if (selection) gamestate.mapon = 0;
      return;
    }
    if (c.yaxis == -1 || LastScan == sc_UpArrow)
      selection = false;
    else if (c.yaxis == 1 || LastScan == sc_DownArrow)
      selection = true;
  }
}

}  // namespace

// Far pointer tables of level names (DGROUP 18F4) and entry texts (1958).
const char* LevelName(int level) {
  word ofs = g_exe.Word(static_cast<std::uint16_t>(0x18F4 + level * 4));
  word seg = g_exe.Word(static_cast<std::uint16_t>(0x18F6 + level * 4));
  return keen4::lang::Tr(reinterpret_cast<const char*>(g_exe.At(seg, ofs)));
}
const char* LevelEnter(int level) {
  word ofs = g_exe.Word(static_cast<std::uint16_t>(0x1958 + level * 4));
  word seg = g_exe.Word(static_cast<std::uint16_t>(0x195A + level * 4));
  return keen4::lang::Tr(reinterpret_cast<const char*>(g_exe.At(seg, ofs)));
}

void InstallCacheBoxHooks() {
  drawcachebox = DialogDraw;
  updatecachebox = DialogUpdate;
  finishcachebox = DialogFinish;
}

// Port: co-op saves. The objects are written as for one player, without Tom
// and his score box, so a build without co-op loads the game for one player.
// After them comes this trailer: the tag, a version, the number of players,
// each player's co-op status and respawn tics left, player 2's score, extra
// Keen threshold, ammo and drops, then Tom's and his score box's objects.
constexpr byte kCoopTag[4] = {'K', 'C', 'O', 'P'};
constexpr word kCoopVersion = 1;

bool WriteCoopTrailer(std::FILE* file) {
  byte buf[4 + 2 + 2 + 4 * MAXPLAYERS + 12];
  byte* p = buf;
  std::memcpy(p, kCoopTag, 4);
  PutWord(p + 4, kCoopVersion);
  PutWord(p + 6, static_cast<word>(numplayers));
  p += 8;
  for (int i = 0; i < MAXPLAYERS; i++) {
    sword status;
    word left;
    CoopSaveStatus(i, &status, &left);
    PutWord(p, static_cast<word>(status));
    PutWord(p + 2, left);
    p += 4;
  }
  const PlayerState& tom = players[1];
  PutWord(p, static_cast<word>(tom.score));
  PutWord(p + 2, static_cast<word>(tom.score >> 16));
  PutWord(p + 4, static_cast<word>(tom.nextextra));
  PutWord(p + 6, static_cast<word>(tom.nextextra >> 16));
  PutWord(p + 8, static_cast<word>(tom.ammo));
  PutWord(p + 10, static_cast<word>(tom.drops));
  if (!WriteBytes(file, buf, sizeof buf)) return false;
  for (objtype* ob : {tom.ob, tom.scorebox}) {
    byte obj[OBJSIZE];
    PackObj(ob, nullptr, obj);
    if (!WriteBytes(file, obj, sizeof obj)) return false;
  }
  return true;
}

// Reads the co-op trailer if there is one; false for a one-player save.
bool ReadCoopTrailer(std::FILE* file) {
  byte buf[4 + 2 + 2 + 4 * MAXPLAYERS + 12];
  if (!ReadBytes(file, buf, sizeof buf) || std::memcmp(buf, kCoopTag, 4) || GetWord(buf + 4) != kCoopVersion ||
      GetWord(buf + 6) != 2)
    return false;
  byte objs[2][OBJSIZE];
  if (!ReadBytes(file, objs[0], OBJSIZE) || !ReadBytes(file, objs[1], OBJSIZE)) return false;
  numplayers = 2;
  const byte* p = buf + 8;
  for (int i = 0; i < MAXPLAYERS; i++, p += 4)
    CoopLoadStatus(i, static_cast<sword>(GetWord(p)), GetWord(p + 2));
  PlayerState& tom = players[1];
  tom.score = static_cast<std::int32_t>(GetWord(p) | (GetWord(p + 2) << 16));
  tom.nextextra = static_cast<std::int32_t>(GetWord(p + 4) | (GetWord(p + 6) << 16));
  tom.ammo = static_cast<sword>(GetWord(p + 8));
  tom.drops = static_cast<sword>(GetWord(p + 10));
  tom.riding = nullptr;
  for (int k = 0; k < 2; k++) {
    GetNewObj(false);
    objtype* prev = newobj->prev;
    UnpackObj(newobj, objs[k]);
    newobj->prev = prev;
    newobj->next = nullptr;
    newobj->needtoreact = true;
    newobj->sprite = nullptr;
    if (k == 0)
      tom.ob = newobj;
    else
      tom.scorebox = newobj;
  }
  tom.scorebox->temp1 = tom.scorebox->temp2 = tom.scorebox->temp3 = tom.scorebox->temp4 = -1;
  // the players pick their devices again (ID_US_2 asks after loading)
  players[0].pad = players[1].pad = 0;
  restartcoop = true;
  return true;
}

// 05C4:0063
void NewGame() {
  std::memset(&gamestate, 0, sizeof gamestate);
  numplayers = 1;
  ResetPlayers();
  gamestate.lives = 3;
}

// 05C4:00D3
boolean SaveTheGame(std::FILE* file) {
  for (PlayerState& ps : players) ps.riding = nullptr;
  byte buf[GAMESTATESIZE];
  PackGameState(buf);
  if (!WriteBytes(file, buf, sizeof buf)) return false;

  std::uint32_t size = static_cast<std::uint32_t>(mapwidth) * mapheight * 2;
  std::vector<word> packed(size + 8);
  for (int plane = 0; plane < 3; plane++) {
    word length = static_cast<word>(CA_RLEWCompress(mapsegs[plane], size, packed.data() + 1, RLEWTAG));
    packed[0] = length;
    std::vector<byte> bytes(length + 2u);
    for (std::size_t i = 0; i < bytes.size() / 2; i++) PutWord(&bytes[i * 2], packed[i]);
    if (!WriteBytes(file, bytes.data(), bytes.size())) return false;
  }

  // port: Tom and his score box go in the co-op trailer
  auto tomsown = [](const objtype* ob) {
    return numplayers > 1 && (ob == players[1].ob || ob == players[1].scorebox);
  };
  for (objtype* ob = player; ob; ob = ob->next) {
    if (tomsown(ob)) continue;
    objtype* next = ob->next;
    while (next && tomsown(next)) next = next->next;
    byte obj[OBJSIZE];
    PackObj(ob, next, obj);
    if (!WriteBytes(file, obj, sizeof obj)) return false;
  }
  if (numplayers > 1) return WriteCoopTrailer(file);
  return true;
}

// 05C4:01E0
boolean LoadTheGame(std::FILE* file) {
  byte buf[GAMESTATESIZE];
  if (!ReadBytes(file, buf, sizeof buf)) return false;
  // port: one player unless the co-op trailer says otherwise
  numplayers = 1;
  restartcoop = false;
  UnpackGameState(buf);

  ca_levelbit >>= 1;
  ca_levelnum--;
  SetupGameLevel(false);
  ca_levelbit <<= 1;
  ca_levelnum++;

  std::uint32_t size = static_cast<std::uint32_t>(mapwidth) * mapheight * 2;
  for (int plane = 0; plane < 3; plane++) {
    byte lenbytes[2];
    if (!ReadBytes(file, lenbytes, 2)) return false;
    word length = GetWord(lenbytes);
    std::vector<byte> bytes(length + 2u);
    if (!ReadBytes(file, bytes.data(), length)) return false;
    std::vector<word> packed(length / 2 + 2);
    for (std::size_t i = 0; i < length / 2u; i++) packed[i] = GetWord(&bytes[i * 2]);
    CA_RLEWexpand(packed.data(), mapsegs[plane], size, RLEWTAG);
  }

  InitObjArray();
  newobj = player;
  for (;;) {
    objtype* prev = newobj->prev;
    objtype* next = newobj->next;
    byte obj[OBJSIZE];
    if (!ReadBytes(file, obj, sizeof obj)) return false;
    word following = UnpackObj(newobj, obj);
    newobj->prev = prev;
    newobj->next = next;
    newobj->needtoreact = true;
    newobj->sprite = nullptr;
    if (newobj == player) {
      newobj = scoreobj;
      continue;
    }
    if (newobj->obclass == stunnedobj) {
      newobj->temp3 = 0;
    } else if (newobj->obclass == platformobj) {
      newobj->temp2 = newobj->temp3 = 0;
    }
    if (!following) break;
    GetNewObj(false);
  }

  scoreobj->temp2 = -1;
  scoreobj->temp1 = -1;
  scoreobj->temp3 = -1;
  scoreobj->temp4 = -1;
  ReadCoopTrailer(file);
  return true;
}

// 05C4:0466
void ResetGame() {
  NewGame();
  // port: co-op
  if (restartcoop) {
    numplayers = 2;
    players[0].pad = restartpads[0];
    players[1].pad = restartpads[1];
  }
  ca_levelnum--;
  ca_levelbit >>= 1;
  CA_ClearMarks();
  ca_levelbit <<= 1;
  ca_levelnum++;
}

// 05C4:0591
void SetupGameLevel(boolean loadnow) {
  if (DemoMode != demo_Off) {
    US_InitRndT(false);
    gamestate.difficulty = gd_Normal;
  } else {
    US_InitRndT(true);
  }

  CA_CacheMap(gamestate.mapon);
  RF_NewMap();
  CA_ClearMarks();
  ScanInfoPlane();
  if (mapon == 0) PatchWorldMap();
  RF_MarkTileGraphics();
  CA_LoadAllSounds();

  if (loadnow) {
    if (scorescreenkludge) {
      CA_CacheMarks(nullptr);
    } else if (DemoMode != demo_Off) {
      CA_CacheMarks(g_exe.String(0x0299));  // "DEMO"
    } else {
      std::strncpy(str, LevelEnter(mapon), sizeof str - 1);
      CA_CacheMarks(str);
    }
  }
  if (loadnow) FadeAndUnhook();
}

// 05C4:0860
void StartDemoRecord() {
  VW_FixRefreshBuffer();
  US_CenterWindow(30, 3);
  PrintY += 6;
  US_Print(g_exe.String(0x029E));  // "  Record a demo from level(0-21):"
  VW_UpdateScreen();
  if (US_LineInput(PrintX, PrintY, str, nullptr, true, 2, 0)) {
    int level = std::atoi(str);
    if (level >= 0 && level <= 21) {
      gamestate.mapon = static_cast<sword>(level);
      playstate = ex_warped;
      IN_StartDemoRecord(0x1000);
    }
  }
}

// 05C4:08F4
void EndDemoRecord() {
  char filename[10];
  std::memcpy(filename, g_exe.String(0x0244), sizeof filename);  // "DEMO?.CK4"
  IN_StopDemo();
  VW_FixRefreshBuffer();
  US_CenterWindow(22, 3);
  PrintY += 6;
  US_Print(g_exe.String(0x02C0));  // "  Save as demo #(0-9):"
  VW_UpdateScreen();
  if (US_LineInput(PrintX, PrintY, str, nullptr, true, 2, 0) && str[0] >= '0' && str[0] <= '9') {
    filename[4] = str[0];
    std::FILE* f = std::fopen(keen4::files::WritePath(filename).c_str(), "wb");
    if (!f) Quit(g_exe.String(0x02D7));  // "EndDemoRecord:  Cannot write demo file!"
    byte header[4];
    PutWord(header, static_cast<word>(mapon));
    PutWord(header + 2, DemoOffset);
    WriteBytes(f, header, sizeof header);
    WriteBytes(f, DemoBuffer, DemoOffset);
    std::fclose(f);
  }
  IN_FreeDemoBuffer();
}

void HandleDeath() { HandleDeathImpl(); }

// Port: the high score table, for both players in co-op (each enters a name).
void HighScores() {
  const int count = numplayers;
  const std::int32_t tomscore = players[1].score;
  numplayers = 1;  // the table is shown on a level of its own
  if (count == 1) {
    CheckHighScore(players[0].score, static_cast<word>(gamestate.rescued));
    return;
  }
  CheckHighScore(players[0].score, static_cast<word>(gamestate.rescued), "Billy");
  CheckHighScore(tomscore, static_cast<word>(gamestate.rescued), "Tom");
}

void GameOver() { GameOverScreen(); }

// 05C4:0D81
void GameLoop() {
  enum { kNewLevel, kSetup, kPlay } step = kPlay;
  if (playstate != ex_loadedgame) {
    gamestate.difficulty = restartgame;
    restartgame = gd_Continue;
    step = kSetup;
  }

  for (;;) {
    if (step == kNewLevel) {
      gamestate.difficulty = restartgame;
      restartgame = gd_Continue;
      step = kSetup;
    }
    if (step == kSetup) {
      SetupGameLevel(true);
      step = kPlay;
    }

    lockscroll = false;
    SD_WaitSoundDone();
    PlayLoop();
    if (playstate != ex_loadedgame) std::memset(gamestate.keys, 0, sizeof gamestate.keys);
    VW_FixRefreshBuffer();

    if (tedlevel) {
      if (playstate == ex_loadedgame) continue;
      if (playstate == ex_died) {
        step = kSetup;
        continue;
      }
      TEDDeath();
    }

    lastlevel = -1;
    switch (playstate) {
      case ex_died:
        HandleDeath();
        break;

      case ex_rescued:
        if (mapon) SD_PlaySound(SND_LEVELDONE);
        lastlevel = mapon;
        gamestate.leveldone[mapon] = true;
        RescuedMember();
        if (gamestate.rescued != 8) {
          gamestate.mapon = 0;
          break;
        }
        FreeGraphics();
        RF_FixOfs();
        VW_FixRefreshBuffer();
        FinaleLayout();
        HighScores();
        return;

      case ex_completed:
      case ex_foot:
      case ex_portout:
        if (mapon) {
          SD_PlaySound(SND_LEVELDONE);
          gamestate.mapon = 0;
          lastlevel = mapon;
          gamestate.leveldone[mapon] = true;
          if (storedemo && mapon == 2) {
            IN_ClearKeysDown();
            return;
          }
        } else {
          // drawn straight onto the visible page in the original
          US_CenterWindow(26, 8);
          PrintY += 25;
          US_CPrint(g_exe.String(0x0389));  // "One moment"
          VW_UpdateScreen();
        }
        break;

      case ex_resetgame:
        step = kNewLevel;
        continue;

      case ex_loadedgame:
        step = kPlay;
        continue;

      case ex_abortgame:
        IN_ClearKeysDown();
        return;

      default:
        break;
    }

    if (gamestate.lives < 0) break;
    step = kSetup;
  }

  GameOver();
  HighScores();
}
