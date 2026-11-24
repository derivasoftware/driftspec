#ifndef DRIFTSPEC_RENDER_TEXT_HPP
#define DRIFTSPEC_RENDER_TEXT_HPP

#include <string>

#include "ds/emit/Sink.hpp"
#include "ds/render/Ir.hpp"
#include "ds/render/Layout.hpp"

namespace ds {
namespace render {

/// Draws a document on a character grid with box-drawing glyphs: the same
/// layout as the SVG, run with character metrics (Metrics::text()), so a
/// box sits in the terminal where it sits on the page.
class Text {
 public:
  /// With `color`, ANSI colours follow the SVG palette roles (name, type, visibility).
  /// `maxWidth` in columns (0 = unbounded): the drawing wraps to it.
  std::string render(const IrDocument& doc, bool color, int maxWidth = 0) const;
  std::string render(const IrDocument& doc, const LayoutResult& layout, bool color) const;

  /// The same drawing as a cell grid for hosts that paint cells themselves
  /// (a Claude Code mod's Raster, a TUI): JSON {columns, rows, cells, text}
  /// where cells is base64 of little-endian u32 triplets [codepoint, fg, bg]
  /// per cell, row-major, 0x01000000 meaning the terminal's default colour; written as one value where the
  /// sink it is lent stands (DS-397).
  void cells(const IrDocument& doc, int maxWidth, emit::ISink& out_sink) const;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_TEXT_HPP
