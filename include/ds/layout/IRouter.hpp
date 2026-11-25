#ifndef DRIFTSPEC_LAYOUT_IROUTER_HPP
#define DRIFTSPEC_LAYOUT_IROUTER_HPP

#include <cstddef>
#include <map>
#include <vector>

#include "ds/layout/Route.hpp"
#include "ds/layout/Routing.hpp"

namespace ds {
namespace layout {

/// How the edges of a layered drawing run (DS-392): one way of routing, given where the boxes landed and what the
/// layering leaves. The layered engine is handed one; RouterFactory names them.
class IRouter {
 public:
  virtual ~IRouter() = default;
  /// The route of each edge, by edge; an edge without a route has none. Its points are whole units.
  virtual std::map<std::size_t, Route> route(const Routing& routing) const = 0;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_IROUTER_HPP
