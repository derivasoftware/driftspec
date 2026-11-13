#include <gtest/gtest.h>

#include "../../../../src/ds/check/Shared.hpp"

namespace {

/// Which primitives convert into each other without saying so, for the check of a message's types.

TEST(TestPrimitiveFamily, NumbersTextTruthAndNothingAreFourFamilies) {
  using ds::check::primitiveFamily;
  EXPECT_EQ(primitiveFamily("int"), primitiveFamily("double"));
  EXPECT_EQ(primitiveFamily("size_t"), primitiveFamily("char"));
  EXPECT_EQ(primitiveFamily("string"), primitiveFamily("path"));
  EXPECT_NE(primitiveFamily("int"), primitiveFamily("string"));
  EXPECT_NE(primitiveFamily("bool"), primitiveFamily("int"));
  EXPECT_EQ(primitiveFamily("none"), primitiveFamily("void"));
  EXPECT_EQ(primitiveFamily("Cart"), 0);  // no primitive: no family
}

}  // namespace
