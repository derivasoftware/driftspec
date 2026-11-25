#ifndef DRIFTSPEC_LAYOUT_ROUTING_HPP
#define DRIFTSPEC_LAYOUT_ROUTING_HPP

#include <cstddef>
#include <map>
#include <utility>
#include <vector>

#include "ds/layout/Band.hpp"
#include "ds/layout/Rect.hpp"
#include "ds/layout/Waypoint.hpp"

namespace ds {
namespace layout {

/// What a router is given (DS-392): where every box landed and what the layering leaves for the edges, all in whole
/// units: the points a long edge passes between the layers, and the gap under each box's layer. Nothing in it says
/// how a route is to be drawn.
struct Routing {
  std::vector<Rect> boxes;                            ///< by box, as the engine numbers them
  std::vector<std::pair<int, int>> ends;              ///< by edge: the boxes it joins, from and to, or -1
  std::map<std::size_t, std::vector<Waypoint>> ways;  ///< by edge: the points a long edge passes, upper to lower
  std::map<int, Band> bands;                          ///< by box: the gap under its layer
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_ROUTING_HPP
