#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Translations of the game's text.
//
// The original program keeps its strings in KEEN4E.EXE and its help, story
// and ending articles in EGAGRAPH.CK4, neither of which we ship. So a
// translation names an exe string by its address in the load image (or a
// string the port prints itself by its English text) and an article by its
// graphics chunk. Translations are written in UTF-8 and turned into
// ISO-8859-1 when loaded: the game draws one byte per character, and ID_VW
// draws the accented letters the original fonts lack.
namespace keen4::lang {

enum class Language : int { English, Portuguese, Count };

// An exe string at `linear` in the unpacked load image.
struct ExeEntry {
  std::uint32_t linear;
  const char* text;
};
// A string the port prints itself, found by its English text.
struct TextEntry {
  const char* english;
  const char* text;
};
// A whole text article in EGAGRAPH (help topics, the ending).
struct ChunkEntry {
  int chunk;
  const char* text;
};

struct LanguageData {
  const char* code;        // as given to --lang and saved in the config
  const char* name;        // as the LANGUAGE card lists it, in the language itself
  const ExeEntry* exe;
  std::size_t exe_count;
  const TextEntry* text;
  std::size_t text_count;
  const ChunkEntry* chunks;
  std::size_t chunk_count;
};

// Linear address of a string at `ofs` in DGROUP, or in a far segment.
constexpr std::uint32_t DG(std::uint16_t ofs) { return 0x2EE70u + ofs; }
constexpr std::uint32_t FAR(std::uint16_t seg, std::uint16_t ofs) {
  return (static_cast<std::uint32_t>(seg) << 4) + ofs;
}

Language Current();
void Set(Language language);
// "en" or "pt-BR". Parse also accepts "pt" and ignores case.
const char* Code(Language language);
bool Parse(const std::string& code, Language* language);

// --lang on the command line wins over the language saved in the config.
void SetFromCommandLine(Language language);
void SetFromConfig(Language language);

// The text to draw for `s`: its translation in the current language, or `s`.
const char* Tr(const char* s);
// The translated article for a text chunk, or nullptr for the original.
const char* Article(int chunk);
// The language's own name, such as "ENGLISH", for the LANGUAGE card.
const char* Name(Language language);

// UTF-8 to ISO-8859-1; characters outside Latin-1 become '?'.
std::string ToLatin1(const char* utf8);

}  // namespace keen4::lang
