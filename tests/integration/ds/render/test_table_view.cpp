#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "IrJson.hpp"
#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/emit/JsonSink.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/Ir.hpp"
#include "ds/render/IrExport.hpp"
#include "ds/render/Svg.hpp"
#include "ds/render/Text.hpp"
#include "ds/theme/Theme.hpp"

namespace {

/// The style the drawings are written in, made once for the file (DS-393).
const ds::theme::Theme kTheme;

/// A machine drawn as a table (DS-251), over tests/corpus/states: the order, its events down and its states across.
class TestTableView : public ::testing::Test {
 protected:
  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/states")) binder.bind(f);
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
    json = ds::test::irJson(ds::render::IrExport(model, metamodel), "diagram:TB_Order");
    ir = nlohmann::json::parse(json);
  }

  /// The cell of an event under a state.
  nlohmann::json cell(const std::string& event, const std::string& state) const {
    const auto& columns = ir["table"]["columns"];
    for (const auto& row : ir["table"]["rows"]) {
      if (row["label"] != event) continue;
      for (std::size_t i = 0; i < columns.size(); ++i)
        if (columns[i] == state) return row["cells"][i];
    }
    return nullptr;
  }

  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  std::string json;
  nlohmann::json ir;
};

TEST_F(TestTableView, ATableDiagramIsATableOfTheEventsOfAMachineDownItsStatesAcrossAndWhereEachEventTakesEachState) {
  EXPECT_EQ(ir["table"]["corner"], "Order");
  EXPECT_EQ(ir["table"]["columns"], nlohmann::json::array({"Open", "Paid", "Shipped", "Cancelled"}));
  ASSERT_EQ(ir["table"]["rows"].size(), 3u);
  EXPECT_EQ(ir["table"]["rows"][0]["label"], "pay");
  EXPECT_EQ(ir["table"]["rows"][1]["label"], "ship");
  EXPECT_EQ(ir["table"]["rows"][2]["label"], "cancel");
  EXPECT_EQ(cell("pay", "Open")["text"], "Paid");
  EXPECT_EQ(cell("pay", "Open")["kind"], "jump");
  EXPECT_EQ(cell("pay", "Open")["title"], "pay takes Open to Paid");
  EXPECT_EQ(cell("cancel", "Paid")["text"], "Cancelled");
  // where the machine has no jump the cell is empty, and says nothing
  EXPECT_EQ(cell("pay", "Paid")["kind"], "none");
  EXPECT_EQ(cell("pay", "Paid")["text"], "");
  EXPECT_EQ(cell("pay", "Paid")["title"], "");
  // it is a table and nothing else: no boxes, no arrows
  EXPECT_TRUE(ir["nodes"].empty());
  EXPECT_TRUE(ir["edges"].empty());
}

TEST_F(TestTableView, TheTableSaysWhoCallsEachEventAndTheCellInWhichABehaviourCallsOneAgainstTheMachine) {
  EXPECT_EQ(ir["table"]["rows"][0]["title"], "pay is called in Desk::rush, Desk::sell");
  // Desk::rush cancels an order that can only be Shipped: the machine has no jump there
  EXPECT_EQ(cell("cancel", "Shipped")["kind"], "against");
  EXPECT_EQ(cell("cancel", "Shipped")["text"], "Desk::rush");
  EXPECT_EQ(cell("cancel", "Shipped")["title"], "Desk::rush calls cancel when it can be Shipped: the machine has no jump here");
  EXPECT_EQ(cell("cancel", "Cancelled")["kind"], "none");  // nobody calls it there
}

TEST_F(TestTableView, TheTableIsDrawnAsSvgAndAsTextWithTheCellAgainstTheMachineMarked) {
  const auto doc = ds::render::IrDocument::parse(json);
  const std::string svg = ds::render::Svg(kTheme).render(doc, 0);
  EXPECT_NE(svg.find("<g class=\"pr-cell pr-cell-head\"><rect"), std::string::npos);
  EXPECT_NE(svg.find(">Shipped</text>"), std::string::npos);
  EXPECT_NE(svg.find("<g class=\"pr-cell pr-cell-jump\"><title>pay takes Open to Paid</title>"), std::string::npos);
  EXPECT_NE(svg.find("<g class=\"pr-cell pr-cell-against\"><title>Desk::rush calls cancel when it can be Shipped"), std::string::npos);
  EXPECT_NE(svg.find("<title>pay is called in Desk::rush, Desk::sell</title>"), std::string::npos);  // on the heading of the row
  const std::string text = ds::render::Text(kTheme).render(doc, false, 0);
  EXPECT_NE(text.find(" Order  \xE2\x94\x82 Open      \xE2\x94\x82 Paid      \xE2\x94\x82 Shipped      \xE2\x94\x82 Cancelled"),
            std::string::npos)
      << text;
  EXPECT_NE(text.find(" cancel \xE2\x94\x82 Cancelled \xE2\x94\x82 Cancelled \xE2\x94\x82 ! Desk::rush \xE2\x94\x82 \xC2\xB7"),
            std::string::npos)
      << text;
  // the grid a terminal view asks for holds the same table
  ds::emit::JsonSink sink(false, true);
  ds::render::Text(kTheme).cells(doc, 0, sink);
  const auto cells = nlohmann::json::parse(sink.result());
  EXPECT_EQ(cells["rows"], 5);  // the headings, the rule, three events
  EXPECT_NE(cells["text"].get<std::string>().find("! Desk::rush"), std::string::npos);
}

