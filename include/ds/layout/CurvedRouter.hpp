#ifndef DRIFTSPEC_LAYOUT_CURVEDROUTER_HPP
#define DRIFTSPEC_LAYOUT_CURVEDROUTER_HPP

#include <cstddef>
#include <map>
#include <utility>
#include <vector>

#include "ds/layout/IRouter.hpp"
#include "ds/layout/Route.hpp"
#include "ds/layout/Routing.hpp"
#include "ds/layout/Spacing.hpp"

namespace ds {
namespace layout {

/// Routes as smooth curves (DS-443), a state diagram's: each edge a curve through the waypoints the layering left,
/// leaving and entering each box where its border faces the next point; edges between the same two boxes bowed
/// apart; a box's relation with itself a loop beside its right side, one outside the other; each label on its curve,
/// halfway along it, moved off any box it would fall in. It needs no lanes, so the gaps keep the spacing's height.
/// Deterministic and in whole units, as the orthogonal router.
class CurvedRouter : public IRouter {
 public:
  explicit CurvedRouter(const Spacing& spacing);
  std::map<std::size_t, Route> route(const Routing& routing) const override;
  bool lanes() const override { return false; }

 private:
  using P = std::pair<double, double>;
  /// How far a label's text rises above the point it is set at: a line of edge label, its baseline a little above the
  /// point (DS-454). The text runs from the point to a line above it.
  static constexpr int kLabelRise = 16;
  // the geometry its routes are made of (DS-477: members, no hidden namespace)
  static P centreOf(const Rect& r);
  /// Where the ray from the centre of `r` towards `to` leaves `r`, in whole units.
  static P borderTowards(const Rect& r, const P& to);
  /// Whether `outer` holds all of `inner` and is bigger: a frame around it.
  static bool encloses(const Rect& outer, const Rect& inner);
  /// Whether the text of a label set at `p`, a line tall above it, would cross `r` or come within `margin` of it.
  static bool crosses(const Rect& r, const P& p, int margin);
  /// The point halfway along a polyline.
  static P halfway(const std::vector<P>& points);

  Spacing spacing_;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_CURVEDROUTER_HPP
