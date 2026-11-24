#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/machine/Jump.hpp"
#include "ds/model/Model.hpp"

namespace {

/// The jumps of a state machine, read from the field the binder writes them in (DS-250).
TEST(TestJump, EachTransitionIsAJumpWithItsEventAndItsGuard) {
  ds::model::Entity machine;
  machine.fields["transitions"] = "Open>Paid>pay [enough] / note\nPaid>Shipped>ship";
  const std::vector<ds::machine::Jump> jumps = ds::machine::Jump::of(machine);
  ASSERT_EQ(jumps.size(), 2u);
  EXPECT_EQ(jumps[0].from, "Open");
  EXPECT_EQ(jumps[0].to, "Paid");
  EXPECT_EQ(jumps[0].event, "pay");
  EXPECT_EQ(jumps[0].guard, "enough");
  EXPECT_EQ(jumps[1].event, "ship");
  EXPECT_TRUE(jumps[1].guard.empty());
}

}  // namespace
