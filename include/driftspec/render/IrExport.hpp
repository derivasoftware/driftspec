#ifndef DRIFTSPEC_RENDER_IREXPORT_HPP
#define DRIFTSPEC_RENDER_IREXPORT_HPP

#include <set>
#include <string>
#include <vector>

#include "driftspec/model/Model.hpp"

namespace driftspec {
namespace render {

/// Projects one `diagram` of the model onto plantuml-render's render-IR
/// (schema/render-ir.schema.json, contract v1). The diagram selects
/// (`include`, `exclude`) and decides what to print (`show`); it never
/// redraws: if the model changes, the drawing follows.
class IrExport {
 public:
  explicit IrExport(const model::Model& model) : model_(model) {}

  /// The ids of the diagrams the model declares.
  std::vector<std::string> diagrams() const;

  /// The render-IR document of `diagramId` ("diagram:CL_Model"), pretty-printed when asked.
  std::string render(const std::string& diagramId, bool pretty) const;

  /// The entities a selection names: every `include` pattern minus every `exclude` pattern.
  std::set<std::string> select(const model::Entity& diagram) const;

 private:
  bool matches(const std::string& pattern, const model::Entity& entity) const;

  const model::Model& model_;
};

}  // namespace render
}  // namespace driftspec

#endif  // DRIFTSPEC_RENDER_IREXPORT_HPP
