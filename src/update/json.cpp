#include "update/json.h"

#include <cstdlib>

namespace keen4::json {

namespace {
const Value kNull;
}

const Value& Value::operator[](const std::string& key) const {
  if (type_ != Type::Object) return kNull;
  auto found = members_.find(key);
  return found != members_.end() ? found->second : kNull;
}

std::string Value::String(const std::string& fallback) const {
  return type_ == Type::String ? string_ : fallback;
}

double Value::Number(double fallback) const { return type_ == Type::Number ? number_ : fallback; }

bool Value::Bool(bool fallback) const { return type_ == Type::Bool ? bool_ : fallback; }

class Parser {
 public:
  explicit Parser(const std::string& text) : s_(text) {}

  bool Document(Value* out) {
    if (!ParseValue(out, 0)) return false;
    Space();
    return pos_ == s_.size();
  }

 private:
  const std::string& s_;
  std::size_t pos_ = 0;

  void Space() {
    while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' || s_[pos_] == '\r'))
      pos_++;
  }

  bool Literal(const char* word) {
    std::size_t n = 0;
    while (word[n]) n++;
    if (s_.compare(pos_, n, word) != 0) return false;
    pos_ += n;
    return true;
  }

  static void AppendUtf8(std::string* out, unsigned cp) {
    if (cp < 0x80) {
      *out += static_cast<char>(cp);
    } else if (cp < 0x800) {
      *out += static_cast<char>(0xC0 | (cp >> 6));
      *out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      *out += static_cast<char>(0xE0 | (cp >> 12));
      *out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      *out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
      *out += static_cast<char>(0xF0 | (cp >> 18));
      *out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
      *out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      *out += static_cast<char>(0x80 | (cp & 0x3F));
    }
  }

  bool Hex4(unsigned* out) {
    if (pos_ + 4 > s_.size()) return false;
    unsigned v = 0;
    for (int i = 0; i < 4; i++) {
      char c = s_[pos_++];
      v <<= 4;
      if (c >= '0' && c <= '9')
        v |= static_cast<unsigned>(c - '0');
      else if (c >= 'a' && c <= 'f')
        v |= static_cast<unsigned>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F')
        v |= static_cast<unsigned>(c - 'A' + 10);
      else
        return false;
    }
    *out = v;
    return true;
  }

  bool ParseString(std::string* out) {
    if (pos_ >= s_.size() || s_[pos_] != '"') return false;
    pos_++;
    while (pos_ < s_.size()) {
      char c = s_[pos_++];
      if (c == '"') return true;
      if (c != '\\') {
        *out += c;
        continue;
      }
      if (pos_ >= s_.size()) return false;
      char e = s_[pos_++];
      switch (e) {
        case '"': *out += '"'; break;
        case '\\': *out += '\\'; break;
        case '/': *out += '/'; break;
        case 'b': *out += '\b'; break;
        case 'f': *out += '\f'; break;
        case 'n': *out += '\n'; break;
        case 'r': *out += '\r'; break;
        case 't': *out += '\t'; break;
        case 'u': {
          unsigned cp;
          if (!Hex4(&cp)) return false;
          if (cp >= 0xD800 && cp < 0xDC00 && s_.compare(pos_, 2, "\\u") == 0) {
            pos_ += 2;
            unsigned low;
            if (!Hex4(&low)) return false;
            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
          }
          AppendUtf8(out, cp);
          break;
        }
        default: return false;
      }
    }
    return false;
  }

  bool ParseValue(Value* out, int depth) {
    if (depth > 64) return false;
    Space();
    if (pos_ >= s_.size()) return false;
    char c = s_[pos_];
    if (c == '{') {
      pos_++;
      out->type_ = Value::Type::Object;
      Space();
      if (pos_ < s_.size() && s_[pos_] == '}') {
        pos_++;
        return true;
      }
      for (;;) {
        Space();
        std::string key;
        if (!ParseString(&key)) return false;
        Space();
        if (pos_ >= s_.size() || s_[pos_++] != ':') return false;
        Value v;
        if (!ParseValue(&v, depth + 1)) return false;
        out->members_[key] = std::move(v);
        Space();
        if (pos_ >= s_.size()) return false;
        if (s_[pos_] == ',') {
          pos_++;
          continue;
        }
        if (s_[pos_++] != '}') return false;
        return true;
      }
    }
    if (c == '[') {
      pos_++;
      out->type_ = Value::Type::Array;
      Space();
      if (pos_ < s_.size() && s_[pos_] == ']') {
        pos_++;
        return true;
      }
      for (;;) {
        Value v;
        if (!ParseValue(&v, depth + 1)) return false;
        out->items_.push_back(std::move(v));
        Space();
        if (pos_ >= s_.size()) return false;
        if (s_[pos_] == ',') {
          pos_++;
          continue;
        }
        if (s_[pos_++] != ']') return false;
        return true;
      }
    }
    if (c == '"') {
      out->type_ = Value::Type::String;
      return ParseString(&out->string_);
    }
    if (Literal("true")) {
      out->type_ = Value::Type::Bool;
      out->bool_ = true;
      return true;
    }
    if (Literal("false")) {
      out->type_ = Value::Type::Bool;
      return true;
    }
    if (Literal("null")) return true;
    if (c == '-' || (c >= '0' && c <= '9')) {
      const char* begin = s_.c_str() + pos_;
      char* end = nullptr;
      out->number_ = std::strtod(begin, &end);
      if (end == begin) return false;
      out->type_ = Value::Type::Number;
      pos_ += static_cast<std::size_t>(end - begin);
      return true;
    }
    return false;
  }
};

bool Parse(const std::string& text, Value* out) {
  *out = Value{};
  Parser parser(text);
  Value v;
  if (!parser.Document(&v)) return false;
  *out = std::move(v);
  return true;
}

}  // namespace keen4::json
