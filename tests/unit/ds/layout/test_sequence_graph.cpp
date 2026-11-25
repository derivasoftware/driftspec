#include <gtest/gtest.h>

#include "ds/layout/SequenceGraph.hpp"

namespace {

/// A sequence to place (DS-464): empty until its lanes, messages and frames are given.
TEST(TestSequenceGraph, AGraphHoldsItsLanesMessagesAndFrames) {
  ds::layout::SequenceGraph g;
  EXPECT_TRUE(g.lanes.empty());
  g.lanes = {{4, false}, {6, false}};
  g.messages = {{0, 1, 3, 0}};
  EXPECT_EQ(g.lanes.size(), 2u);
  EXPECT_EQ(g.messages[0].to, 1u);
  EXPECT_TRUE(g.blocks.empty());
}

}  // namespace
