#include "ds/text/Text.hpp"

#include <cstring>
#include <sstream>

namespace ds {
namespace text {

constexpr char kBase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::vector<std::string> splitTrimmed(const std::string& text, char sep) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (start <= text.size()) {
    std::size_t end = text.find(sep, start);
    if (end == std::string::npos) end = text.size();
    const std::string_view item = trimSpaces(std::string_view(text).substr(start, end - start));
    if (!item.empty()) out.emplace_back(item);
    if (end == text.size()) break;
    start = end + 1;
  }
  return out;
}

std::string join(const std::vector<std::string>& items, const std::string& sep) {
  std::size_t size = items.empty() ? 0 : sep.size() * (items.size() - 1);
  for (const auto& i : items) size += i.size();
  std::string out;
  out.reserve(size);
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (i > 0) out += sep;  // between every two items, an empty one included (DS-344)
    out += items[i];
  }
  return out;
}

std::vector<std::string> linesOf(const std::string& label) {
  std::vector<std::string> out;
  std::string current;
  for (std::size_t i = 0; i < label.size(); ++i) {
    if (label[i] == '\n' || (label[i] == '\\' && i + 1 < label.size() && label[i + 1] == 'n')) {
      out.push_back(current);
      current.clear();
      if (label[i] == '\\') ++i;
    } else {
      current += label[i];
    }
  }
  out.push_back(current);
  return out;
}

std::string_view trimWhitespace(std::string_view text) {
  const auto begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string_view::npos) return {};
  return text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
}

std::string_view trimSpaces(std::string_view text) {
  const auto begin = text.find_first_not_of(' ');
  if (begin == std::string_view::npos) return {};
  return text.substr(begin, text.find_last_not_of(' ') - begin + 1);
}

std::string base64(const unsigned char* data, std::size_t size) {
  std::string out;
  out.reserve((size + 2) / 3 * 4);
  for (std::size_t i = 0; i < size; i += 3) {
    const unsigned v = (static_cast<unsigned>(data[i]) << 16) | (i + 1 < size ? static_cast<unsigned>(data[i + 1]) << 8 : 0u) |
                       (i + 2 < size ? static_cast<unsigned>(data[i + 2]) : 0u);
    out += kBase64[(v >> 18) & 63];
    out += kBase64[(v >> 12) & 63];
    out += i + 1 < size ? kBase64[(v >> 6) & 63] : '=';
    out += i + 2 < size ? kBase64[v & 63] : '=';
  }
  return out;
}

std::string fromBase64(const std::string& text) {
  std::string out;
  unsigned v = 0;
  int bits = -8;
  for (const char c : text) {
    if (c == '=' || c == 0) break;
    const char* at = std::strchr(kBase64, c);
    if (!at) break;
    v = ((v << 6) | static_cast<unsigned>(at - kBase64)) & 0xFFFFFF;
    bits += 6;
    if (bits >= 0) {
      out += static_cast<char>((v >> bits) & 0xFF);
      bits -= 8;
    }
  }
  return out;
}

std::string escapeXml(const std::string& text) {
  std::string out;
  out.reserve(text.size() + text.size() / 8);
  for (const char c : text) {
    switch (c) {
      case '&':
        out += "&amp;";
        break;
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      case '"':
        out += "&quot;";
        break;
      default:
        out += c;
    }
  }
  return out;
}

std::vector<std::string> wrap(const std::string& text, std::size_t width) {
  std::vector<std::string> lines;
  std::string line;
  std::stringstream in(text);
  std::string word;
  while (in >> word) {
    if (!line.empty() && line.size() + 1 + word.size() > width) {
      lines.push_back(line);
      line.clear();
    }
    line += (line.empty() ? "" : " ") + word;
  }
  if (!line.empty()) lines.push_back(line);
  return lines;
}

std::size_t characters(const std::string& text) {
  std::size_t n = 0;
  for (const unsigned char c : text) n += (c & 0xC0) != 0x80 ? 1 : 0;  // every byte but one that continues a character
  return n;
}

}  // namespace text
}  // namespace ds
