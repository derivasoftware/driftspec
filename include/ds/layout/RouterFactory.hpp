#ifndef DRIFTSPEC_LAYOUT_ROUTERFACTORY_HPP
#define DRIFTSPEC_LAYOUT_ROUTERFACTORY_HPP

#include <memory>

#include "ds/layout/IRouter.hpp"
#include "ds/layout/Spacing.hpp"

namespace ds {
namespace layout {

/// The one place that names the routers of the layered engine (DS-392).
class RouterFactory {
 public:
  /// The router that runs edges orthogonally through the layers' lanes.
  static std::unique_ptr<IRouter> orthogonal(const Spacing& spacing);
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_ROUTERFACTORY_HPP
