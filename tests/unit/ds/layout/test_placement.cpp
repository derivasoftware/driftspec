#include <gtest/gtest.h>

#include "ds/layout/Placement.hpp"

namespace {

/// Where an engine placed the boxes of a graph (DS-392): a box per frame in the order the graph is read, a route per edge index, and the
/// widths a later placement may follow.
TEST(TestPlacement, APlacementSaysWhereEachBoxLandsAndHowEachEdgeRuns) {
  ds::layout::Placement placed;
  EXPECT_TRUE(placed.boxes.empty() && placed.routes.empty() && placed.widths.empty());
  placed.boxes.push_back({10, 20, 30, 40});
  placed.routes[0].points = {{0, 0}, {0, 10}};
  placed.widths["m"] = 200;
  EXPECT_DOUBLE_EQ(placed.boxes.at(0).h, 40);
  EXPECT_EQ(placed.routes.at(0).points.size(), 2u);
  EXPECT_DOUBLE_EQ(placed.widths.at("m"), 200);
}

}  // namespace
