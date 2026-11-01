#include "update/http.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <mutex>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#ifndef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY 4
#endif
#else
#include <dlfcn.h>
#endif

namespace keen4::http {

namespace {

constexpr std::size_t kMaxBody = 4 << 20;  // error pages and API answers, not downloads

// Collects the body into memory or, for a 2xx answer with out_path, a file.
class Sink {
 public:
  Sink(const Request& request, Response* response) : request_(request), response_(response) {}
  ~Sink() { Close(); }

  bool Write(const char* data, std::size_t n) {
    if (!started_) {
      started_ = true;
      if (!request_.out_path.empty() && response_->status >= 200 && response_->status < 300) {
        file_ = std::fopen(request_.out_path.c_str(), "wb");
        if (!file_) {
          response_->error = "cannot write " + request_.out_path;
          return false;
        }
      }
    }
    if (file_) return std::fwrite(data, 1, n, file_) == n;
    if (response_->body.size() + n > kMaxBody) return false;
    response_->body.append(data, n);
    return true;
  }

  bool Close() {
    bool ok = true;
    if (file_) ok = std::fclose(file_) == 0;
    file_ = nullptr;
    return ok;
  }

 private:
  const Request& request_;
  Response* response_;
  bool started_ = false;
  std::FILE* file_ = nullptr;
};

}  // namespace

#ifdef _WIN32

namespace {

std::wstring Widen(const std::string& s) {
  if (s.empty()) return {};
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  std::wstring w(static_cast<std::size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
  return w;
}

std::string Narrow(const std::wstring& w) {
  if (w.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
  std::string s(static_cast<std::size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
  return s;
}

struct Handle {
  HINTERNET h = nullptr;
  ~Handle() {
    if (h) WinHttpCloseHandle(h);
  }
};

std::string WinError(const char* what) {
  char buf[64];
  std::snprintf(buf, sizeof buf, "%s failed (error %lu)", what, static_cast<unsigned long>(GetLastError()));
  return buf;
}

std::wstring QueryString(HINTERNET request, DWORD what) {
  DWORD size = 0;
  WinHttpQueryHeaders(request, what, WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER, &size,
                      WINHTTP_NO_HEADER_INDEX);
  if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size == 0) return {};
  std::wstring value(size / sizeof(wchar_t), L'\0');
  if (!WinHttpQueryHeaders(request, what, WINHTTP_HEADER_NAME_BY_INDEX, value.data(), &size,
                           WINHTTP_NO_HEADER_INDEX))
    return {};
  value.resize(size / sizeof(wchar_t));
  return value;
}

}  // namespace

bool Available(std::string*) { return true; }

Response Send(const Request& request, const Progress& progress) {
  Response response;
  std::wstring url = Widen(request.url);
  URL_COMPONENTS parts{};
  parts.dwStructSize = sizeof parts;
  parts.dwSchemeLength = parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength =
      static_cast<DWORD>(-1);
  if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts)) {
    response.error = "bad URL " + request.url;
    return response;
  }
  std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
  std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
  if (parts.lpszExtraInfo) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);

