#ifndef DRIFTSPEC_RENDER_SVG_HPP
#define DRIFTSPEC_RENDER_SVG_HPP

#include <string>

#include "ds/render/Ir.hpp"
#include "ds/render/Layout.hpp"

namespace ds {
namespace render {

/// Emits the SVG of a laid-out document: the same stable ids, CSS classes,
/// tokens and markers as plantuml-render's engine, so a host page styles
/// both the same way and a drawing from either engine is comparable.
class Svg {
 public:
  /// `maxWidth` in pixels (0 = unbounded): the drawing wraps to it.
  std::string render(const IrDocument& doc, int maxWidth = 0) const;
  std::string render(const IrDocument& doc, const LayoutResult& layout) const;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_SVG_HPP
