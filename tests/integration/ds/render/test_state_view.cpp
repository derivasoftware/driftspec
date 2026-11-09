#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
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
    ds::bind::Binder binder(model);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/" + corpus)) binder.bind(f);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, checkSteps).run(binder);
    return nlohmann::json::parse(ds::render::IrExport(model).render("diagram:" + diagram, false));
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

  ds::model::Model model;
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
  const ds::render::LayoutResult out = ds::render::layout(doc);
  ASSERT_EQ(out.routes.count(0), 1U);
  ASSERT_EQ(out.routes.at(0).points.size(), 4U);
  EXPECT_EQ(out.routes.at(0).points.front().x, out.routes.at(0).points.back().x);
  EXPECT_GT(out.routes.at(0).points[1].x, out.routes.at(0).points.front().x);
}

}  // namespace
