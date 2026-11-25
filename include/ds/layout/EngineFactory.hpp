#ifndef DRIFTSPEC_LAYOUT_ENGINEFACTORY_HPP
#define DRIFTSPEC_LAYOUT_ENGINEFACTORY_HPP

#include <memory>

#include "ds/layout/IEngine.hpp"
#include "ds/layout/IRouter.hpp"
#include "ds/layout/Spacing.hpp"

namespace ds {
namespace layout {

/// The one place that names the layout's engines (DS-392); whoever places is handed an IEngine.
class EngineFactory {
 public:
  /// The engine that packs nested boxes in shelves, fixed places and trees: the scene's.
  static std::unique_ptr<IEngine> packing();
  /// The engine that layers boxes along their edges, leaving `spacing`, its edges run by `router`: orthogonally when
  /// it is handed none. Render's.
  static std::unique_ptr<IEngine> layered(const Spacing& spacing, std::unique_ptr<IRouter> router = nullptr);
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_ENGINEFACTORY_HPP
