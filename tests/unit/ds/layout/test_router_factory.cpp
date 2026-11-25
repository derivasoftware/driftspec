#include <gtest/gtest.h>

#include "ds/layout/OrthogonalRouter.hpp"
#include "ds/layout/RouterFactory.hpp"
#include "ds/layout/Spacing.hpp"

namespace {

/// The one place that names the routers (DS-392).
TEST(TestRouterFactory, TheOrthogonalRouterIsHandedOutAsARouter) {
  const auto router = ds::layout::RouterFactory::orthogonal(ds::layout::Spacing{});
  ASSERT_NE(router, nullptr);
  EXPECT_NE(dynamic_cast<const ds::layout::OrthogonalRouter*>(router.get()), nullptr);
}

}  // namespace
