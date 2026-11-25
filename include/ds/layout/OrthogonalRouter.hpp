#ifndef DRIFTSPEC_LAYOUT_ORTHOGONALROUTER_HPP
#define DRIFTSPEC_LAYOUT_ORTHOGONALROUTER_HPP

#include "ds/layout/IRouter.hpp"
#include "ds/layout/Spacing.hpp"

namespace ds {
namespace layout {

/// Routes orthogonal through the layers (DS-185, DS-392): anchors spread along the sides, a long edge down the
/// channels of its waypoints, and every horizontal jog a lane of its own in the gap it turns in, the jogs of a gap in
/// the order they come from. A box's relation with itself goes out by its right side and back.
class OrthogonalRouter : public IRouter {
 public:
  explicit OrthogonalRouter(const Spacing& spacing);
  std::map<std::size_t, Route> route(const Routing& routing) const override;
  bool lanes() const override { return true; }

 private:
  Spacing spacing_;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_ORTHOGONALROUTER_HPP
