#ifndef DRIFTSPEC_RENDER_SVG_HPP
#define DRIFTSPEC_RENDER_SVG_HPP

#include <string>

#include "driftspec/render/Ir.hpp"
#include "driftspec/render/Layout.hpp"

namespace driftspec {
namespace render {

/// Emits the SVG of a laid-out document: the same stable ids, CSS classes,
/// tokens and markers as plantuml-render's engine, so a host page styles
/// both the same way and a drawing from either engine is comparable.
class Svg {
 public:
  std::string render(const IrDocument& doc) const;
  std::string render(const IrDocument& doc, const LayoutResult& layout) const;
};

}  // namespace render
}  // namespace driftspec

#endif  // DRIFTSPEC_RENDER_SVG_HPP
