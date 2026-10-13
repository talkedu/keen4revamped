#pragma once

#include <map>
#include <string>
#include <vector>

// A small JSON reader for GitHub's API answers. Numbers are kept as doubles,
// which is exact for the ids and sizes GitHub sends.
namespace keen4::json {

class Value {
 public:
  enum class Type { Null, Bool, Number, String, Array, Object };

  Type type() const { return type_; }
  bool IsObject() const { return type_ == Type::Object; }
  bool IsArray() const { return type_ == Type::Array; }

  // Member of an object, or a null value when there is none.
  const Value& operator[](const std::string& key) const;
  const std::vector<Value>& Items() const { return items_; }

  // The value as a string, number or bool; `fallback` when it is something else.
  std::string String(const std::string& fallback = "") const;
  double Number(double fallback = 0) const;
  bool Bool(bool fallback = false) const;

 private:
  friend class Parser;
  Type type_ = Type::Null;
  bool bool_ = false;
  double number_ = 0;
  std::string string_;
  std::vector<Value> items_;
  std::map<std::string, Value> members_;
};

// Parses `text`. Returns false (and leaves *out null) on malformed input.
bool Parse(const std::string& text, Value* out);

}  // namespace keen4::json
