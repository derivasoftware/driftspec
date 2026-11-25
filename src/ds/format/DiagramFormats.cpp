#include "ds/format/DiagramFormats.hpp"

#include "ds/render/Svg.hpp"
#include "ds/render/Text.hpp"

namespace ds {
namespace format {

std::string DiagramSvg::name() const { return "svg"; }

std::string DiagramSvg::write(const render::IrDocument& document, const FormatOptions& options, emit::ISink&) const {
  return svg_.render(document, options.width);
}

std::string DiagramText::name() const { return "text"; }

std::string DiagramText::write(const render::IrDocument& document, const FormatOptions& options, emit::ISink&) const {
  return text_.render(document, options.color, options.width);
}

}  // namespace format
}  // namespace ds
