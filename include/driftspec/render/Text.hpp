#ifndef DRIFTSPEC_RENDER_TEXT_HPP
#define DRIFTSPEC_RENDER_TEXT_HPP

#include <string>

#include "driftspec/render/Ir.hpp"
#include "driftspec/render/Layout.hpp"

namespace driftspec {
namespace render {

/// Draws a document on a character grid with box-drawing glyphs: the same
/// layout as the SVG, run with character metrics (Metrics::text()), so a
/// box sits in the terminal where it sits on the page.
class Text {
 public:
  /// With `color`, ANSI colours follow the SVG palette roles (name, type, visibility).
  std::string render(const IrDocument& doc, bool color) const;
  std::string render(const IrDocument& doc, const LayoutResult& layout, bool color) const;
};

}  // namespace render
}  // namespace driftspec

#endif  // DRIFTSPEC_RENDER_TEXT_HPP