  Handle session, connection, req;
  session.h = WinHttpOpen(L"keen4revamped", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                          WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session.h)
    session.h = WinHttpOpen(L"keen4revamped", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                            WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session.h) {
    response.error = WinError("WinHttpOpen");
    return response;
  }
  WinHttpSetTimeouts(session.h, 15000, 15000, 30000, 30000);
  connection.h = WinHttpConnect(session.h, host.c_str(), parts.nPort, 0);
  if (!connection.h) {
    response.error = WinError("WinHttpConnect");
    return response;
  }
  std::wstring method = Widen(request.method);
  req.h = WinHttpOpenRequest(connection.h, method.c_str(), path.c_str(), nullptr, WINHTTP_NO_REFERER,
                             WINHTTP_DEFAULT_ACCEPT_TYPES,
                             parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
  if (!req.h) {
    response.error = WinError("WinHttpOpenRequest");
    return response;
  }
  DWORD feature = WINHTTP_DISABLE_REDIRECTS;
  WinHttpSetOption(req.h, WINHTTP_OPTION_DISABLE_FEATURE, &feature, sizeof feature);
  std::wstring headers;
  for (const std::string& h : request.headers) headers += Widen(h) + L"\r\n";
  if (!headers.empty())
    WinHttpAddRequestHeaders(req.h, headers.c_str(), static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);
  auto body_len = static_cast<DWORD>(request.body.size());
  if (!WinHttpSendRequest(req.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                          body_len ? const_cast<char*>(request.body.data()) : WINHTTP_NO_REQUEST_DATA, body_len,
                          body_len, 0) ||
      !WinHttpReceiveResponse(req.h, nullptr)) {
    response.error = WinError("HTTPS request");
    return response;
  }
  DWORD status = 0, size = sizeof status;
  WinHttpQueryHeaders(req.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                      &status, &size, WINHTTP_NO_HEADER_INDEX);
  response.status = static_cast<int>(status);
  response.location = Narrow(QueryString(req.h, WINHTTP_QUERY_LOCATION));
  std::wstring length = QueryString(req.h, WINHTTP_QUERY_CONTENT_LENGTH);
  if (!length.empty()) response.content_length = std::wcstoull(length.c_str(), nullptr, 10);

  Sink sink(request, &response);
  std::uint64_t done = 0;
  std::vector<char> buffer(64 * 1024);
  for (;;) {
    DWORD got = 0;
    if (!WinHttpReadData(req.h, buffer.data(), static_cast<DWORD>(buffer.size()), &got)) {
      response.error = WinError("WinHttpReadData");
      response.status = 0;
      return response;
    }
    if (got == 0) break;
    if (!sink.Write(buffer.data(), got)) {
      if (response.error.empty()) response.error = "answer too large";
      response.status = 0;
      return response;
    }
    done += got;
    if (progress && !progress(done, response.content_length)) {
      response.canceled = true;
      response.status = 0;
      return response;
    }
  }
  if (!sink.Close()) {
    response.error = "cannot write " + request.out_path;
    response.status = 0;
  }
  return response;
}

#else  // libcurl

namespace {

// The few libcurl entry points and constants used here (stable ABI since 7.x).
using CURL = void;
constexpr int kOptWriteData = 10001, kOptUrl = 10002, kOptPostFields = 10015, kOptUserAgent = 10018,
              kOptHttpHeader = 10023, kOptHeaderData = 10029, kOptCustomRequest = 10036, kOptXferInfoData = 10057,
              kOptWriteFunction = 20011, kOptHeaderFunction = 20079, kOptXferInfoFunction = 20219,
              kOptLowSpeedLimit = 19, kOptLowSpeedTime = 20, kOptNoProgress = 43, kOptFollowLocation = 52,
              kOptPostFieldSize = 60, kOptConnectTimeout = 78, kOptNoSignal = 99;
constexpr int kInfoResponseCode = 0x200000 + 2;
constexpr int kAbortedByCallback = 42;

struct Curl {
  bool loaded = false;
  std::string why;
  CURL* (*easy_init)() = nullptr;
  int (*easy_setopt)(CURL*, int, ...) = nullptr;
  int (*easy_perform)(CURL*) = nullptr;
  int (*easy_getinfo)(CURL*, int, ...) = nullptr;
  void (*easy_cleanup)(CURL*) = nullptr;
  const char* (*easy_strerror)(int) = nullptr;
  void* (*slist_append)(void*, const char*) = nullptr;
  void (*slist_free_all)(void*) = nullptr;
  int (*global_init)(long) = nullptr;
};

bool HeaderIs(const std::string& line, const char* name, std::string* value) {
  std::size_t n = std::strlen(name);
  if (line.size() <= n || line[n] != ':') return false;
  for (std::size_t i = 0; i < n; i++) {
    char a = line[i], b = name[i];
    if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
    if (a != b) return false;
  }
  std::size_t begin = n + 1, end = line.size();
  while (begin < end && (line[begin] == ' ' || line[begin] == '\t')) begin++;
  while (end > begin && (line[end - 1] == '\r' || line[end - 1] == '\n' || line[end - 1] == ' ')) end--;
  *value = line.substr(begin, end - begin);
  return true;
}

template <typename F>
bool Sym(void* lib, const char* name, F* out) {
  *out = reinterpret_cast<F>(dlsym(lib, name));
  return *out != nullptr;
}

const Curl& GetCurl() {
  static Curl curl;
  static std::once_flag once;
  std::call_once(once, [] {
    void* lib = nullptr;
    for (const char* name : {"libcurl.so.4", "libcurl-gnutls.so.4", "libcurl.so", "libcurl.4.dylib"}) {
      lib = dlopen(name, RTLD_NOW | RTLD_LOCAL);
      if (lib) break;
    }
    if (!lib) {
      curl.why = "libcurl is not installed";
      return;
    }
    bool ok = Sym(lib, "curl_easy_init", &curl.easy_init) && Sym(lib, "curl_easy_setopt", &curl.easy_setopt) &&
              Sym(lib, "curl_easy_perform", &curl.easy_perform) &&
              Sym(lib, "curl_easy_getinfo", &curl.easy_getinfo) &&
              Sym(lib, "curl_easy_cleanup", &curl.easy_cleanup) &&
              Sym(lib, "curl_easy_strerror", &curl.easy_strerror) &&
              Sym(lib, "curl_slist_append", &curl.slist_append) &&
              Sym(lib, "curl_slist_free_all", &curl.slist_free_all) &&
              Sym(lib, "curl_global_init", &curl.global_init);
    if (!ok) {
      curl.why = "libcurl is too old";
      return;
    }
    curl.global_init(3);  // CURL_GLOBAL_DEFAULT
    curl.loaded = true;
  });
  return curl;
}

struct Transfer {
  const Curl* curl;
  CURL* easy;
  Response* response;
  Sink* sink;
  const Progress* progress;
};

std::size_t OnHeader(char* data, std::size_t size, std::size_t n, void* user) {
  auto* t = static_cast<Transfer*>(user);
  std::string line(data, size * n), value;
  if (line.compare(0, 5, "HTTP/") == 0) {
    t->response->location.clear();
    t->response->content_length = 0;
  } else if (HeaderIs(line, "location", &value)) {
    t->response->location = value;
  } else if (HeaderIs(line, "content-length", &value)) {
    t->response->content_length = std::strtoull(value.c_str(), nullptr, 10);
  }
  return size * n;
}

std::size_t OnWrite(char* data, std::size_t size, std::size_t n, void* user) {
  auto* t = static_cast<Transfer*>(user);
  long status = 0;
  t->curl->easy_getinfo(t->easy, kInfoResponseCode, &status);
  t->response->status = static_cast<int>(status);
  return t->sink->Write(data, size * n) ? size * n : 0;
}

int OnProgress(void* user, std::int64_t total, std::int64_t now, std::int64_t, std::int64_t) {
  auto* t = static_cast<Transfer*>(user);
  if (!*t->progress) return 0;
  return (*t->progress)(static_cast<std::uint64_t>(now), static_cast<std::uint64_t>(total)) ? 0 : 1;
}

}  // namespace

bool Available(std::string* why) {
  const Curl& curl = GetCurl();
  if (!curl.loaded && why) *why = curl.why;
  return curl.loaded;
}

Response Send(const Request& request, const Progress& progress) {
  Response response;
  const Curl& curl = GetCurl();
  if (!curl.loaded) {
    response.error = curl.why;
    return response;
  }
  CURL* easy = curl.easy_init();
  if (!easy) {
    response.error = "curl_easy_init failed";
    return response;
  }
  Sink sink(request, &response);
  Transfer transfer{&curl, easy, &response, &sink, &progress};
  void* headers = nullptr;
  for (const std::string& h : request.headers) headers = curl.slist_append(headers, h.c_str());

  using WriteFn = std::size_t (*)(char*, std::size_t, std::size_t, void*);
  using ProgressFn = int (*)(void*, std::int64_t, std::int64_t, std::int64_t, std::int64_t);
  curl.easy_setopt(easy, kOptUrl, request.url.c_str());
  curl.easy_setopt(easy, kOptUserAgent, "keen4revamped");
  curl.easy_setopt(easy, kOptHttpHeader, headers);
  curl.easy_setopt(easy, kOptFollowLocation, 0L);
  curl.easy_setopt(easy, kOptNoSignal, 1L);
  curl.easy_setopt(easy, kOptConnectTimeout, 15L);
  curl.easy_setopt(easy, kOptLowSpeedLimit, 1L);  // give up after 30 s without data
  curl.easy_setopt(easy, kOptLowSpeedTime, 30L);
  curl.easy_setopt(easy, kOptWriteFunction, static_cast<WriteFn>(OnWrite));
  curl.easy_setopt(easy, kOptWriteData, &transfer);
  curl.easy_setopt(easy, kOptHeaderFunction, static_cast<WriteFn>(OnHeader));
  curl.easy_setopt(easy, kOptHeaderData, &transfer);
  curl.easy_setopt(easy, kOptXferInfoFunction, static_cast<ProgressFn>(OnProgress));
  curl.easy_setopt(easy, kOptXferInfoData, &transfer);
  curl.easy_setopt(easy, kOptNoProgress, 0L);
  if (request.method == "POST") {
    curl.easy_setopt(easy, kOptPostFields, request.body.c_str());
    curl.easy_setopt(easy, kOptPostFieldSize, static_cast<long>(request.body.size()));
  } else if (request.method != "GET") {
    curl.easy_setopt(easy, kOptCustomRequest, request.method.c_str());
  }
  int rc = curl.easy_perform(easy);
  long status = 0;
  curl.easy_getinfo(easy, kInfoResponseCode, &status);
  curl.slist_free_all(headers);
  curl.easy_cleanup(easy);
  response.status = static_cast<int>(status);
  if (!sink.Close()) {
    response.error = "cannot write " + request.out_path;
    response.status = 0;
  } else if (rc == kAbortedByCallback) {
    response.canceled = true;
    response.status = 0;
  } else if (rc != 0) {
    if (response.error.empty()) response.error = curl.easy_strerror(rc);
    response.status = 0;
  }
  return response;
}

#endif

}  // namespace keen4::http
