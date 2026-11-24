#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/machine/StateRule.hpp"
#include "ds/model/Model.hpp"

namespace {

/// The rules of a state machine, read from the field the binder writes them in (DS-249).
TEST(TestStateRule, EachLineOfTheRulesIsAStateItsLiteralsAndItsLine) {
  ds::model::Entity machine;
  machine.fields["rules"] = "Open|status=Open,!cancelled|12\nPaid|status=Paid|14";
  const std::vector<ds::machine::StateRule> rules = ds::machine::StateRule::of(machine);
  ASSERT_EQ(rules.size(), 2u);
  EXPECT_EQ(rules[0].state, "Open");
  EXPECT_EQ(rules[0].literals.size(), 2u);
  EXPECT_EQ(rules[0].text, "status = Open, not cancelled");
  EXPECT_EQ(rules[0].line, 12);
  EXPECT_EQ(rules[1].state, "Paid");
  EXPECT_EQ(rules[1].line, 14);
}

TEST(TestStateRule, ALineWithoutItsThreePartsIsNoRule) {
  ds::model::Entity machine;
  machine.fields["rules"] = "Open|status=Open";
  EXPECT_TRUE(ds::machine::StateRule::of(machine).empty());
}

}  // namespace
