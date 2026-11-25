#include <gtest/gtest.h>

#include "ds/layout/CurvedRouter.hpp"
#include "ds/layout/OrthogonalRouter.hpp"
#include "ds/layout/RouterFactory.hpp"
#include "ds/layout/Spacing.hpp"

namespace {

/// The one place that names the routers (DS-392).
TEST(TestRouterFactory, TheOrthogonalRouterIsHandedOutAsARouter) {
  const auto router = ds::layout::RouterFactory::orthogonal(ds::layout::Spacing{});
  ASSERT_NE(router, nullptr);
  EXPECT_NE(dynamic_cast<const ds::layout::OrthogonalRouter*>(router.get()), nullptr);
  EXPECT_TRUE(router->lanes());
}

/// The curved router, a state diagram's, is handed out as a router too, one that needs no lanes (DS-443).
TEST(TestRouterFactory, TheCurvedRouterIsHandedOutAsARouterThatNeedsNoLanes) {
  const auto router = ds::layout::RouterFactory::curved(ds::layout::Spacing{});
  ASSERT_NE(router, nullptr);
  EXPECT_NE(dynamic_cast<const ds::layout::CurvedRouter*>(router.get()), nullptr);
  EXPECT_FALSE(router->lanes());
}

/// A diagram's kind picks its router (DS-443): a state diagram's curved, any other's orthogonal.
TEST(TestRouterFactory, AStateDiagramIsRoutedCurvedAndAnyOtherOrthogonal) {
  const ds::layout::Spacing s;
  EXPECT_NE(dynamic_cast<const ds::layout::CurvedRouter*>(ds::layout::RouterFactory::of("state", s).get()), nullptr);
  EXPECT_NE(dynamic_cast<const ds::layout::OrthogonalRouter*>(ds::layout::RouterFactory::of("class", s).get()), nullptr);
  EXPECT_NE(dynamic_cast<const ds::layout::OrthogonalRouter*>(ds::layout::RouterFactory::of("", s).get()), nullptr);
}

}  // namespace
