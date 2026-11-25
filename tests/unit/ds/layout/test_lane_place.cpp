#include <gtest/gtest.h>

#include "ds/layout/LanePlace.hpp"

namespace {

/// Where a lane lands (DS-464): its centre and its head's width.
TEST(TestLanePlace, ALaneLandsAtItsCentreWithItsHead) {
  const ds::layout::LanePlace p{60, 40};
  EXPECT_EQ(p.centre, 60);
  EXPECT_EQ(p.head, 40);
  EXPECT_EQ(ds::layout::LanePlace{}.centre, 0);
}

}  // namespace
