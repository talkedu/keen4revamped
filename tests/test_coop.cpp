// Co-op tests.
//
// The scenarios run the game headless on level 1 in co-op (/TEDLEVEL 1 COOP),
// player 1 on the keyboard and player 2 on a scripted gamepad, and check the
// co-op events the game logs on stderr. They drive deaths and exits with the
// co-op debug keys (F10 + 1/2 kills Billy/Tom, F10 + 3/4 sends him off the
// map). They need the game data: set KEEN4_DATA, or keep it in gamedata/ of
// the source tree; without it the test is skipped.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

static int failures = 0;

#define CHECK(cond)                                              \
  do {                                                           \
    if (!(cond)) {                                               \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                \
    }                                                            \
  } while (0)

namespace {

namespace fs = std::filesystem;

std::string datadir;

// One logged co-op event: "co-op: tic T player P EVENT, L lives".
struct Event {
  unsigned long tic = 0;
  int player = 0;
  std::string what;
  int lives = 0;
};

struct Run {
  std::vector<Event> events;
  std::vector<std::string> lines;  // every co-op line
  std::string end;                 // the "map ... ends" line
};

// Keys pressed together at `tic` for 5 tics.
std::string Press(int tic, std::initializer_list<int> scans) {
  std::string s;
  char buf[32];
  for (int sc : scans) {
    std::snprintf(buf, sizeof buf, "%d:%d:1,", tic, sc);
    s += buf;
  }
  for (int sc : scans) {
    std::snprintf(buf, sizeof buf, "%d:%d:0,", tic + 5, sc);
    s += buf;
  }
  return s;
}

constexpr int F10 = 0x44, ONE = 0x02, TWO = 0x03, THREE = 0x04, FOUR = 0x05, KEY_I = 0x17, SPACE = 0x39;

// `mode`: COOP (player 1 on the keyboard, 2 on the pad) or COOPPAD (the other way round).
Run Play(const std::string& keys, int tics, const char* mode = "COOP") {
  fs::path dir = fs::temp_directory_path() / "keen4-coop-test";
  fs::remove_all(dir);
  fs::create_directories(dir);
  std::string cmd = std::string("\"") + KEEN4_BINARY + "\" --headless --data \"" + datadir + "\" --save \"" +
                    dir.string() + "\" --out \"" + dir.string() + "\" --frames 1000000 --tics " +
                    std::to_string(tics) + " --keys \"" + keys + "\" --gamepad2 0:0 /TEDLEVEL 1 " + mode + " 2>&1";
  Run run;
  std::FILE* p = popen(cmd.c_str(), "r");
  if (!p) return run;
  char line[512];
  while (std::fgets(line, sizeof line, p)) {
    if (std::strncmp(line, "co-op: ", 7)) continue;
    std::string text(line + 7);
    run.lines.push_back(text);
    if (text.find(" ends ") != std::string::npos) {
      if (run.end.empty()) run.end = text;
      continue;
    }
    Event e;
    char what[128] = {};
    if (std::sscanf(line, "co-op: tic %lu player %d %127[^,], %d lives", &e.tic, &e.player, what, &e.lives) == 4) {
      e.what = what;
      run.events.push_back(e);
    }
  }
  pclose(p);
  fs::remove_all(dir);
  return run;
}

const Event* Find(const Run& run, int player, const char* what, std::size_t from = 0) {
  for (std::size_t i = from; i < run.events.size(); i++)
    if (run.events[i].player == player && run.events[i].what == what) return &run.events[i];
  return nullptr;
}

}  // namespace

