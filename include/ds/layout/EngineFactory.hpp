#ifndef DRIFTSPEC_LAYOUT_ENGINEFACTORY_HPP
#define DRIFTSPEC_LAYOUT_ENGINEFACTORY_HPP

#include <memory>

#include "ds/layout/IEngine.hpp"
#include "ds/layout/IRouter.hpp"
#include "ds/layout/ISequenceEngine.hpp"
#include "ds/layout/ITableEngine.hpp"
#include "ds/layout/SequenceSpacing.hpp"
#include "ds/layout/Spacing.hpp"
#include "ds/layout/TableSpacing.hpp"

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
  /// The engine that places a sequence's lanes, rows and frames with `spacing`, the sink's (DS-464): the stacked one.
  static std::unique_ptr<ISequenceEngine> sequence(const SequenceSpacing& spacing);
  /// The engine that places a table's columns, rows and lines with `spacing`, the sink's (DS-464): the grid one.
  static std::unique_ptr<ITableEngine> table(const TableSpacing& spacing);
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_ENGINEFACTORY_HPP
