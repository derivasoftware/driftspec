#ifndef DRIFTSPEC_LAYOUT_COMPONENT_HPP
#define DRIFTSPEC_LAYOUT_COMPONENT_HPP

#include <cstddef>
#include <map>
#include <vector>

#include "ds/layout/Band.hpp"
#include "ds/layout/Projected.hpp"
#include "ds/layout/Waypoint.hpp"

namespace ds {
namespace layout {

/// The layered placement of one connected component (DS-392): nodes get x/y relative to the component.
struct Component {
  static constexpr int source = -1;  ///< the pseudo node above, for an edge that leaves the frame upwards
  static constexpr int sink = -2;    ///< the pseudo node below
  std::vector<int> nodes;            // kid indices; source and sink are pseudo nodes
  std::vector<Projected> edges;
  std::map<std::size_t, std::vector<Waypoint>> ways;  // by edge index, upper to lower
  std::map<int, Band> bands;                          // by kid: the gap under its layer
  int w = 0;
  int h = 0;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_COMPONENT_HPP
