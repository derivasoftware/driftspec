#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// The coherence of two levels of one behaviour (DS-233), over a door that asks a, which asks b: below, the
/// crossings are door to a, then a to b. Each scenario of the use case says them another way.
class TestLevelCoherence : public ::testing::Test {
 protected:
  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    const auto file = parser.parseText("shop.ds",
                                       "package shop {\n"
                                       "  usecase UC-00001-1 run() -> int {\n"
                                       "    title: Run\n"
                                       "    outcome default: done\n"
                                       "  }\n"
                                       "  usecase UC-00002-1 stop() -> int {\n"
                                       "    title: Stop\n"
                                       "    outcome default: done\n"
                                       "  }\n"
                                       "  interface door {\n"
                                       "    kind: cli\n"
                                       "    operation run() -> int : UC-00001-1\n"
                                       "    operation stop() -> int : UC-00002-1\n"
                                       "  }\n"
                                       "  sequence door::run {\n"
                                       "    local first: shop@a@work::First\n"
                                       "    self -> first.go()\n"
                                       "  }\n"
                                       "  package a {\n"
                                       "    module work {\n"
                                       "      class First {\n"
                                       "        - refs second: shop@b@work::Second\n"
                                       "        + go() -> none\n"
                                       "      }\n"
                                       "      sequence First::go {\n"
                                       "        self -> second.go()\n"
                                       "      }\n"
                                       "    }\n"
                                       "  }\n"
                                       "  package b {\n"
                                       "    module work {\n"
                                       "      class Second {\n"
                                       "        + go() -> none\n"
                                       "      }\n"
                                       "      sequence Second::go {\n"
                                       "      }\n"
                                       "    }\n"
                                       "  }\n"
                                       "  package c { }\n"
                                       "  sequence UC-00001-1 {\n"
                                       "    -> door.run()\n"
                                       "    door -> a.go()\n"
                                       "    a -> b.go()\n"
                                       "  }\n"
                                       "  sequence UC-00001-1 / omits {\n"
                                       "    -> door.run()\n"
                                       "    a -> b.go()\n"
                                       "  }\n"
                                       "  sequence UC-00001-1 / adds {\n"
                                       "    -> door.run()\n"
                                       "    door -> a.go()\n"
                                       "    a -> c.go()\n"
                                       "  }\n"
                                       "  sequence UC-00001-1 / turned {\n"
                                       "    -> door.run()\n"
                                       "    a -> b.go()\n"
                                       "    door -> a.go()\n"
                                       "  }\n"
                                       "  sequence UC-00001-1 / twice {\n"
                                       "    -> door.run()\n"
                                       "    door -> a.go()\n"
                                       "    door -> a.go()\n"
                                       "  }\n"
                                       "  sequence UC-00001-1 / unknown {\n"
                                       "    -> door.run()\n"
                                       "    c -> a.go()\n"
                                       "  }\n"
                                       "  sequence UC-00002-1 {\n"
                                       "    -> door.stop()\n"
                                       "    door -> c.go()\n"
                                       "  }\n"
                                       "}\n");
    ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
    binder.bind(file);
    const auto steps = ds::check::steps();
    ds::check::Checker(model, steps).run(binder);
  }

  /// The findings (or the coverage) of one code about one scenario.
  static int count(const std::vector<ds::model::Finding>& in, const std::string& code, const std::string& scenario) {
    int n = 0;
    for (const auto& f : in) n += f.code == code && f.message.rfind(scenario + " has ", 0) == 0;
    return n;
  }

  int levels(const std::string& scenario) const {
    return count(model.findings(), "W-LEVEL-NO-CROSSING", scenario) + count(model.findings(), "W-LEVEL-ORDER", scenario) +
           count(model.coverage(), "W-LEVEL-NOT-BELOW", scenario);
  }

  ds::model::Model model;
};

TEST_F(TestLevelCoherence, TwoLevelsThatCrossAlikeInTheSameOrderAgree) { EXPECT_EQ(levels("UC-00001-1"), 0); }

TEST_F(TestLevelCoherence, TheHigherLevelMayLeaveACrossingOut) { EXPECT_EQ(levels("UC-00001-1/omits"), 0); }

TEST_F(TestLevelCoherence, ACrossingASenderAllWrittenBelowNeverMakesIsAFinding) {
  EXPECT_EQ(count(model.findings(), "W-LEVEL-NO-CROSSING", "UC-00001-1/adds"), 1);
  EXPECT_EQ(levels("UC-00001-1/adds"), 1);  // and the crossings that agree draw nothing
}

TEST_F(TestLevelCoherence, TwoCrossingsTurnedAroundAreAFindingOnTheOneOutOfPlace) {
  EXPECT_EQ(count(model.findings(), "W-LEVEL-ORDER", "UC-00001-1/turned"), 1);
  EXPECT_EQ(levels("UC-00001-1/turned"), 1);
}

TEST_F(TestLevelCoherence, ACrossingSaidMoreTimesThanItIsMadeIsAFinding) {
  int said = 0;
  for (const auto& f : model.findings())
    said += f.code == "W-LEVEL-NO-CROSSING" && f.message.find("UC-00001-1/twice") == 0 &&
            f.message.find("does not do it that many times") != std::string::npos;
  EXPECT_EQ(said, 1);
}

TEST_F(TestLevelCoherence, ACrossingFromASenderNotWrittenBelowIsCoverageAndNoFinding) {
  EXPECT_EQ(count(model.coverage(), "W-LEVEL-NOT-BELOW", "UC-00001-1/unknown"), 1);  // nothing below happens in c
  EXPECT_EQ(count(model.findings(), "W-LEVEL-NO-CROSSING", "UC-00001-1/unknown"), 0);
}

TEST_F(TestLevelCoherence, AScenarioWithNoBehaviourBelowItIsHeldToNothing) {
  EXPECT_EQ(levels("UC-00002-1"), 0);  // door::stop has no sequence: that level is not written
}

}  // namespace
