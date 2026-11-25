#ifndef DRIFTSPEC_LAYOUT_TABLETEXT_HPP
#define DRIFTSPEC_LAYOUT_TABLETEXT_HPP

#include <cstddef>
#include <vector>

namespace ds {
namespace layout {

/// A text of a table to place (DS-464): how long each of its words is, in characters. A cell longer than the wrap goes
/// on more lines, broken between words.
struct TableText {
  std::vector<std::size_t> words;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_TABLETEXT_HPP
