#include "ds/emit/JsonSink.hpp"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace ds {
namespace emit {

JsonSink::JsonSink(bool pretty, bool checked) : pretty_(pretty), checked_(checked) {}

bool JsonSink::checkedByDefault() {
  // DS_JSON_CHECK says it whatever the build (DS-461): 1 checks, so a Release test run in CI holds the contract
  // too; 0 does not. Without it, a debug build checks and a release build does not
  const char* said = std::getenv("DS_JSON_CHECK");
  if (said && *said) return std::strcmp(said, "0") != 0;
#ifdef NDEBUG
  return false;
#else
  return true;
#endif
}

void JsonSink::misuse(const std::string& what) { throw std::logic_error("ds::emit::JsonSink: " + what); }

void JsonSink::separator() {
  char& top = open_.back();
  const bool first = top == 'r' || top == 'l';
  if (pretty_) {
    if (first)
      out_ += '\n';
    else
      out_.append(",\n", 2);
    out_.append(2 * open_.size(), ' ');
  } else if (!first) {
    out_ += ',';
  }
  if (first) top = top == 'r' ? 'R' : 'L';
}

void JsonSink::beforeValue() {
  if (open_.empty()) {  // the document's one value
    if (checked_) {
      if (done_) misuse("a second value after the document's one");
      done_ = true;
    }
    return;
  }
  const char top = open_.back();
  if (top == 'l' || top == 'L') {
    separator();
    return;
  }
  if (checked_) {  // in a record, the value of the field just named
    if (!awaiting_) misuse("a value in a record without its field");
    awaiting_ = false;
  }
}

void JsonSink::open(char kind) {
  beforeValue();
  out_ += kind;
  open_ += kind == '{' ? 'r' : 'l';
  if (checked_ && kind == '{' && keys_.size() < open_.size()) keys_.resize(open_.size());
}

void JsonSink::close(char kind) {
  if (checked_) {
    if (open_.empty()) misuse(std::string("'") + kind + "' closes nothing open");
    const bool record = open_.back() == 'r' || open_.back() == 'R';
    if (record != (kind == '}')) misuse(std::string("'") + kind + "' closes a " + (record ? "record" : "list"));
    if (awaiting_) misuse("a record closed after a field without its value");
  }
  const bool written = open_.back() == 'R' || open_.back() == 'L';
  open_.pop_back();
  if (pretty_ && written) {
    out_ += '\n';
    out_.append(2 * open_.size(), ' ');
  }
  out_ += kind;
}

ISink& JsonSink::beginRecord() {
  open('{');
  return *this;
}

ISink& JsonSink::endRecord() {
  close('}');
  return *this;
}

ISink& JsonSink::field(std::string_view name) {
  if (checked_) {
    if (open_.empty() || (open_.back() != 'r' && open_.back() != 'R')) misuse("field \"" + std::string(name) + "\" outside a record");
    if (awaiting_) misuse("field \"" + std::string(name) + "\" after a field without its value");
    std::string& last = keys_[open_.size() - 1];
    if (open_.back() == 'R' && !(std::string_view(last) < name))
      misuse("field \"" + std::string(name) + "\" after \"" + last + "\": not in byte order, or repeated");
    last.assign(name.data(), name.size());
    awaiting_ = true;
  }
  separator();
  quoted(name);
  if (pretty_)
    out_.append(": ", 2);
  else
    out_ += ':';
  return *this;
}

ISink& JsonSink::beginList() {
  open('[');
  return *this;
}

ISink& JsonSink::endList() {
  close(']');
  return *this;
}

ISink& JsonSink::value(std::string_view text) {
  beforeValue();
  quoted(text);
  return *this;
}

ISink& JsonSink::integer(std::int64_t value) {
  beforeValue();
  char digits[24];
  const auto written = std::to_chars(digits, digits + sizeof digits, value);
  out_.append(digits, static_cast<std::size_t>(written.ptr - digits));
  return *this;
}

ISink& JsonSink::number(double value) {
  beforeValue();
  if (!std::isfinite(value)) {
    out_.append("null", 4);
    return *this;
  }
  if (std::signbit(value)) {
    out_ += '-';
    value = -value;
  }
  if (value == 0) {
    out_.append("0.0", 3);
    return *this;
  }
  // the shortest digits that read back, in scientific form: d[.ddd]e[+-]XX
  char sci[40];
  const char* end = std::to_chars(sci, sci + sizeof sci, value, std::chars_format::scientific).ptr;
  const char* e = static_cast<const char*>(std::memchr(sci, 'e', static_cast<std::size_t>(end - sci)));
  char digits[24];
  int k = 0;
  for (const char* p = sci; p < e; ++p)
    if (*p != '.') digits[k++] = *p;
  int exponent = 0;
  for (const char* p = e + 2; p < end; ++p) exponent = exponent * 10 + (*p - '0');
  if (e[1] == '-') exponent = -exponent;
  // nlohmann's layout: value = digits * 10^(n - k), n the place of the point; plain from 1e-5 to below 1e15
  const int n = exponent + 1;
  if (k <= n && n <= 15) {
    out_.append(digits, static_cast<std::size_t>(k));
    out_.append(static_cast<std::size_t>(n - k), '0');
    out_.append(".0", 2);
  } else if (0 < n && n <= 15) {
    out_.append(digits, static_cast<std::size_t>(n));
    out_ += '.';
    out_.append(digits + n, static_cast<std::size_t>(k - n));
  } else if (-4 < n && n <= 0) {
    out_.append("0.", 2);
    out_.append(static_cast<std::size_t>(-n), '0');
    out_.append(digits, static_cast<std::size_t>(k));
  } else {
    out_ += digits[0];
    if (k > 1) {
      out_ += '.';
      out_.append(digits + 1, static_cast<std::size_t>(k - 1));
    }
    const int shown = n - 1;
    out_ += 'e';
    out_ += shown < 0 ? '-' : '+';
    const std::string magnitude = std::to_string(shown < 0 ? -shown : shown);
    if (magnitude.size() < 2) out_ += '0';  // at least two digits: 1e+15, 1e-05
    out_ += magnitude;
  }
  return *this;
}

ISink& JsonSink::boolean(bool value) {
  beforeValue();
  if (value)
    out_.append("true", 4);
  else
    out_.append("false", 5);
  return *this;
}

ISink& JsonSink::null() {
  beforeValue();
  out_.append("null", 4);
  return *this;
}

std::string JsonSink::result() {
  if (checked_) {
    if (!open_.empty()) misuse("the result taken with a record or a list still open");
    if (!done_) misuse("the result taken with nothing written");
  }
  std::string written;
  written.swap(out_);
  open_.clear();
  keys_.clear();
  awaiting_ = false;
  done_ = false;
  return written;
}

void JsonSink::quoted(std::string_view text) {
  // what each byte asks for: 0 it is copied, 1 it is escaped, 2 it starts or continues a sequence of UTF-8
  // clang-format off
  static constexpr unsigned char kClass[256] = {
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  };
  // clang-format on
  static constexpr char kHex[] = "0123456789abcdef";
  out_ += '"';
  const char* data = text.data();
  const std::size_t size = text.size();
  std::size_t copied = 0, at = 0;  // the bytes from `copied` to `at` are copied as they are, in one go
  while (at < size) {
    const unsigned char c = static_cast<unsigned char>(data[at]);
    const unsigned char kind = kClass[c];
    if (kind == 0) {
      ++at;
      continue;
    }
    if (kind == 2) {
      at = sequenceEnd(text, at);
      continue;
    }
    out_.append(data + copied, at - copied);
    switch (c) {
      case '\b':
        out_.append("\\b", 2);
        break;
      case '\t':
        out_.append("\\t", 2);
        break;
      case '\n':
        out_.append("\\n", 2);
        break;
      case '\f':
        out_.append("\\f", 2);
        break;
      case '\r':
        out_.append("\\r", 2);
        break;
      case '"':
        out_.append("\\\"", 2);
        break;
      case '\\':
        out_.append("\\\\", 2);
        break;
      default: {  // another control character, in lower-case hex
        const char escaped[6] = {'\\', 'u', '0', '0', kHex[c >> 4], kHex[c & 15]};
        out_.append(escaped, 6);
      }
    }
    copied = ++at;
  }
  out_.append(data + copied, size - copied);
  out_ += '"';
}

// One sequence of UTF-8, checked as nlohmann's decoder checks it (the table of well-formed sequences of Unicode):
// the lead byte says how many bytes follow and the range of the first of them; every other is 80..BF.
std::size_t JsonSink::sequenceEnd(std::string_view text, std::size_t at) {
  const auto byte = [&text](std::size_t i) { return static_cast<unsigned char>(text[i]); };
  const unsigned char lead = byte(at);
  unsigned char low = 0x80, high = 0xBF;  // the range of the second byte
  int tail = 0;
  if (lead >= 0xC2 && lead <= 0xDF) {
    tail = 1;
  } else if (lead == 0xE0) {
    tail = 2, low = 0xA0;
  } else if ((lead >= 0xE1 && lead <= 0xEC) || lead == 0xEE || lead == 0xEF) {
    tail = 2;
  } else if (lead == 0xED) {
    tail = 2, high = 0x9F;
  } else if (lead == 0xF0) {
    tail = 3, low = 0x90;
  } else if (lead >= 0xF1 && lead <= 0xF3) {
    tail = 3;
  } else if (lead == 0xF4) {
    tail = 3, high = 0x8F;
  } else {
    invalidByte(at, lead);  // a tail without its lead, an overlong lead (C0, C1) or past U+10FFFF (F5..FF)
  }
  for (int k = 1; k <= tail; ++k) {
    if (at + k >= text.size()) incomplete(byte(text.size() - 1));
    const unsigned char b = byte(at + k);
    if (k == 1 ? (b < low || b > high) : (b < 0x80 || b > 0xBF)) invalidByte(at + k, b);
  }
  return at + tail + 1;
}

std::string JsonSink::hexByte(unsigned char byte) {
  static constexpr char kHex[] = "0123456789ABCDEF";
  return std::string("0x") + kHex[byte >> 4] + kHex[byte & 15];
}

void JsonSink::invalidByte(std::size_t at, unsigned char byte) {
  throw std::runtime_error("[json.exception.type_error.316] invalid UTF-8 byte at index " + std::to_string(at) + ": " + hexByte(byte));
}

void JsonSink::incomplete(unsigned char last) {
  throw std::runtime_error("[json.exception.type_error.316] incomplete UTF-8 string; last byte: " + hexByte(last));
}

}  // namespace emit
}  // namespace ds
