#ifndef DRIFTSPEC_LAYOUT_FRAMER_HPP
#define DRIFTSPEC_LAYOUT_FRAMER_HPP

#include <cstddef>
#include <map>
#include <vector>

#include "ds/layout/Band.hpp"
#include "ds/layout/Component.hpp"
#include "ds/layout/Edge.hpp"
#include "ds/layout/Frame.hpp"
#include "ds/layout/Graph.hpp"
#include "ds/layout/IRouter.hpp"
#include "ds/layout/Placement.hpp"
#include "ds/layout/Rect.hpp"
#include "ds/layout/Spacing.hpp"
#include "ds/layout/Waypoint.hpp"

namespace ds {
namespace layout {

/// Lays out the frames of one graph (DS-370, DS-392): every layered frame lays out its own kids, a connected component
/// at a time (layered, ordered, positioned), and packs its components in rows towards a readable aspect; then every
/// box gets its place on the page and the router is given what the layering left for the edges. One framer serves one
/// placement.
class Framer {
 public:
  Framer(const Graph& graph, const Spacing& s);
  /// Where every frame lands and how every edge runs, routed by `router`.
  Placement run(const IRouter& router);

 private:
  /// The frame laid out under a number, or -1 when the number is no frame under the root.
  int nodeAt(std::size_t number) const;
  /// The ancestor of `node` that is a kid of `frameParent`, or -1.
  int ancestorIn(int node, int frameParent) const;
  /// Lays out the kids of `parent` (-1 is the root), `available` wide at most, and sizes `parent` around them.
  void frame(int parent, int available);
  /// One connected component of a frame's kids: layered, ordered and positioned.
  void layered(Component& c, int available);

  std::vector<const Frame*> nodes_;  ///< every frame under the root, in the order the tree is read
  const std::vector<Edge>& edges_;
  const Spacing S;
  const int maxWidth_;
  bool lanes_ = true;                ///< whether the router run needs lanes in the gaps (DS-443)
  std::vector<std::size_t> number_;  ///< each frame's number in the order the graph is read
  std::vector<int> nodeOf_;          ///< the frame laid out under each number, or -1
  std::size_t count_ = 0;            ///< how many frames the graph numbers
  std::vector<Rect> rel_;
  std::vector<int> parent_;
  std::vector<std::vector<int>> children_;
  std::vector<int> roots_;
  std::map<size_t, std::vector<Waypoint>> ways_;
  std::map<size_t, int> wayFrame_;
  std::map<int, Band> bands_;  ///< by node, relative to its frame's content origin
  std::map<int, int> bandFrame_;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_FRAMER_HPP
