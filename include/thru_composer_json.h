#pragma once
// Strict bounded JSON for portable composition files. Used only by
// workers/tools.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
namespace s3g_thru_json {
struct Value {
  using Array = std::vector<Value>;
  using Object = std::map<std::string, Value>;
  std::variant<std::nullptr_t, bool, double, std::string, Array, Object> data =
      nullptr;
  Value() = default;
  Value(std::nullptr_t) : data(nullptr) {}
  Value(bool v) : data(v) {}
  Value(double v) : data(v) {}
  Value(float v) : data(double(v)) {}
  Value(int v) : data(double(v)) {}
  Value(unsigned v) : data(double(v)) {}
  Value(const char *v) : data(std::string(v)) {}
  Value(std::string v) : data(std::move(v)) {}
  Value(Array v) : data(std::move(v)) {}
  Value(Object v) : data(std::move(v)) {}
  static Value array() { return Array{}; }
  static Value object() { return Object{}; }
  bool isNull() const { return std::holds_alternative<std::nullptr_t>(data); }
  bool isObject() const { return std::holds_alternative<Object>(data); }
  bool isArray() const { return std::holds_alternative<Array>(data); }
  bool isString() const { return std::holds_alternative<std::string>(data); }
  bool isNumber() const { return std::holds_alternative<double>(data); }
  bool isBool() const { return std::holds_alternative<bool>(data); }
  Object &objectValue() { return std::get<Object>(data); }
  const Object &objectValue() const { return std::get<Object>(data); }
  Array &arrayValue() { return std::get<Array>(data); }
  const Array &arrayValue() const { return std::get<Array>(data); }
  const std::string &string() const { return std::get<std::string>(data); }
  double number() const { return std::get<double>(data); }
  bool boolean() const { return std::get<bool>(data); }
  Value &operator[](const std::string &key) { return objectValue()[key]; }
  const Value &at(const std::string &key) const {
    const auto p = objectValue().find(key);
    if (p == objectValue().end())
      throw std::runtime_error("Missing field: " + key);
    return p->second;
  }
  const Value *find(const std::string &key) const {
    if (!isObject())
      return nullptr;
    const auto p = objectValue().find(key);
    return p == objectValue().end() ? nullptr : &p->second;
  }
  // Construct scalar variants in place. Besides avoiding a move/allocation,
  // this avoids GCC 15's false uninitialized-container warning when it inlines
  // a recursive variant move from a scalar temporary at -O3.
  void push(const char *v) { arrayValue().emplace_back(v); }
  void push(std::string v) { arrayValue().emplace_back(std::move(v)); }
  void push(float v) { arrayValue().emplace_back(v); }
  void push(double v) { arrayValue().emplace_back(v); }
  void push(int v) { arrayValue().emplace_back(v); }
  void push(unsigned v) { arrayValue().emplace_back(v); }
  void push(bool v) { arrayValue().emplace_back(v); }
  void push(Value v) { arrayValue().push_back(std::move(v)); }
};
class Parser {
  std::string_view input;
  std::size_t p = 0, nodes = 0;
  [[noreturn]] void fail(const char *why) const {
    throw std::runtime_error(std::string(why) + " at byte " +
                             std::to_string(p));
  }
  void space() {
    while (p < input.size() && (input[p] == ' ' || input[p] == '\n' ||
                                input[p] == '\r' || input[p] == '\t'))
      ++p;
  }
  bool take(char c) {
    space();
    if (p < input.size() && input[p] == c) {
      ++p;
      return true;
    }
    return false;
  }
  unsigned hex() {
    if (p + 4 > input.size())
      fail("Invalid unicode escape");
    unsigned n = 0;
    for (int i = 0; i < 4; ++i) {
      const char c = input[p++];
      const auto d = c >= '0' && c <= '9'
                         ? c - '0'
                         : c >= 'a' && c <= 'f'
                               ? c - 'a' + 10
                               : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
      if (d < 0)
        fail("Invalid unicode escape");
      n = (n << 4) | unsigned(d);
    }
    return n;
  }
  static void utf8(std::string &out, unsigned c) {
    if (c < 0x80)
      out += char(c);
    else if (c < 0x800) {
      out += char(0xc0 | (c >> 6));
      out += char(0x80 | (c & 63));
    } else if (c < 0x10000) {
      out += char(0xe0 | (c >> 12));
      out += char(0x80 | ((c >> 6) & 63));
      out += char(0x80 | (c & 63));
    } else {
      out += char(0xf0 | (c >> 18));
      out += char(0x80 | ((c >> 12) & 63));
      out += char(0x80 | ((c >> 6) & 63));
      out += char(0x80 | (c & 63));
    }
  }
  std::string text() {
    space();
    if (p >= input.size() || input[p++] != '"')
      fail("Expected string");
    std::string out;
    while (p < input.size()) {
      unsigned char c = input[p++];
      if (c == '"')
        return out;
      if (c < 32)
        fail("Control character in string");
      if (c == '\\') {
        if (p >= input.size())
          fail("Incomplete escape");
        c = input[p++];
        switch (c) {
        case '"':
        case '\\':
        case '/':
          out += char(c);
          break;
        case 'b':
          out += '\b';
          break;
        case 'f':
          out += '\f';
          break;
        case 'n':
          out += '\n';
          break;
        case 'r':
          out += '\r';
          break;
        case 't':
          out += '\t';
          break;
        case 'u': {
          unsigned cp = hex();
          if (cp >= 0xd800 && cp <= 0xdbff) {
            if (p + 2 > input.size() || input[p++] != '\\' || input[p++] != 'u')
              fail("Unpaired surrogate");
            const auto low = hex();
            if (low < 0xdc00 || low > 0xdfff)
              fail("Unpaired surrogate");
            cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
          } else if (cp >= 0xdc00 && cp <= 0xdfff)
            fail("Unpaired surrogate");
          utf8(out, cp);
          break;
        }
        default:
          fail("Unknown escape");
        }
      } else if (c < 0x80)
        out += char(c);
      else {
        const unsigned count =
            c >= 0xc2 && c <= 0xdf
                ? 1
                : c >= 0xe0 && c <= 0xef ? 2 : c >= 0xf0 && c <= 0xf4 ? 3 : 0;
        if (!count || p + count > input.size())
          fail("Invalid UTF-8");
        unsigned cp = c & ((1u << (6 - count)) - 1u);
        std::string bytes(1, char(c));
        for (unsigned i = 0; i < count; ++i) {
          const unsigned char next = input[p++];
          if ((next & 0xc0) != 0x80)
            fail("Invalid UTF-8");
          cp = (cp << 6) | (next & 63);
          bytes += char(next);
        }
        if ((count == 1 && cp < 0x80) || (count == 2 && cp < 0x800) ||
            (count == 3 && cp < 0x10000) || cp > 0x10ffff ||
            (cp >= 0xd800 && cp <= 0xdfff))
          fail("Invalid UTF-8");
        out += bytes;
      }
      if (out.size() > 1024 * 1024)
        fail("String too long");
    }
    fail("Unterminated string");
  }
  Value value(unsigned depth) {
    space();
    if (depth > 48 || ++nodes > 600000)
      fail("JSON complexity limit");
    if (p >= input.size())
      fail("Missing value");
    if (input[p] == '"')
      return text();
    if (take('{')) {
      auto out = Value::object();
      if (take('}'))
        return out;
      do {
        const auto key = text();
        if (!take(':'))
          fail("Missing colon");
        auto v = value(depth + 1);
        if (!out.objectValue().emplace(key, std::move(v)).second)
          fail("Duplicate object key");
        if (take('}'))
          return out;
      } while (take(','));
      fail("Expected object delimiter");
    }
    if (take('[')) {
      auto out = Value::array();
      if (take(']'))
        return out;
      do {
        out.push(value(depth + 1));
        if (take(']'))
          return out;
      } while (take(','));
      fail("Expected array delimiter");
    }
    for (const auto &literal :
         {std::string("true"), std::string("false"), std::string("null")})
      if (input.substr(p, literal.size()) == literal) {
        p += literal.size();
        if (literal == "null")
          return {};
        return literal == "true";
      }
    const auto start = p;
    if (input[p] == '-')
      ++p;
    if (p >= input.size())
      fail("Invalid number");
    if (input[p] == '0')
      ++p;
    else {
      if (input[p] < '1' || input[p] > '9')
        fail("Invalid value");
      while (p < input.size() && input[p] >= '0' && input[p] <= '9')
        ++p;
    }
    if (p < input.size() && input[p] == '.') {
      ++p;
      const auto first = p;
      while (p < input.size() && input[p] >= '0' && input[p] <= '9')
        ++p;
      if (p == first)
        fail("Invalid fraction");
    }
    if (p < input.size() && (input[p] == 'e' || input[p] == 'E')) {
      ++p;
      if (p < input.size() && (input[p] == '+' || input[p] == '-'))
        ++p;
      const auto first = p;
      while (p < input.size() && input[p] >= '0' && input[p] <= '9')
        ++p;
      if (p == first)
        fail("Invalid exponent");
    }
    const std::string number(input.substr(start, p - start));
    std::istringstream stream(number);
    stream.imbue(std::locale::classic());
    double n = 0;
    stream >> n;
    if (!stream || !std::isfinite(n))
      fail("Non-finite number");
    return n;
  }

public:
  explicit Parser(std::string_view text) : input(text) {
    if (text.size() > 16u * 1024u * 1024u)
      throw std::runtime_error("JSON file exceeds 16 MiB");
  }
  Value parse() {
    auto v = value(0);
    space();
    if (p != input.size())
      fail("Trailing JSON data");
    return v;
  }
};
inline std::string quote(std::string_view value) {
  std::string out = "\"";
  for (unsigned char c : value) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += char(c);
    } else if (c < 32) {
      char escaped[7]{};
      std::snprintf(escaped, sizeof(escaped), "\\u%04x", unsigned(c));
      out += escaped;
    } else
      out += char(c);
  }
  return out + '"';
}
inline void write(const Value &v, std::string &out) {
  if (v.isNull())
    out += "null";
  else if (v.isBool())
    out += v.boolean() ? "true" : "false";
  else if (v.isNumber()) {
    if (!std::isfinite(v.number()))
      throw std::runtime_error("Non-finite JSON number");
    std::ostringstream number;
    number.imbue(std::locale::classic());
    number << std::setprecision(17) << v.number();
    out += number.str();
  } else if (v.isString())
    out += quote(v.string());
  else if (v.isArray()) {
    out += '[';
    bool sep = false;
    for (const auto &x : v.arrayValue()) {
      if (sep)
        out += ',';
      sep = true;
      write(x, out);
    }
    out += ']';
  } else {
    out += '{';
    bool sep = false;
    for (const auto &[k, x] : v.objectValue()) {
      if (sep)
        out += ',';
      sep = true;
      out += quote(k) + ":";
      write(x, out);
    }
    out += '}';
  }
}
inline std::string stringify(const Value &v) {
  std::string out;
  write(v, out);
  return out;
}
inline Value parse(std::string_view text) { return Parser(text).parse(); }
} // namespace s3g_thru_json
