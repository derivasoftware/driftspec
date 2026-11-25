#ifndef DRIFTSPEC_LAYOUT_STACKEDENGINE_HPP
#define DRIFTSPEC_LAYOUT_STACKEDENGINE_HPP

#include "ds/layout/ISequenceEngine.hpp"
#include "ds/layout/SequenceGraph.hpp"
#include "ds/layout/SequencePlacement.hpp"
#include "ds/layout/SequenceSpacing.hpp"

namespace ds {
namespace layout {

/// The stacked engine (DS-464): lanes side by side, wide enough for their heads and for the labels that cross them, a
/// label's lack of room spread over the gaps it crosses; a row per message, one under the other, with a lead above it
/// for each frame that opens there and each else that comes before it; frames around the rows they span, those that
/// open together stacked outer first, nested ones further out.
class StackedEngine : public ISequenceEngine {
 public:
  explicit StackedEngine(const SequenceSpacing& spacing) : spacing_(spacing) {}
  SequencePlacement place(const SequenceGraph& graph) const override;

 private:
  const SequenceSpacing spacing_;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_STACKEDENGINE_HPP
