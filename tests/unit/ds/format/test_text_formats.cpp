#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "ds/check/Report.hpp"
#include "ds/format/TextFormats.hpp"

namespace {

/// The answers of a query and the report of a check are documents (DS-126): each written as text or
/// as JSON by formats found by name; the report is built once from the model and written without it.
ds::model::Finding finding(const std::string& code, const std::string& severity) {
  ds::model::Finding f;
  f.code = code;
  f.severity = severity;
  f.message = "said";
  f.location.file = "a.ds";
  f.location.line = 3;
  f.location.column = 5;
  return f;
}

TEST(TestAnswerFormats, AnswersAreWrittenAsTextOrJsonByName) {
  for (const auto& f : ds::format::answerFormats()) EXPECT_EQ(f.first, f.second->name());
  EXPECT_EQ(ds::format::answerFormats().size(), 2u);
}

TEST(TestReportFormats, ReportsAreWrittenAsTextOrJsonByName) {
  for (const auto& f : ds::format::reportFormats()) EXPECT_EQ(f.first, f.second->name());
  EXPECT_EQ(ds::format::reportFormats().size(), 2u);
}

TEST(TestTextFormats, AReportIsBuiltOnceFromTheModelAndCountsWhatItSays) {
  ds::model::Model model;
  ds::model::Entity e;
  e.kind = ds::model::Kind::Package;
  e.id = "p";
  model.declare(e);
  model.report(finding("E-X", "error"));
  model.report(finding("W-Y", "warning"));
  const ds::check::Report report = ds::check::Report::of(model);
  EXPECT_EQ(report.findings.size(), 2u);
  EXPECT_EQ(report.errors, 1);
  EXPECT_EQ(report.entities, 1);
  EXPECT_EQ(report.expected, 0);
}

TEST(TestTextFormats, AReportReadsTheSameAsTextAndAsJson) {
  ds::check::Report report;
  report.findings = {finding("E-X", "error")};
  report.entities = 4;
  report.relations = 2;
  report.errors = 1;
  EXPECT_EQ(ds::format::ReportText().write(report, {}), "a.ds:3:5  E-X  said\n4 entities · 0 expected · 2 relations · 1 findings\n");
  const auto json = nlohmann::json::parse(ds::format::ReportJson().write(report, {}));
  EXPECT_EQ(json["errors"], 1);
  EXPECT_EQ(json["findings"][0]["location"]["column"], 5);
  EXPECT_EQ(json["entities"], 4);
}

}  // namespace
