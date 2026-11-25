#ifndef DRIFTSPEC_LAYOUT_POSITIONER_HPP
#define DRIFTSPEC_LAYOUT_POSITIONER_HPP

#include <vector>

#include "ds/layout/Component.hpp"
#include "ds/layout/Layering.hpp"
#include "ds/layout/Rect.hpp"
#include "ds/layout/Spacing.hpp"

namespace ds {
namespace layout {

/// Places a layered component (DS-370, DS-392): each layer as tall as its tallest node with lanes in the gap under it,
/// every node pulled under its neighbours without overlapping, a layer wider than the room given wrapped onto further
/// lines, and the gaps left only the lanes their turning edges need.
class Positioner {
 public:
  /// `lanes`: whether the gaps hold a lane for every edge that turns in them, as an orthogonal router needs (DS-443);
  /// when not, every gap keeps the spacing's height.
  Positioner(const Spacing& spacing, bool lanes);
  /// Works out where each node of `layering` goes, `available` wide at most (0 is unbounded).
  void place(Layering& layering, int available) const;
  /// Writes where the real nodes went into `rel`, the component's size, its bands and the waypoints of its long edges.
  void apply(Layering& layering, Component& c, std::vector<Rect>& rel) const;

 private:
  Spacing spacing_;
  bool lanes_;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_POSITIONER_HPP
