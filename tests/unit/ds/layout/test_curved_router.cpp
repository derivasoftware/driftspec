#include <gtest/gtest.h>

#include "ds/layout/CurvedRouter.hpp"
#include "ds/layout/Routing.hpp"
#include "ds/layout/Spacing.hpp"

namespace {

/// Routes as smooth curves (DS-443): an edge is a curve from the border of one box to the border of the other, two
/// edges between the same boxes bow apart, a loop goes out beside the right side and back, each label lies on its
/// curve and in no box, and the gaps need no lanes.
TEST(TestCurvedRouter, AnEdgeCurvesBetweenTheBordersTwoBetweenTheSameBoxesBowApartAndALoopSitsBeside) {
  ds::layout::Routing r;
  r.boxes = {{0, 0, 100, 40}, {0, 100, 100, 40}};
  r.ends = {{0, 1}, {1, 0}, {0, 0}};
  const ds::layout::CurvedRouter router{ds::layout::Spacing{}};
  EXPECT_FALSE(router.lanes());
  const auto routes = router.route(r);
  ASSERT_EQ(routes.size(), 3u);
  for (const auto& route : routes) EXPECT_EQ(route.second.shape, ds::layout::Route::Shape::curve);
  const auto& down = routes.at(0).points;
  const auto& up = routes.at(1).points;
  ASSERT_EQ(down.size(), 3u);                 // bowed through one point off the straight line
  EXPECT_DOUBLE_EQ(down.front().second, 40);  // out of the bottom of the upper box
  EXPECT_DOUBLE_EQ(down.back().second, 100);  // into the top of the lower one
  EXPECT_DOUBLE_EQ(up.front().second, 100);   // the way back leaves the top of the lower box
  EXPECT_NE(down[1].first, up[1].first);      // and bows to the other side
  const auto& loop = routes.at(2).points;
  ASSERT_EQ(loop.size(), 4u);
  EXPECT_DOUBLE_EQ(loop.front().first, 100);  // out of the right side
  EXPECT_GT(loop[1].first, 100);
  EXPECT_DOUBLE_EQ(loop.back().first, 100);  // and back into it
  ASSERT_TRUE(routes.at(2).label.has_value());
  EXPECT_GT(routes.at(2).label->first, loop[1].first);  // the label beside the loop
  for (const auto& route : routes) {
    ASSERT_TRUE(route.second.label.has_value());
    const auto [x, y] = *route.second.label;
    for (const auto& b : r.boxes) EXPECT_FALSE(x > b.x && x < b.x + b.w && y > b.y && y < b.y + b.h) << route.first;
  }
}

/// A long edge follows the waypoints the layering left, upper to lower, and an edge pointing up takes them in reverse.
TEST(TestCurvedRouter, ALongEdgePassesThroughItsWaypointsInTheOrderItRuns) {
  ds::layout::Routing r;
  r.boxes = {{0, 0, 100, 40}, {200, 300, 100, 40}};
  r.ends = {{0, 1}, {1, 0}};
  r.ways[0] = {{150, 100, 120, 56}, {150, 200, 220, 56}};
  r.ways[1] = {{20, 100, 120, 56}, {20, 200, 220, 56}};
  const auto routes = ds::layout::CurvedRouter(ds::layout::Spacing{}).route(r);
  const auto& down = routes.at(0).points;
  ASSERT_EQ(down.size(), 4u);
  EXPECT_DOUBLE_EQ(down[1].second, 110);
  EXPECT_DOUBLE_EQ(down[2].second, 210);
  const auto& up = routes.at(1).points;
  ASSERT_EQ(up.size(), 4u);
  EXPECT_DOUBLE_EQ(up[1].second, 210);  // from the lower box, the lower waypoint first
  EXPECT_DOUBLE_EQ(up[2].second, 110);
}

/// A label's whole text clears the boxes, not only the point it is set at (DS-454): where halfway along falls just
/// under a box, the text, which rises a line above its point, would sit on the box's border, so it is moved off.
TEST(TestCurvedRouter, ALabelsTextClearsTheBoxesNotOnlyItsPoint) {
  ds::layout::Routing r;
  // the edge runs from the first box down to the second; halfway, at y 120, lies 6 under a third box
  r.boxes = {{0, 0, 100, 40}, {0, 200, 100, 40}, {20, 60, 60, 54}};
  r.ends = {{0, 1}};
  const auto routes = ds::layout::CurvedRouter(ds::layout::Spacing{}).route(r);
  ASSERT_TRUE(routes.at(0).label.has_value());
  const auto [x, y] = *routes.at(0).label;
  for (const auto& b : r.boxes) {
    const bool across = x > b.x && x < b.x + b.w;
    const bool within = y - 16 < b.y + b.h && y > b.y;
    EXPECT_FALSE(across && within) << "the label at " << x << "," << y << " crosses the box at " << b.y;
  }
}

}  // namespace
