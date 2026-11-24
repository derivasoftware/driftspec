#ifndef DRIFTSPEC_LAYOUT_EDGE_HPP
#define DRIFTSPEC_LAYOUT_EDGE_HPP

#include <string>

namespace ds {
namespace layout {

/// An edge between two boxes (DS-392), by their identities, for an engine that layers the boxes along it and routes it.
/// An engine reads geometry only: whether the edge points up, its from box below its to box.
struct Edge {
  std::string from;
  std::string to;
  bool upward = false;  ///< the to box sits above the from box: a base above what inherits from it
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_EDGE_HPP
