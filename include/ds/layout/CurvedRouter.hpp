#ifndef DRIFTSPEC_LAYOUT_CURVEDROUTER_HPP
#define DRIFTSPEC_LAYOUT_CURVEDROUTER_HPP

#include <cstddef>
#include <map>

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
  Spacing spacing_;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_CURVEDROUTER_HPP
