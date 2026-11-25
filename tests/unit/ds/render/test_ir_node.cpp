#include <gtest/gtest.h>

#include "ds/render/Ir.hpp"

namespace {

/// The badge a node carries is the same letter in the SVG and in the text drawing (DS-118).
TEST(TestIrNode, EachClassifierHasOneBadgeLetterInEveryDrawing) {
  EXPECT_EQ(ds::render::IrNode::badgeOf("function", false), "f");
  EXPECT_EQ(ds::render::IrNode::badgeOf("interface", true), "I");
  EXPECT_EQ(ds::render::IrNode::badgeOf("constraint", false), "K");
  EXPECT_EQ(ds::render::IrNode::badgeOf("", true), "A");  // an abstract class names no classifier
  EXPECT_EQ(ds::render::IrNode::badgeOf("", false), "C");
  EXPECT_EQ(ds::render::IrNode::badgeOf("usecase", false), "U");  // one the table does not name: its first letter
}

TEST(TestIrNode, ASectionHoldsMembersOrWhatAClosedBoxOffers) {
  EXPECT_TRUE(ds::render::IrNode::holdsMembers("attributes"));
  EXPECT_TRUE(ds::render::IrNode::holdsMembers("methods"));
  for (const char* offered : {"package", "namespace", "interface", "class", "dataclass", "enum", "function"})
    EXPECT_FALSE(ds::render::IrNode::holdsMembers(offered)) << offered;  // a compartment of a closed box: names, all of that kind
}

}  // namespace
