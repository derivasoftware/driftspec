#include <gtest/gtest.h>

#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/IrExport.hpp"

namespace {

/// A behaviour says what it gives back and a call which scenario it enters, over examples/scenarios.
class TestScenarios : public ::testing::Test {
 protected:
  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/scenarios")) {
      syntaxErrors += f.hasSyntaxErrors ? 1 : 0;
      binder.bind(f);
    }
    ds::check::Checker(model).run(binder);
    for (const auto& f : model.findings()) lines[f.code].push_back(f.message);
  }

  bool reported(const std::string& code, const std::string& text) const {
    const auto found = lines.find(code);
    if (found == lines.end()) return false;
    for (const auto& line : found->second) {
      if (line.find(text) != std::string::npos) return true;
    }
    return false;
  }

  std::vector<std::string> labels(const std::string& diagram) {
    const auto ir = nlohmann::json::parse(ds::render::IrExport(model).render("diagram:" + diagram, false));
    std::vector<std::string> out;
    for (const auto& edge : ir["edges"]) out.push_back(edge["label"].get<std::string>());
    return out;
  }

  static bool has(const std::vector<std::string>& labels, const std::string& label) {
    for (const auto& l : labels) {
      if (l == label) return true;
    }
    return false;
  }

  ds::model::Model model;
  std::map<std::string, std::vector<std::string>> lines;
  int syntaxErrors = 0;
};

TEST_F(TestScenarios, ACallThatNamesAScenarioExpandsToThatOne) {
  EXPECT_EQ(syntaxErrors, 0);
  const auto* message = model.find("bank@desk::Teller::serve::sequence/refused::m0");
  ASSERT_NE(message, nullptr);
  EXPECT_EQ(message->fields.at("expands"), "bank@core::Vault::withdraw::sequence/refused");
}

TEST_F(TestScenarios, ACallThatNamesNoScenarioExpandsToTheDefault) {
  const auto* message = model.find("bank@desk::Teller::serve::sequence::m0");
  ASSERT_NE(message, nullptr);
  EXPECT_EQ(message->fields.at("expands"), "bank@core::Vault::withdraw::sequence");
}

TEST_F(TestScenarios, AScenarioNobodyDeclaresIsAFinding) {
  EXPECT_TRUE(reported("W-SCENARIO-UNDECLARED", "no scenario 'stolen'"));
  EXPECT_EQ(lines["W-SCENARIO-UNDECLARED"].size(), 1U);
}

TEST_F(TestScenarios, AReturnIsCheckedAgainstWhatTheOperationDeclares) {
  EXPECT_TRUE(reported("W-RETURN-TYPE", "Teller::quiet returns none"));
  EXPECT_TRUE(reported("W-RETURN-TYPE", "Teller::count returns int"));
  EXPECT_TRUE(reported("W-RETURN-TYPE", "Teller::describe returns string"));
  EXPECT_EQ(lines["W-RETURN-TYPE"].size(), 3U);
}

TEST_F(TestScenarios, AReturnThatAgreesIsNotAFinding) {
  EXPECT_FALSE(reported("W-RETURN-TYPE", "Teller::name"));
  EXPECT_FALSE(reported("W-RETURN-TYPE", "Teller::ready"));
  EXPECT_FALSE(reported("W-RETURN-TYPE", "Teller::serve"));
}

TEST_F(TestScenarios, ASubjectAloneDrawsItsDefaultScenario) {
  const auto drawn = labels("SEQ_Default");
  EXPECT_TRUE(has(drawn, "withdraw(amount)"));
  EXPECT_TRUE(has(drawn, "audit()"));
  EXPECT_FALSE(has(drawn, "withdraw(amount) / refused"));
}

TEST_F(TestScenarios, AScenarioIsSelectedAsItIsWrittenAndItsTraceFollowsIt) {
  const auto drawn = labels("SEQ_Refused");
  EXPECT_TRUE(has(drawn, "withdraw(amount) / refused"));
  EXPECT_FALSE(has(drawn, "audit()"));  // the default of withdraw is not what this trace enters
}

TEST_F(TestScenarios, AStarSelectsEveryScenario) {
  const auto drawn = labels("SEQ_All");
  EXPECT_TRUE(has(drawn, "withdraw(amount)"));
  EXPECT_TRUE(has(drawn, "withdraw(amount) / refused"));
  EXPECT_TRUE(has(drawn, "withdraw(amount) / stolen"));
}

TEST_F(TestScenarios, TheArrowBackCarriesWhatTheBehaviourReturns) {
  EXPECT_TRUE(has(labels("SEQ_Default"), "code"));
  EXPECT_FALSE(has(labels("SEQ_Default"), "int"));
}

}  // namespace
