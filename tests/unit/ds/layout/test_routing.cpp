#include <gtest/gtest.h>

#include "ds/layout/Routing.hpp"

namespace {

/// A router is given nothing until told (DS-392).
TEST(TestRouting, ItIsEmptyUntilItIsTold) {
  const ds::layout::Routing v;
  EXPECT_TRUE(v.boxes.empty() && v.ends.empty() && v.ways.empty() && v.bands.empty());
}

}  // namespace