int main() {
#ifdef _WIN32
  std::printf("co-op scenarios skipped on Windows\n");
  return 77;
#else
  if (const char* env = std::getenv("KEEN4_DATA")) datadir = env;
  if (datadir.empty() && fs::exists(fs::path(KEEN4_SOURCE_DIR) / "gamedata")) datadir = KEEN4_SOURCE_DIR "/gamedata";
  if (datadir.empty()) {
    std::printf("co-op scenarios skipped: no game data (set KEEN4_DATA)\n");
    return 77;
  }

  // Tom dies with Billy in play: he sits out three seconds and comes back
  // for a life. Then Billy dies and, while he waits, Tom: the level fails.
  {
    Run run = Play(Press(300, {F10, TWO}) + Press(1200, {F10, ONE}) + Press(1250, {F10, TWO}), 1500);
    const Event* died = Find(run, 2, "died");
    const Event* back = Find(run, 2, "is back");
    CHECK(died && back);
    if (died && back) {
      CHECK(back->tic - died->tic >= 210 && back->tic - died->tic <= 216);
      CHECK(died->lives == 3 && back->lives == 2);
    }
    CHECK(Find(run, 1, "died"));
    CHECK(Find(run, 2, "died and the level fails"));
    CHECK(run.end.find("(playstate 1)") != std::string::npos);  // ex_died
  }

  // Tom spends all three lives coming back. Dying once more with none left he
  // is out, and Billy plays on alone until he dies too: then the level fails.
  {
    Run run = Play(Press(300, {F10, TWO}) + Press(700, {F10, TWO}) + Press(1100, {F10, TWO}) +
                       Press(1500, {F10, TWO}) + Press(2000, {F10, ONE}),
                   2300);
    const Event* out = Find(run, 2, "died with no lives left");
    CHECK(out);
    if (out) CHECK(out->lives == 0 && out->tic < 1600);
    CHECK(!Find(run, 2, "died and the level fails"));
    const Event* last = Find(run, 1, "died and the level fails");
    CHECK(last);
    if (out && last) CHECK(last->tic > out->tic + 300);
    CHECK(run.end.find("(playstate 1)") != std::string::npos);  // ex_died
  }

  // With Tom out, Billy finishes the level alone by walking off the map.
  {
    Run run = Play(Press(300, {F10, TWO}) + Press(700, {F10, TWO}) + Press(1100, {F10, TWO}) +
                       Press(1500, {F10, TWO}) + Press(1800, {F10, THREE}),
                   2000);
    CHECK(Find(run, 2, "died with no lives left"));
    CHECK(Find(run, 1, "reached the exit"));
    CHECK(run.end.find("(playstate 2)") != std::string::npos);  // ex_completed
  }

  // Coming back, Tom flickers and cannot be hurt for two seconds.
  {
    Run run = Play(Press(300, {F10, TWO}) + Press(600, {F10, TWO}) + Press(900, {F10, TWO}), 1100);
    const Event* back = Find(run, 2, "is back");
    CHECK(back);
    int deaths = 0;
    for (const Event& e : run.events) deaths += e.player == 2 && e.what == "died";
    CHECK(deaths == 2);  // the second key comes while he is immune
    CHECK(run.end.empty());
  }

  // The level ends when both have walked off the map; the first waits.
  {
    Run run = Play(Press(300, {F10, THREE}) + Press(600, {F10, FOUR}), 1000);
    const Event* first = Find(run, 1, "reached the exit");
    const Event* second = Find(run, 2, "reached the exit");
    CHECK(first && second && first->tic < second->tic && second->tic - first->tic > 200);
    CHECK(run.end.find("(playstate 2)") != std::string::npos);  // ex_completed
  }

  // Billy walks away to the left: the screen splits side by side, and when he
  // comes back it is one view again.
  {
    Run run = Play("300:75:1,700:75:0,800:77:1,1150:77:0", 1600);
    std::size_t split = run.lines.size(), merged = run.lines.size();
    for (std::size_t i = 0; i < run.lines.size(); i++) {
      if (split == run.lines.size() && run.lines[i].find("screen splits side by side") != std::string::npos) split = i;
      if (split < i && run.lines[i].find("screen is one view") != std::string::npos) merged = i;
    }
    CHECK(split < run.lines.size());
    CHECK(merged < run.lines.size());
  }

  // Player 1 can take a gamepad and leave the keyboard to player 2: the keys
  // then walk Tom away to the left and the screen splits.
  {
    Run run = Play("300:75:1,700:75:0", 900, "COOPPAD");
    bool pad1 = false, keys2 = false, split = false;
    for (const std::string& line : run.lines) {
      pad1 |= line.find("player 1 plays with a gamepad") != std::string::npos;
      keys2 |= line.find("player 2 plays with the keyboard") != std::string::npos;
      split |= line.find("screen splits") != std::string::npos;
    }
    CHECK(pad1 && keys2 && split);
  }

  // Points are each player's own: the free-items cheat gives Billy 3000.
  {
    Run run = Play(Press(300, {F10, KEY_I}) + Press(400, {SPACE}) + Press(500, {F10, THREE}) + Press(520, {F10, FOUR}),
                   900);
    CHECK(run.end.find("scores 3000 0,") != std::string::npos);
    CHECK(run.end.find("lives 3,") != std::string::npos);
  }

  if (failures == 0) std::printf("all co-op tests passed\n");
  return failures == 0 ? 0 : 1;
#endif
}
