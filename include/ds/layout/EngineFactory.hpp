#ifndef DRIFTSPEC_LAYOUT_ENGINEFACTORY_HPP
#define DRIFTSPEC_LAYOUT_ENGINEFACTORY_HPP

#include <memory>

#include "ds/layout/IEngine.hpp"

namespace ds {
namespace layout {

/// The one place that names the layout's engines (DS-392); whoever places is handed an IEngine.
class EngineFactory {
 public:
  /// The engine that packs nested boxes in shelves, fixed places and trees: the scene's.
  static std::unique_ptr<IEngine> packing();
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_ENGINEFACTORY_HPP
