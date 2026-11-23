#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// The step that holds the calls of the behaviours written to the machine of what they are made on (DS-250).
class TestCallOrder : public ::testing::Test {
 protected:
  /// What the check of a module `m` that holds the text says of the order of its calls.
  std::vector<std::string> said(const std::string& body) {
    const ds::model::Metamodel metamodel;
    ds::model::Model model{metamodel};
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    binder.bind(parser.parseText("a.ds", "package p {\n  module m {\n" + body + "  }\n}\n"));
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, metamodel, checkSteps).run(binder);
    std::vector<std::string> out;
    for (const auto& f : model.findings())
      if (f.code == "W-STATE-ORDER") out.push_back(std::to_string(f.location.line) + ": " + f.message);
    return out;
  }
};

const char* const kDoor =
    "    class Door {\n"
    "      + open() -> none\n"
    "      + shut() -> none\n"
    "      + slam() -> none\n"
    "    }\n"
    "    state Door {\n"
    "      initial Shut\n"
    "      Shut -> Open on open\n"
    "      Open -> Shut on shut\n"
    "    }\n"
    "    class Porter {\n"
    "      - owns door: Door\n"
    "      + round() -> none\n"
    "    }\n";

TEST_F(TestCallOrder, ItIsAStepOfTheCheckThatNeedsTheMessagesTyped) {
  const auto all = ds::check::steps();
  ASSERT_EQ(all.count("order"), 1u);
  EXPECT_EQ(all.at("order")->name(), "order");
  EXPECT_EQ(all.at("order")->needs(), (std::vector<std::string>{"behaviour"}));
}

TEST_F(TestCallOrder, ACallNoJumpLetsHappenIsReportedWhereItIsMadeWithTheStatesOnBothSides) {
  EXPECT_EQ(said(std::string(kDoor) + "    sequence Porter::round {\n"
                                      "      self -> door.open()\n"
                                      "      self -> door.open()\n"
                                      "    }\n"),
            (std::vector<std::string>{
                "19: Porter::round calls door.open() when door can only be Open: the machine of Door lets open happen from Shut"}));
}

TEST_F(TestCallOrder, ABehaviourOfTheSubjectIsSaidToCallItsOwn) {
  EXPECT_EQ(said(std::string(kDoor) + "    sequence Door::slam {\n"
                                      "      -> shut()\n"
                                      "      -> shut()\n"
                                      "    }\n"),
            (std::vector<std::string>{
                "19: Door::slam calls its own shut() when it can only be Shut: the machine of Door lets shut happen from Open"}));
}

TEST_F(TestCallOrder, CallsInTheOrderTheMachineWritesAndABehaviourThatCallsNoEventSayNothing) {
  EXPECT_TRUE(said(std::string(kDoor) + "    sequence Porter::round {\n"
                                        "      self -> door.open()\n"
                                        "      self -> door.slam()\n"
                                        "      self -> door.shut()\n"
                                        "      self -> door.open()\n"
                                        "    }\n")
                  .empty());
}

}  // namespace
