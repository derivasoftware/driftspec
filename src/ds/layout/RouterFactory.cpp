#include "ds/layout/RouterFactory.hpp"

#include "ds/layout/OrthogonalRouter.hpp"

namespace ds {
namespace layout {

std::unique_ptr<IRouter> RouterFactory::orthogonal(const Spacing& spacing) { return std::make_unique<OrthogonalRouter>(spacing); }

}  // namespace layout
}  // namespace ds
