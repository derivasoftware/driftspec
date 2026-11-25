#ifndef DRIFTSPEC_LAYOUT_WAYPOINT_HPP
#define DRIFTSPEC_LAYOUT_WAYPOINT_HPP

namespace ds {
namespace layout {

/// A waypoint an edge passes through between two layers (DS-392): the x of a dummy
/// node and the vertical band of its layer.
struct Waypoint {
  int x = 0;
  int top = 0;
  int bottom = 0;
  int gapBelow = 0;  ///< the height of the gap under its layer: where the next jog lives
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_WAYPOINT_HPP
