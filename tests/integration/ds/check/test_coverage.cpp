#include <gtest/gtest.h>

#include <set>
#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Report.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/query/Query.hpp"

namespace {

/// What is not yet written is no finding (DS-216): a design with only its high level checks clean, and what is
/// left to write is one question away.
class TestCoverage : public ::testing::Test {};

TEST_F(TestCoverage, AnAbsenceIsNoFindingAndTheQueryAnswersIt) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  binder.bind(parser.parseText("a.ds",
                               "package shop {\n"
                               "  requirement SREQ-00001-1 {\n"
                               "    title: The shop shall sell an item\n"
                               "    status: approved\n"
                               "  }\n"
                               "  interface shell {\n"
                               "    kind: cli\n"
                               "    operation buy(item: string) -> int\n"
                               "  }\n"
                               "  package sales {\n"
                               "    @requirement SREQ-00001-1\n"
                               "  }\n"
                               "}\n"));
  ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
  EXPECT_TRUE(model.findings().empty()) << model.findings().front().code;
  const auto report = ds::check::Report::of(model, metamodel);
  EXPECT_TRUE(report.findings.empty());
  EXPECT_EQ(report.coverage.size(), 2u);  // the operation with no sequence, the approved requirement nothing verifies
  for (const auto& c : report.coverage) EXPECT_EQ(c.severity, ds::model::Severity::coverage);
  const auto all = ds::query::Query(model, metamodel).ask("coverage", "");
  EXPECT_EQ(all.size(), 2u);
  const auto one = ds::query::Query(model, metamodel).ask("coverage", "shop@shell::buy");
  ASSERT_EQ(one.size(), 1u);
  EXPECT_EQ(one[0].kind, "W-OPERATION-NO-BEHAVIOUR");
}

// DS-429, DS-430: an approved requirement is realised by what satisfies it, or through every requirement that refines
// it (a diamond is asked by each way, a cycle realises nothing); an approved constraint is verified; a draft owes nothing
TEST_F(TestCoverage, AnApprovedRequirementNothingRealisesAndAConstraintNothingVerifiesAreCoverage) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  binder.bind(parser.parseText("a.ds",
                               "package p {\n"
                               "  requirement SREQ-00001-1 {\n"  // realised through its one refinement
                               "    title: The system shall be whole\n"
                               "    status: approved\n"
                               "    requirement REQ-00001-1 {\n"
                               "      title: A shall be done\n"
                               "      status: approved\n"
                               "    }\n"
                               "  }\n"
                               "  requirement SREQ-00002-1 {\n"  // half realised: one refinement of two
                               "    title: The system shall be half done\n"
                               "    status: approved\n"
                               "    requirement REQ-00002-1 {\n"
                               "      title: B shall be done\n"
                               "      status: approved\n"
                               "    }\n"
                               "    requirement REQ-00003-1 {\n"
                               "      title: Nothing does this\n"
                               "      status: approved\n"
                               "    }\n"
                               "  }\n"
                               "  requirement REQ-00009-1: A draft owes nothing\n"
                               "  constraint CST-00001-1 {\n"
                               "    category: design\n"
                               "    title: Every class shall be tidy\n"
                               "    status: approved\n"
                               "  }\n"
                               "  constraint CST-00002-1 {\n"
                               "    category: design\n"
                               "    title: Every class shall be kept\n"
                               "    status: approved\n"
                               "  }\n"
                               "  constraint CST-00003-1: A draft constraint owes nothing\n"
                               "  verification VER-00001-1 {\n"
                               "    @constraint CST-00002-1\n"
                               "    title: Review that every class is kept\n"
                               "    method: review\n"
                               "  }\n"
                               "  module m {\n"
                               "    class C {\n"
                               "      @requirement REQ-00001-1\n"
                               "      @requirement REQ-00002-1\n"
                               "    }\n"
                               "  }\n"
                               "}\n"));
  ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
  std::set<std::string> unrealised, unverified;
  for (const auto& c : model.coverage()) {
    EXPECT_EQ(c.severity, ds::model::Severity::coverage) << c.code;
    if (c.code == "W-REQ-UNREALISED") unrealised.insert(c.message.substr(0, c.message.find(' ')));
    if (c.code == "W-CST-NO-VERIFICATION") unverified.insert(c.message.substr(0, c.message.find(' ')));
  }
  EXPECT_EQ(unrealised, (std::set<std::string>{"SREQ-00002-1", "REQ-00003-1"}));
  EXPECT_EQ(unverified, (std::set<std::string>{"CST-00001-1"}));
  for (const auto& f : model.findings()) EXPECT_NE(f.severity, ds::model::Severity::coverage) << f.code;  // never a finding
}

// DS-110: a rule that holds a constraint is its verification, checked by the machine, and the constraint is the rule's
// reason; a rule with no claim checks nothing, and a rule with neither link still answers to nothing
TEST_F(TestCoverage, ARuleThatHoldsAConstraintAnswersForItAndIsAnsweredByIt) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  binder.bind(parser.parseText("a.ds",
                               "package p {\n"
                               "  constraint CST-00001-1 {\n"  // held by a rule that says one claim
                               "    category: design\n"
                               "    title: The store shall never depend on the web\n"
                               "    status: approved\n"
                               "  }\n"
                               "  constraint CST-00002-1 {\n"  // named by a rule that says no claim: still unchecked
                               "    category: design\n"
                               "    title: Every class shall be kept\n"
                               "    status: approved\n"
                               "  }\n"
                               "  module web {\n"
                               "    class Page { }\n"
                               "  }\n"
                               "  module store {\n"
                               "    class Order {\n"
                               "      + total() -> int\n"
                               "    }\n"
                               "    rule blind {\n"
                               "      @constraint CST-00001-1\n"
                               "      for class c in p@store\n"
                               "      never c owns x, x in p@web\n"
                               "    }\n"
                               "    rule kept {\n"
                               "      @constraint CST-00002-1\n"
                               "      for class c in p@store\n"
                               "    }\n"
                               "    rule loose {\n"
                               "      for class c in p@store\n"
                               "      never c owns x, x in p@web\n"
                               "    }\n"
                               "  }\n"
                               "}\n"));
  ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
  std::set<std::string> unverified, reasonless;
  for (const auto& c : model.coverage()) {
    if (c.code == "W-CST-NO-VERIFICATION") unverified.insert(c.message.substr(0, c.message.find(' ')));
    if (c.code == "W-RULE-NO-REQUIREMENT") reasonless.insert(c.message.substr(0, c.message.find(' ')));
  }
  EXPECT_EQ(unverified, (std::set<std::string>{"CST-00002-1"}));
  EXPECT_EQ(reasonless, (std::set<std::string>{"p@store::loose"}));
}

}  // namespace
