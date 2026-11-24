#include <gtest/gtest.h>

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
  const auto checkSteps = ds::check::steps();
  ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
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

}  // namespace
