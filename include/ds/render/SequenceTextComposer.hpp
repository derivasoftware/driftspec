#ifndef DRIFTSPEC_RENDER_SEQUENCETEXTCOMPOSER_HPP
#define DRIFTSPEC_RENDER_SEQUENCETEXTCOMPOSER_HPP

#include "ds/render/Figure.hpp"
#include "ds/render/IComposer.hpp"
#include "ds/render/Ir.hpp"
#include "ds/render/Layout.hpp"

namespace ds {
namespace render {

/// A sequence composed for a terminal (DS-400): lanes wide enough for their heads and for the longest label that
/// crosses each gap, two rows per message (three for one to its own lane), and frames stacked a row further out for
/// each frame inside them that opens or closes at the same message.
class SequenceTextComposer : public IComposer {
 public:
  explicit SequenceTextComposer(const Metrics& metrics) : metrics_(metrics) {}
  Figure compose(const IrDocument& doc, int width) const override;

 private:
  const Metrics metrics_;  ///< the cells, as every composer of a terminal is given
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_SEQUENCETEXTCOMPOSER_HPP
