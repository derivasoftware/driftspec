#include "ds/format/DiagramFormats.hpp"

#include <nlohmann/json.hpp>

#include "ds/render/Svg.hpp"
#include "ds/render/Text.hpp"

namespace ds {
namespace format {

std::string DiagramIr::name() const { return "ir"; }

std::string DiagramIr::write(const render::IrText& document, const FormatOptions& options, emit::ISink&) const {
  return options.pretty ? nlohmann::json::parse(document.json).dump(2) : document.json;  // as it came, unless asked to indent
}

std::string DiagramSvg::name() const { return "svg"; }

std::string DiagramSvg::write(const render::IrText& document, const FormatOptions& options, emit::ISink&) const {
  return svg_.render(render::IrDocument::parse(document.json), options.width);
}

std::string DiagramText::name() const { return "text"; }

std::string DiagramText::write(const render::IrText& document, const FormatOptions& options, emit::ISink&) const {
  return text_.render(render::IrDocument::parse(document.json), options.color, options.width);
}

}  // namespace format
}  // namespace ds
