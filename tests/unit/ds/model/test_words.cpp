#include <gtest/gtest.h>

#include "ds/model/Words.hpp"

namespace {

using Words = std::vector<std::string>;

/// A list written in a field, taken apart and put back (DS-185): one place for every module.

TEST(TestSplit, EachItemWithoutTheSpacesAroundItAndNoEmptyOne) {
  EXPECT_EQ(ds::model::split("a, b,, c ", ','), (Words{"a", "b", "c"}));
  EXPECT_EQ(ds::model::split("one", ','), (Words{"one"}));
  EXPECT_TRUE(ds::model::split("", ',').empty());
  EXPECT_EQ(ds::model::split("x\ny", '\n'), (Words{"x", "y"}));
}

TEST(TestJoin, TheItemsWithTheSeparatorBetweenThem) {
  EXPECT_EQ(ds::model::join({"a", "b", "c"}, ", "), "a, b, c");
  EXPECT_EQ(ds::model::join({"a"}, ", "), "a");
  EXPECT_EQ(ds::model::join({}, ", "), "");
}

}  // namespace
