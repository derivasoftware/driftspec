#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/IrExport.hpp"

namespace {

/// A diagram opens the boxes it names and closes the rest, over examples/visibility.
class TestOpenBoxes : public ::testing::Test {
 protected:
  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/visibility")) binder.bind(f);
    ds::check::Checker(model).run(binder);
  }

  nlohmann::json diagram(const std::string& name) {
    return nlohmann::json::parse(ds::render::IrExport(model).render("diagram:" + name, false));
  }

  static const nlohmann::json* node(const nlohmann::json& ir, const std::string& id) {
    for (const auto& n : ir["nodes"]) {
      if (n["id"] == id) return &n;
    }
    return nullptr;
  }

  static bool edge(const nlohmann::json& ir, const std::string& from, const std::string& to) {
    for (const auto& e : ir["edges"]) {
      if (e["from"] == from && e["to"] == to) return true;
    }
    return false;
  }

  ds::model::Model model;
};

TEST_F(TestOpenBoxes, OpenAloneClosesEveryBox) {
  const auto ir = diagram("HLD_Shop");
  ASSERT_EQ(ir["nodes"].size(), 2U);  // store and front, and nothing of what is inside them
  EXPECT_EQ((*node(ir, "shop@store"))["kind"], "box");
  EXPECT_EQ((*node(ir, "shop@front"))["kind"], "box");
}

TEST_F(TestOpenBoxes, AClosedBoxShowsOnlyItsPublicChildren) {
  const auto ir = diagram("HLD_Shop");
  const std::string shown = (*node(ir, "shop@store"))["sections"].dump();
  EXPECT_NE(shown.find("stock"), std::string::npos);
  EXPECT_EQ(shown.find("internal"), std::string::npos);  // a private module
}

TEST_F(TestOpenBoxes, EdgesLiftToTheBoxesThatAreSeenAndOnlyOnce) {
  const auto ir = diagram("HLD_Shop");
  EXPECT_TRUE(edge(ir, "shop@front", "shop@store"));
  EXPECT_EQ(ir["edges"].size(), 1U);  // what stays inside a closed box is not drawn
  EXPECT_EQ(ir["edges"][0]["kind"], "dependency");
}

TEST_F(TestOpenBoxes, OpeningABoxShowsTheWayToItAndClosesTheRest) {
  const auto ir = diagram("HLD_Store");
  EXPECT_EQ((*node(ir, "shop@store"))["kind"], "container");
  EXPECT_EQ((*node(ir, "shop@store@stock"))["kind"], "container");
  EXPECT_NE(node(ir, "shop@store@stock::Inventory"), nullptr);
  EXPECT_EQ((*node(ir, "shop@store@internal"))["kind"], "box");  // its sibling stays closed
  EXPECT_EQ((*node(ir, "shop@front"))["kind"], "box");
  EXPECT_EQ(node(ir, "shop@front@till::Till"), nullptr);
}

TEST_F(TestOpenBoxes, AnEdgeReachesInsideAnOpenBoxFromAClosedOne) {
  const auto ir = diagram("HLD_Store");
  EXPECT_TRUE(edge(ir, "shop@front", "shop@store@stock::Inventory"));
  EXPECT_TRUE(edge(ir, "shop@store@stock::Inventory", "shop@store@internal"));
}

TEST_F(TestOpenBoxes, ADiagramThatDoesNotSayOpenDrawsEverythingOpen) {
  ds::model::Model plain;
  ds::parse::Parser parser;
  ds::bind::Binder binder(plain);
  for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/static")) binder.bind(f);
  ds::check::Checker(plain).run(binder);
  const auto ir = nlohmann::json::parse(ds::render::IrExport(plain).render("diagram:CL_Model", false));
  EXPECT_EQ(ir["nodes"][0]["kind"], "container");
}

TEST_F(TestOpenBoxes, DepthStarIsEveryLevelAndNeverAnAbort) {
  const auto ir = diagram("SEQ_All");
  EXPECT_FALSE(ir["edges"].empty());
}

TEST_F(TestOpenBoxes, ADepthThatIsNoNumberIsAFindingAndStillDraws) {
  bool found = false;
  for (const auto& f : model.findings()) found = found || (f.code == "W-DIAGRAM-DEPTH" && f.message.find("lots") != std::string::npos);
  EXPECT_TRUE(found);
  EXPECT_FALSE(diagram("HLD_Bad")["edges"].empty());
}

}  // namespace
