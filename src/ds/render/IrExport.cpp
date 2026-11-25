#include "ds/render/IrExport.hpp"

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
std::optional<IrDocument> IrExport::document(const std::string& diagramId) const {
  const model::Entity* diagram = model_.find(diagramId);
  if (!diagram || diagram->kind != model::Kind::Diagram) return std::nullopt;
  IrDocument ir = ProjectorFactory::of(diagram->field("kind"), model_, metamodel_)->project(*diagram);
  documentHead(*diagram, ir);
  return ir;
}

void IrExport::documentHead(const model::Entity& diagram, IrDocument& out_document) {
  const std::string kind = diagram.field("kind");
  out_document.kind = kind.empty() ? "class" : kind;
  out_document.title = diagram.name;
}

}  // namespace render
}  // namespace ds
