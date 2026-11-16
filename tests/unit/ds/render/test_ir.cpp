#include <gtest/gtest.h>

#include "ds/render/Ir.hpp"

namespace {

/// The badge a node carries is the same letter in the SVG and in the text drawing (DS-118).
TEST(TestBadgeOf, EachClassifierHasOneLetterInEveryDrawing) {
  EXPECT_EQ(ds::render::badgeOf("function", false), "f");
  EXPECT_EQ(ds::render::badgeOf("interface", true), "I");
  EXPECT_EQ(ds::render::badgeOf("constraint", false), "K");
  EXPECT_EQ(ds::render::badgeOf("", true), "A");  // an abstract class names no classifier
  EXPECT_EQ(ds::render::badgeOf("", false), "C");
  EXPECT_EQ(ds::render::badgeOf("usecase", false), "U");  // one the table does not name: its first letter
}

TEST(TestHoldsMembers, ASectionHoldsMembersOrWhatAClosedBoxOffers) {
  EXPECT_TRUE(ds::render::holdsMembers("attributes"));
  EXPECT_TRUE(ds::render::holdsMembers("methods"));
  for (const char* offered : {"package", "namespace", "interface", "class", "dataclass", "enum", "function"})
    EXPECT_FALSE(ds::render::holdsMembers(offered)) << offered;  // a compartment of a closed box: names, all of that kind
}

}  // namespace
