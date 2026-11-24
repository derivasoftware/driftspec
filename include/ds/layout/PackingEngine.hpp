#ifndef DRIFTSPEC_LAYOUT_PACKINGENGINE_HPP
#define DRIFTSPEC_LAYOUT_PACKINGENGINE_HPP

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ds/layout/Box.hpp"
#include "ds/layout/Frame.hpp"
#include "ds/layout/IEngine.hpp"
#include "ds/layout/Packed.hpp"

namespace ds {
namespace layout {

/// The engine that packs nested boxes (DS-389, DS-392): a frame is fixed, fills shelves or is a tree. A graph can hold
/// its shelves to the widths an earlier placement kept, so a second placement of the same boxes, some of them shorter,
/// keeps every box as narrow as the first one made it. It routes no edge.
class PackingEngine : public IEngine {
 public:
  Placement place(const Graph& graph) const override;

 private:
  Packed size(const Frame& frame, const Graph& graph, Placement& out) const;
  Packed fixed(const Frame& frame, const Graph& graph, Placement& out) const;
  Packed shelves(const Frame& frame, const Graph& graph, Placement& out) const;
  Packed tree(const Frame& frame, const Graph& graph, Placement& out) const;
  /// The width a frame's shelves fill: the one computed or, following, the one the graph kept for the frame.
  static double width(const std::string& id, double computed, const Graph& graph);
  /// Places the children left to right in shelves no wider than `target`, into `at`; the width and height used.
  static std::pair<double, double> shelf(const std::vector<Packed>& kids, double gap, double target,
                                         std::vector<std::pair<double, double>>& at);
  /// The width to aim at: the widest child, at least `atLeast`, or the side of the children's area by `factor`.
  static double target(const std::vector<Packed>& kids, double atLeast, double factor);
  /// Writes where a frame and everything under it land, the frame at `x`, `y`, in the order the graph is read.
  static void land(const Frame& frame, const Packed& packed, double x, double y, std::vector<Box>& out);
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_PACKINGENGINE_HPP
