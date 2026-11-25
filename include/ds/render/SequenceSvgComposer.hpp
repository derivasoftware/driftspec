#ifndef DRIFTSPEC_RENDER_SEQUENCESVGCOMPOSER_HPP
#define DRIFTSPEC_RENDER_SEQUENCESVGCOMPOSER_HPP

#include "ds/render/Figure.hpp"
#include "ds/render/IComposer.hpp"
#include "ds/render/Ir.hpp"
#include "ds/render/Layout.hpp"

namespace ds {
namespace render {

/// A sequence composed for SVG (DS-400): lanes wide enough for their heads and for the labels that cross them, a row
/// per message in pixels, and frames around the messages they span, nested ones inset.
class SequenceSvgComposer : public IComposer {
 public:
  explicit SequenceSvgComposer(const Metrics& metrics) : metrics_(metrics) {}
  Figure compose(const IrDocument& doc, int width) const override;

 private:
  const Metrics metrics_;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_SEQUENCESVGCOMPOSER_HPP
