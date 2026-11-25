#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/machine/Machine.hpp"
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
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
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
  // records, one per rule and per transition (DS-384): an enum value, a boolean turned round by `not`, and a difference
  const auto rule = [&](const std::string& state) {
    for (const auto& r : machine->rules)
      if (r.state == state) return &r;
    return static_cast<const ds::model::WrittenRule*>(nullptr);
  };
  ASSERT_NE(rule("Open"), nullptr);
  ASSERT_EQ(rule("Open")->literals.size(), 2u);
  EXPECT_EQ(rule("Open")->literals[0].left, "status");
  EXPECT_EQ(rule("Open")->literals[0].op, "=");
  EXPECT_EQ(rule("Open")->literals[0].right, "Open");
  EXPECT_TRUE(rule("Open")->literals[1].negated);
  EXPECT_EQ(rule("Open")->literals[1].left, "cancelled");
  ASSERT_NE(rule("Cancelled"), nullptr);
  EXPECT_EQ(rule("Cancelled")->literals[1].op, "!=");
  EXPECT_TRUE(std::any_of(machine->transitions.begin(), machine->transitions.end(),
                          [](const ds::model::WrittenTransition& t) { return t.from == "Open" && t.to == "Paid" && t.event == "pay"; }));
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

/// A transition out of a final state is a contradiction, said where the machine is written (DS-442); a state nothing
/// leaves is not taken as final, and a machine that says no end says nothing of one.
TEST_F(TestStates, ATransitionOutOfAFinalStateIsAFindingAndAStateWithNoWayOutIsNotTakenAsFinal) {
  EXPECT_EQ(said("W-STATE-LEAVES-FINAL").size(), 1U);
  EXPECT_TRUE(says("W-STATE-LEAVES-FINAL", "the transition Closed -> Open of Ticket leaves a final state"));
  EXPECT_FALSE(says("W-STATE-LEAVES-FINAL", "Kettle"));
}

/// A machine keeps the states it ends in as its final lines name them, in order, and counts them among its states
/// (DS-442): a final state nothing leads to would be unreachable as any other.
TEST_F(TestStates, AMachineKeepsItsFinalStatesInOrderAmongItsStates) {
  const ds::model::Entity* ticket = model.find("shop@ends::Ticket::state");
  ASSERT_NE(ticket, nullptr);
  EXPECT_EQ(ticket->field("final"), "Closed, Rejected");
  const ds::machine::Machine machine(model, *ticket);
  EXPECT_EQ(machine.finals(), (std::vector<std::string>{"Closed", "Rejected"}));
  const std::vector<std::string> states = machine.states();
  EXPECT_NE(std::find(states.begin(), states.end(), "Rejected"), states.end());
  EXPECT_FALSE(says("W-STATE-UNREACHABLE", "Ticket"));  // its finals are reached
}

}  // namespace
