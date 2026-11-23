#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/IrExport.hpp"

namespace {

/// A scenario is a path through one behaviour, named on a branch, over tests/corpus/branches.
class TestBranches : public ::testing::Test {
 protected:
  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/branches")) {
      syntaxErrors += f.hasSyntaxErrors ? 1 : 0;
      binder.bind(f);
    }
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
    for (const auto& f : model.findings()) lines[f.code].push_back(f.message);
  }

  std::vector<std::string> path(const std::string& scenario) {
    std::vector<std::string> out;
    for (const auto* m : ds::model::pathOf(model, "bank@core::Desk::withdraw::sequence", scenario, nullptr)) {
      out.push_back(m->fields.count("return") ? "return " + m->fields.at("result") : m->fields.at("op"));
    }
    return out;
  }

  std::vector<std::string> labels(const std::string& diagram) {
    const auto ir = nlohmann::json::parse(ds::render::IrExport(model).render("diagram:" + diagram, false));
    std::vector<std::string> out;
    for (const auto& e : ir["edges"]) out.push_back(e["label"]);
    return out;
  }

  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  std::map<std::string, std::vector<std::string>> lines;
  int syntaxErrors = 0;
};

TEST_F(TestBranches, ABranchNamesAScenarioAndABareReturnEndsIt) {
  EXPECT_EQ(syntaxErrors, 0);
  EXPECT_TRUE(ds::model::hasScenario(model, "bank@core::Desk::withdraw::sequence", "frozen"));
  EXPECT_TRUE(ds::model::hasScenario(model, "bank@core::Desk::withdraw::sequence", "refused"));
  EXPECT_FALSE(ds::model::hasScenario(model, "bank@core::Desk::withdraw::sequence", "stolen"));
}

TEST_F(TestBranches, APathTakesWhatIsSharedAndItsBranchUpToItsReturn) {
  EXPECT_EQ(path("frozen"), (std::vector<std::string>{"balance", "log", "return 3"}));
  EXPECT_EQ(path("refused"), (std::vector<std::string>{"balance", "log", "return 2"}));
}

TEST_F(TestBranches, TheDefaultPathTakesNoNamedBranch) {
  EXPECT_EQ(path(""), (std::vector<std::string>{"balance", "take", "log", "return 0"}));
}

TEST_F(TestBranches, ACallThatNamesABranchEntersThatPath) {
  const auto* message = model.find("bank@teller::withdraw::sequence/frozen::m0");
  ASSERT_NE(message, nullptr);
  EXPECT_EQ(message->fields.at("expands"), "bank@core::Desk::withdraw::sequence");
  EXPECT_EQ(message->fields.at("expands-path"), "frozen");
}

TEST_F(TestBranches, AnExitCodeIsCheckedAgainstTheEndOfItsPath) {
  EXPECT_EQ(lines.count("W-OUTCOME-MISMATCH"), 0U);      // 0, 2 and 3, each through its path
  EXPECT_EQ(lines["W-SCENARIO-UNDECLARED"].size(), 1U);  // only the diagram that asks for stolen
}

TEST_F(TestBranches, ANameGivenTwiceIsAFinding) { EXPECT_EQ(lines["W-SCENARIO-TWICE"].size(), 2U); }

TEST_F(TestBranches, ABehaviourAskedForByItsSubjectIsDrawnWholeWithItsEarlyReturns) {
  const auto drawn = labels("SEQ_Whole");
  EXPECT_NE(std::find(drawn.begin(), drawn.end(), "return 3"), drawn.end());
  EXPECT_NE(std::find(drawn.begin(), drawn.end(), "return 2"), drawn.end());
}

TEST_F(TestBranches, AScenarioIsDrawnAsItsPathWithoutTheBranchesItSkips) {
  const auto drawn = labels("SEQ_Refused");
  EXPECT_EQ(std::find(drawn.begin(), drawn.end(), "take(amount)"), drawn.end());
  EXPECT_EQ(drawn.back(), "2");
}

TEST_F(TestBranches, AnUnnamedCallFollowsTheDefaultPath) {
  const auto drawn = labels("SEQ_Default");
  EXPECT_NE(std::find(drawn.begin(), drawn.end(), "take(amount)"), drawn.end());
  EXPECT_EQ(std::find(drawn.begin(), drawn.end(), "return 3"), drawn.end());
}

TEST_F(TestBranches, ADiagramThatAsksForAScenarioNobodyDeclaresIsAFinding) {
  bool found = false;
  for (const auto& line : lines["W-SCENARIO-UNDECLARED"]) found = found || line.find("SEQ_Bogus") != std::string::npos;
  EXPECT_TRUE(found);
}

}  // namespace
