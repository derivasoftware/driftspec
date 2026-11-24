#ifndef DRIFTSPEC_LAYOUT_IENGINE_HPP
#define DRIFTSPEC_LAYOUT_IENGINE_HPP

#include "ds/layout/Graph.hpp"
#include "ds/layout/Placement.hpp"

namespace ds {
namespace layout {

/// A layout engine (DS-392): nested sized boxes and the edges between them in, where they land and how the edges run
/// out. It knows nothing of what the boxes stand for.
class IEngine {
 public:
  virtual ~IEngine() = default;
  /// Where the boxes of `graph` land and how its edges run.
  virtual Placement place(const Graph& graph) const = 0;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_IENGINE_HPP
