#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "ds/check/Report.hpp"
#include "ds/emit/JsonSink.hpp"
#include "ds/format/DescriptionFactory.hpp"
#include "ds/format/FormatFactory.hpp"
#include "ds/format/TextFormats.hpp"
#include "ds/model/Metamodel.hpp"

namespace {

/// The answers of a query and the report of a check are documents (DS-126): each described once into the sink it
/// is lent, or written as text by a writer found by name (DS-398); the report is built once from the model and
/// written without it.
ds::model::Finding finding(const std::string& code, ds::model::Severity severity) {
  ds::model::Finding f;
  f.code = code;
  f.severity = severity;
  f.message = "said";
  f.location.file = "a.ds";
  f.location.line = 3;
  f.location.column = 5;
  return f;
}

TEST(TestFormatFactory, AnswersAreWrittenAsTextByName) {
  for (const auto& f : ds::format::FormatFactory::answers()) EXPECT_EQ(f.first, f.second->name());
  EXPECT_EQ(ds::format::FormatFactory::answers().size(), 1u);
}

TEST(TestFormatFactory, ReportsAreWrittenAsTextByName) {
  for (const auto& f : ds::format::FormatFactory::report()) EXPECT_EQ(f.first, f.second->name());
  EXPECT_EQ(ds::format::FormatFactory::report().size(), 1u);
}

TEST(TestTextFormats, AReportIsBuiltOnceFromTheModelAndCountsWhatItSays) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::model::Entity e;
  e.kind = ds::model::Kind::Package;
  e.id = "p";
  model.declare(e);
  model.report(finding("E-X", ds::model::Severity::error));
  model.report(finding("W-Y", ds::model::Severity::warning));
  const ds::check::Report report = ds::check::Report::of(model, metamodel);
  EXPECT_EQ(report.findings.size(), 2u);
  EXPECT_EQ(report.errors, 1);
  EXPECT_EQ(report.entities, 1);
  EXPECT_EQ(report.expected, 0);
}

TEST(TestTextFormats, AReportReadsTheSameAsTextAndAsJson) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  ds::check::Report report;
  report.severities = ds::model::Metamodel().severities();
  report.findings = {finding("E-X", ds::model::Severity::error)};
  report.entities = 4;
  report.relations = 2;
  report.errors = 1;
  EXPECT_EQ(ds::format::ReportText().write(report, {}, sink), "a.ds:3:5  E-X  said\n4 entities · 0 expected · 2 relations · 1 findings\n");
  report.coverage = {finding("W-REQ-NO-VERIFICATION", ds::model::Severity::coverage),
                     finding("W-OPERATION-NO-BEHAVIOUR", ds::model::Severity::coverage)};
  ds::format::FormatOptions where;
  where.title = "design";
  EXPECT_EQ(ds::format::ReportText().write(report, where, sink),
            "a.ds:3:5  E-X  said\n4 entities · 0 expected · 2 relations · 1 findings · 2 open\n"
            "2 open items of coverage: ds query design coverage lists them\n");  // DS-431
  report.coverage.pop_back();
  EXPECT_EQ(ds::format::ReportText().write(report, {}, sink),
            "a.ds:3:5  E-X  said\n4 entities · 0 expected · 2 relations · 1 findings · 1 open\n"
            "1 open item of coverage: ds query <workspace> coverage lists them\n");
  ds::format::DescriptionFactory::report(report)->describe(sink);
  const auto json = nlohmann::json::parse(sink.result());
  EXPECT_EQ(json["errors"], 1);
  EXPECT_EQ(json["findings"][0]["location"]["column"], 5);
  EXPECT_EQ(json["entities"], 4);
}

}  // namespace
