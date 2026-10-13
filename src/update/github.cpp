#include "update/github.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>

#include "engine/datafiles.h"
#include "engine/version.h"
#include "update/json.h"

#ifndef KEEN4_GITHUB_CLIENT_ID
#define KEEN4_GITHUB_CLIENT_ID ""
#endif
#ifndef KEEN4_UPDATE_REPO
#define KEEN4_UPDATE_REPO "talkedu/keen4revamped"
#endif

namespace keen4::update {

namespace {

constexpr char kTokenFile[] = "github-token.txt";

std::string Env(const char* name, const char* fallback) {
  const char* v = std::getenv(name);
  return v && *v ? v : fallback;
}

std::string UserAgent() { return std::string("User-Agent: keen4revamped/") + VersionNumber(); }

std::vector<std::string> ApiHeaders(const std::string& token, const char* accept) {
  std::vector<std::string> h = {std::string("Accept: ") + accept, "X-GitHub-Api-Version: 2022-11-28", UserAgent()};
  if (!token.empty()) h.push_back("Authorization: Bearer " + token);
  return h;
}

// A POST to github.com's OAuth endpoints, answered as JSON.
bool PostForm(const std::string& url, const std::string& body, json::Value* out, std::string* error) {
  http::Request request;
  request.method = "POST";
  request.url = url;
  request.body = body;
  request.headers = {"Accept: application/json", "Content-Type: application/x-www-form-urlencoded", UserAgent()};
  http::Response response = http::Send(request);
  if (response.status == 0) {
    *error = response.error;
    return false;
  }
  if (!json::Parse(response.body, out) || !out->IsObject()) {
    *error = "GitHub answered " + std::to_string(response.status);
    return false;
  }
  return true;
}

std::string ApiError(const http::Response& response) {
  if (response.status == 0) return response.error;
  json::Value v;
  std::string message;
  if (json::Parse(response.body, &v)) message = v["message"].String();
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
#else
  return "-linux-x64.tar.gz";
#endif
}

std::string ClientId() { return Env("KEEN4_GITHUB_CLIENT_ID", KEEN4_GITHUB_CLIENT_ID); }
std::string Repo() { return Env("KEEN4_UPDATE_REPO", KEEN4_UPDATE_REPO); }

std::string LoadToken() {
  // a token in the environment (for scripted runs) wins over the saved one
  if (const char* env = std::getenv("KEEN4_GITHUB_TOKEN"); env && *env) return env;
  std::ifstream in(files::WritePath(kTokenFile));
  std::string token;
  std::getline(in, token);
  while (!token.empty() && (token.back() == '\r' || token.back() == ' ')) token.pop_back();
  return token;
}

void SaveToken(const std::string& token) {
  namespace fs = std::filesystem;
  std::string path = files::WritePath(kTokenFile);
  {
    std::ofstream out(path, std::ios::trunc);
    out << token << "\n";
  }
  std::error_code ec;
  fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace, ec);
}

void ForgetToken() {
  std::error_code ec;
  std::filesystem::remove(files::WritePath(kTokenFile), ec);
}

bool RequestDeviceCode(DeviceCode* out, std::string* error) {
  json::Value v;
  // "repo" is the scope that can read a private repository's releases.
  if (!PostForm("https://github.com/login/device/code", "client_id=" + http::FormEncode(ClientId()) + "&scope=repo",
                &v, error))
    return false;
  if (v["device_code"].String().empty()) {
    std::string e = v["error"].String();
    *error = e == "device_flow_disabled" ? "device flow is not enabled for the OAuth app"
             : e.empty()                  ? "GitHub did not send a code"
                                          : e;
    return false;
  }
  *out = DeviceCode{};
  out->device_code = v["device_code"].String();
  out->user_code = v["user_code"].String();
  out->verification_uri = v["verification_uri"].String("https://github.com/login/device");
  out->interval = static_cast<int>(v["interval"].Number(5));
  out->expires_in = static_cast<int>(v["expires_in"].Number(900));
  return true;
}

Poll PollForToken(const DeviceCode& code, std::string* token, std::string* error) {
  json::Value v;
  if (!PostForm("https://github.com/login/oauth/access_token",
                "client_id=" + http::FormEncode(ClientId()) + "&device_code=" + http::FormEncode(code.device_code) +
                    "&grant_type=" + http::FormEncode("urn:ietf:params:oauth:grant-type:device_code"),
                &v, error))
    return Poll::Failed;
  *token = v["access_token"].String();
  if (!token->empty()) return Poll::Token;
  std::string e = v["error"].String();
  if (e == "authorization_pending") return Poll::Pending;
  if (e == "slow_down") return Poll::SlowDown;
  if (e == "access_denied") return Poll::Denied;
  if (e == "expired_token") return Poll::Expired;
  *error = e.empty() ? "GitHub sent no token" : e;
  return Poll::Failed;
}

Check FetchLatestRelease(const std::string& token, Release* out, std::string* error) {
  std::string api = "https://api.github.com/repos/" + Repo();
  http::Request request;
  request.url = api + "/releases/latest";
  request.headers = ApiHeaders(token, "application/vnd.github+json");
  http::Response response = http::Send(request);
  if (response.status == 200) {
    if (ParseRelease(response.body, out)) return Check::Ok;
    *error = "could not read the release";
    return Check::Failed;
  }
  if (response.status == 401) return Check::SignIn;
  if (response.status == 404) {
    // a private repository looks missing to accounts that cannot see it
    request.url = api;
    http::Response repo = http::Send(request);
    if (repo.status == 200) return Check::NoRelease;
    if (repo.status == 404 || repo.status == 403) return Check::NoAccess;
    *error = ApiError(repo);
    return Check::Failed;
  }
  if (response.status == 403) return Check::NoAccess;
  *error = ApiError(response);
  return Check::Failed;
}

bool DownloadAsset(const std::string& token, const Asset& asset, const std::string& path,
                   const http::Progress& progress, bool* canceled, std::string* error) {
  *canceled = false;
  http::Request request;
  request.url = "https://api.github.com/repos/" + Repo() + "/releases/assets/" + std::to_string(asset.id);
  request.headers = ApiHeaders(token, "application/octet-stream");
  request.out_path = path;
  auto with_size = [&](std::uint64_t done, std::uint64_t total) {
    return !progress || progress(done, total ? total : asset.size);
  };
  http::Response response = http::Send(request, with_size);
  // GitHub answers with a redirect to a signed download URL, which must be
  // fetched without the token.
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
