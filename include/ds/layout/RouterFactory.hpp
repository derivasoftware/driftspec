#ifndef DRIFTSPEC_LAYOUT_ROUTERFACTORY_HPP
#define DRIFTSPEC_LAYOUT_ROUTERFACTORY_HPP

#include <memory>
#include <string>

#include "ds/layout/IRouter.hpp"
#include "ds/layout/Spacing.hpp"

namespace ds {
namespace layout {

/// The one place that names the routers of the layered engine (DS-392).
class RouterFactory {
 public:
  /// The router that runs edges orthogonally through the layers' lanes.
  static std::unique_ptr<IRouter> orthogonal(const Spacing& spacing);
  /// The router that runs edges as smooth curves through the layering's waypoints, a box's relation with itself a loop
  /// beside it, each label on its curve clear of the boxes (DS-443): a state diagram's.
  static std::unique_ptr<IRouter> curved(const Spacing& spacing);
  /// The router a diagram of `kind` is drawn with (DS-443): a state diagram's curved, any other's orthogonal.
  static std::unique_ptr<IRouter> of(const std::string& kind, const Spacing& spacing);
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_ROUTERFACTORY_HPP
