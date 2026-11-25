#ifndef DRIFTSPEC_LAYOUT_EDGE_HPP
#define DRIFTSPEC_LAYOUT_EDGE_HPP

#include <cstddef>

namespace ds {
namespace layout {

/// An edge between two boxes (DS-392), by their numbers in the order the graph is read, for an engine that layers
/// the boxes along it and routes it. An engine reads geometry only: whether the edge points up, its from box below its
/// to box. An end that is no box of the graph is `none`, and the edge is left unplaced.
struct Edge {
  static constexpr std::size_t none = static_cast<std::size_t>(-1);
  std::size_t from = none;
  std::size_t to = none;
  bool upward = false;  ///< the to box sits above the from box: a base above what inherits from it
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_EDGE_HPP
