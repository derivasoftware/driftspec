#ifndef DRIFTSPEC_TEXT_TEXT_HPP
#define DRIFTSPEC_TEXT_TEXT_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ds {
namespace text {

/// The text helpers of the whole engine, in one place (DS-344): what every module splits, joins, trims and
/// escapes. None of them knows the model.

/// The items of a list written with `sep`, each without the spaces around it, an empty item dropped:
/// "a, b,, c" → a, b, c. The name says it trims.
std::vector<std::string> splitTrimmed(const std::string& text, char sep);

/// The items written one after the other with `sep` between every two of them, an empty one included:
/// a, b + ", " → "a, b"; "", a + "," → ",a".
std::string join(const std::vector<std::string>& items, const std::string& sep);

/// The lines of a label: a new line ends one, and so does a written `\n`.
std::vector<std::string> linesOf(const std::string& label);

/// `text` without the white space (spaces, tabs, new lines, carriage returns) at either end.
std::string_view trimWhitespace(std::string_view text);

/// `text` without the spaces at either end; tabs and new lines are kept.
std::string_view trimSpaces(std::string_view text);

/// `size` bytes as base64, padded with '='.
std::string base64(const unsigned char* data, std::size_t size);

/// The bytes a base64 text stands for, up to its padding or the first character that is no base64.
std::string fromBase64(const std::string& text);

/// `text` with the characters XML gives a meaning written as entities: & < > ".
std::string escapeXml(const std::string& text);

}  // namespace text
}  // namespace ds

#endif  // DRIFTSPEC_TEXT_TEXT_HPP
