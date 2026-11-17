#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/machine/Machine.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// A machine as its author wrote it (DS-249): its states, the rules that compute them over the context of
/// its subject, where it starts, and what the rules say that cannot be.
class TestMachine : public ::testing::Test {
 protected:
  /// The model of a module `m` of a package `p` that holds the text given.
  void load(const std::string& body) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    binder.bind(parser.parseText("a.ds", "package p {\n  module m {\n" + body + "  }\n}\n"));
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, checkSteps).run(binder);
  }

  ds::machine::Machine machine(const std::string& subject) const {
    return ds::machine::Machine(model, *model.find("p@m::" + subject + "::state"));
  }

  /// The codes of what a machine says cannot be, in the order found.
  std::vector<std::string> codes(const std::string& subject) const {
    std::vector<std::string> out;
    for (const auto& f : machine(subject).faults()) out.push_back(f.code);
    return out;
  }

  ds::model::Model model;
};

const char* const kOrder =
    "    enum Status { Open, Paid, Shipped }\n"
    "    class Order {\n"
    "      - status: Status = Open\n"
    "      - cancelled: bool = false\n"
    "      + pay() -> none\n"
    "    }\n";

TEST_F(TestMachine, ItsStatesAreThoseItsRulesComputeThenThoseOnlyAJumpNames) {
  load(std::string(kOrder) +
       "    state Order {\n"
       "      Open if status = Open\n"
       "      Paid if status = Paid\n"
       "      Paid -> Refunded on pay\n"
       "    }\n");
  EXPECT_EQ(machine("Order").states(), (std::vector<std::string>{"Open", "Paid", "Refunded"}));
}

TEST_F(TestMachine, ARuleIsGivenBackAsItIsWrittenAndAStateMayHaveSeveral) {
  load(std::string(kOrder) +
       "    state Order {\n"
       "      Open if status = Open, not cancelled\n"
       "      Gone if cancelled\n"
       "      Gone if status != Open, not cancelled\n"
       "    }\n");
  EXPECT_EQ(machine("Order").rules("Open"), (std::vector<std::string>{"status = Open, not cancelled"}));
  EXPECT_EQ(machine("Order").rules("Gone"), (std::vector<std::string>{"cancelled", "status != Open, not cancelled"}));
  EXPECT_TRUE(machine("Order").rules("Paid").empty());
}

TEST_F(TestMachine, ItStartsInTheStateItsDefaultsAreIn) {
  load(std::string(kOrder) +
       "    state Order {\n"
       "      Paid if status = Paid\n"
       "      Open if status = Open, not cancelled\n"
       "    }\n");
  EXPECT_EQ(machine("Order").initial(), "Open");
}

TEST_F(TestMachine, AStartThatIsWrittenIsTheStart) {
  load(std::string(kOrder) +
       "    state Order {\n"
       "      initial Paid\n"
       "      Open if status = Open\n"
       "      Paid if status = Paid\n"
       "    }\n");
  EXPECT_EQ(machine("Order").initial(), "Paid");
}

TEST_F(TestMachine, DefaultsThatAreInNoStateOrDoNotTellGiveNoStart) {
  load(
      "    enum Status { Open, Paid }\n"
      "    class Order {\n"
      "      - status: Status = Open\n"
      "      - paid: bool\n"
      "    }\n"
      "    class Bill {\n"
      "      - status: Status = Open\n"
      "    }\n"
      "    state Order {\n"
      "      Settled if paid\n"
      "      Owed if not paid\n"
      "    }\n"
      "    state Bill {\n"
      "      Settled if status = Paid\n"
      "    }\n");
  EXPECT_EQ(machine("Order").initial(), "");  // paid has no default: it does not tell
  EXPECT_EQ(machine("Bill").initial(), "");   // Open is in no state
}

TEST_F(TestMachine, RulesThatExcludeEachOtherSayNothing) {
  load(std::string(kOrder) +
       "    state Order {\n"
       "      Open if status = Open, not cancelled\n"
       "      Paid if status = Paid, not cancelled\n"
       "      Shipped if status = Shipped\n"
       "      Cancelled if cancelled, status != Shipped\n"
       "    }\n");
  EXPECT_TRUE(machine("Order").faults().empty());
}

