// The updater's offline parts: JSON, version numbers, release assets.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "engine/version.h"
#include "update/github.h"
#include "update/http.h"
#include "update/json.h"

[[noreturn]] void Quit(const char* error) {
  std::printf("Quit: %s\n", error ? error : "");
  std::exit(1);
}

static int failures = 0;

#define CHECK(cond)                                              \
  do {                                                           \
    if (!(cond)) {                                               \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                \
    }                                                            \
  } while (0)

int main() {
  using namespace keen4;

  // JSON
  {
    json::Value v;
    CHECK(json::Parse(R"({"a": [1, 2.5, -3e2], "s": "x\"é😀", "t": true, "n": null, "o": {}})", &v));
    CHECK(v.IsObject());
    CHECK(v["a"].Items().size() == 3 && v["a"].Items()[2].Number() == -300);
    CHECK(v["s"].String() == "x\"\xC3\xA9\xF0\x9F\x98\x80");
    CHECK(v["t"].Bool() && v["n"].type() == json::Value::Type::Null && v["o"].IsObject());
    CHECK(v["missing"]["deeper"].String("none") == "none");
    CHECK(!json::Parse("{\"a\": }", &v) && v.type() == json::Value::Type::Null);
    CHECK(!json::Parse("[1, 2", &v));
    CHECK(!json::Parse("{} trailing", &v));
  }

  // version numbers
  {
    using update::CompareVersions;
    CHECK((update::ParseVersion("keen4revamped 1.2.0") == std::vector<int>{1, 2, 0}));
    CHECK((update::ParseVersion("v10.3") == std::vector<int>{10, 3}));
    CHECK(update::ParseVersion("latest").empty());
    CHECK(CompareVersions("v1.3", "1.2.0") > 0);
    CHECK(CompareVersions("v1.2", "1.2.0") == 0);
    CHECK(CompareVersions("v1.10", "1.9.9") > 0);
    CHECK(CompareVersions("v1.2", "1.2.1") < 0);
    CHECK(CompareVersions("v1.5.2", "1.5.2-SNAPSHOT") > 0);
    CHECK(CompareVersions("v1.5.1", "1.5.2-SNAPSHOT") < 0);
    CHECK(CompareVersions("1.5.2-SNAPSHOT", "keen4revamped 1.5.2-SNAPSHOT") == 0);
    CHECK(CompareVersions(std::string("v") + VersionNumber(), VersionString()) == 0);
  }

  // a release as GitHub sends it (trimmed)
  {
    const char* text = R"({
      "tag_name": "v1.3", "name": "keen4revamped v1.3", "draft": false,
      "assets": [
        {"id": 301, "name": "keen4revamped-v1.3-windows-x64.zip", "size": 2310144},
        {"id": 302, "name": "keen4revamped-v1.3-linux-x64.tar.gz", "size": 2201000},
        {"id": 303, "name": "keen4revamped-v1.3-windows-x64-installer.exe", "size": 2400000}
      ]})";
    update::Release r;
    CHECK(update::ParseRelease(text, &r));
    CHECK(r.tag == "v1.3" && r.assets.size() == 3);
    const update::Asset* win = update::PickAsset(r, "-windows-x64.zip");
    const update::Asset* lin = update::PickAsset(r, "-linux-x64.tar.gz");
    CHECK(win && win->id == 301 && win->size == 2310144);
    CHECK(lin && lin->id == 302);
    CHECK(update::PickAsset(r, update::PlatformAssetSuffix()) != nullptr);
    CHECK(!update::PickAsset(r, "-macos.zip"));
    CHECK(!update::ParseRelease(R"({"message": "Not Found"})", &r));
  }

  CHECK(http::FormEncode("urn:a b/c~d") == "urn%3Aa%20b%2Fc~d");

  if (failures == 0) std::printf("all tests passed\n");
  return failures == 0 ? 0 : 1;
}
