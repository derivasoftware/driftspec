#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// Rules and definitions (REQ-00095-2): design pieces with a name, in the box they apply to, that
/// make a requirement real; what they say is formal, read from its syntax and never from prose.
class TestRules : public ::testing::Test {
 protected:
  ds::model::Model model;

  bool parses(const std::string& text) const {
    ds::parse::Parser parser;
    return !parser.parseText("a.ds", text).hasSyntaxErrors;
  }

  const ds::model::Formula& formula(const std::string& id) const { return model.formulas().at(id); }

  void load(const std::string& text) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    const auto file = parser.parseText("a.ds", text);
    ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
    binder.bind(file);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, checkSteps).run(binder);
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
    for (const auto& r : model.index().all())
      if (r.kind == "requires" && r.from == from && r.to == to) return true;
    return false;
  }
};

TEST_F(TestRules, ARuleIsADesignPieceInTheBoxItAppliesTo) {
  load(
      "package p {\n"
      "  requirement REQ-00001-1: The core shall not depend on the face\n"
      "  module face { }\n"
      "  rule core-blind {\n"
      "    title: Nothing in the core uses the face\n"
      "    @requirement REQ-00001-1\n"
      "    for class c in p@core, method m\n"
      "    where m in c, not m is attribute\n"
      "    never c uses x, x in p@face\n"
      "  }\n"
      "  module core {\n"
      "    class Checker {\n"
      "      + run() -> none\n"
      "      rule named {\n"
      "        @requirement REQ-00001-1\n"
      "        for method m in p@core::Checker\n"
      "        holds m requires r\n"
      "      }\n"
      "    }\n"
      "  }\n"
      "}\n");
  ASSERT_NE(model.find("p@core-blind"), nullptr);
  EXPECT_EQ(model.find("p@core-blind")->kind, ds::model::Kind::Rule);
  const auto& f = formula("p@core-blind");
  ASSERT_EQ(f.ranges.size(), 2u);
  EXPECT_EQ(f.ranges[0].kind, "class");
  EXPECT_EQ(f.ranges[0].variable, "c");
  EXPECT_EQ(f.ranges[0].scope, "p@core");
  EXPECT_EQ(f.ranges[1].scope, "");
  ASSERT_EQ(f.where.size(), 2u);
  EXPECT_EQ(f.where[0].form, ds::model::LiteralForm::In);
  EXPECT_TRUE(f.where[1].negated);
  EXPECT_EQ(f.where[1].form, ds::model::LiteralForm::Kind);
  ASSERT_EQ(f.claims.size(), 1u);
  EXPECT_TRUE(f.never);
  EXPECT_EQ(f.claims[0][0].name, "uses");
  EXPECT_TRUE(f.claims[0][0].terms[1].variable);                       // a bare name is a variable
  EXPECT_FALSE(f.claims[0][1].terms[1].variable);                      // a qualified name is an entity
  EXPECT_EQ(field("p@core-blind", "never"), "c uses x, x in p@face");  // the card shows it as written
  EXPECT_NE(model.find("p@core::Checker::named"), nullptr);            // a class's rule is named under it
  EXPECT_TRUE(requires("p@core-blind", "REQ-00001-1"));
  EXPECT_TRUE(requires("p@core::Checker::named", "REQ-00001-1"));
  EXPECT_TRUE(codes("W-RULE").empty());
}

TEST_F(TestRules, ADefinitionIsARelationWithTheCasesInWhichItHolds) {
  load(
      "package p {\n"
      "  definition reaches(a, b) {\n"
      "    title: What a box reaches through what it uses\n"
      "    if a uses b\n"
      "    if a uses c, reaches(c, b)\n"
      "  }\n"
      "}\n");
  ASSERT_NE(model.find("p@reaches"), nullptr);
  EXPECT_EQ(model.find("p@reaches")->kind, ds::model::Kind::Definition);
  const auto& f = formula("p@reaches");
  EXPECT_EQ(f.parameters, (std::vector<std::string>{"a", "b"}));
  ASSERT_EQ(f.cases.size(), 2u);
  EXPECT_EQ(f.cases[1][1].form, ds::model::LiteralForm::Call);
  EXPECT_EQ(f.cases[1][1].name, "reaches");
  EXPECT_EQ(f.cases[1][1].terms.size(), 2u);
  EXPECT_TRUE(codes("W-DEFINITION").empty());
}

