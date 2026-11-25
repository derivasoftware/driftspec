#include "ds/render/IrExport.hpp"

#include <nlohmann/json.hpp>

#include "ds/render/ProjectorFactory.hpp"

namespace ds {
namespace render {

std::vector<std::string> IrExport::diagrams() const {
  std::vector<std::string> out;
  for (const auto& pair : model_.entities()) {
    if (pair.second.kind == model::Kind::Diagram) out.push_back(pair.first);
  }
  return out;
}

// The projector of the diagram's kind draws its body; the head every document opens with, and the writing, pretty-printed
// when asked, are this one's.
std::string IrExport::render(const std::string& diagramId, bool pretty) const {
  const model::Entity* diagram = model_.find(diagramId);
  if (!diagram || diagram->kind != model::Kind::Diagram) return "";
  nlohmann::json ir = ProjectorFactory::of(diagram->field("kind"), model_, metamodel_)->project(*diagram);
  ir.update(documentHead(*diagram));
  return pretty ? ir.dump(2) : ir.dump();
}

nlohmann::json IrExport::documentHead(const model::Entity& diagram) { return {{"ir", 1}, {"title", diagram.name}}; }

}  // namespace render
}  // namespace ds
