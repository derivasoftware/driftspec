#include "ds/format/DiagramFormats.hpp"

#include <nlohmann/json.hpp>

#include "ds/render/Layout.hpp"
#include "ds/render/Svg.hpp"
#include "ds/render/Text.hpp"

namespace ds {
namespace format {

std::string DiagramIr::name() const { return "ir"; }

std::string DiagramIr::write(const render::IrText& document, const FormatOptions& options) const {
  return options.pretty ? nlohmann::json::parse(document.json).dump(2) : document.json;  // as it came, unless asked to indent
}

std::string DiagramLayout::name() const { return "layout"; }

std::string DiagramLayout::write(const render::IrText& document, const FormatOptions& options) const {
  // the placed rectangles and routes, in cells: the portable form for hosts that draw themselves
  const render::IrDocument parsed = render::IrDocument::parse(document.json).bundled();
  const render::LayoutResult lay = render::Layout(render::Metrics::text()).place(parsed, options.width);
  nlohmann::json out = {{"nodes", nlohmann::json::array()}, {"routes", nlohmann::json::array()}};
  for (const auto& p : lay.placed) {
    out["nodes"].push_back(
        {{"id", p.node->id}, {"kind", p.node->kind}, {"x", p.rect.x}, {"y", p.rect.y}, {"w", p.rect.w}, {"h", p.rect.h}});
  }
  for (const auto& r : lay.routes) {
    const render::IrEdge& e = parsed.edges[r.first];
    nlohmann::json points = nlohmann::json::array();
    for (const auto& pt : r.second.points) points.push_back({pt.x, pt.y});
    out["routes"].push_back({{"from", e.from}, {"to", e.to}, {"kind", e.kind}, {"points", points}});
  }
  return out.dump() + "\n";
}

std::string DiagramSvg::name() const { return "svg"; }

std::string DiagramSvg::write(const render::IrText& document, const FormatOptions& options) const {
  return render::Svg().render(render::IrDocument::parse(document.json), options.width);
}

std::string DiagramText::name() const { return "text"; }

std::string DiagramText::write(const render::IrText& document, const FormatOptions& options) const {
  return render::Text().render(render::IrDocument::parse(document.json), options.color, options.width);
}

std::string DiagramCells::name() const { return "cells"; }

std::string DiagramCells::write(const render::IrText& document, const FormatOptions& options) const {
  return render::Text().cells(render::IrDocument::parse(document.json), options.width) + "\n";
}

std::map<std::string, std::unique_ptr<IFormat<render::IrText>>> diagramFormats() {
  std::map<std::string, std::unique_ptr<IFormat<render::IrText>>> out;
  out["ir"] = std::make_unique<DiagramIr>();
  out["layout"] = std::make_unique<DiagramLayout>();
  out["svg"] = std::make_unique<DiagramSvg>();
  out["text"] = std::make_unique<DiagramText>();
  out["cells"] = std::make_unique<DiagramCells>();
  return out;
}

}  // namespace format
}  // namespace ds
