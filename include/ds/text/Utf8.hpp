#ifndef DRIFTSPEC_TEXT_UTF8_HPP
#define DRIFTSPEC_TEXT_UTF8_HPP

#include <cstddef>
#include <string_view>

namespace ds {
namespace text {

/// UTF-8 as the Unicode standard writes it (DS-411): each character in one to four bytes, in the well-formed forms
/// of its table 3-7, so no overlong form, no surrogate and nothing above U+10FFFF. It holds nothing: each question
/// reads the text it is given.
class Utf8 {
 public:
  /// The offset of the first byte of `text` that is not part of a well-formed character; std::string_view::npos when
  /// the whole text is UTF-8. That byte is a continuation byte no lead comes before, a byte that leads nothing (0xC0,
  /// 0xC1, 0xF5 to 0xFF), or the lead of a character cut short, written in an overlong form, a surrogate or above
  /// U+10FFFF. One pass over the bytes, and nothing allocated.
  static std::size_t invalidAt(std::string_view text);
};

}  // namespace text
}  // namespace ds

#endif  // DRIFTSPEC_TEXT_UTF8_HPP
