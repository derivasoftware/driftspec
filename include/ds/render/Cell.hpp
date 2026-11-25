#ifndef DRIFTSPEC_RENDER_CELL_HPP
#define DRIFTSPEC_RENDER_CELL_HPP

#include <array>
#include <cstdint>

namespace ds {
namespace render {

/// One character of the drawing, in 7 bytes (DS-350): its UTF-8 bytes inline, the index of its colour in the
/// grid's palette and what an edge drew there. A glyph longer than one code point (never drawn today) is kept
/// in the grid's list of long glyphs and found by its index.
struct Cell {
  static constexpr std::uint8_t kLong = 0xFF;  ///< the size that says glyph holds the index of a long glyph

  std::array<char, 4> glyph = {' ', 0, 0, 0};
  std::uint8_t size = 1;    ///< bytes of glyph in use; kLong: glyph holds the index of a long glyph
  std::uint8_t colour = 0;  ///< the grid's palette; 0 is the default colour
  std::uint8_t line = 0;    ///< 1 horizontal, 2 vertical, 3 both: what an edge already drew here
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_CELL_HPP
