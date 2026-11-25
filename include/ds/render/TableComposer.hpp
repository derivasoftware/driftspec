#ifndef DRIFTSPEC_RENDER_TABLECOMPOSER_HPP
#define DRIFTSPEC_RENDER_TABLECOMPOSER_HPP

#include "ds/render/Figure.hpp"
#include "ds/render/IComposer.hpp"
#include "ds/render/Ir.hpp"
#include "ds/render/Layout.hpp"

namespace ds {
namespace render {

/// A table composed for a sink (DS-464): projected into the layout's table graph, as the sink writes its cells, and
/// placed by its table engine with the sink's metrics, the same proportions in pixels or in cells.
class TableComposer : public IComposer {
 public:
  explicit TableComposer(const Metrics& metrics) : metrics_(metrics) {}
  Figure compose(const IrDocument& doc, int width) const override;

 private:
  const Metrics metrics_;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_TABLECOMPOSER_HPP
