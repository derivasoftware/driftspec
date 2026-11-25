#ifndef DRIFTSPEC_RENDER_TOURPROJECTOR_HPP
#define DRIFTSPEC_RENDER_TOURPROJECTOR_HPP

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/render/IProjector.hpp"
#include "ds/render/Ir.hpp"

namespace ds {
namespace render {

/// A tour (DS-245, DS-369): the boxes that take part in a behaviour and its sequence, two parts of one drawing on one
/// cut, each drawn by its own projector.
class TourProjector : public IProjector {
 public:
  TourProjector(const model::Model& model, const model::Metamodel& metamodel) : model_(model), metamodel_(metamodel) {}
  IrDocument project(const model::Entity& diagram) const override;

 private:
  const model::Model& model_;
  const model::Metamodel& metamodel_;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_TOURPROJECTOR_HPP
