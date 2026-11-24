#include <gtest/gtest.h>

#include <limits>
#include <vector>

#include "ds/layout/Component.hpp"
#include "ds/layout/Layerer.hpp"
#include "ds/layout/Rect.hpp"
#include "ds/layout/Spacing.hpp"

namespace {

using ds::layout::Component;
using ds::layout::Layerer;
using ds::layout::Rect;
using ds::layout::Spacing;

/// Assigns a component's layers (DS-370): longest paths from the sources, a dummy where a long edge crosses a layer.
TEST(TestLayerer, EachNodeGoesBelowItsLongestPathAndALongEdgeGetsADummy) {
  constexpr auto none = std::numeric_limits<std::size_t>::max();
  Component c;
  c.nodes = {0, 1, 2};
  c.edges = {{0, 1, none}, {1, 2, none}, {0, 2, none}};  // 0 above 1 above 2, and 0 above 2 across a layer
  const std::vector<Rect> rel = {{0, 0, 100, 40}, {0, 0, 80, 30}, {0, 0, 60, 20}};
  const auto L = Layerer(Spacing{}).of(c, rel);
  EXPECT_EQ(L.real, 3);
  EXPECT_EQ(L.depth, 2);
  EXPECT_EQ(L.layer[0], 0);
  EXPECT_EQ(L.layer[1], 1);
  EXPECT_EQ(L.layer[2], 2);
  EXPECT_EQ(L.total, 4);  // one dummy, for the edge from 0 to 2
  EXPECT_EQ(L.widths[3], Spacing{}.channel);
  EXPECT_EQ(L.chain.at({0, 2}).size(), 1u);
  EXPECT_EQ(L.rows[1].size(), 2u);  // node 1 and the dummy share the middle layer
}

}  // namespace
