#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "ds/check/Report.hpp"
#include "ds/emit/JsonSink.hpp"
#include "ds/emit/TextSink.hpp"
#include "ds/format/DescriptionFactory.hpp"
#include "ds/model/Metamodel.hpp"

namespace {

/// The answers of a query and the report of a check are documents (DS-126): each described once into the sink it
/// is lent (DS-398), in both halves, so the JSON sink writes their structure and the text sink what ds prints
/// (DS-399); the report is built once from the model and written without it.
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
  ds::emit::TextSink text;
  const auto written = [&text](const ds::check::Report& report, const std::string& workspace) {
    ds::format::DescriptionFactory::report(report, workspace)->describe(text);
    return text.result();
  };
  ds::check::Report report;
  report.severities = ds::model::Metamodel().severities();
  report.findings = {finding("E-X", ds::model::Severity::error)};
  report.entities = 4;
  report.relations = 2;
  report.errors = 1;
  EXPECT_EQ(written(report, ""), "a.ds:3:5  E-X  said\n4 entities · 0 expected · 2 relations · 1 finding\n");
  report.coverage = {finding("W-REQ-NO-VERIFICATION", ds::model::Severity::coverage),
                     finding("W-OPERATION-NO-BEHAVIOUR", ds::model::Severity::coverage)};
  EXPECT_EQ(written(report, "design"),
            "a.ds:3:5  E-X  said\n4 entities · 0 expected · 2 relations · 1 finding · 2 open\n"
            "2 open items of coverage: ds query design coverage lists them\n");  // DS-431
  report.coverage.pop_back();
  EXPECT_EQ(written(report, ""),
            "a.ds:3:5  E-X  said\n4 entities · 0 expected · 2 relations · 1 finding · 1 open\n"
            "1 open item of coverage: ds query <workspace> coverage lists them\n");
  report.findings.clear();
  report.entities = 1;
  report.relations = 1;
  EXPECT_EQ(written(report, "design"),
            "1 entity · 0 expected · 1 relation · 0 findings · 1 open\n"
            "1 open item of coverage: ds query design coverage lists them\n");  // one in the singular (DS-435)
  report.findings = {finding("E-X", ds::model::Severity::error)};
  ds::format::DescriptionFactory::report(report, "design")->describe(sink);  // the workspace is no field of the JSON
  const auto json = nlohmann::json::parse(sink.result());
  EXPECT_EQ(json["errors"], 1);
  EXPECT_EQ(json["findings"][0]["location"]["column"], 5);
  EXPECT_EQ(json["entities"], 1);
  EXPECT_FALSE(json.contains("workspace"));
}

TEST(TestTextFormats, TheAnswersAsTextAreALineEachWithTheirDetailWhenTheyHaveOne) {
  ds::emit::TextSink text;
  const std::vector<ds::query::Answer> answers{{"class", "a@b::C", "x"}, {"package", "a", ""}, {"error", "", ""}};
  ds::format::DescriptionFactory::answers(answers)->describe(text);
  EXPECT_EQ(text.result(), "class  a@b::C  x\npackage  a\nerror  \n");  // an empty identity is a cell: kind, two spaces
  ds::format::DescriptionFactory::answers({})->describe(text);          // no answers: nothing, and no failure
  EXPECT_EQ(text.result(), "");
}

}  // namespace
