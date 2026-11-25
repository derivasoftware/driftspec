#include <gtest/gtest.h>

#include <vector>

#include "ds/layout/Layering.hpp"
#include "ds/layout/Orderer.hpp"

namespace {

/// Orders each layer to cross less (DS-370): two edges that cross are uncrossed.
TEST(TestOrderer, TwoEdgesThatCrossAreUncrossed) {
  ds::layout::Layering L;
  L.total = 4;
  L.depth = 1;
  L.rows = {{0, 1}, {2, 3}};
  L.succA = {{3}, {2}, {}, {}};  // 0 above 3, 1 above 2: as written they cross
  L.predA = {{}, {}, {1}, {0}};
  ds::layout::Orderer::order(L);
  EXPECT_EQ(L.rows[0], (std::vector<int>{0, 1}));
  EXPECT_EQ(L.rows[1], (std::vector<int>{3, 2}));
}

}  // namespace
