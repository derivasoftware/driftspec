#ifndef DRIFTSPEC_RENDER_TABLEPROJECTOR_HPP
#define DRIFTSPEC_RENDER_TABLEPROJECTOR_HPP

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/render/IProjector.hpp"
#include "ds/render/Ir.hpp"

namespace ds {
namespace render {

/// A machine as a table (DS-251, DS-369): what ds::machine::Table says, as the table of the IR.
class TableProjector : public IProjector {
 public:
  TableProjector(const model::Model& model, const model::Metamodel& metamodel) : model_(model), metamodel_(metamodel) {}
  IrDocument project(const model::Entity& diagram) const override;

 private:
  const model::Model& model_;
  const model::Metamodel& metamodel_;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_TABLEPROJECTOR_HPP
