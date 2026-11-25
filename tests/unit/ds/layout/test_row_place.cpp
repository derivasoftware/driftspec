#include <gtest/gtest.h>

#include "ds/layout/RowPlace.hpp"

namespace {

/// Where a row lands (DS-464): its arrow's line between its top and its bottom.
TEST(TestRowPlace, ARowsLineIsBetweenItsTopAndItsBottom) {
  const ds::layout::RowPlace r{90, 110, 120};
  EXPECT_LT(r.top, r.line);
  EXPECT_LT(r.line, r.bottom);
}

}  // namespace
