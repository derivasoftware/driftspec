#include <gtest/gtest.h>

#include "ds/layout/Band.hpp"

namespace {

/// A band is a gap under a layer (DS-392).
TEST(TestBand, ItIsEmptyUntilItIsTold) {
  const ds::layout::Band v;
  EXPECT_EQ(v.top + v.bottom, 0);
}

}  // namespace
