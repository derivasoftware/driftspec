#ifndef DRIFTSPEC_RENDER_IREXPORT_HPP
#define DRIFTSPEC_RENDER_IREXPORT_HPP

#include <optional>
#include <string>
#include <vector>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/render/Ir.hpp"

namespace ds {
namespace render {

/// Projects one `diagram` of the model onto plantuml-render's render-IR
/// (schema/render-ir.schema.json, contract v1). What the diagram takes is the query's filter of it
/// (DS-127); this decides how it is drawn (`show`, `hide`, and the cut of `open`, `close` and `cut`): if the model changes, the drawing
/// follows. Each kind of diagram is drawn by its own projector (DS-369); this chooses it and writes what it gives.
class IrExport {
 public:
  /// Draws the diagrams of `model`, written in `metamodel`, which names the kinds it draws (DS-351).
  IrExport(const model::Model& model, const model::Metamodel& metamodel) : model_(model), metamodel_(metamodel) {}

  /// The ids of the diagrams the model declares.
  std::vector<std::string> diagrams() const;

  /// The render-IR document of `diagramId` ("diagram:CL_Model"), headed, as a value (DS-418): none when the model
  /// declares no such diagram. format::IrDescription writes it into whatever sink its door lends.
  std::optional<IrDocument> document(const std::string& diagramId) const;

 private:
  /// What heads every render-IR document, and the one place it is filled (DS-443, DS-418): the diagram's kind, so a
  /// drawing can tell how to draw it without guessing from its nodes (a box diagram that says no kind is a class
  /// diagram), and its title. The contract's version is the description's to write.
  static void documentHead(const model::Entity& diagram, IrDocument& out_document);

  const model::Model& model_;
  const model::Metamodel& metamodel_;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_IREXPORT_HPP