TEST_F(TestRules, ARuleThatAnswersToNothingOrClaimsNothingIsReported) {
  load(
      "package p {\n"
      "  requirement REQ-00001-1: Something real\n"
      "  rule orphan {\n"
      "    holds p@shapeless is definition\n"
      "  }\n"
      "  rule silent {\n"
      "    @requirement REQ-00001-1\n"
      "    for class c\n"
      "  }\n"
      "  rule torn {\n"
      "    @requirement REQ-00001-1\n"
      "    for class c\n"
      "    holds c is class\n"
      "    never c is class\n"
      "  }\n"
      "  definition shapeless(x) {\n"
      "    title: Nothing says when\n"
      "  }\n"
      "}\n");
  EXPECT_EQ(codes("W-RULE-NO-REQUIREMENT").size(), 1u);
  EXPECT_EQ(codes("W-RULE-NO-CLAIM").size(), 2u);  // silent says nothing, torn says two things
  EXPECT_EQ(codes("W-DEFINITION-NO-CASE").size(), 1u);
}

TEST_F(TestRules, ProseIsNotARule) {
  // what a rule says is formal: written as a field, it does not parse, and nothing reads its words
  EXPECT_FALSE(parses("package p {\n  rule r {\n    holds: every class has a method\n  }\n}\n"));
  EXPECT_FALSE(parses("package p {\n  rule r {\n    never some class with more than 30 members\n  }\n}\n"));
  EXPECT_FALSE(parses("package p {\n  definition d(x) {\n    if x is a big class\n  }\n}\n"));
  EXPECT_TRUE(parses("package p {\n  rule r {\n    for class c\n    holds m in c, m is method\n  }\n}\n"));
}

TEST_F(TestRules, ARuleOfAMemberIsNamedUnderIt) {
  load(
      "package p {\n"
      "  requirement REQ-00001-1: A message shall fit in a frame\n"
      "  module m {\n"
      "    class Link {\n"
      "      + send(msg: int) -> bool {\n"
      "        rule in-range {\n"
      "          @requirement REQ-00001-1\n"
      "          for message k\n"
      "          where k calls p@m::Link::send, arg(k, msg, v)\n"
      "          holds v > 0, v <= 99\n"
      "        }\n"
      "      }\n"
      "    }\n"
      "  }\n"
      "}\n");
  ASSERT_NE(model.find("p@m::Link::send::in-range"), nullptr);
  EXPECT_TRUE(requires("p@m::Link::send::in-range", "REQ-00001-1"));
  const auto& f = formula("p@m::Link::send::in-range");
  ASSERT_EQ(f.where.size(), 2u);
  EXPECT_EQ(f.where[1].form, ds::model::LiteralForm::Arg);
  EXPECT_EQ(f.where[1].name, "msg");  // the parameter, by name
  ASSERT_EQ(f.claims.size(), 1u);
  EXPECT_EQ(f.claims[0][0].form, ds::model::LiteralForm::Greater);
  EXPECT_TRUE(f.claims[0][0].terms[1].value);  // 0 is a value, not a name
  EXPECT_EQ(f.claims[0][1].form, ds::model::LiteralForm::LessEqual);
  EXPECT_TRUE(codes("RULE").empty());
}

TEST_F(TestRules, ABracketContractDoesNotParse) {
  // what a member requires is a rule in its body: one syntax for one thing
  EXPECT_FALSE(parses("package p {\n  class C {\n    + send(msg: int) -> bool {\n      [requires: 0 < msg < 100]\n    }\n  }\n}\n"));
  EXPECT_FALSE(parses("package p {\n  class C {\n    + send(msg: int) -> bool {\n      [ensures: ret == false]\n    }\n  }\n}\n"));
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
      "    class in {\n"
      "      + is(where: string) -> none\n"
      "      + holds: int\n"
      "    }\n"
      "  }\n"
      "}\n");
  EXPECT_NE(model.find("p@editor::definition"), nullptr);
  EXPECT_NE(model.find("p@editor::rule"), nullptr);
  EXPECT_NE(model.find("p@m::definition"), nullptr);
  EXPECT_NE(model.find("p@m::in::is"), nullptr);
  EXPECT_NE(model.find("p@m::in::holds"), nullptr);
}

}  // namespace
