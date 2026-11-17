#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// The states of a machine as rules over the context of its subject (DS-249), over examples/states: machines
/// whose rules exclude each other, and the faults a rule can have (its `!` lines are the oracle).
class TestStates : public ::testing::Test {
 protected:
  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/states")) binder.bind(f);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, checkSteps).run(binder);
  }

  /// The findings with a code, as the messages they say.
  std::vector<std::string> said(const std::string& code) const {
    std::vector<std::string> out;
    for (const auto& f : model.findings())
      if (f.code == code) out.push_back(f.message);
    return out;
  }

  /// Whether some finding of a code says a text.
  bool says(const std::string& code, const std::string& text) const {
    const auto all = said(code);
    return std::any_of(all.begin(), all.end(), [&](const std::string& m) { return m.find(text) != std::string::npos; });
  }

  ds::model::Model model;
};

TEST_F(TestStates, AStateIsWrittenWithTheRuleThatComputesItOverWhatItsSubjectHolds) {
  const ds::model::Entity* machine = model.find("shop@order::Order::state");
  ASSERT_NE(machine, nullptr);
  const std::string rules = machine->field("rules");
  // an enum value, a boolean turned round by `not`, and a difference; the jumps beside them, as they were
  EXPECT_NE(rules.find("Open|status=Open,!cancelled|"), std::string::npos) << rules;
  EXPECT_NE(rules.find("Cancelled|cancelled,status!=Shipped|"), std::string::npos) << rules;
  EXPECT_NE(machine->field("transitions").find("Open>Paid>pay"), std::string::npos);
  // nothing of the three machines whose rules exclude each other is reported
  for (const auto& f : model.findings()) {
    EXPECT_EQ(f.message.find("Order"), std::string::npos) << f.message;
    EXPECT_EQ(f.message.find("Offer"), std::string::npos) << f.message;
    EXPECT_EQ(f.message.find("Lamp "), std::string::npos) << f.message;
  }
}

TEST_F(TestStates, SeveralRulesOfOneStateAndAConditionWithANameAreNoFinding) {
  // Dark has two rules, both may hold at once: they are two ways of being in one state
  EXPECT_FALSE(says("W-STATE-OVERLAP", "Dark"));
  // `expired` is nothing the offer holds: a name alone is a condition with a name, never undeclared
  EXPECT_FALSE(says("W-STATE-UNDECLARED", "expired"));
}

TEST_F(TestStates, TwoStatesThatCanHoldAtOnceAreAFindingThatSaysAContextInWhichTheyDo) {
  EXPECT_EQ(said("W-STATE-OVERLAP").size(), 2u);
  EXPECT_TRUE(says("W-STATE-OVERLAP", "Lamps can be Left and Both at once: when hazard = true, lever = Left"));
  EXPECT_TRUE(says("W-STATE-OVERLAP", "Lamps can be Right and Both at once: when hazard = true, lever = Right"));
}

TEST_F(TestStates, AValueItsTypeLacksAnAttributeTheSubjectLacksAndARuleThatExcludesItselfAreFindings) {
  EXPECT_TRUE(says("W-STATE-VALUE", "lever is never Up: it is one of Rest, Left, Right"));
  EXPECT_TRUE(says("W-STATE-UNDECLARED", "Door holds no hinge"));
  EXPECT_TRUE(says("W-STATE-IMPOSSIBLE", "the state Stuck of Door can never be"));
  // a rule that is already wrong is said once: it is not held to the others
  EXPECT_FALSE(says("W-STATE-OVERLAP", "Door"));
}

}  // namespace