TEST_F(TestTableView, ATableThatNamesNoMachineSaysSo) {
  ds::parse::Parser parser;
  ds::model::Model other{metamodel};
  ds::bind::Binder binder(other, metamodel);
  binder.bind(parser.parseText("a.ds",
                               "package p {\n  module m {\n    class Thing { }\n  }\n}\n"
                               "diagram MX { kind: table\n include: p@m::Thing }\n"));
  ds::check::CheckerFactory::make(other, metamodel)->run(binder.notes());
  const auto none = nlohmann::json::parse(ds::test::irJson(ds::render::IrExport(other, metamodel), "diagram:MX"));
  EXPECT_FALSE(none.contains("table"));
  EXPECT_NE(none["notice"].get<std::string>().find("No state machine selected"), std::string::npos);
}

TEST_F(TestTableView, ATableWrittenWithRowsAndColumnsAnswersEachCell) {
  ds::parse::Parser parser;
  ds::model::Model other{metamodel};
  ds::bind::Binder binder(other, metamodel);
  binder.bind(parser.parseText(
      "a.ds",
      "package p {\n"
      "  requirement REQ-00001-1 {\n    title: The thing shall count\n    status: approved\n"
      "    verification VER-00001-1 {\n      title: Tests count\n      status: approved\n      method: review\n    }\n  }\n"
      "  requirement REQ-00002-1 {\n    title: The thing shall rest\n    status: draft\n  }\n"
      "  module m {\n    class Thing {\n      @requirement REQ-00001-1\n    }\n  }\n}\n"
      "diagram TB {\n  kind: table\n  for requirement r in p\n  column requirement: r.id\n  column says: r.title\n"
      "  column proved: v where v verifies r\n  column kept: holds c satisfies r\n}\n"
      "diagram TD {\n  kind: table\n  for requirement r in p\n  where r.status = \"draft\"\n  column requirement: r.id\n}\n"));
  ds::check::CheckerFactory::make(other, metamodel)->run(binder.notes());
  const auto t = nlohmann::json::parse(ds::test::irJson(ds::render::IrExport(other, metamodel), "diagram:TB"))["table"];
  EXPECT_EQ(t["corner"], "requirement");
  EXPECT_EQ(t["columns"], (std::vector<std::string>{"says", "proved", "kept"}));
  ASSERT_EQ(t["rows"].size(), 2u);
  EXPECT_EQ(t["rows"][0]["label"], "REQ-00001-1");
  EXPECT_EQ(t["rows"][0]["cells"][0]["text"], "The thing shall count");
  EXPECT_EQ(t["rows"][0]["cells"][1]["text"], "VER-00001-1");
  EXPECT_EQ(t["rows"][0]["cells"][2]["text"], "yes");
  EXPECT_EQ(t["rows"][1]["cells"][1]["kind"], "none");
  EXPECT_EQ(t["rows"][1]["cells"][2]["text"], "no");
  EXPECT_EQ(t["rows"][1]["cells"][2]["kind"], "against");
  // a filter reads a field of the row: only the drafts
  const auto drafts = nlohmann::json::parse(ds::test::irJson(ds::render::IrExport(other, metamodel), "diagram:TD"))["table"];
  ASSERT_EQ(drafts["rows"].size(), 1u);
  EXPECT_EQ(drafts["rows"][0]["label"], "REQ-00002-1");
}

/// A column reads the same terms as a rule (DS-437): name(x), id(x) and kind(x) say what the entity of the row is.
TEST_F(TestTableView, AColumnReadsWhatTheEntityOfTheRowIsAsARuleDoes) {
  ds::parse::Parser parser;
  ds::model::Model other{metamodel};
  ds::bind::Binder binder(other, metamodel);
  binder.bind(parser.parseText(
      "a.ds",
      "package p {\n  module m {\n    class Cart {\n    }\n    enum Status { Open }\n  }\n}\n"
      "diagram TK {\n  kind: table\n  for class c in p\n  column entity: name(c)\n  column is: kind(c)\n  column identity: id(c)\n}\n"));
  ds::check::CheckerFactory::make(other, metamodel)->run(binder.notes());
  for (const auto& f : other.findings()) EXPECT_NE(f.code, "E-SYNTAX") << f.message;
  const auto t = nlohmann::json::parse(ds::test::irJson(ds::render::IrExport(other, metamodel), "diagram:TK"))["table"];
  EXPECT_EQ(t["corner"], "entity");
  EXPECT_EQ(t["columns"], (std::vector<std::string>{"is", "identity"}));
  ASSERT_EQ(t["rows"].size(), 1u);
  EXPECT_EQ(t["rows"][0]["label"], "Cart");
  EXPECT_EQ(t["rows"][0]["cells"][0]["text"], "class");
  EXPECT_EQ(t["rows"][0]["cells"][1]["text"], "p@m::Cart");
}

}  // namespace
