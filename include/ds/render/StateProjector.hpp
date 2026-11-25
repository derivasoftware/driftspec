#ifndef DRIFTSPEC_RENDER_STATEPROJECTOR_HPP
#define DRIFTSPEC_RENDER_STATEPROJECTOR_HPP

#include <nlohmann/json_fwd.hpp>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/render/IProjector.hpp"

namespace ds {
namespace render {

/// A state diagram (DS-369): each state with the rule that computes it, and the jumps between them.
class StateProjector : public IProjector {
 public:
  StateProjector(const model::Model& model, const model::Metamodel& metamodel) : model_(model), metamodel_(metamodel) {}
  nlohmann::json project(const model::Entity& diagram) const override;

 private:
  const model::Model& model_;
  const model::Metamodel& metamodel_;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_STATEPROJECTOR_HPP