TEST_F(TestMachine, TwoStatesThatCanHoldAtOnceAreSaidWithAContextInWhichTheyDo) {
  load(std::string(kOrder) +
       "    state Order {\n"
       "      Open if status = Open\n"
       "      Cancelled if cancelled\n"
       "    }\n");
  const auto faults = machine("Order").faults();
  ASSERT_EQ(faults.size(), 1u);
  EXPECT_EQ(faults[0].code, "W-STATE-OVERLAP");
  EXPECT_NE(faults[0].message.find("Order can be Open and Cancelled at once: when cancelled = true, status = Open"), std::string::npos)
      << faults[0].message;
  EXPECT_EQ(faults[0].location.line, 11);  // where the second of the two rules is written
}

TEST_F(TestMachine, TwoRulesOfOneStateMayHoldAtOnce) {
  load(std::string(kOrder) +
       "    state Order {\n"
       "      Gone if cancelled\n"
       "      Gone if status = Shipped\n"
       "      Live if not cancelled, status != Shipped\n"
       "    }\n");
  EXPECT_TRUE(machine("Order").faults().empty());
}

TEST_F(TestMachine, AValueItsTypeDoesNotHaveIsSaidWithTheValuesItHas) {
  load(std::string(kOrder) +
       "    state Order {\n"
       "      Open if status = Open\n"
       "      Lost if status = Missing\n"
       "    }\n");
  const auto faults = machine("Order").faults();
  ASSERT_EQ(faults.size(), 1u);
  EXPECT_EQ(faults[0].code, "W-STATE-VALUE");
  EXPECT_NE(faults[0].message.find("status is never Missing: it is one of Open, Paid, Shipped"), std::string::npos) << faults[0].message;
}

TEST_F(TestMachine, AComparisonOfWhatTheSubjectDoesNotHoldIsSaid) {
  load(std::string(kOrder) +
       "    state Order {\n"
       "      Open if status = Open\n"
       "      Late if delay = Long\n"
       "    }\n");
  EXPECT_EQ(codes("Order"), (std::vector<std::string>{"W-STATE-UNDECLARED"}));
}

TEST_F(TestMachine, ANameAloneTheSubjectDoesNotHoldIsAConditionWithAName) {
  load(std::string(kOrder) +
       "    state Order {\n"
       "      Fresh if status = Open, not stale\n"
       "      Stale if status = Open, stale\n"
       "      Done if status != Open\n"
       "    }\n");
  EXPECT_TRUE(machine("Order").faults().empty());
}

TEST_F(TestMachine, ARuleThatExcludesItselfIsSaidAndIsNotHeldToTheOthers) {
  load(std::string(kOrder) +
       "    state Order {\n"
       "      Open if status = Open\n"
       "      Never if cancelled, not cancelled\n"
       "    }\n");
  EXPECT_EQ(codes("Order"), (std::vector<std::string>{"W-STATE-IMPOSSIBLE"}));
}

TEST_F(TestMachine, ARuleThatNamesWhatIsNotThereIsSaidOnce) {
  load(std::string(kOrder) +
       "    state Order {\n"
       "      Open if status = Open\n"
       "      Lost if status = Missing\n"
       "      Late if delay = Long\n"
       "    }\n");
  EXPECT_EQ(codes("Order"), (std::vector<std::string>{"W-STATE-VALUE", "W-STATE-UNDECLARED"}));  // and no overlap with Open
}

TEST_F(TestMachine, AnAttributeWhoseValuesCannotBeListedIsComparedAsWritten) {
  load(
      "    class Tank {\n"
      "      - level: int = 0\n"
      "    }\n"
      "    state Tank {\n"
      "      Empty if level = 0\n"
      "      Filled if level != 0\n"
      "    }\n");
  EXPECT_TRUE(machine("Tank").faults().empty());
  EXPECT_EQ(machine("Tank").initial(), "Empty");
}

}  // namespace
