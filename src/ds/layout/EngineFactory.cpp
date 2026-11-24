#include "ds/layout/EngineFactory.hpp"

#include <utility>

#include "ds/layout/LayeredEngine.hpp"
#include "ds/layout/PackingEngine.hpp"
#include "ds/layout/RouterFactory.hpp"

namespace ds {
namespace layout {

std::unique_ptr<IEngine> EngineFactory::packing() { return std::make_unique<PackingEngine>(); }

std::unique_ptr<IEngine> EngineFactory::layered(const Spacing& spacing, std::unique_ptr<IRouter> router) {
  return std::make_unique<LayeredEngine>(spacing, router ? std::move(router) : RouterFactory::orthogonal(spacing));
}

}  // namespace layout
}  // namespace ds
