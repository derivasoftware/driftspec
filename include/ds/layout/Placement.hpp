#ifndef DRIFTSPEC_LAYOUT_PLACEMENT_HPP
#define DRIFTSPEC_LAYOUT_PLACEMENT_HPP

#include <cstddef>
#include <map>
#include <string>

#include "ds/layout/Box.hpp"
#include "ds/layout/Route.hpp"

namespace ds {
namespace layout {

/// Where an engine placed the boxes of a graph (DS-392), and how its edges run.
struct Placement {
  std::map<std::string, Box> boxes;      ///< where every box lands, by its identity, from the root's top left corner
  std::map<std::size_t, Route> routes;   ///< how each edge runs, by its index in the graph; one not placed has none
  std::map<std::string, double> widths;  ///< the width each shelved box's shelves came out at, for a placement to follow
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_PLACEMENT_HPP
