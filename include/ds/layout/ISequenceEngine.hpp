#ifndef DRIFTSPEC_LAYOUT_ISEQUENCEENGINE_HPP
#define DRIFTSPEC_LAYOUT_ISEQUENCEENGINE_HPP

#include "ds/layout/SequenceGraph.hpp"
#include "ds/layout/SequencePlacement.hpp"

namespace ds {
namespace layout {

/// A sequence engine (DS-464): a sequence graph in, where its lanes, rows and frames land out.
class ISequenceEngine {
 public:
  virtual ~ISequenceEngine() = default;
  virtual SequencePlacement place(const SequenceGraph& graph) const = 0;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_ISEQUENCEENGINE_HPP
