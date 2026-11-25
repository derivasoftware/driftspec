#ifndef DRIFTSPEC_RENDER_SEQUENCEPROJECTION_HPP
#define DRIFTSPEC_RENDER_SEQUENCEPROJECTION_HPP

#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ds/layout/SequenceGraph.hpp"
#include "ds/render/Ir.hpp"

namespace ds {
namespace render {

/// A sequence of the render-IR as the layout's sequence graph (DS-464): its lifelines as lanes in the order written,
/// its messages between two of them as messages in the order of their rows, its frames that span rows as blocks. What
/// each lane, message and block stands for is kept beside the graph, in the same order, pointing into the document.
class SequenceProjection {
 public:
  explicit SequenceProjection(const IrDocument& doc);

  const layout::SequenceGraph& graph() const { return graph_; }
  const std::vector<const IrNode*>& lanes() const { return lanes_; }            ///< the lifeline of each lane
  const std::vector<const IrEdge*>& messages() const { return messages_; }      ///< the edge of each message
  const std::vector<const IrNode*>& blocks() const { return blocks_; }          ///< the frame of each block
  const std::map<std::string, std::size_t>& laneOf() const { return laneOf_; }  ///< a lifeline's lane, by its id
  /// The lanes by id, handed over: the projection keeps none after.
  std::map<std::string, std::size_t> takeLaneOf() { return std::move(laneOf_); }

 private:
  layout::SequenceGraph graph_;
  std::vector<const IrNode*> lanes_;
  std::vector<const IrEdge*> messages_;
  std::vector<const IrNode*> blocks_;
  std::map<std::string, std::size_t> laneOf_;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_SEQUENCEPROJECTION_HPP
