#include "ds/layout/EngineFactory.hpp"

#include <utility>

#include "ds/layout/GridEngine.hpp"
#include "ds/layout/LayeredEngine.hpp"
#include "ds/layout/PackingEngine.hpp"
#include "ds/layout/RouterFactory.hpp"
#include "ds/layout/StackedEngine.hpp"

namespace ds {
namespace layout {

std::unique_ptr<IEngine> EngineFactory::packing() { return std::make_unique<PackingEngine>(); }

std::unique_ptr<IEngine> EngineFactory::layered(const Spacing& spacing, std::unique_ptr<IRouter> router) {
  return std::make_unique<LayeredEngine>(spacing, router ? std::move(router) : RouterFactory::orthogonal(spacing));
}

std::unique_ptr<ISequenceEngine> EngineFactory::sequence(const SequenceSpacing& spacing) {
  return std::make_unique<StackedEngine>(spacing);
}

std::unique_ptr<ITableEngine> EngineFactory::table(const TableSpacing& spacing) { return std::make_unique<GridEngine>(spacing); }

}  // namespace layout
}  // namespace ds
