#ifndef DRIFTSPEC_LAYOUT_ENGINEFACTORY_HPP
#define DRIFTSPEC_LAYOUT_ENGINEFACTORY_HPP

#include <memory>

#include "ds/layout/IEngine.hpp"
#include "ds/layout/Spacing.hpp"

namespace ds {
namespace layout {

/// The one place that names the layout's engines (DS-392); whoever places is handed an IEngine.
class EngineFactory {
 public:
  /// The engine that packs nested boxes in shelves, fixed places and trees: the scene's.
  static std::unique_ptr<IEngine> packing();
  /// The engine that layers boxes along their edges and routes them orthogonally, leaving `spacing`: render's.
  static std::unique_ptr<IEngine> layered(const Spacing& spacing);
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_ENGINEFACTORY_HPP
