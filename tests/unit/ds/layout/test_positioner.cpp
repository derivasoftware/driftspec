#include <gtest/gtest.h>

#include <limits>
#include <vector>

#include "ds/layout/Component.hpp"
#include "ds/layout/Layerer.hpp"
#include "ds/layout/Orderer.hpp"
#include "ds/layout/Positioner.hpp"
#include "ds/layout/Rect.hpp"
#include "ds/layout/Spacing.hpp"

namespace {

/// Places a layered component (DS-370): every layer below the one above it, nothing in a layer overlapping, the
/// component as big as what it holds.
TEST(TestPositioner, EachLayerSitsBelowTheOneAboveAndNothingInALayerOverlaps) {
  constexpr auto none = std::numeric_limits<std::size_t>::max();
  ds::layout::Component c;
  c.nodes = {0, 1, 2};
  c.edges = {{0, 1, none}, {0, 2, none}};
  std::vector<ds::layout::Rect> rel = {{0, 0, 100, 40}, {0, 0, 80, 30}, {0, 0, 60, 20}};
  const ds::layout::Spacing s;
  auto L = ds::layout::Layerer(s).of(c, rel);
  ds::layout::Orderer::order(L);
  const ds::layout::Positioner positioner(s, true);
  positioner.place(L, 0);
  positioner.apply(L, c, rel);
  EXPECT_GT(rel[1].y, rel[0].y + rel[0].h);
  EXPECT_GT(rel[2].y, rel[0].y + rel[0].h);
  const auto& left = rel[1].x < rel[2].x ? rel[1] : rel[2];
  const auto& right = rel[1].x < rel[2].x ? rel[2] : rel[1];
  EXPECT_LE(left.x + left.w + s.gapX, right.x);
  EXPECT_GE(c.w, right.x + right.w);
  EXPECT_GE(c.h, rel[1].y + rel[1].h);
}

/// Without lanes, as a curved router asks, every gap keeps the spacing's height (DS-443); with them, a gap an edge
/// turns in is taller.
TEST(TestPositioner, WithoutLanesEveryGapKeepsTheSpacingsHeight) {
  constexpr auto none = std::numeric_limits<std::size_t>::max();
  const ds::layout::Spacing s;
  const auto lowerTop = [&](bool lanes) {
    ds::layout::Component c;
    c.nodes = {0, 1, 2, 3, 4};  // four edges turn under the first layer: with lanes, more than gapY holds them
    c.edges = {{0, 1, none}, {0, 2, none}, {0, 3, none}, {0, 4, none}};
    std::vector<ds::layout::Rect> rel = {{0, 0, 100, 40}, {0, 0, 80, 30}, {0, 0, 60, 20}, {0, 0, 60, 20}, {0, 0, 60, 20}};
    auto L = ds::layout::Layerer(s).of(c, rel);
    ds::layout::Orderer::order(L);
    const ds::layout::Positioner positioner(s, lanes);
    positioner.place(L, 0);
    positioner.apply(L, c, rel);
    return rel[1].y - (rel[0].y + rel[0].h);
  };
  EXPECT_EQ(lowerTop(false), s.gapY);
  EXPECT_GT(lowerTop(true), s.gapY);
}

}  // namespace
