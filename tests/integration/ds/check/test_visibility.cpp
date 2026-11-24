#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// A box connects to another through a public child, at every level, over tests/corpus/visibility.
class TestVisibility : public ::testing::Test {
 protected:
  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/visibility")) {
      syntaxErrors += f.hasSyntaxErrors ? 1 : 0;
      binder.bind(f);
    }
    const auto checkSteps = ds::check::StepFactory::of();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
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

  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  std::map<std::string, std::vector<std::string>> lines;
  int syntaxErrors = 0;
};

TEST_F(TestVisibility, AnyChildMayBePrivateAndIsPublicUnlessItSaysSo) {
  EXPECT_EQ(syntaxErrors, 0);
  EXPECT_EQ(model.find("shop@store@internal")->fields.at("visibility"), "-");
  EXPECT_EQ(model.find("shop@store@stock::Helper")->fields.at("visibility"), "-");
  EXPECT_EQ(model.find("shop@store@stock::Item")->fields.count("visibility"), 0U);
}

TEST_F(TestVisibility, APrivateModuleIsNotReachedFromAnotherBox) {
  EXPECT_TRUE(reported("W-NOT-VISIBLE", "Till::cache reaches shop@store@internal::Cache through shop@store@internal"));
}

TEST_F(TestVisibility, APrivateClassIsNotReachedFromAnotherModule) {
  EXPECT_TRUE(reported("W-NOT-VISIBLE", "Till::helper reaches shop@store@stock::Helper, which is private"));
}

TEST_F(TestVisibility, APrivateMethodIsNotCalledFromAnotherClass) {
  EXPECT_TRUE(reported("W-NOT-VISIBLE", "reaches shop@store@stock::Item::margin, which is private"));
}

TEST_F(TestVisibility, APrivateFunctionIsNotCalledFromAnotherBox) {
  EXPECT_TRUE(reported("W-NOT-VISIBLE", "reaches shop@store@stock::round, which is private"));
}

TEST_F(TestVisibility, SiblingsSeeEachOther) {
  EXPECT_FALSE(reported("W-NOT-VISIBLE", "Inventory"));
  EXPECT_FALSE(reported("W-NOT-VISIBLE", "shop@store@stock::count"));
}

TEST_F(TestVisibility, EveryOtherCrossingIsReportedExactlyAsMarked) { EXPECT_EQ(lines["W-NOT-VISIBLE"].size(), 4U); }

TEST_F(TestVisibility, APublicMemberThatShowsAPrivateTypeIsALeak) {
  EXPECT_TRUE(reported("W-PRIVATE-LEAK", "Inventory::aid is public and shows shop@store@stock::Helper"));
  EXPECT_EQ(lines["W-PRIVATE-LEAK"].size(), 1U);
}

TEST(TestVisibilityAlone, APublicOperatorThatShowsAPrivateTypeIsALeak) {
  // an operator is a member with a signature like a method: what it takes and gives is shown (DS-117)
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  const auto file = parser.parseText("a.ds",
                                     "package p {\n"
                                     "  module money {\n"
                                     "    - class Rounding {\n"
                                     "      + mode: int\n"
                                     "    }\n"
                                     "    dataclass Cents <<+>> {\n"
                                     "      + value: int\n"
                                     "      operator +(other: Rounding) -> Cents\n"
                                     "    }\n"
                                     "  }\n"
                                     "}\n");
  ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
  binder.bind(file);
  const auto checkSteps = ds::check::StepFactory::of();
  ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  std::vector<std::string> leaks;
  for (const auto& f : model.findings())
    if (f.code == "W-PRIVATE-LEAK") leaks.push_back(f.message);
  ASSERT_EQ(leaks.size(), 1u);
  EXPECT_NE(leaks[0].find("p@money::Cents::operator+ is public and shows p@money::Rounding"), std::string::npos) << leaks[0];
}

}  // namespace
