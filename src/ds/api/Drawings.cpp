#include "ds/api/Drawings.hpp"

#include <optional>

#include "ds/emit/SinkFactory.hpp"
#include "ds/format/FormatFactory.hpp"
#include "ds/model/Identity.hpp"
#include "ds/render/IrExport.hpp"

namespace ds {
namespace api {

std::string Drawings::svg(const model::Model& model, const model::Metamodel& metamodel, const theme::Theme& theme,
                          const std::string& diagram, int width) {
  const std::optional<render::IrDocument> document =
      render::IrExport(model, metamodel).document(model::Identity::ofDiagram(diagram).text());
  if (!document) return "";
  format::FormatOptions options;
  options.width = width;
  const auto sink = emit::SinkFactory::json(false);
  return format::FormatFactory::diagram(theme).at("svg")->write(*document, options, *sink);
}

std::map<std::string, std::string> Drawings::of(const model::Model& model, const model::Metamodel& metamodel, const theme::Theme& theme) {
  std::map<std::string, std::string> out;
  for (const auto& pair : model.entities()) {
    const model::Entity& d = pair.second;
    if (d.kind != model::Kind::Diagram || d.expected || d.field("kind") == "walkthrough") continue;
    const std::string name = model::Identity(d.id).diagramName();
    const std::string drawn = svg(model, metamodel, theme, name, 0);
    if (!drawn.empty()) out[name] = drawn;
  }
  return out;
}

}  // namespace api
}  // namespace ds
