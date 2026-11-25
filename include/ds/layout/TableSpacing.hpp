#ifndef DRIFTSPEC_LAYOUT_TABLESPACING_HPP
#define DRIFTSPEC_LAYOUT_TABLESPACING_HPP

#include <cstddef>

namespace ds {
namespace layout {

/// The distances a table is placed with (DS-464), in the sink's units: proportions written once, across in characters
/// and down in lines, scaled by the sink's measures and rounded; the SVG's own, so its 8 px and 18 px give its pixels.
/// A cell wraps at the same number of characters in every sink.
struct TableSpacing {
  int advance = 0;       ///< a character
  int padX = 0;          ///< a cell's room each side of its text
  int headHeight = 0;    ///< the row of headings
  int rowHeight = 0;     ///< a row, at least
  int lineHeight = 0;    ///< a line of a cell
  int linePad = 0;       ///< a row's room above and below its lines
  std::size_t wrap = 0;  ///< the characters a line of a cell holds

  static TableSpacing of(int advance, int lineHeight);
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_TABLESPACING_HPP
