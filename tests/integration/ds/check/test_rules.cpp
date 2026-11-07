#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// Rules and definitions (REQ-00095-1): design pieces with a name, in the box they apply to, that
/// make a requirement real; what they say is kept as text until the evaluator reads it.
class TestRules : public ::testing::Test {
 protected:
  ds::model::Model model;

  void load(const std::string& text) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    const auto file = parser.parseText("a.ds", text);
    ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
    binder.bind(file);
    ds::check::Checker(model).run(binder);
  }

  std::string field(const std::string& id, const std::string& key) const {
    const ds::model::Entity* e = model.find(id);
    if (!e) return "?";
    const auto it = e->fields.find(key);
    return it == e->fields.end() ? "" : it->second;
  }

  std::vector<std::string> codes(const char* prefix) const {
    std::vector<std::string> out;
    for (const auto& f : model.findings())
      if (f.code.find(prefix) != std::string::npos) out.push_back(f.code);
    return out;
  }

  bool
    requires(const std::string& from, const std::string& to)
  const {
    for (const auto& r : model.relations())
      if (r.kind == "requires" && r.from == from && r.to == to) return true;
    return false;
  }
};

TEST_F(TestRules, ARuleIsADesignPieceInTheBoxItAppliesTo) {
  load(
      "package p {\n"
      "  requirement REQ-00001-1: The engine shall be reached only through its interface\n"
      "  rule through-engine {\n"
      "    title: Python reaches the engine only through engine\n"
      "    @requirement REQ-00001-1\n"
      "    for: every calls from p@face to p@core\n"
      "    holds: target realises p@engine\n"
      "  }\n"
      "  module core {\n"
      "    rule small {\n"
      "      @requirement REQ-00001-1\n"
      "      never: some class with more than 30 members\n"
      "    }\n"
      "    class Checker {\n"
      "      + run() -> none\n"
      "      rule named {\n"
      "        @requirement REQ-00001-1\n"
      "        for: every method in p@core::Checker\n"
      "        holds: some requires from it\n"
      "      }\n"
      "    }\n"
      "  }\n"
      "}\n");
  ASSERT_NE(model.find("p@through-engine"), nullptr);
  EXPECT_EQ(model.find("p@through-engine")->kind, ds::model::Kind::Rule);
  EXPECT_EQ(field("p@through-engine", "for"), "every calls from p@face to p@core");
  EXPECT_EQ(field("p@through-engine", "holds"), "target realises p@engine");
  EXPECT_EQ(field("p@core::small", "never"), "some class with more than 30 members");
  EXPECT_NE(model.find("p@core::Checker::named"), nullptr);  // a class's rule is named under it
  EXPECT_TRUE(requires("p@through-engine", "REQ-00001-1"));
  EXPECT_TRUE(requires("p@core::Checker::named", "REQ-00001-1"));
  EXPECT_TRUE(codes("RULE").empty());
}

TEST_F(TestRules, ADefinitionNamesAConceptWithWhatItRangesOver) {
  load(
      "package p {\n"
      "  definition empty-box {\n"
      "    title: A box that answers to requirements and holds nothing\n"
      "    for: every package, module\n"
      "    where: some requires from it and no contains from it to class, function, interface\n"
      "  }\n"
      "}\n");
  ASSERT_NE(model.find("p@empty-box"), nullptr);
  EXPECT_EQ(model.find("p@empty-box")->kind, ds::model::Kind::Definition);
  EXPECT_EQ(field("p@empty-box", "for"), "every package, module");
  EXPECT_TRUE(codes("DEFINITION").empty());
}

TEST_F(TestRules, ARuleThatAnswersToNothingOrClaimsNothingIsReported) {
  load(
      "package p {\n"
      "  requirement REQ-00001-1: Something real\n"
      "  rule orphan {\n"
      "    holds: true\n"
      "  }\n"
      "  rule silent {\n"
      "    @requirement REQ-00001-1\n"
      "    for: every class\n"
      "  }\n"
      "  rule torn {\n"
      "    @requirement REQ-00001-1\n"
      "    holds: a\n"
      "    never: b\n"
      "  }\n"
      "  definition shapeless {\n"
      "    where: no member\n"
      "  }\n"
      "}\n");
  EXPECT_EQ(codes("W-RULE-NO-REQUIREMENT").size(), 1u);
  EXPECT_EQ(codes("W-RULE-NO-CLAIM").size(), 2u);  // silent says nothing, torn says both
  EXPECT_EQ(codes("W-DEFINITION-NO-RANGE").size(), 1u);
}

TEST_F(TestRules, RuleAndDefinitionStayNamesWhereANameIsExpected) {
  load(
      "package p {\n"
      "  interface editor {\n"
      "    kind: lsp\n"
      "    operation definition(uri: string) -> string\n"
      "    operation rule(uri: string) -> string\n"
      "  }\n"
      "  module m {\n"
      "    function definition(uri: string) -> string\n"
      "  }\n"
      "}\n");
  EXPECT_NE(model.find("p@editor::definition"), nullptr);
  EXPECT_NE(model.find("p@editor::rule"), nullptr);
  EXPECT_NE(model.find("p@m::definition"), nullptr);
}

}  // namespace
