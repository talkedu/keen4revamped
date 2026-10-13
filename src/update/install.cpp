#include "update/install.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace keen4::update {

namespace fs = std::filesystem;

namespace {

constexpr char kNewSuffix[] = ".keen4-new";
constexpr char kOldSuffix[] = ".keen4-old";

fs::path g_exe;
std::vector<std::string> g_args;
bool g_relaunch_allowed = true;

#ifdef _WIN32
constexpr char kExeName[] = "keen4.exe";
#else
constexpr char kExeName[] = "keen4";
#endif

bool EndsWith(const std::string& s, const char* suffix) {
  std::size_t n = std::strlen(suffix);
  return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

fs::path WithSuffix(const fs::path& p, const char* suffix) {
  fs::path out = p;
  out += suffix;
  return out;
}

// Leftovers of an interrupted or finished update: files that were about to
// replace others, and replaced files that were still in use.
// Only where releases put files, not the art or game data next to them.
void RemoveLeftovers(const fs::path& dir) {
  std::error_code ec;
  std::vector<fs::path> doomed;
  auto scan = [&](const fs::path& d, bool recursive) {
    for (auto it = fs::recursive_directory_iterator(d, ec); !ec && it != fs::recursive_directory_iterator();
         it.increment(ec)) {
      if (!recursive) it.disable_recursion_pending();
      std::string name = it->path().filename().string();
      if (EndsWith(name, kNewSuffix) || EndsWith(name, kOldSuffix)) doomed.push_back(it->path());
    }
    ec.clear();
  };
  scan(dir, false);
  scan(dir / "docs", true);
  scan(dir / "tools", true);
  for (const fs::path& p : doomed) fs::remove(p, ec);
}

bool Writable(const fs::path& dir) {
  fs::path probe = dir / ".keen4-write-test";
  {
    std::ofstream out(probe, std::ios::trunc);
    if (!out) return false;
  }
  std::error_code ec;
  fs::remove(probe, ec);
  return true;
}

// Puts every file under `from` in its place under `to`. A replaced file is
// first renamed aside, which Windows allows even for the running executable.
bool CopyTree(const fs::path& from, const fs::path& to, std::string* error) {
  std::error_code ec;
  std::vector<fs::path> files;
  for (auto it = fs::recursive_directory_iterator(from, ec); !ec && it != fs::recursive_directory_iterator();
       it.increment(ec)) {
    if (it->is_regular_file(ec)) files.push_back(it->path());
  }
  if (ec) {
    *error = "cannot read the update: " + ec.message();
    return false;
  }
  for (const fs::path& src : files) {
    fs::path dst = to / src.lexically_relative(from);
    fs::path tmp = WithSuffix(dst, kNewSuffix);
    fs::create_directories(dst.parent_path(), ec);
    if (!fs::copy_file(src, tmp, fs::copy_options::overwrite_existing, ec)) {
      *error = "cannot write " + dst.string() + ": " + ec.message();
      return false;
    }
    fs::permissions(tmp, fs::status(src, ec).permissions(), fs::perm_options::replace, ec);
#ifdef _WIN32
    if (fs::exists(dst, ec)) {
      fs::path old = WithSuffix(dst, kOldSuffix);
      fs::remove(old, ec);
      fs::rename(dst, old, ec);
      if (ec) {
        *error = "cannot replace " + dst.string() + ": " + ec.message();
        return false;
      }
      fs::rename(tmp, dst, ec);
      std::error_code ignored;
      // the running executable stays in use until the game exits
      if (!fs::remove(old, ignored))
        MoveFileExW(old.wstring().c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
    } else {
      fs::rename(tmp, dst, ec);
    }
#else
    fs::rename(tmp, dst, ec);  // atomic, and fine for the running executable
#endif
    if (ec) {
      *error = "cannot replace " + dst.string() + ": " + ec.message();
      return false;
    }
  }
  return true;
}

void Relaunch() {
  if (g_exe.empty()) return;
#ifdef _WIN32
  std::wstring cmdline = GetCommandLineW();
  STARTUPINFOW si{};
  si.cb = sizeof si;
  PROCESS_INFORMATION pi{};
  if (CreateProcessW(g_exe.wstring().c_str(), cmdline.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si,
                     &pi)) {
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
  }
#else
  std::vector<char*> argv;
  for (std::string& a : g_args) argv.push_back(a.data());
  argv.push_back(nullptr);
  pid_t pid;
  posix_spawn(&pid, g_exe.c_str(), nullptr, nullptr, argv.data(), environ);
#endif
}

int RunProcess(const std::vector<std::string>& args) {
#ifdef _WIN32
  std::wstring cmdline;
  for (const std::string& a : args) {
    if (!cmdline.empty()) cmdline += L' ';
    cmdline += L'"' + fs::path(a).wstring() + L'"';
  }
  STARTUPINFOW si{};
  si.cb = sizeof si;
  PROCESS_INFORMATION pi{};
  if (!CreateProcessW(fs::path(args[0]).wstring().c_str(), cmdline.data(), nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    return -1;
  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD code = 1;
  GetExitCodeProcess(pi.hProcess, &code);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return static_cast<int>(code);
#else
  std::vector<std::string> copy = args;
  std::vector<char*> argv;
  for (std::string& a : copy) argv.push_back(a.data());
  argv.push_back(nullptr);
  pid_t pid;
  if (posix_spawnp(&pid, argv[0], nullptr, nullptr, argv.data(), environ) != 0) return -1;
  int status = 0;
  if (waitpid(pid, &status, 0) < 0) return -1;
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

}  // namespace

void Init(int argc, char** argv) {
  g_args.assign(argv, argv + argc);
  for (const std::string& a : g_args) {
    if (a == "--headless") g_relaunch_allowed = false;
  }
#ifdef _WIN32
  std::vector<wchar_t> buf(32768);
  DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
  if (n > 0 && n < buf.size()) g_exe = fs::path(std::wstring(buf.data(), n));
#else
  std::error_code ec;
  g_exe = fs::read_symlink("/proc/self/exe", ec);
  if (ec && argc > 0) g_exe = fs::absolute(argv[0], ec);
#endif
  if (!g_exe.empty()) RemoveLeftovers(g_exe.parent_path());
}

bool RunHelper(int argc, char** argv, int* exit_code) {
  if (argc < 4 || std::strcmp(argv[1], "--apply-update") != 0) return false;
  fs::path from = argv[2], to = argv[3];
#ifdef _WIN32
  // the paths as typed, not through the ANSI code page
  int wargc = 0;
  if (LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc)) {
    if (wargc >= 4) {
      from = wargv[2];
      to = wargv[3];
    }
    LocalFree(wargv);
  }
#endif
  std::string error;
  bool ok = CopyTree(from, to, &error);
  if (!ok) std::fprintf(stderr, "keen4 --apply-update: %s\n", error.c_str());
  *exit_code = ok ? 0 : 1;
  return true;
}

std::string InstallDir() { return g_exe.parent_path().string(); }

bool Extract(const std::string& archive, const std::string& dir, std::string* root, std::string* error) {
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
#ifdef _WIN32
  // Windows 10 and later ship bsdtar, which also reads .zip files.
  wchar_t sys[MAX_PATH];
  UINT n = GetSystemDirectoryW(sys, MAX_PATH);
  std::string tar = (fs::path(std::wstring(sys, n)) / "tar.exe").string();
  int rc = RunProcess({tar, "-xf", archive, "-C", dir});
#else
  int rc = RunProcess({"tar", "-xzf", archive, "-C", dir});
#endif
  if (rc != 0) {
    *error = rc < 0 ? "cannot run tar to unpack the update" : "the update archive is damaged";
    return false;
  }
  if (fs::is_regular_file(fs::path(dir) / kExeName, ec)) {
    *root = dir;
    return true;
  }
  for (const auto& entry : fs::directory_iterator(dir, ec)) {
    if (entry.is_directory(ec) && fs::is_regular_file(entry.path() / kExeName, ec)) {
      *root = entry.path().string();
      return true;
    }
  }
  *error = std::string("the update has no ") + kExeName;
  return false;
}

Apply ApplyUpdate(const std::string& root, std::string* error) {
  if (g_exe.empty()) {
    *error = "cannot tell where the game is installed";
    return Apply::Failed;
  }
  fs::path dir = g_exe.parent_path();
  if (Writable(dir)) return CopyTree(root, dir, error) ? Apply::Done : Apply::Failed;
#ifdef _WIN32
  // Program Files: let a copy of the game with administrator rights do it.
  // (This runs on a worker thread, and ShellExecuteEx may use COM.)
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  std::wstring params = L"--apply-update \"" + fs::path(root).wstring() + L"\" \"" + dir.wstring() + L"\"";
  std::wstring exe = g_exe.wstring();
  SHELLEXECUTEINFOW info{};
  info.cbSize = sizeof info;
  info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
  info.lpVerb = L"runas";
  info.lpFile = exe.c_str();
  info.lpParameters = params.c_str();
  info.nShow = SW_HIDE;
  if (!ShellExecuteExW(&info) || !info.hProcess) {
    if (GetLastError() == ERROR_CANCELLED) return Apply::Declined;
    *error = "cannot start the updater with administrator rights";
    return Apply::Failed;
  }
  WaitForSingleObject(info.hProcess, INFINITE);
  DWORD code = 1;
  GetExitCodeProcess(info.hProcess, &code);
  CloseHandle(info.hProcess);
  if (code != 0) {
    *error = "the updater could not replace the game files";
    return Apply::Failed;
  }
  return Apply::Done;
#else
  *error = dir.string() + " is read-only; run the installer of the new version";
  return Apply::Failed;
#endif
}

void RelaunchOnExit() {
  static bool registered = false;
  if (registered || !g_relaunch_allowed) return;
  registered = true;
  std::atexit(Relaunch);
}

}  // namespace keen4::update
