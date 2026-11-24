#include <gtest/gtest.h>

#include "ds/layout/Edge.hpp"

namespace {

/// An edge between two boxes (DS-392), by their identities and the kind of relation it stands for.
TEST(TestEdge, AnEdgeJoinsTwoBoxesByTheirIdentities) {
  const ds::layout::Edge e{"p@m::Heir", "p@m::Base", "inherits"};
  EXPECT_EQ(e.from, "p@m::Heir");
  EXPECT_EQ(e.to, "p@m::Base");
  EXPECT_EQ(e.kind, "inherits");
}

}  // namespace
