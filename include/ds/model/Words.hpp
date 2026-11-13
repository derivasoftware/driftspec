#ifndef DRIFTSPEC_MODEL_WORDS_HPP
#define DRIFTSPEC_MODEL_WORDS_HPP

#include <string>
#include <vector>

namespace ds {
namespace model {

/// The items of a list written with `sep`, each without the spaces around it; an empty item is
/// dropped: "a, b,, c" → a, b, c.
std::vector<std::string> split(const std::string& text, char sep);

/// The items written one after the other with `sep` between them: a, b + ", " → "a, b".
std::string join(const std::vector<std::string>& items, const std::string& sep);

}  // namespace model
}  // namespace ds

#endif  // DRIFTSPEC_MODEL_WORDS_HPP
