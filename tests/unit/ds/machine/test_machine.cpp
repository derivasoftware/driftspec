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

/// A machine as its author wrote it (DS-249): its states, the rules that compute them over the context of
/// its subject, where it starts, and what the rules say that cannot be.
class TestMachine : public ::testing::Test {
 protected:
  /// The model of a module `m` of a package `p` that holds the text given.
  void load(const std::string& body) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    binder.bind(parser.parseText("a.ds", "package p {\n  module m {\n" + body + "  }\n}\n"));
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, metamodel, checkSteps).run(binder);
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

  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
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

// ---- DS-250: the calls of its events in the behaviours written --------------------------------------

const char* const kShop =
    "    class Order {\n"
    "      + pay() -> none\n"
    "      + ship() -> none\n"
    "      + cancel() -> none\n"
    "      + audit() -> none\n"
    "      + settle() -> none\n"
    "    }\n"
    "    state Order {\n"
    "      initial Open\n"
    "      Open -> Paid on pay [enough]\n"
    "      Paid -> Shipped on ship\n"
    "      Open -> Cancelled on cancel\n"
    "      Paid -> Cancelled on cancel\n"
    "    }\n"
    "    class Desk {\n"
    "      - owns order: Order\n"
    "      - owns other: Order\n"
    "      + run() -> none\n"
    "    }\n";

/// The calls of the events of the order a behaviour of the desk makes: `event from A,B` and `!` when it does not fit.
class TestCalls : public TestMachine {
 protected:
  std::vector<std::string> made(const std::string& behaviour) {
    load(std::string(kShop) + behaviour);
    std::vector<std::string> out;
    for (const auto& call : machine("Order").calls()) {
      std::string from;
      for (const auto& state : call.from) from += (from.empty() ? "" : ",") + state;
      out.push_back(call.receiver + "." + call.event + " from " + from + (call.fits ? "" : " !"));
    }
    return out;
  }
};

TEST_F(TestCalls, ItsEventsAreWhatItsJumpsAreMadeOnAndEachHasTheStatesItHappensFrom) {
  load(kShop);
  EXPECT_EQ(machine("Order").events(), (std::vector<std::string>{"pay", "ship", "cancel"}));
  EXPECT_EQ(machine("Order").sources("cancel"), (std::vector<std::string>{"Open", "Paid"}));
  EXPECT_TRUE(machine("Order").sources("audit").empty());
  EXPECT_TRUE(machine("Order").calls().empty());  // no behaviour is written
}

TEST_F(TestCalls, ACallIsFoundWithWhoMakesItAndTheStatesItsReceiverMayBeIn) {
  load(std::string(kShop) +
       "    sequence Desk::run {\n"
       "      self -> order.ship()\n"
       "    }\n");
  const auto calls = machine("Order").calls();
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].behaviour, "Desk::run");
  EXPECT_EQ(calls[0].receiver, "order");
  EXPECT_EQ(calls[0].event, "ship");
  EXPECT_EQ(calls[0].from, (std::vector<std::string>{"Open", "Paid", "Shipped", "Cancelled"}));  // the first call: any state
  EXPECT_TRUE(calls[0].fits);
  ASSERT_NE(model.find(calls[0].message), nullptr);
  EXPECT_EQ(model.find(calls[0].message)->location.line, 23);
}

TEST_F(TestCalls, ACallMovesItsReceiverByTheJumpsOnItsEventAndOneNoJumpLetsHappenDoesNotFit) {
  EXPECT_EQ(made("    sequence Desk::run {\n"
                 "      self -> order.cancel()\n"
                 "      self -> order.ship()\n"
                 "    }\n"),
            (std::vector<std::string>{"order.cancel from Open,Paid,Shipped,Cancelled", "order.ship from Cancelled !"}));
}

TEST_F(TestCalls, AJumpWithAGuardMayLeaveItsReceiverWhereItWas) {
  EXPECT_EQ(made("    sequence Desk::run {\n"
                 "      self -> order.pay()\n"
                 "      self -> order.cancel()\n"
                 "    }\n"),
            (std::vector<std::string>{"order.pay from Open,Paid,Shipped,Cancelled", "order.cancel from Open,Paid"}));
}

TEST_F(TestCalls, AFrameMayNotHappenAndInsideItTheOrderIsHeld) {
  EXPECT_EQ(made("    sequence Desk::run {\n"
                 "      opt it was paid {\n"
                 "        self -> order.ship()\n"
                 "        self -> order.cancel()\n"
                 "      }\n"
                 "      self -> order.cancel()\n"
                 "    }\n"),
            (std::vector<std::string>{"order.ship from Open,Paid,Shipped,Cancelled", "order.cancel from Shipped !",
                                      "order.cancel from Open,Paid,Shipped,Cancelled"}));
}

TEST_F(TestCalls, TheSidesOfAnAltAreEachOneWay) {
  EXPECT_EQ(made("    sequence Desk::run {\n"
                 "      alt it was paid {\n"
                 "        self -> order.ship()\n"
                 "      } else {\n"
                 "        self -> order.cancel()\n"
                 "      }\n"
                 "      self -> order.cancel()\n"
                 "    }\n"),
            (std::vector<std::string>{"order.ship from Open,Paid,Shipped,Cancelled", "order.cancel from Open,Paid,Shipped,Cancelled",
                                      "order.cancel from Shipped,Cancelled !"}));
}

TEST_F(TestCalls, ALoopIsReadOnceSinceItsReceiverMayBeAnotherEachTime) {
  EXPECT_EQ(made("    sequence Desk::run {\n"
                 "      loop each order {\n"
                 "        self -> order.cancel()\n"
                 "      }\n"
                 "    }\n"),
            (std::vector<std::string>{"order.cancel from Open,Paid,Shipped,Cancelled"}));
}

TEST_F(TestCalls, AnOperationThatIsNoEventMovesNothingAndTwoReceiversAreFollowedApart) {
  EXPECT_EQ(made("    sequence Desk::run {\n"
                 "      self -> order.ship()\n"
                 "      self -> order.audit()\n"
                 "      self -> other.cancel()\n"
                 "      self -> order.cancel()\n"
                 "    }\n"),
            (std::vector<std::string>{"order.ship from Open,Paid,Shipped,Cancelled", "other.cancel from Open,Paid,Shipped,Cancelled",
                                      "order.cancel from Shipped !"}));
}

TEST_F(TestCalls, ABehaviourOfTheSubjectCallingItsOwnEventsIsFollowed) {
  EXPECT_EQ(made("    sequence Order::settle {\n"
                 "      -> ship()\n"
                 "      -> cancel()\n"
                 "    }\n"),
            (std::vector<std::string>{"self.ship from Open,Paid,Shipped,Cancelled", "self.cancel from Shipped !"}));
}

// ---- DS-251: where an event leads from a state --------------------------------------------------------

TEST_F(TestCalls, AnEventLeadsFromAStateWhereItsJumpsSayEachWithItsGuard) {
  load(kShop);
  EXPECT_EQ(machine("Order").subject(), "p@m::Order");
  EXPECT_EQ(machine("Order").targets("pay", "Open"), (std::vector<std::string>{"Paid [enough]"}));
  EXPECT_EQ(machine("Order").targets("cancel", "Paid"), (std::vector<std::string>{"Cancelled"}));
  EXPECT_TRUE(machine("Order").targets("cancel", "Shipped").empty());  // the machine does not let it happen there
  EXPECT_TRUE(machine("Order").targets("audit", "Open").empty());      // no event of the machine
}

}  // namespace
