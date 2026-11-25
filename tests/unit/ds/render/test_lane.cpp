#include <gtest/gtest.h>

#include "ds/render/Ir.hpp"
#include "ds/render/Lane.hpp"

namespace {

/// A lifeline of a sequence drawn in SVG (DS-371): the node it stands for, where its centre is and how wide its head.
TEST(TestLane, ALaneStandsForItsNodeAtTheCentreItIsGiven) {
  ds::render::IrNode node;
  node.id = "caller";
  const ds::render::Lane lane{&node, 40, 60};
  EXPECT_EQ(lane.node->id, "caller");
  EXPECT_EQ(lane.cx, 40);
  EXPECT_EQ(lane.headW, 60);
}

}  // namespace
