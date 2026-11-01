#include "update/github.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <system_error>

#include "engine/version.h"
#include "update/json.h"

#ifndef KEEN4_UPDATE_REPO
#define KEEN4_UPDATE_REPO "talkedu/keen4revamped"
#endif

namespace keen4::update {

namespace {

std::string Env(const char* name, const char* fallback) {
  const char* v = std::getenv(name);
  return v && *v ? v : fallback;
}

std::string UserAgent() { return std::string("User-Agent: keen4revamped/") + VersionNumber(); }

// Anonymous requests: the repository is public, so no token is sent.
std::vector<std::string> ApiHeaders(const char* accept) {
  return {std::string("Accept: ") + accept, "X-GitHub-Api-Version: 2022-11-28", UserAgent()};
}

std::string ApiError(const http::Response& response) {
  if (response.status == 0) return response.error;
  json::Value v;
  std::string message;
  if (json::Parse(response.body, &v)) message = v["message"].String();
  // GitHub allows 60 anonymous API requests an hour from one address
  if (response.status == 429 || (response.status == 403 && message.find("rate limit") != std::string::npos))
    return "GitHub is busy, try again in an hour";
  return "GitHub answered " + std::to_string(response.status) + (message.empty() ? "" : ": " + message);
}

}  // namespace

std::vector<int> ParseVersion(const std::string& text) {
  auto alnum = [&](std::size_t k) { return std::isalnum(static_cast<unsigned char>(text[k])) != 0; };
  auto digit = [&](std::size_t k) { return text[k] >= '0' && text[k] <= '9'; };
  // the first number that starts a word, or follows a "v" that does
  // (so not the 4 in "keen4revamped 1.2.0")
  std::size_t i = 0;
  for (; i < text.size(); i++) {
    if (!digit(i)) continue;
    if (i == 0 || !alnum(i - 1)) break;
    if ((text[i - 1] == 'v' || text[i - 1] == 'V') && (i == 1 || !alnum(i - 2))) break;
  }
  std::vector<int> parts;
  while (i < text.size() && digit(i)) {
    int n = 0;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9') n = n * 10 + (text[i++] - '0');
    parts.push_back(n);
    if (i + 1 < text.size() && text[i] == '.' && text[i + 1] >= '0' && text[i + 1] <= '9')
      i++;
    else
      break;
  }
  return parts;
}

int CompareVersions(const std::string& a, const std::string& b) {
  std::vector<int> va = ParseVersion(a), vb = ParseVersion(b);
  std::size_t n = va.size() > vb.size() ? va.size() : vb.size();
  for (std::size_t i = 0; i < n; i++) {
    int x = i < va.size() ? va[i] : 0, y = i < vb.size() ? vb[i] : 0;
    if (x != y) return x < y ? -1 : 1;
  }
  // a snapshot build comes before the release of its number
  bool sa = a.find("-SNAPSHOT") != std::string::npos, sb = b.find("-SNAPSHOT") != std::string::npos;
  if (sa != sb) return sa ? -1 : 1;
  return 0;
}

bool ParseRelease(const std::string& text, Release* out) {
  json::Value v;
  if (!json::Parse(text, &v) || !v.IsObject()) return false;
  *out = Release{};
  out->tag = v["tag_name"].String();
  for (const json::Value& a : v["assets"].Items()) {
    Asset asset;
    asset.name = a["name"].String();
    asset.id = static_cast<std::uint64_t>(a["id"].Number());
    asset.size = static_cast<std::uint64_t>(a["size"].Number());
    if (!asset.name.empty() && asset.id) out->assets.push_back(asset);
  }
  return !out->tag.empty();
}

const Asset* PickAsset(const Release& release, const std::string& suffix) {
  for (const Asset& a : release.assets) {
    if (a.name.size() >= suffix.size() && a.name.compare(a.name.size() - suffix.size(), suffix.size(), suffix) == 0)
      return &a;
  }
  return nullptr;
}

std::string PlatformAssetSuffix() {
#ifdef _WIN32
  return "-windows-x64.zip";
#elif defined(__aarch64__)
  // the handheld build: the port folder, without the PortMaster launcher
  return "-linux-aarch64.tar.gz";
#else
  return "-linux-x64.tar.gz";
#endif
}

std::string Repo() { return Env("KEEN4_UPDATE_REPO", KEEN4_UPDATE_REPO); }

Check FetchLatestRelease(Release* out, std::string* error) {
  std::string api = "https://api.github.com/repos/" + Repo();
  http::Request request;
  request.url = api + "/releases/latest";
  request.headers = ApiHeaders("application/vnd.github+json");
  http::Response response = http::Send(request);
  if (response.status == 200) {
    if (ParseRelease(response.body, out)) return Check::Ok;
    *error = "could not read the release";
    return Check::Failed;
  }
  if (response.status == 404) {
    // no release yet, or no such public repository
    request.url = api;
    http::Response repo = http::Send(request);
    if (repo.status == 200) return Check::NoRelease;
    *error = repo.status == 404 ? "no public repository " + Repo() + " on GitHub" : ApiError(repo);
    return Check::Failed;
  }
  *error = ApiError(response);
  return Check::Failed;
}

bool DownloadAsset(const Asset& asset, const std::string& path, const http::Progress& progress, bool* canceled,
                   std::string* error) {
  *canceled = false;
  http::Request request;
  request.url = "https://api.github.com/repos/" + Repo() + "/releases/assets/" + std::to_string(asset.id);
  request.headers = ApiHeaders("application/octet-stream");
  request.out_path = path;
  auto with_size = [&](std::uint64_t done, std::uint64_t total) {
    return !progress || progress(done, total ? total : asset.size);
  };
  http::Response response = http::Send(request, with_size);
  // GitHub answers with a redirect to a signed download URL on another host.
  for (int hops = 0; hops < 3 && response.status >= 301 && response.status <= 308 && !response.location.empty();
       hops++) {
    http::Request next;
    next.url = response.location;
    next.headers = {UserAgent()};
    next.out_path = path;
    response = http::Send(next, with_size);
  }
  *canceled = response.canceled;
  if (response.status == 200) {
    std::error_code ec;
    if (asset.size && std::filesystem::file_size(path, ec) != asset.size) {
      *error = "the download is incomplete";
      return false;
    }
    return true;
  }
  if (!response.canceled) *error = ApiError(response);
  return false;
}

}  // namespace keen4::update
