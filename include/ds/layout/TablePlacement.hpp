#ifndef DRIFTSPEC_LAYOUT_TABLEPLACEMENT_HPP
#define DRIFTSPEC_LAYOUT_TABLEPLACEMENT_HPP

#include <cstddef>
#include <vector>

namespace ds {
namespace layout {

/// Where a table lands (DS-464), in the sink's units: each column's left edge and width (the corner's first), the
/// headings' height, each row's top and height, and where each text breaks into lines: the word each line after the
/// first starts with, for the corner, the headings, then each row's label and every cell of it, in that order.
struct TablePlacement {
  std::vector<int> columnX;
  std::vector<int> columnWidth;
  int headHeight = 0;
  std::vector<int> rowY;
  std::vector<int> rowHeight;
  std::vector<std::vector<std::size_t>> breaks;
  int width = 0;
  int height = 0;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_TABLEPLACEMENT_HPP
