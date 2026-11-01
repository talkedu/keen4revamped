#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "update/http.h"

// GitHub side of the in-game updater: reading the latest release of the
// public repository and downloading its files. It needs no GitHub account:
// the requests go to the public REST API without a token.
namespace keen4::update {

struct Asset {
  std::string name;
  std::uint64_t id = 0;
  std::uint64_t size = 0;
};

struct Release {
  std::string tag;  // such as "v1.3"
  std::vector<Asset> assets;
};

// Version numbers in "v1.3", "1.3.0" or "keen4revamped 1.3.0", as
// numbers; missing parts count as 0, so v1.3 and 1.3.0 are equal.
std::vector<int> ParseVersion(const std::string& text);
int CompareVersions(const std::string& a, const std::string& b);

bool ParseRelease(const std::string& json, Release* out);
// The release archive for this platform, such as keen4revamped-v1.3-linux-x64.tar.gz.
const Asset* PickAsset(const Release& release, const std::string& suffix);
std::string PlatformAssetSuffix();

// The repository the game updates from: KEEN4_UPDATE_REPO in the
// environment, else the one the build was configured with.
std::string Repo();

enum class Check { Ok, NoRelease, Failed };
Check FetchLatestRelease(Release* out, std::string* error);

// Downloads `asset` to `path`; `progress` may cancel.
bool DownloadAsset(const Asset& asset, const std::string& path, const http::Progress& progress, bool* canceled,
                   std::string* error);

}  // namespace keen4::update
