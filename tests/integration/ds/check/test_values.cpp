#include <gtest/gtest.h>

#include <map>
#include <nlohmann/json.hpp>
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

/// The values of a sequence have types, over tests/corpus/values (its `!` lines are the oracle).
class TestValues : public ::testing::Test {
 protected:
  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/values")) binder.bind(f);
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
};

TEST_F(TestValues, AResultIsAParticipantOfTheMessagesAfterIt) {
  bool called = false;
  for (const auto& r : model.index().all()) {
    called = called || (r.kind == "calls" && r.to == "shop@store::Item::price");
  }
  EXPECT_TRUE(called);
}

TEST_F(TestValues, AResultUsedBeforeItsMessageIsText) {
  // a name used before the message that gives it is no value yet: text, its call unchecked and unsaid (DS-18)
  for (const auto& entry : lines)
    for (const auto& line : entry.second) EXPECT_EQ(line.find("early"), std::string::npos) << entry.first << " " << line;
}

TEST_F(TestValues, ACallOnAResultIsCheckedAgainstItsType) {
  EXPECT_TRUE(reported("W-MSG-UNDECLARED", "shop@store::Item::nothing"));
  EXPECT_EQ(lines["W-MSG-UNDECLARED"].size(), 1U);
}

TEST_F(TestValues, AValueOfUnknownTypeHasNothingToBeCheckedAgainst) { EXPECT_FALSE(reported("W-MSG-UNDECLARED", "anything")); }

TEST_F(TestValues, AnArgumentOfTheWrongTypeIsReported) {
  EXPECT_TRUE(reported("W-ARG-TYPE", "'price' is Price"));  // a designed class that is not the one expected
  EXPECT_TRUE(reported("W-ARG-TYPE", "'3' is a literal"));  // a literal where a designed class is expected
  EXPECT_EQ(lines["W-ARG-TYPE"].size(), 2U);                // std::string, int and void are the code's words: never compared (DS-18)
}

TEST_F(TestValues, ADerivedClassSatisfiesItsBaseAsAnArgument) { EXPECT_FALSE(reported("W-ARG-TYPE", "'special'")); }

TEST_F(TestValues, ATemplateWithCommasIsOneParameter) { EXPECT_EQ(lines.count("E-MSG-ARITY"), 0U); }

TEST_F(TestValues, ALiteralDiffersOnlyFromADesignedClass) {
  EXPECT_TRUE(reported("W-ARG-TYPE", "count takes item: Item, and '3' is a literal"));
  EXPECT_FALSE(reported("W-ARG-TYPE", "restock"));  // 3 against int: the code's word, never compared
  EXPECT_FALSE(reported("W-ARG-TYPE", "'7'"));      // 7 against std::string, the same
  EXPECT_FALSE(reported("W-ARG-TYPE", "A-1"));
}

TEST_F(TestValues, TheResultOfAnyOperationMayBeTaken) {
  // ds does not read void, none or None: taking the result of restock is no finding (DS-18)
  for (const auto& entry : lines)
    for (const auto& line : entry.second) EXPECT_EQ(line.find("'gone'"), std::string::npos) << entry.first << " " << line;
}

TEST_F(TestValues, ALocalInsideAFrameIsALocal) { EXPECT_FALSE(reported("W-MSG-UNDECLARED", "spare")); }

TEST_F(TestValues, TwoObjectsOfOneTypeAreTwoLifelines) {
  const auto ir = nlohmann::json::parse(ds::render::IrExport(model, metamodel).render("diagram:SEQ_Sell", false));
  std::vector<std::string> items;
  for (const auto& n : ir["nodes"]) {
    const std::string label = n["label"];
    if (n["kind"] == "lifeline" && label.find(": Item") != std::string::npos) items.push_back(label);
  }
  EXPECT_EQ(items.size(), 3U);  // item, other and spare
}

TEST_F(TestValues, AFrameWithNoMessageIsStillDrawn) {
  const auto ir = nlohmann::json::parse(ds::render::IrExport(model, metamodel).render("diagram:SEQ_Sell", false));
  bool receipt = false;
  for (const auto& n : ir["nodes"]) receipt = receipt || (n["kind"] == "frame" && n["label"] == "loop each line of the receipt");
  EXPECT_TRUE(receipt);
}

/// A name in a behaviour resolves as the code scopes it: a local hides a member of its name, a method
/// is no value a message is sent to, and a result named like an attribute is stored in it.
TEST(TestNameScopes, ALocalHidesAMemberOfItsNameAndAResultIsStoredInTheAttributeOfItsName) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
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
  const auto checkSteps = ds::check::StepFactory::of();
  ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
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
