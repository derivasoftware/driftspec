#include <gtest/gtest.h>

#include "ds/render/SequenceRow.hpp"

namespace {

/// Where one message of a sequence lands (DS-400): nothing until it is composed.
TEST(TestSequenceRow, ARowStartsAtTheTop) {
  const ds::render::SequenceRow r{1, 2, 4};
  EXPECT_EQ(r.top, 1);
  EXPECT_EQ(r.line, 2);
  EXPECT_EQ(r.bottom, 4);
  EXPECT_EQ(ds::render::SequenceRow{}.bottom, 0);
}

}  // namespace
