#include "ds/text/Utf8.hpp"

namespace ds {
namespace text {

std::size_t Utf8::invalidAt(std::string_view text) {
  const std::size_t size = text.size();
  std::size_t at = 0;
  while (at < size) {
    const auto lead = static_cast<unsigned char>(text[at]);
    if (lead < 0x80) {  // ASCII: one byte
      ++at;
      continue;
    }
    // how many bytes the lead says, and the range its second byte takes (table 3-7): after E0 and F0 it starts past the
    // overlong forms, after ED it stops before the surrogates, after F4 at U+10FFFF
    std::size_t length = 0;
    unsigned char low = 0x80, high = 0xBF;
    if (lead >= 0xC2 && lead <= 0xDF) {
      length = 2;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
      length = 3;
      if (lead == 0xE0) low = 0xA0;
      if (lead == 0xED) high = 0x9F;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
      length = 4;
      if (lead == 0xF0) low = 0x90;
      if (lead == 0xF4) high = 0x8F;
    } else {
      return at;  // a continuation byte with no lead, or one that leads nothing: C0 and C1 (overlong), F5 to FF
    }
    if (size - at < length) return at;  // cut short by the end of the text
    const auto second = static_cast<unsigned char>(text[at + 1]);
    if (second < low || second > high) return at;
    for (std::size_t k = 2; k < length; ++k)
      if ((static_cast<unsigned char>(text[at + k]) & 0xC0) != 0x80) return at;
    at += length;
  }
  return std::string_view::npos;
}

}  // namespace text
}  // namespace ds
