#ifndef DRIFTSPEC_RENDER_IREXPORT_HPP
#define DRIFTSPEC_RENDER_IREXPORT_HPP

#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

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

  /// The render-IR document of `diagramId` ("diagram:CL_Model"), pretty-printed when asked.
  std::string render(const std::string& diagramId, bool pretty) const;

 private:
  /// What every render-IR document opens with, whatever its kind: the contract it is written in and its title.
  static nlohmann::json documentHead(const model::Entity& diagram);

  const model::Model& model_;
  const model::Metamodel& metamodel_;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_IREXPORT_HPP
