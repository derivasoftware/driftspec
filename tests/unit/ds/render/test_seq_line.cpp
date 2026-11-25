#include <gtest/gtest.h>

#include "ds/render/SeqLine.hpp"

namespace {

/// A lifeline of a sequence drawn as text (DS-371): its column and width are told before it is drawn.
TEST(TestSeqLine, ALifelineStartsAtTheFirstColumnWithNoWidth) {
  const ds::render::SeqLine l{"caller", "caller"};
  EXPECT_EQ(l.id, "caller");
  EXPECT_EQ(l.label, "caller");
  EXPECT_EQ(l.centre, 0);
  EXPECT_EQ(l.width, 0);
}

}  // namespace
