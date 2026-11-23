#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/Ir.hpp"
#include "ds/render/IrExport.hpp"
#include "ds/render/Layout.hpp"

namespace {

/// A state machine drawn, of an interface (its navigation) and of a class.
class TestStateView : public ::testing::Test {
 protected:
  nlohmann::json draw(const std::string& corpus, const std::string& diagram) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/" + corpus)) binder.bind(f);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
    return nlohmann::json::parse(ds::render::IrExport(model, metamodel).render("diagram:" + diagram, false));
  }

  static const nlohmann::json* edge(const nlohmann::json& ir, const std::string& from, const std::string& to) {
    for (const auto& e : ir["edges"]) {
      if (e["from"] == from && e["to"] == to) return &e;
    }
    return nullptr;
  }

  static const nlohmann::json* node(const nlohmann::json& ir, const std::string& id) {
    for (const auto& n : ir["nodes"]) {
      if (n["id"] == id) return &n;
    }
    return nullptr;
  }

  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
};

TEST_F(TestStateView, TheViewsAreBoxesWithWhatTheyShowAndWhatBindsThem) {
  const auto ir = draw("interfaces", "ST_Web");
  const auto* home = node(ir, "shop@web::state::Home");
  ASSERT_NE(home, nullptr);
  EXPECT_EQ((*home)["classifier"], "state");
  const std::string text = (*home)["sections"].dump();
  EXPECT_NE(text.find("A field for the code"), std::string::npos);
  EXPECT_NE(text.find("SREQ-00001-1"), std::string::npos);
  EXPECT_NE(text.find("CST-00001-1"), std::string::npos);
}

TEST_F(TestStateView, TheJumpsAreEdgesWithTheirEventAndGuard) {
  const auto ir = draw("interfaces", "ST_Web");
  const auto* buy = edge(ir, "shop@web::state::Home", "shop@web::state::Done");
  ASSERT_NE(buy, nullptr);
  EXPECT_EQ((*buy)["label"], "buy");
  const auto* stay = edge(ir, "shop@web::state::Home", "shop@web::state::Home");
  ASSERT_NE(stay, nullptr);
  EXPECT_EQ((*stay)["label"], "buy [refused]");
}

TEST_F(TestStateView, TheMachineStartsFromItsInitialState) {
  const auto ir = draw("interfaces", "ST_Web");
  EXPECT_NE(edge(ir, "shop@web::state::initial", "shop@web::state::Home"), nullptr);
  EXPECT_EQ(ir["nodes"][0]["id"], "shop@web::state::initial");
  EXPECT_EQ(ir["nodes"][1]["id"], "shop@web::state::Home");
}

TEST_F(TestStateView, AViewNoJumpNamesIsStillDrawn) {
  const auto ir = draw("interfaces", "ST_Web");
  EXPECT_NE(node(ir, "shop@web::state::Lost"), nullptr);
  EXPECT_NE(node(ir, "shop@web::state::Ghost"), nullptr);  // and what a jump names without a view
}

TEST_F(TestStateView, TheMachineOfAClassIsDrawnFromItsTransitionsAlone) {
  const auto ir = draw("behaviour", "STM_Checker");
  EXPECT_GE(ir["nodes"].size(), 2U);
  EXPECT_FALSE(ir["edges"].empty());
  EXPECT_EQ(ir.count("notice"), 0U);
}

TEST(TestSelfLoop, ARelationOfABoxWithItselfIsRoutedOutOfItsSideAndBack) {
  ds::render::IrDocument doc;
  doc.nodes.push_back({});
  doc.nodes[0].id = "a";
  doc.nodes[0].kind = "box";
  doc.nodes[0].label = "a";
  ds::render::IrEdge loop;
  loop.from = "a";
  loop.to = "a";
  loop.kind = "association";
  doc.edges.push_back(loop);
  const ds::render::LayoutResult out = ds::render::Layout().place(doc);
  ASSERT_EQ(out.routes.count(0), 1U);
  ASSERT_EQ(out.routes.at(0).points.size(), 4U);
  EXPECT_EQ(out.routes.at(0).points.front().x, out.routes.at(0).points.back().x);
  EXPECT_GT(out.routes.at(0).points[1].x, out.routes.at(0).points.front().x);
}

