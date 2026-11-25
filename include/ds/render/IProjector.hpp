#ifndef DRIFTSPEC_RENDER_IPROJECTOR_HPP
#define DRIFTSPEC_RENDER_IPROJECTOR_HPP

#include "ds/model/Model.hpp"
#include "ds/render/Ir.hpp"

namespace ds {
namespace render {

/// What draws one kind of diagram (DS-369): a diagram of the model in, the body of its render-IR out, as a document
/// (DS-418) that format::IrDescription writes
/// (schema/render-ir.schema.json, contract v1). IrExport chooses the projector, heads the document and writes it.
class IProjector {
 public:
  virtual ~IProjector() = default;
  /// The body of `diagram`'s render-IR: everything but the head IrExport writes.
  virtual IrDocument project(const model::Entity& diagram) const = 0;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_IPROJECTOR_HPP
