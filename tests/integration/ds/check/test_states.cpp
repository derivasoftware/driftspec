#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// The states of a machine as rules over the context of its subject (DS-249), over tests/corpus/states: machines
/// whose rules exclude each other, and the faults a rule can have (its `!` lines are the oracle).
class TestStates : public ::testing::Test {
 protected:
  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/states")) binder.bind(f);
    const auto checkSteps = ds::check::StepFactory::of();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
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

  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
};

TEST_F(TestStates, AStateIsWrittenWithTheRuleThatComputesItOverWhatItsSubjectHolds) {
  const ds::model::Entity* machine = model.find("shop@order::Order::state");
  ASSERT_NE(machine, nullptr);
  const std::string rules = machine->field("rules");
  // an enum value, a boolean turned round by `not`, and a difference; the jumps beside them, as they were
  EXPECT_NE(rules.find("Open|status=Open,!cancelled|"), std::string::npos) << rules;
  EXPECT_NE(rules.find("Cancelled|cancelled,status!=Shipped|"), std::string::npos) << rules;
  EXPECT_NE(machine->field("transitions").find("Open>Paid>pay"), std::string::npos);
  // nothing is said of the rules of the three machines whose rules exclude each other
  for (const std::string code : {"W-STATE-OVERLAP", "W-STATE-VALUE", "W-STATE-UNDECLARED", "W-STATE-IMPOSSIBLE"}) {
    EXPECT_FALSE(says(code, "Order")) << code;
    EXPECT_FALSE(says(code, "Offer")) << code;
    EXPECT_FALSE(says(code, "Lamp ")) << code;
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

// ---- DS-250: the calls of the behaviours written, held to the machine --------------------------------------

TEST_F(TestStates, ACallOfAnEventTheMachineLetsHappenFromNoStateItsReceiverMayBeInIsAFindingWhereItIsMade) {
  const auto all = said("W-STATE-ORDER");
  ASSERT_EQ(all.size(), 1u);
  // pay, ship, then cancel: once shipped, the machine lets nothing cancel the order
  EXPECT_EQ(all[0],
            "Desk::rush calls order.cancel() when order can only be Shipped: the machine of Order lets cancel happen from Open or Paid");
  for (const auto& f : model.findings())
    if (f.code == "W-STATE-ORDER") {  // the line of the call
      EXPECT_EQ(f.location.line, 53);
    }
}

TEST_F(TestStates, CallsInTheOrderOfTheMachineACallAfterAFrameThatMayNotHappenAndAFirstCallAreNoFinding) {
  EXPECT_FALSE(says("W-STATE-ORDER", "Desk::sell"));   // pay, then ship
  EXPECT_FALSE(says("W-STATE-ORDER", "Desk::close"));  // ship inside an opt, then cancel: the order may still be open
  EXPECT_FALSE(says("W-STATE-ORDER", "Desk::label"));  // ship as the first call: the order may be in any state
}

}  // namespace
