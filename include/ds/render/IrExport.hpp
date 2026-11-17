#ifndef DRIFTSPEC_RENDER_IREXPORT_HPP
#define DRIFTSPEC_RENDER_IREXPORT_HPP

#include <set>
#include <string>
#include <vector>

#include "ds/axis/Cut.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace render {

/// Projects one `diagram` of the model onto plantuml-render's render-IR
/// (schema/render-ir.schema.json, contract v1). What the diagram takes is the query's filter of it
/// (DS-127); this decides how it is drawn (`show`, `hide`, and the cut of `open`, `close` and `cut`): if the model changes, the drawing
/// follows.
class IrExport {
 public:
  explicit IrExport(const model::Model& model) : model_(model) {}

  /// The ids of the diagrams the model declares.
  std::vector<std::string> diagrams() const;

  /// The render-IR document of `diagramId` ("diagram:CL_Model"), pretty-printed when asked.
  std::string render(const std::string& diagramId, bool pretty) const;

 private:
  std::string renderSequence(const model::Entity& diagram, bool pretty) const;
  /// A tour (DS-245): the boxes that take part in a behaviour and its sequence, two parts of one drawing on one cut.
  std::string renderTour(const model::Entity& tour, bool pretty) const;
  /// A class diagram: the boxes a diagram takes, and the cut it is lifted onto.
  std::string renderBoxes(const model::Entity& diagram, bool pretty) const;
  std::string closeBoxes(const std::string& ir, const axis::Cut& cut, bool pretty) const;
  std::string renderState(const model::Entity& diagram, bool pretty) const;
  /// A machine as a table (DS-251): what ds::machine::Matrix says, as the table of the IR.
  std::string renderMatrix(const model::Entity& diagram, bool pretty) const;
  std::string renderRequirements(const model::Entity& diagram, bool pretty) const;

  const model::Model& model_;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_IREXPORT_HPP
