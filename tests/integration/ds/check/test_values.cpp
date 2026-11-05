#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// The values of a sequence have types, over examples/values (its `!` lines are the oracle).
class TestValues : public ::testing::Test {
 protected:
  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/values")) binder.bind(f);
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

  ds::model::Model model;
  std::map<std::string, std::vector<std::string>> lines;
};

TEST_F(TestValues, AResultIsAParticipantOfTheMessagesAfterIt) {
  EXPECT_FALSE(reported("W-PARTICIPANT-UNDECLARED", "'item'"));
  EXPECT_FALSE(reported("W-PARTICIPANT-UNDECLARED", "'special'"));
  bool called = false;
  for (const auto& r : model.relations()) {
    called = called || (r.kind == "calls" && r.to == "shop@store::Item::price");
  }
  EXPECT_TRUE(called);
}

TEST_F(TestValues, AResultDoesNotExistBeforeItsMessage) {
  EXPECT_TRUE(reported("W-PARTICIPANT-UNDECLARED", "'early'"));
  EXPECT_EQ(lines["W-PARTICIPANT-UNDECLARED"].size(), 1U);
}

TEST_F(TestValues, ACallOnAResultIsCheckedAgainstItsType) {
  EXPECT_TRUE(reported("W-MSG-UNDECLARED", "shop@store::Item::nothing"));
  EXPECT_EQ(lines["W-MSG-UNDECLARED"].size(), 1U);
}

TEST_F(TestValues, AValueOfUnknownTypeHasNothingToBeCheckedAgainst) {
  EXPECT_FALSE(reported("W-PARTICIPANT-UNDECLARED", "'unknown'"));
  EXPECT_FALSE(reported("W-MSG-UNDECLARED", "anything"));
}

TEST_F(TestValues, AnArgumentOfTheWrongTypeIsReported) {
  EXPECT_TRUE(reported("W-ARG-TYPE", "'price' is Price"));
  EXPECT_TRUE(reported("W-ARG-TYPE", "'sku' is string"));
  EXPECT_EQ(lines["W-ARG-TYPE"].size(), 2U);
}

TEST_F(TestValues, ADerivedClassSatisfiesItsBaseAsAnArgument) { EXPECT_FALSE(reported("W-ARG-TYPE", "'special'")); }

TEST_F(TestValues, ATemplateWithCommasIsOneParameter) { EXPECT_EQ(lines.count("E-MSG-ARITY"), 0U); }

TEST_F(TestValues, TakingTheResultOfAnOperationThatReturnsNothingIsReported) {
  EXPECT_TRUE(reported("W-RESULT-NONE", "'gone'"));
  EXPECT_EQ(lines["W-RESULT-NONE"].size(), 1U);
}

}  // namespace
