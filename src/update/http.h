#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Minimal HTTPS client for the updater: WinHTTP on Windows, the system's
// libcurl (loaded at run time, so it is not a build dependency) elsewhere.
// Redirects are not followed; the caller decides where to go next.
namespace keen4::http {

struct Request {
  std::string method = "GET";
  std::string url;
  std::vector<std::string> headers;  // "Name: value"
  std::string body;
  // When set, a 2xx answer's body goes to this file instead of Response::body.
  std::string out_path;
};

struct Response {
  int status = 0;  // 0 when there was no answer; see `error`
  std::string body;
  std::string location;          // the Location header
  std::uint64_t content_length = 0;  // 0 when not sent
  std::string error;
  bool canceled = false;
};

// Called as the body arrives (`total` is 0 when unknown). Return false to cancel.
using Progress = std::function<bool(std::uint64_t done, std::uint64_t total)>;

// False (with the reason) when this system cannot make HTTPS requests.
bool Available(std::string* why);

// Thread safe; blocks until the answer is in, or the progress callback cancels.
Response Send(const Request& request, const Progress& progress = {});

}  // namespace keen4::http
