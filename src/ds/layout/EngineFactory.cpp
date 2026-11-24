#include "ds/layout/EngineFactory.hpp"

#include "ds/layout/LayeredEngine.hpp"
#include "ds/layout/PackingEngine.hpp"

namespace ds {
namespace layout {

std::unique_ptr<IEngine> EngineFactory::packing() { return std::make_unique<PackingEngine>(); }

std::unique_ptr<IEngine> EngineFactory::layered(const Spacing& spacing) { return std::make_unique<LayeredEngine>(spacing); }

}  // namespace layout
}  // namespace ds
