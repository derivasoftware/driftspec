#ifndef DRIFTSPEC_RENDER_TABLELAYOUT_HPP
#define DRIFTSPEC_RENDER_TABLELAYOUT_HPP

#include <string>
#include <vector>

namespace ds {
namespace render {

/// Where a table lands (DS-464), composed for one technology: each column's left edge and width (the corner's first),
/// the headings' height, each row's top and height, and each text in the lines it is written on, as the sink writes it.
struct TableLayout {
  std::vector<int> columnX;
  std::vector<int> columnWidth;
  int headHeight = 0;
  std::vector<int> rowY;
  std::vector<int> rowHeight;
  int width = 0;
  int height = 0;
  int padX = 0;                                              ///< a cell's room each side of its text
  int lineHeight = 0;                                        ///< a line of a cell
  std::vector<std::string> corner;                           ///< its lines
  std::vector<std::vector<std::string>> headings;            ///< each heading's lines
  std::vector<std::vector<std::string>> labels;              ///< each row's label's lines
  std::vector<std::vector<std::vector<std::string>>> cells;  ///< each row's cells' lines
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_TABLELAYOUT_HPP
