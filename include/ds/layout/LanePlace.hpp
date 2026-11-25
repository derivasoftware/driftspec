#ifndef DRIFTSPEC_LAYOUT_LANEPLACE_HPP
#define DRIFTSPEC_LAYOUT_LANEPLACE_HPP

namespace ds {
namespace layout {

/// Where a lane lands (DS-464): its centre and the width of its head, in the sink's units.
struct LanePlace {
  int centre = 0;
  int head = 0;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_LANEPLACE_HPP
