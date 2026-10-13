#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "update/http.h"

// GitHub side of the in-game updater: signing in with the OAuth device flow
// (the game shows a code, the player enters it on github.com, so no secret
// ships in the binary) and reading the latest release of the repository.
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

// The OAuth app the game signs in with and the repository it updates from:
// KEEN4_GITHUB_CLIENT_ID and KEEN4_UPDATE_REPO in the environment, else the
// values the build was configured with. An empty client id disables updates.
std::string ClientId();
std::string Repo();
// True when the client id is a GitHub App's ("Iv..."), not an OAuth app's.
bool IsGitHubApp();

// The player's token, kept in the folder with CONFIG.CK4 and the saved games
// (or KEEN4_GITHUB_TOKEN in the environment).
std::string LoadToken();
void SaveToken(const std::string& token);
void ForgetToken();

struct DeviceCode {
  std::string device_code, user_code, verification_uri;
  int interval = 5;      // seconds between polls
  int expires_in = 900;  // seconds
};

bool RequestDeviceCode(DeviceCode* out, std::string* error);

enum class Poll { Pending, SlowDown, Token, Denied, Expired, Failed };
Poll PollForToken(const DeviceCode& code, std::string* token, std::string* error);

enum class Check { Ok, SignIn, NoAccess, NoRelease, Failed };
Check FetchLatestRelease(const std::string& token, Release* out, std::string* error);

// Downloads `asset` to `path`; `progress` may cancel.
bool DownloadAsset(const std::string& token, const Asset& asset, const std::string& path,
                   const http::Progress& progress, bool* canceled, std::string* error);

}  // namespace keen4::update
