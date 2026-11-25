#ifndef DRIFTSPEC_RENDER_REQUIREMENTPROJECTOR_HPP
#define DRIFTSPEC_RENDER_REQUIREMENTPROJECTOR_HPP

#include <nlohmann/json_fwd.hpp>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/render/IProjector.hpp"

namespace ds {
namespace render {

/// A requirement diagram (DS-369): the requirements and what answers to each, as the boxes that stand for them.
class RequirementProjector : public IProjector {
 public:
  RequirementProjector(const model::Model& model, const model::Metamodel& metamodel) : model_(model), metamodel_(metamodel) {}
  nlohmann::json project(const model::Entity& diagram) const override;

 private:
  static bool isArtefact(const model::Entity& e);

  const model::Model& model_;
  const model::Metamodel& metamodel_;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_REQUIREMENTPROJECTOR_HPP
