#include <gtest/gtest.h>

#include <algorithm>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "ds/check/Report.hpp"
#include "ds/emit/JsonSink.hpp"
#include "ds/emit/TextSink.hpp"
#include "ds/format/Descriptions.hpp"
#include "ds/format/ViewerPage.hpp"
#include "ds/query/Query.hpp"
#include "ds/scene/Document.hpp"
#include "ds/theme/Theme.hpp"

namespace {

/// The style the drawings are written in, made once for the file (DS-393).
const ds::theme::Theme kTheme;

/// Each writer and each description on its own (DS-186, DS-398): the viewer's page, the one writer left (DS-474), its
/// name and what it writes from the document it is given; what a description writes into the sink it is lent.

const ds::render::IrDocument kDiagram =
    ds::render::IrDocument::parse(R"({"ir":1,"title":"T","nodes":[{"id":"p@m::A","kind":"box","label":"A","classifier":"class"},)"
                                  R"({"id":"p@m::B","kind":"box","label":"B","classifier":"class"}],)"
                                  R"("edges":[{"from":"p@m::A","to":"p@m::B","kind":"dependency"}]})");

/// A diagram's render-IR written once (DS-418): every record's keys in byte order (the sink's check would throw
/// otherwise), a key written when it says something: a node's label even when empty, abstract when said either way,
/// a message's order from 0, every key of a table's records; an empty text, list or map not at all.
TEST(TestIrDescription, EveryKeyInByteOrderAndWrittenWhenItSaysSomething) {
  ds::render::IrDocument doc;
  doc.kind = "class";
  doc.title = "T";
  ds::render::IrNode dot;
  dot.id = "p@m::A::initial";
  dot.kind = "initial";
  dot.classifier = "initial";
  ds::render::IrNode box;
  box.id = "p@m::A";
  box.kind = "box";
  box.label = "A";
  box.classifier = "class";
  box.abstract = false;
  box.sections = {{"+ x: int"}};
  box.sectionKinds = {"attributes"};
  box.refs = {{"location", "a.ds:3"}, {"change", "added"}};
  doc.nodes = {dot, box};
  ds::render::IrEdge first = ds::render::IrEdge::between("p@m::A", "p@m::B", "message");
  first.order = 0;
  first.dashed = true;
  doc.edges = {first};
  doc.table = ds::render::IrTable{"Order", {"Open"}, {{"pay", "", {{"", "none", ""}}}}};
  ds::emit::JsonSink sink(false, true);  // checked: a key out of byte order or repeated throws
  ds::format::IrDescription(doc).describe(sink);
  EXPECT_EQ(
      sink.result(),
      R"({"edges":[{"dashed":true,"from":"p@m::A","kind":"message","order":0,"to":"p@m::B"}],"ir":1,"kind":"class",)"
      R"("nodes":[{"classifier":"initial","id":"p@m::A::initial","kind":"initial","label":""},)"
      R"({"abstract":false,"classifier":"class","id":"p@m::A","kind":"box","label":"A","refs":{"change":"added","location":"a.ds:3"},)"
      R"("sectionKinds":["attributes"],"sections":[["+ x: int"]]}],)"
      R"("table":{"columns":["Open"],"corner":"Order","rows":[{"cells":[{"kind":"none","text":"","title":""}],"label":"pay","title":""}]},)"
      R"("title":"T"})");
}

TEST(TestLayoutDescription, EveryNodePlacedAndEveryEdgeRouted) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  ds::format::LayoutDescription(kDiagram, kTheme, 0).describe(sink);
  const auto json = nlohmann::json::parse(sink.result());
  EXPECT_EQ(json["nodes"].size(), 2u);
  EXPECT_EQ(json["routes"].size(), 1u);
}

TEST(TestCellsDescription, TheGridAsJson) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  ds::format::CellsDescription(kDiagram, kTheme, 60).describe(sink);
  const auto json = nlohmann::json::parse(sink.result());
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

TEST(TestSceneDescription, TheSceneAsJsonThatSaysHowToReadIt) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const ds::scene::SceneDocument scene = onePackage();
  ds::format::SceneDescription(scene).describe(sink);
  const auto json = nlohmann::json::parse(sink.result());
  EXPECT_EQ(json["version"], 2);
  EXPECT_EQ(json["nodes"][0][0], "p");  // the id is the first column, as fields says
  EXPECT_EQ(json["fields"]["node"][0], "id");
}

TEST(TestViewerPage, APageTitledAsAskedThatCarriesTheScene) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const ds::format::ViewerPage f;
  EXPECT_EQ(f.name(), "html");
  ds::format::FormatOptions options;
  options.title = "The shop";
  const std::string page = f.write(onePackage(), options, sink);
  EXPECT_NE(page.find("The shop"), std::string::npos);
  EXPECT_NE(page.find("<html"), std::string::npos);
}

const std::vector<ds::query::Answer> kAnswers{{"class", "p@m::A", "a class"}, {"class", "p@m::B", ""}};

TEST(TestAnswersDescription, AsTextOneLinePerAnswerKindIdentityAndDetail) {
  ds::emit::TextSink sink;  // the presentation half (DS-399)
  const ds::format::AnswersDescription description(kAnswers);
  EXPECT_EQ(description.name(), "answers");
  description.describe(sink);
  const std::string text = sink.result();
  EXPECT_NE(text.find("p@m::A"), std::string::npos);
  EXPECT_NE(text.find("a class"), std::string::npos);
  EXPECT_EQ(std::count(text.begin(), text.end(), '\n'), 2);
}

TEST(TestAnswersDescription, AListOfTheSameThreeFields) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  ds::format::AnswersDescription(kAnswers).describe(sink);
  const auto json = nlohmann::json::parse(sink.result());
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

TEST(TestReportDescription, AsTextEachFindingAtItsPlaceThenTheCounts) {
  ds::emit::TextSink sink;  // the presentation half (DS-399)
  const ds::check::Report report = aReport();
  const ds::format::ReportDescription description(report, "");
  EXPECT_EQ(description.name(), "report");
  description.describe(sink);
  EXPECT_EQ(sink.result(), "a.ds:2:1  W-Y  odd\n3 entities · 0 expected · 1 relation · 1 finding\n");
}

TEST(TestReportDescription, TheFindingsAndTheCountsAsJson) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const ds::check::Report report = aReport();
  ds::format::ReportDescription(report, "").describe(sink);
  const auto json = nlohmann::json::parse(sink.result());
  EXPECT_EQ(json["entities"], 3);
  EXPECT_EQ(json["errors"], 0);
  EXPECT_EQ(json["findings"][0]["code"], "W-Y");
}

TEST(TestReportDescription, TheReportAsOneValueWhereTheSinkStandsAsItIsWrittenWhole) {
  // the browser carries the report as one field of its answer (DS-397): the same value ds check --json writes whole
  const ds::check::Report report = aReport();
  const ds::format::ReportDescription description(report, "");
  ds::emit::JsonSink whole(false, true);
  description.describe(whole);
  const std::string written = whole.result();
  ds::emit::JsonSink carried(false, true);
  carried.beginRecord().field("report");
  description.describe(carried);
  carried.endRecord();
  EXPECT_EQ(carried.result(), R"({"report":)" + written + "}");
}

}  // namespace
