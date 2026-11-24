#ifndef DRIFTSPEC_LAYOUT_ROUTE_HPP
#define DRIFTSPEC_LAYOUT_ROUTE_HPP

#include <utility>
#include <vector>

namespace ds {
namespace layout {

/// How an edge runs (DS-392): the corners of its polyline, from where it leaves to where it arrives.
struct Route {
  std::vector<std::pair<double, double>> points;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_ROUTE_HPP
