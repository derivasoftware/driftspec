#include <gtest/gtest.h>

#include <algorithm>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "ds/check/Report.hpp"
#include "ds/emit/JsonSink.hpp"
#include "ds/format/DiagramFormats.hpp"
#include "ds/format/SceneFormats.hpp"
#include "ds/format/TextFormats.hpp"
#include "ds/query/Query.hpp"
#include "ds/scene/Document.hpp"
#include "ds/theme/Theme.hpp"

namespace {

/// The style the drawings are written in, made once for the file (DS-393).
const ds::theme::Theme kTheme;

/// Each format on its own (DS-186): its name, and what it writes from the document it is given.

const ds::render::IrText kDiagram{R"({"ir":1,"title":"T","nodes":[{"id":"p@m::A","kind":"box","label":"A","classifier":"class"},)"
                                  R"({"id":"p@m::B","kind":"box","label":"B","classifier":"class"}],)"
                                  R"("edges":[{"from":"p@m::A","to":"p@m::B","kind":"dependency"}]})"};

ds::format::FormatOptions wide(int width) {
  ds::format::FormatOptions o;
  o.width = width;
  return o;
}

TEST(TestDiagramIr, TheIrAsItCameOrIndented) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const ds::format::DiagramIr f;
  EXPECT_EQ(f.name(), "ir");
  ds::format::FormatOptions pretty;
  pretty.pretty = true;
  EXPECT_EQ(nlohmann::json::parse(f.write(kDiagram, pretty, sink)), nlohmann::json::parse(kDiagram.json));
}

TEST(TestDiagramLayout, EveryNodePlacedAndEveryEdgeRouted) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const ds::format::DiagramLayout f(kTheme);
  EXPECT_EQ(f.name(), "layout");
  const auto json = nlohmann::json::parse(f.write(kDiagram, {}, sink));
  EXPECT_EQ(json["nodes"].size(), 2u);
  EXPECT_EQ(json["routes"].size(), 1u);
}

TEST(TestDiagramSvg, AnSvgThatNamesEveryBox) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const ds::format::DiagramSvg f(kTheme);
  EXPECT_EQ(f.name(), "svg");
  const std::string svg = f.write(kDiagram, {}, sink);
  EXPECT_EQ(svg.find("<svg"), svg.find('<'));
  EXPECT_NE(svg.find(">A<"), std::string::npos);
  EXPECT_NE(svg.find(">B<"), std::string::npos);
}

TEST(TestDiagramText, BoxCharactersWithinTheWidthAsked) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const ds::format::DiagramText f(kTheme);
  EXPECT_EQ(f.name(), "text");
  const std::string text = f.write(kDiagram, wide(60), sink);
  EXPECT_NE(text.find('A'), std::string::npos);
  EXPECT_NE(text.find('B'), std::string::npos);
  std::istringstream lines(text);
  for (std::string line; std::getline(lines, line);) {
    const auto columns = std::count_if(line.begin(), line.end(), [](char c) { return (c & 0xC0) != 0x80; });  // code points
    EXPECT_LE(columns, 60) << line;
  }
}

TEST(TestDiagramCells, TheGridAsJson) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const ds::format::DiagramCells f(kTheme);
  EXPECT_EQ(f.name(), "cells");
  const auto json = nlohmann::json::parse(f.write(kDiagram, wide(60), sink));
  EXPECT_FALSE(json.empty());
}

ds::scene::SceneDocument onePackage() {
  ds::scene::SceneDocument doc;
  ds::scene::Node n;
  n.id = "p";
  n.kind = "package";
  n.label = "p";
  n.full = {0, 0, 100, 50};
  doc.nodes.push_back(n);
  doc.world = {0, 0, 100, 50};
  doc.leanWorld = doc.world;
  return doc;
}

TEST(TestSceneJson, TheSceneAsCompactJsonThatSaysHowToReadIt) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const ds::format::SceneJson f;
  EXPECT_EQ(f.name(), "json");
  const auto json = nlohmann::json::parse(f.write(onePackage(), {}, sink));
  EXPECT_EQ(json["nodes"][0][0], "p");
  EXPECT_EQ(json["fields"]["node"][0], "id");
}

TEST(TestSceneHtml, APageTitledAsAskedThatCarriesTheScene) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const ds::format::SceneHtml f;
  EXPECT_EQ(f.name(), "html");
  ds::format::FormatOptions options;
  options.title = "The shop";
  const std::string page = f.write(onePackage(), options, sink);
  EXPECT_NE(page.find("The shop"), std::string::npos);
  EXPECT_NE(page.find("<html"), std::string::npos);
}

const std::vector<ds::query::Answer> kAnswers{{"class", "p@m::A", "a class"}, {"class", "p@m::B", ""}};

TEST(TestAnswersText, OneLinePerAnswerKindIdentityAndDetail) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const ds::format::AnswersText f;
  EXPECT_EQ(f.name(), "text");
  const std::string text = f.write(kAnswers, {}, sink);
  EXPECT_NE(text.find("p@m::A"), std::string::npos);
  EXPECT_NE(text.find("a class"), std::string::npos);
  EXPECT_EQ(std::count(text.begin(), text.end(), '\n'), 2);
}

TEST(TestAnswersJson, AnArrayOfTheSameThreeFields) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const ds::format::AnswersJson f;
  EXPECT_EQ(f.name(), "json");
  const auto json = nlohmann::json::parse(f.write(kAnswers, {}, sink));
  ASSERT_EQ(json.size(), 2u);
  EXPECT_EQ(json[0]["kind"], "class");
  EXPECT_EQ(json[0]["id"], "p@m::A");
  EXPECT_EQ(json[0]["detail"], "a class");
}

ds::check::Report aReport() {
  ds::check::Report r;
  ds::model::Finding f;
  f.code = "W-Y";
  f.severity = ds::model::Severity::warning;
  f.message = "odd";
  f.location.file = "a.ds";
  f.location.line = 2;
  f.location.column = 1;
  r.severities = ds::model::Metamodel().severities();
  r.findings = {f};
  r.entities = 3;
  r.relations = 1;
  return r;
}

TEST(TestReportText, EachFindingAtItsPlaceThenTheCounts) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const ds::format::ReportText f;
  EXPECT_EQ(f.name(), "text");
  EXPECT_EQ(f.write(aReport(), {}, sink), "a.ds:2:1  W-Y  odd\n3 entities · 0 expected · 1 relations · 1 findings\n");
}

TEST(TestReportJson, TheFindingsAndTheCountsAsJson) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const ds::format::ReportJson f;
  EXPECT_EQ(f.name(), "json");
  const auto json = nlohmann::json::parse(f.write(aReport(), {}, sink));
  EXPECT_EQ(json["entities"], 3);
  EXPECT_EQ(json["errors"], 0);
  EXPECT_EQ(json["findings"][0]["code"], "W-Y");
}

TEST(TestReportEncoder, TheReportAsOneValueWhereTheSinkStandsAsReportJsonWritesIt) {
  // the browser carries the report as one field of its answer (DS-397): the same value ds check --json writes whole
  const ds::check::Report report = aReport();
  ds::emit::JsonSink whole(false, true);
  const std::string written = ds::format::ReportJson().write(report, {}, whole);
  ds::emit::JsonSink carried(false, true);
  carried.beginRecord().field("report");
  ds::format::common::ReportEncoder(report).encode(carried);
  carried.endRecord();
  EXPECT_EQ(carried.result(), R"({"report":)" + written + "}");
}

}  // namespace
