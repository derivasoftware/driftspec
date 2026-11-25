#ifndef DRIFTSPEC_LAYOUT_ROUTE_HPP
#define DRIFTSPEC_LAYOUT_ROUTE_HPP

#include <optional>
#include <utility>
#include <vector>

namespace ds {
namespace layout {

/// How an edge runs (DS-392): the corners of its polyline, from where it leaves to where it arrives; or, for a
/// curve (DS-443), the points a smooth curve passes through, which read as a polyline are its rough course.
struct Route {
  enum class Shape { polyline, curve };
  std::vector<std::pair<double, double>> points;
  Shape shape = Shape::polyline;
  std::optional<std::pair<double, double>> label;  ///< where its label goes, when the router chose: on it, clear of the boxes
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_ROUTE_HPP
