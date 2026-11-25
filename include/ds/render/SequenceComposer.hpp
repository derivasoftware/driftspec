#ifndef DRIFTSPEC_RENDER_SEQUENCECOMPOSER_HPP
#define DRIFTSPEC_RENDER_SEQUENCECOMPOSER_HPP

#include "ds/render/Figure.hpp"
#include "ds/render/IComposer.hpp"
#include "ds/render/Ir.hpp"
#include "ds/render/Layout.hpp"

namespace ds {
namespace render {

/// A sequence composed for a sink (DS-464): projected into the layout's sequence graph and placed by its sequence
/// engine with the sink's metrics, the same proportions in pixels or in cells, its marks given the least room they need.
class SequenceComposer : public IComposer {
 public:
  explicit SequenceComposer(const Metrics& metrics) : metrics_(metrics) {}
  Figure compose(const IrDocument& doc, int width) const override;

 private:
  const Metrics metrics_;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_SEQUENCECOMPOSER_HPP
