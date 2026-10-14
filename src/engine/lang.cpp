#include "engine/lang.h"

#include <cctype>
#include <string>
#include <unordered_map>

#include "exe/keen4exe.h"

namespace keen4::lang {

extern const LanguageData kPortuguese;  // src/lang/pt_br.cpp

namespace {

// Translations converted to Latin-1, built the first time a language is used.
struct Table {
  bool built = false;
  std::unordered_map<std::uint32_t, std::string> exe;
  std::unordered_map<std::string, std::string> text;
  std::unordered_map<int, std::string> chunks;
  std::string name;
};

Language current = Language::English;
bool from_command_line = false;
Table tables[static_cast<int>(Language::Count)];

const LanguageData* Data(Language language) {
  switch (language) {
    case Language::Portuguese: return &kPortuguese;
    default: return nullptr;
  }
}

const Table& Get(Language language) {
  Table& t = tables[static_cast<int>(language)];
  if (t.built) return t;
  t.built = true;
  const LanguageData* d = Data(language);
  if (!d) {
    t.name = "ENGLISH";
    return t;
  }
  t.name = ToLatin1(d->name);
  for (std::size_t i = 0; i < d->exe_count; i++) t.exe[d->exe[i].linear] = ToLatin1(d->exe[i].text);
  for (std::size_t i = 0; i < d->text_count; i++) t.text[d->text[i].english] = ToLatin1(d->text[i].text);
  for (std::size_t i = 0; i < d->chunk_count; i++) t.chunks[d->chunks[i].chunk] = ToLatin1(d->chunks[i].text);
  return t;
}

}  // namespace

Language Current() { return current; }

void Set(Language language) {
  if (static_cast<int>(language) < 0 || language >= Language::Count) language = Language::English;
  current = language;
}

const char* Code(Language language) {
  const LanguageData* d = Data(language);
  return d ? d->code : "en";
}

bool Parse(const std::string& code, Language* language) {
  std::string c;
  for (char ch : code) c += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  if (c == "en" || c == "en-us" || c == "english") {
    *language = Language::English;
    return true;
  }
  if (c == "pt" || c == "pt-br" || c == "pt_br" || c == "portuguese") {
    *language = Language::Portuguese;
    return true;
  }
  return false;
}

void SetFromCommandLine(Language language) {
  Set(language);
  from_command_line = true;
}

void SetFromConfig(Language language) {
  if (!from_command_line) Set(language);
}

const char* Tr(const char* s) {
  if (!s || current == Language::English) return s;
  const Table& t = Get(current);
  std::uint32_t linear;
  if (exe::g_exe.LinearOf(s, &linear)) {
    auto found = t.exe.find(linear);
    return found != t.exe.end() ? found->second.c_str() : s;
  }
  auto found = t.text.find(s);
  return found != t.text.end() ? found->second.c_str() : s;
}

const char* Article(int chunk) {
  if (current == Language::English) return nullptr;
  const Table& t = Get(current);
  auto found = t.chunks.find(chunk);
  return found != t.chunks.end() ? found->second.c_str() : nullptr;
}

const char* Name(Language language) {
  if (static_cast<int>(language) < 0 || language >= Language::Count) language = Language::English;
  return Get(language).name.c_str();
}

std::string ToLatin1(const char* utf8) {
  std::string out;
  const auto* p = reinterpret_cast<const unsigned char*>(utf8);
  while (*p) {
    unsigned c = *p++;
    if (c < 0x80) {
      out += static_cast<char>(c);
      continue;
    }
    int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    unsigned cp = c & (0x3F >> extra);
    for (int i = 0; i < extra && (*p & 0xC0) == 0x80; i++) cp = (cp << 6) | (*p++ & 0x3F);
    out += cp >= 0xA0 && cp <= 0xFF && extra ? static_cast<char>(cp) : '?';
  }
  return out;
}

}  // namespace keen4::lang
