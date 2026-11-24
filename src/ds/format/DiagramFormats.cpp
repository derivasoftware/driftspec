#include "ds/format/DiagramFormats.hpp"

#include <nlohmann/json.hpp>

#include "ds/render/Layout.hpp"
#include "ds/render/Svg.hpp"
#include "ds/render/Text.hpp"

namespace ds {
namespace format {

std::string DiagramIr::name() const { return "ir"; }

std::string DiagramIr::write(const render::IrText& document, const FormatOptions& options, emit::ISink&) const {
  return options.pretty ? nlohmann::json::parse(document.json).dump(2) : document.json;  // as it came, unless asked to indent
}

std::string DiagramLayout::name() const { return "layout"; }

// The placed rectangles and routes of one drawing, in cells; of a drawing made of parts, those of each part. A
// part is named after the fields its drawing gives: part before parts, nodes before part before routes.
void DiagramLayout::placed(const render::IrDocument& document, int width, const std::string* part, emit::ISink& out_sink) {
  out_sink.beginRecord();
  if (!document.parts.empty()) {
    if (part) out_sink.field("part").value(*part);
    out_sink.field("parts").beginList();
    for (const auto& name : document.parts) placed(document.part(name), width, &name, out_sink);
    out_sink.endList().endRecord();
    return;
  }
  const render::IrDocument parsed = document.bundled();
  const render::LayoutResult lay = render::Layout(render::Metrics::text()).place(parsed, width);
  out_sink.field("nodes").beginList();
  for (const auto& p : lay.placed) {
    out_sink.beginRecord().field("h").integer(p.rect.h).field("id").value(p.node->id).field("kind").value(p.node->kind);
    out_sink.field("w").integer(p.rect.w).field("x").integer(p.rect.x).field("y").integer(p.rect.y).endRecord();
  }
  out_sink.endList();
  if (part) out_sink.field("part").value(*part);
  out_sink.field("routes").beginList();
  for (const auto& r : lay.routes) {
    const render::IrEdge& e = parsed.edges[r.first];
    out_sink.beginRecord().field("from").value(e.from).field("kind").value(e.kind).field("points").beginList();
    for (const auto& pt : r.second.points) out_sink.beginList().integer(pt.x).integer(pt.y).endList();
    out_sink.endList().field("to").value(e.to).endRecord();
  }
  out_sink.endList().endRecord();
}

std::string DiagramLayout::write(const render::IrText& document, const FormatOptions& options, emit::ISink& out_sink) const {
  // the portable form for hosts that draw themselves
  placed(render::IrDocument::parse(document.json), options.width, nullptr, out_sink);
  return out_sink.result() + "\n";
}

std::string DiagramSvg::name() const { return "svg"; }

std::string DiagramSvg::write(const render::IrText& document, const FormatOptions& options, emit::ISink&) const {
  return render::Svg().render(render::IrDocument::parse(document.json), options.width);
}

std::string DiagramText::name() const { return "text"; }

std::string DiagramText::write(const render::IrText& document, const FormatOptions& options, emit::ISink&) const {
  return render::Text().render(render::IrDocument::parse(document.json), options.color, options.width);
}

std::string DiagramCells::name() const { return "cells"; }

std::string DiagramCells::write(const render::IrText& document, const FormatOptions& options, emit::ISink& out_sink) const {
  render::Text().cells(render::IrDocument::parse(document.json), options.width, out_sink);
  return out_sink.result() + "\n";
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
