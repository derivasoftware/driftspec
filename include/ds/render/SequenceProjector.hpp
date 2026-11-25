#ifndef DRIFTSPEC_RENDER_SEQUENCEPROJECTOR_HPP
#define DRIFTSPEC_RENDER_SEQUENCEPROJECTOR_HPP

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/render/IProjector.hpp"
#include "ds/render/Ir.hpp"

namespace ds {
namespace render {

/// A sequence diagram (DS-369): the lifelines a behaviour stands on and its messages, followed as deep as it says.
class SequenceProjector : public IProjector {
 public:
  SequenceProjector(const model::Model& model, const model::Metamodel& metamodel) : model_(model), metamodel_(metamodel) {}
  IrDocument project(const model::Entity& diagram) const override;

 private:
  const model::Model& model_;
  const model::Metamodel& metamodel_;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_SEQUENCEPROJECTOR_HPP
