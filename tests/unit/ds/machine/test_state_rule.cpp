#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/machine/StateRule.hpp"
#include "ds/model/Model.hpp"

namespace {

/// The rules of a state machine, from the records the binder reads them into (DS-249, DS-384).
TEST(TestStateRule, EachRuleIsAStateItsLiteralsAndItsLine) {
  ds::model::Entity machine;
  ds::model::Location at12, at14;
  at12.line = 12;
  at14.line = 14;
  machine.rules = {{"Open", {{false, "status", "=", "Open"}, {true, "cancelled", "", ""}}, at12},
                   {"Paid", {{false, "status", "=", "Paid"}}, at14}};
  const std::vector<ds::machine::StateRule> rules = ds::machine::StateRule::of(machine);
  ASSERT_EQ(rules.size(), 2u);
  EXPECT_EQ(rules[0].state, "Open");
  EXPECT_EQ(rules[0].literals.size(), 2u);
  EXPECT_EQ(rules[0].text, "status = Open, not cancelled");
  EXPECT_EQ(rules[0].line, 12);
  EXPECT_EQ(rules[1].state, "Paid");
  EXPECT_EQ(rules[1].line, 14);
}

/// A machine that writes no rule has none, and a literal says not when it is negated and stands alone when it compares
/// nothing (DS-384).
TEST(TestStateRule, NoRuleWrittenIsNoneAndALiteralSaysWhatItIs) {
  ds::model::Entity machine;
  EXPECT_TRUE(ds::machine::StateRule::of(machine).empty());
  machine.rules = {{"Idle", {{true, "busy", "", ""}, {false, "mode", "!=", "Off"}}, {}}};
  EXPECT_EQ(ds::machine::StateRule::of(machine).front().text, "not busy, mode != Off");
}

}  // namespace
