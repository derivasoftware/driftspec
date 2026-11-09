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

}  // namespace
