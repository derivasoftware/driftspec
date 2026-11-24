#include <gtest/gtest.h>

#include "ds/layout/Component.hpp"

namespace {

/// The pseudo nodes are below zero and an empty component takes no room (DS-392).
TEST(TestComponent, ItIsEmptyUntilItIsTold) {
  const ds::layout::Component v;
  EXPECT_EQ(ds::layout::Component::source, -1);
  EXPECT_EQ(ds::layout::Component::sink, -2);
  EXPECT_TRUE(v.nodes.empty() && v.edges.empty());
  EXPECT_EQ(v.w + v.h, 0);
}

}  // namespace
