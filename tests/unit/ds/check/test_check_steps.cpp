#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Context.hpp"
#include "ds/check/Report.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// Each step of the check on its own (DS-186): a workspace written here, run through that step and
/// only the steps it needs, and the finding that step owns.
std::vector<std::string> codesOf(const std::string& text, const std::string& step) {
  ds::model::Model model;
  ds::parse::Parser parser;
  ds::bind::Binder binder(model);
  const auto file = parser.parseText("a.ds", text);
  EXPECT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
  binder.bind(file);
  auto all = ds::check::steps();
  std::map<std::string, std::unique_ptr<ds::check::IStep>> chosen;
  std::vector<std::string> pending{step};
  while (!pending.empty()) {  // the step and every step it needs, nothing else
    const std::string key = pending.back();
    pending.pop_back();
    if (chosen.count(key)) continue;
    for (const auto& need : all.at(key)->needs()) pending.push_back(need);
    chosen[key] = std::move(all.at(key));
  }
  ds::check::Checker(model, chosen).run(binder);
  std::vector<std::string> out;
  for (const auto& f : model.findings()) out.push_back(f.code);
  for (const auto& f : model.coverage()) out.push_back(f.code);  // what is not yet written, kept apart (DS-216)
  return out;
}

bool has(const std::vector<std::string>& codes, const std::string& code) {
  for (const auto& c : codes)
    if (c == code) return true;
  return false;
}

const char* const kShop =
    "package shop {\n"
    "  requirement REQ-00001-1 {\n"
    "    title: The shop shall sell\n"
    "  }\n"
    "  module till {\n"
    "    class Stock {\n"
    "      + take(item: string) -> none\n"
    "    }\n"
    "    class Till {\n"
    "      @requirement REQ-00001-1\n"
    "      - stock: Stock\n"
    "      + sell(item: string) -> none\n"
    "    }\n";

TEST(TestLinker, AReferenceBecomesARelationAndAnUnpinnedArtefactIsAnError) {
  const auto codes = codesOf(std::string(kShop) +
                                 "    class Refund {\n"
                                 "      @requirement REQ-00001\n"
                                 "    }\n"
                                 "  }\n"
                                 "}\n",
                             "linker");
  EXPECT_TRUE(has(codes, "E-REF-UNPINNED"));
}

TEST(TestDerivation, ALanguageDsDoesNotKnowIsAFinding) {
  EXPECT_TRUE(has(codesOf("package shop {\n  language: cobol\n}\n", "derivation"), "W-LANGUAGE-UNKNOWN"));
  EXPECT_FALSE(has(codesOf("package shop {\n  language: cpp\n}\n", "derivation"), "W-LANGUAGE-UNKNOWN"));
}

TEST(TestBehaviourTyping, AMessageGivenMoreArgumentsThanItsOperationTakesIsAnError) {
  const auto codes = codesOf(std::string(kShop) +
                                 "    sequence Till::sell {\n"
                                 "      self -> stock.take(item, item)\n"
                                 "    }\n"
                                 "  }\n"
                                 "}\n",
                             "behaviour");
  EXPECT_TRUE(has(codes, "E-MSG-ARITY"));
}

TEST(TestContractCheck, AStateNothingLeadsToIsAFinding) {
  const auto codes = codesOf(std::string(kShop) +
                                 "    state Stock {\n"
                                 "      initial Full\n"
                                 "      Full -> Empty on take\n"
                                 "      Lost -> Full on take\n"
                                 "    }\n"
                                 "  }\n"
                                 "}\n",
                             "contracts");
  EXPECT_TRUE(has(codes, "W-STATE-UNREACHABLE"));
}

TEST(TestStructureCheck, AFieldItsKindDoesNotDeclareIsAFinding) {
  EXPECT_TRUE(has(codesOf("package shop {\n  colour: red\n}\n", "structure"), "W-FIELD-UNKNOWN"));
}

TEST(TestDependencyCheck, ACallIntoAnotherBoxsInterfaceNeedsAUse) {
  const auto codes = codesOf(
      "package p {\n"
      "  interface engine {\n"
      "    operation check(text: string) -> string\n"
      "  }\n"
      "}\n"
      "package q {\n"
      "  module face {\n"
      "    function run(text: string) -> string\n"
      "    sequence run {\n"
      "      local e: p@engine\n"
      "      -> e.check(text) -> json\n"
      "      return json\n"
      "    }\n"
      "  }\n"
      "}\n",
      "dependencies");
  EXPECT_TRUE(has(codes, "W-USES-UNDECLARED"));
}

TEST(TestRuleCheck, ARuleThatAnswersToNoRequirementIsAFinding) {
  const auto codes = codesOf(std::string(kShop) +
                                 "  }\n"
                                 "  rule till-draws-nothing {\n"
                                 "    title: the till sells; it never draws\n"
                                 "    for class c in shop@till\n"
                                 "    never m in c, s behaviour-of m, s uses x, x in shop@render\n"
                                 "  }\n"
                                 "}\n",
                             "rules");
  EXPECT_TRUE(has(codes, "W-RULE-NO-REQUIREMENT"));
}

TEST(TestContext, AnIdentityIsReadAfterTheRenamesLinkingMade) {
  ds::model::Model model;
  ds::bind::Binder binder(model);
  const ds::check::Resolver resolver(model);
  ds::check::Context context(model, resolver, binder);
  context.renamed()["p@m::Old"] = "p@m::New";
  EXPECT_EQ(context.current("p@m::Old"), "p@m::New");
  EXPECT_EQ(context.current("p@m::Kept"), "p@m::Kept");
}

TEST(TestReport, ItCountsWhatTheModelDeclaresExpectsRelatesAndFails) {
  ds::model::Model model;
  ds::model::Entity a;
  a.kind = ds::model::Kind::Class;
  a.id = "p@m::A";
  model.declare(a);
  ds::model::Entity b = a;
  b.id = "p@m::B";
  b.expected = true;
  model.declare(b);
  model.report({"E-X", "error", "broken", {}});
  model.report({"W-Y", "warning", "odd", {}});
  const auto report = ds::check::Report::of(model);
  EXPECT_EQ(report.entities, 1);
  EXPECT_EQ(report.expected, 1);
  EXPECT_EQ(report.errors, 1);
  EXPECT_EQ(report.findings.size(), 2u);
}

}  // namespace
