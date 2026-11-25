#ifndef DRIFTSPEC_LAYOUT_SEQUENCEGRAPH_HPP
#define DRIFTSPEC_LAYOUT_SEQUENCEGRAPH_HPP

#include <vector>

#include "ds/layout/SequenceBlock.hpp"
#include "ds/layout/SequenceLane.hpp"
#include "ds/layout/SequenceMessage.hpp"

namespace ds {
namespace layout {

/// A sequence to place (DS-464), knowing nothing of what it stands for: its lanes in the order written, its messages
/// in the order of their rows, and its frames.
struct SequenceGraph {
  std::vector<SequenceLane> lanes;
  std::vector<SequenceMessage> messages;
  std::vector<SequenceBlock> blocks;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_SEQUENCEGRAPH_HPP
