#include "ds/layout/LayeredEngine.hpp"

#include <utility>

#include "ds/layout/Framer.hpp"

namespace ds {
namespace layout {

LayeredEngine::LayeredEngine(const Spacing& spacing, std::unique_ptr<IRouter> router) : spacing_(spacing), router_(std::move(router)) {}

Placement LayeredEngine::place(const Graph& graph) const { return Framer(graph, spacing_).run(*router_); }

}  // namespace layout
}  // namespace ds
