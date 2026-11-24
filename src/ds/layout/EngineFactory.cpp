#include "ds/layout/EngineFactory.hpp"

#include "ds/layout/PackingEngine.hpp"

namespace ds {
namespace layout {

std::unique_ptr<IEngine> EngineFactory::packing() { return std::make_unique<PackingEngine>(); }

}  // namespace layout
}  // namespace ds
