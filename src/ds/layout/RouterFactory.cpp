#include "ds/layout/RouterFactory.hpp"

#include "ds/layout/CurvedRouter.hpp"
#include "ds/layout/OrthogonalRouter.hpp"

namespace ds {
namespace layout {

std::unique_ptr<IRouter> RouterFactory::orthogonal(const Spacing& spacing) { return std::make_unique<OrthogonalRouter>(spacing); }

std::unique_ptr<IRouter> RouterFactory::curved(const Spacing& spacing) { return std::make_unique<CurvedRouter>(spacing); }

std::unique_ptr<IRouter> RouterFactory::of(const std::string& kind, const Spacing& spacing) {
  return kind == "state" ? curved(spacing) : orthogonal(spacing);
}

}  // namespace layout
}  // namespace ds