// DS-249: a state a rule computes is drawn with its rule
TEST_F(TestStateView, AStateIsDrawnWithTheRuleThatComputesIt) {
  const auto ir = draw("states", "STM_Order");
  const auto* open = node(ir, "shop@order::Order::state::Open");
  ASSERT_NE(open, nullptr);
  EXPECT_EQ((*open)["sections"][0][0], "if status = Open, not cancelled");
  const auto* cancelled = node(ir, "shop@order::Order::state::Cancelled");
  ASSERT_NE(cancelled, nullptr);
  EXPECT_EQ((*cancelled)["sections"][0][0], "if cancelled, status != Shipped");
  // the jumps are drawn as they always were
  const auto* pay = edge(ir, "shop@order::Order::state::Open", "shop@order::Order::state::Paid");
  ASSERT_NE(pay, nullptr);
  EXPECT_EQ((*pay)["label"], "pay");
}

TEST_F(TestStateView, AStateWithNoJumpIsStillDrawnAndOneWithTwoRulesShowsBoth) {
  const auto ir = draw("states", "STM_Lamp");
  const auto* dark = node(ir, "shop@order::Lamp::state::Dark");
  ASSERT_NE(dark, nullptr);
  ASSERT_EQ((*dark)["sections"][0].size(), 2u);
  EXPECT_EQ((*dark)["sections"][0][0], "if not lit");
  EXPECT_EQ((*dark)["sections"][0][1], "if broken, lit");
  EXPECT_NE(node(ir, "shop@order::Lamp::state::On"), nullptr);
}

// DS-249: nobody wrote `initial`: the machine starts in the state its subject's defaults are in
TEST_F(TestStateView, AMachineWithNoStartWrittenStartsWhereTheDefaultsOfItsSubjectAre) {
  const auto order = draw("states", "STM_Order");  // status = Open, cancelled = false: Open
  EXPECT_NE(edge(order, "shop@order::Order::state::initial", "shop@order::Order::state::Open"), nullptr);
  EXPECT_EQ(edge(order, "shop@order::Order::state::initial", "shop@order::Order::state::Paid"), nullptr);
  const auto lamp = draw("states", "STM_Lamp");  // lit = false: Dark
  EXPECT_NE(edge(lamp, "shop@order::Lamp::state::initial", "shop@order::Lamp::state::Dark"), nullptr);
}

// DS-250: a jump says the behaviours that call its event
TEST_F(TestStateView, AJumpSaysTheBehavioursThatCallItsEventAndOneNobodyCallsSaysNothing) {
  const auto order = draw("states", "STM_Order");
  const auto* pay = edge(order, "shop@order::Order::state::Open", "shop@order::Order::state::Paid");
  ASSERT_NE(pay, nullptr);
  EXPECT_EQ((*pay)["label"], "pay");  // the label stays as it is written
  EXPECT_EQ((*pay)["title"], "pay is called in Desk::rush, Desk::sell");
  const auto* ship = edge(order, "shop@order::Order::state::Paid", "shop@order::Order::state::Shipped");
  ASSERT_NE(ship, nullptr);
  EXPECT_EQ((*ship)["title"], "ship is called in Desk::close, Desk::label, Desk::rush, Desk::sell");
  // the navigation of an interface, whose events no behaviour calls: its jumps say nothing more
  const auto web = draw("interfaces", "ST_Web");
  ASSERT_FALSE(web["edges"].empty());
  for (const auto& e : web["edges"]) EXPECT_FALSE(e.contains("title")) << e;
}

}  // namespace
