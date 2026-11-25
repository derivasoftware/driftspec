#include "ds/render/Drawings.hpp"

#include <optional>

#include "ds/model/Identity.hpp"
#include "ds/render/IrExport.hpp"
#include "ds/render/Svg.hpp"

namespace ds {
namespace render {

std::string Drawings::svg(const model::Model& model, const model::Metamodel& metamodel, const theme::Theme& theme,
                          const std::string& diagram, int width) {
  const std::optional<IrDocument> document = IrExport(model, metamodel).document(model::Identity::ofDiagram(diagram).text());
  if (!document) return "";
  return Svg(theme).render(*document, width);  // as the svg writer drew it before it went (DS-474)
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

}  // namespace render
}  // namespace ds
