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

/// The step that holds the rules of every machine to each other and to the structure (DS-249).
class TestStateRules : public ::testing::Test {
 protected:
  /// The codes the check of a module `m` that holds the text reports for its machines.
  std::vector<std::string> codes(const std::string& body) {
    const ds::model::Metamodel metamodel;
    ds::model::Model model{metamodel};
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    binder.bind(parser.parseText("a.ds", "package p {\n  module m {\n" + body + "  }\n}\n"));
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
    std::vector<std::string> out;
    for (const auto& f : model.findings())
      if (f.code.rfind("W-STATE-", 0) == 0) out.push_back(f.code);
    return out;
  }
};

const char* const kLamp =
    "    class Lamp {\n"
    "      - lit: bool = false\n"
    "      - broken: bool = false\n"
    "      + flip() -> none\n"
    "    }\n";

TEST_F(TestStateRules, ItIsAStepOfTheCheckThatNeedsTheSubjectsLinked) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::check::Context context(model, metamodel);
  const auto all = ds::check::StepFactory::of(context);
  ASSERT_EQ(all.count("states"), 1u);
  EXPECT_EQ(all.at("states")->name(), "states");
  EXPECT_EQ(all.at("states")->needs(), (std::vector<std::string>{"linker"}));
}

TEST_F(TestStateRules, TheFaultsOfEveryMachineWithRulesAreReported) {
  EXPECT_EQ(codes(std::string(kLamp) + "    class Door {\n"
                                       "      - shut: bool = true\n"
                                       "    }\n"
                                       "    state Lamp {\n"
                                       "      On if lit\n"
                                       "      Broken if broken\n"
                                       "    }\n"
                                       "    state Door {\n"
                                       "      Shut if shut\n"
                                       "      Ajar if shut, not shut\n"
                                       "    }\n"),
            (std::vector<std::string>{"W-STATE-IMPOSSIBLE", "W-STATE-OVERLAP"}));
}

TEST_F(TestStateRules, AMachineWhoseRulesExcludeEachOtherReportsNothing) {
  EXPECT_TRUE(codes(std::string(kLamp) + "    state Lamp {\n"
                                         "      Dark if not lit\n"
                                         "      Dark if broken, lit\n"
                                         "      On if lit, not broken\n"
                                         "    }\n")
                  .empty());
}

TEST_F(TestStateRules, AMachineWrittenWithJumpsAloneIsLeftAsItWas) {
  EXPECT_TRUE(codes(std::string(kLamp) + "    state Lamp {\n"
                                         "      initial Dark\n"
                                         "      Dark -> On on flip\n"
                                         "      On -> Dark on flip\n"
                                         "    }\n")
                  .empty());
}

}  // namespace
