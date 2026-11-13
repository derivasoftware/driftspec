#include <gtest/gtest.h>

#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/IrExport.hpp"

namespace {

/// The values of a sequence have types, over examples/values (its `!` lines are the oracle).
class TestValues : public ::testing::Test {
 protected:
  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/values")) binder.bind(f);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, checkSteps).run(binder);
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
  for (const auto& r : model.index().all()) {
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
  EXPECT_EQ(lines["W-ARG-TYPE"].size(), 3U);
}

TEST_F(TestValues, ADerivedClassSatisfiesItsBaseAsAnArgument) { EXPECT_FALSE(reported("W-ARG-TYPE", "'special'")); }

TEST_F(TestValues, ATemplateWithCommasIsOneParameter) { EXPECT_EQ(lines.count("E-MSG-ARITY"), 0U); }

TEST_F(TestValues, TakingTheResultOfAnOperationThatReturnsNothingIsReported) {
  EXPECT_TRUE(reported("W-RESULT-NONE", "'gone'"));
  EXPECT_EQ(lines["W-RESULT-NONE"].size(), 1U);
}

TEST_F(TestValues, ALiteralArgumentHasTheTypeItIsWrittenIn) {
  EXPECT_TRUE(reported("W-ARG-TYPE", "'7' is int"));
  EXPECT_FALSE(reported("W-ARG-TYPE", "'3'"));
  EXPECT_FALSE(reported("W-ARG-TYPE", "A-1"));
}

TEST_F(TestValues, ALocalInsideAFrameIsALocal) { EXPECT_FALSE(reported("W-PARTICIPANT-UNDECLARED", "'spare'")); }

TEST_F(TestValues, TwoObjectsOfOneTypeAreTwoLifelines) {
  const auto ir = nlohmann::json::parse(ds::render::IrExport(model).render("diagram:SEQ_Sell", false));
  std::vector<std::string> items;
  for (const auto& n : ir["nodes"]) {
    const std::string label = n["label"];
    if (n["kind"] == "lifeline" && label.find(": Item") != std::string::npos) items.push_back(label);
  }
  EXPECT_EQ(items.size(), 3U);  // item, other and spare
}

TEST_F(TestValues, AFrameWithNoMessageIsStillDrawn) {
  const auto ir = nlohmann::json::parse(ds::render::IrExport(model).render("diagram:SEQ_Sell", false));
  bool receipt = false;
  for (const auto& n : ir["nodes"]) receipt = receipt || (n["kind"] == "frame" && n["label"] == "loop each line of the receipt");
  EXPECT_TRUE(receipt);
}

/// A name in a behaviour resolves as the code scopes it: a local hides a member of its name, a method
/// is no value a message is sent to, and a result named like an attribute is stored in it.
TEST(TestNameScopes, ALocalHidesAMemberOfItsNameAndAResultIsStoredInTheAttributeOfItsName) {
  ds::model::Model model;
  ds::parse::Parser parser;
  ds::bind::Binder binder(model);
  const auto file = parser.parseText("a.ds",
                                     "package p {\n"
                                     "  module m {\n"
                                     "    class Tree {\n"
                                     "      + frames() -> int\n"
                                     "    }\n"
                                     "    class Store {\n"
                                     "      + keep() -> int\n"
                                     "    }\n"
                                     "    class Shelf {\n"
                                     "      + keep() -> int\n"
                                     "    }\n"
                                     "    class Desk {\n"
                                     "      - shelf: Store\n"
                                     "      + tree() -> int\n"
                                     "      + fill() -> int\n"
                                     "    }\n"
                                     "    sequence Desk::tree {\n"
                                     "      local tree: Tree\n"
                                     "      -> tree.frames() -> n\n"
                                     "      return n\n"
                                     "    }\n"
                                     "    sequence Desk::fill {\n"
                                     "      local made: Shelf\n"
                                     "      -> made.keep() -> shelf\n"
                                     "      -> shelf.keep() -> n\n"
                                     "      return n\n"
                                     "    }\n"
                                     "  }\n"
                                     "}\n");
  ASSERT_FALSE(file.hasSyntaxErrors);
  binder.bind(file);
  const auto checkSteps = ds::check::steps();
  ds::check::Checker(model, checkSteps).run(binder);
  auto calls = [&model](const std::string& message, const std::string& op) {
    for (const auto& r : model.index().all())
      if (r.kind == "calls" && r.from == message && r.to == op) return true;
    return false;
  };
  EXPECT_TRUE(calls("p@m::Desk::tree::sequence::m0", "p@m::Tree::frames"));  // the local, not the method tree
  EXPECT_TRUE(calls("p@m::Desk::fill::sequence::m1", "p@m::Store::keep"));   // the attribute shelf holds a Store
  EXPECT_TRUE(model.findings().empty()) << model.findings().front().code;
}

}  // namespace
