#include <gtest/gtest.h>

#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/IrExport.hpp"

namespace {

/// A diagram opens the boxes it names and closes the rest, over tests/corpus/visibility.
class TestOpenBoxes : public ::testing::Test {
 protected:
  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/visibility")) binder.bind(f);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
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

  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
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

TEST_F(TestOpenBoxes, EdgesLiftToTheBoxesThatAreSeenAndOnlyOnceForEachBond) {
  const auto ir = diagram("HLD_Shop");
  EXPECT_TRUE(edge(ir, "shop@front", "shop@store"));
  ASSERT_EQ(ir["edges"].size(), 2U);  // what stays inside a closed box is not drawn, and the rest is said twice at most
  std::set<std::string> kinds;
  for (const auto& e : ir["edges"]) kinds.insert(e["kind"].get<std::string>());
  EXPECT_EQ(kinds, (std::set<std::string>{"association", "dependency"}));  // a structural bond and a use (DS-230)
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
  ds::model::Model plain{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(plain, metamodel);
  for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/static")) binder.bind(f);
  const auto checkSteps = ds::check::steps();
  ds::check::Checker(plain, metamodel, checkSteps).run(binder.notes());
  const auto ir = nlohmann::json::parse(ds::render::IrExport(plain).render("diagram:CL_Model", false));
  EXPECT_EQ(ir["nodes"][0]["kind"], "container");
}

TEST(TestOpenBoxesBehaviour, ABehaviourThatCrossesBoxesIsAnEdgeOfTheClosedDrawing) {
  const ds::model::Metamodel metamodel;
  ds::model::Model shop{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(shop, metamodel);
  for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/interfaces")) binder.bind(f);
  const auto checkSteps = ds::check::steps();
  ds::check::Checker(shop, metamodel, checkSteps).run(binder.notes());
  const auto ir = nlohmann::json::parse(ds::render::IrExport(shop).render("diagram:HLD_Shop", false));
  bool reached = false;
  for (const auto& e : ir["edges"]) reached = reached || (e["from"] == "shop@shell" && e["to"] == "shop@till");
  EXPECT_TRUE(reached);  // no type says it: only the sequence of shell::buy calls till::main
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

TEST_F(TestOpenBoxes, APatternThatNamesWhatNothingDeclaresIsAFinding) {
  int found = 0;
  for (const auto& f : model.findings())
    found += f.code == "W-DIAGRAM-UNDECLARED" && f.message.find("shop@store@shelf::*") != std::string::npos;
  EXPECT_EQ(found, 1);
}

TEST(TestDiagramPatterns, ARequirementDiagramReadsAModulePatternAsAClassDiagramDoes) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  binder.bind(parser.parseText("a.ds",
                               "package shop {\n"
                               "  module store {\n"
                               "    requirement REQ-00001-1: The store shall keep its items\n"
                               "    class Cart {\n"
                               "      @requirement REQ-00001-1\n"
                               "    }\n"
                               "  }\n"
                               "  module empty { }\n"
                               "}\n"
                               "diagram REQ_Store {\n"
                               "  kind: requirement\n"
                               "  include: shop@store::*\n"
                               "}\n"
                               "diagram REQ_Empty {\n"
                               "  kind: requirement\n"
                               "  include: shop@empty::*\n"
                               "}\n"));
  const auto checkSteps = ds::check::steps();
  ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  const auto ir = nlohmann::json::parse(ds::render::IrExport(model).render("diagram:REQ_Store", false));
  bool requirement = false;
  for (const auto& n : ir["nodes"]) requirement = requirement || n["id"] == "REQ-00001-1";
  EXPECT_TRUE(requirement);
  for (const auto& f : model.findings()) EXPECT_NE(f.code, "W-DIAGRAM-UNDECLARED") << f.message;  // an empty box is no broken name
}

TEST(TestEveryBox, ABoxIsDrawnWithOrWithoutChildren) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  binder.bind(parser.parseText("a.ds",
                               "package shop {\n"
                               "  package sales {\n"
                               "    @consumes shop@warehouse@counter\n"
                               "  }\n"
                               "  package warehouse {\n"
                               "    interface counter {\n"
                               "      kind: library\n"
                               "      operation take(item: string) -> bool\n"
                               "    }\n"
                               "  }\n"
                               "  module empty { }\n"
                               "}\n"
                               "diagram HLD_Shop {\n"
                               "  kind: class\n"
                               "  include: shop@*\n"
                               "}\n"));
  const auto checkSteps = ds::check::steps();
  ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  const auto ir = nlohmann::json::parse(ds::render::IrExport(model).render("diagram:HLD_Shop", false));
  std::map<std::string, std::string> parentOf;
  for (const auto& n : ir["nodes"]) parentOf[n["id"]] = n.contains("parent") ? n["parent"].get<std::string>() : "";
  EXPECT_TRUE(parentOf.count("shop@sales"));                        // a box with nothing inside
  EXPECT_TRUE(parentOf.count("shop@empty"));                        // a module with nothing inside
  EXPECT_EQ(parentOf["shop@warehouse@counter"], "shop@warehouse");  // an interface sits in its box
}

TEST(TestDependencyDrawing, OneDependencyIsDrawnAlikeWithOrWithoutOpenAndADeclaredUseIsDrawn) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  binder.bind(parser.parseText("a.ds",
                               "package shop {\n"
                               "  package front {\n"
                               "    @consumes shop@back@orders\n"
                               "    module web {\n"
                               "      class Page {\n"
                               "        + show(id: string) -> none\n"
                               "      }\n"
                               "      sequence Page::show {\n"
                               "        local ledger: shop@back@store::Ledger\n"
                               "        self -> ledger.find(id) -> order\n"
                               "      }\n"
                               "    }\n"
                               "  }\n"
                               "  package back {\n"
                               "    interface orders {\n"
                               "      kind: library\n"
                               "      operation find(id: string) -> string\n"
                               "    }\n"
                               "    module store {\n"
                               "      class Ledger {\n"
                               "        + find(id: string) -> string\n"
                               "      }\n"
                               "    }\n"
                               "  }\n"
                               "}\n"
                               "diagram CL_Open {\n"
                               "  kind: class\n"
                               "  include: shop@*\n"
                               "}\n"
                               "diagram CL_Named {\n"
                               "  kind: class\n"
                               "  include: shop@*\n"
                               "  open: shop@front, shop@front@web, shop@back, shop@back@store\n"
                               "}\n"));
  const auto checkSteps = ds::check::steps();
  ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  const auto edges = [&model](const std::string& d) {
    std::set<std::string> out;
    const auto ir = nlohmann::json::parse(ds::render::IrExport(model).render("diagram:" + d, false));
    for (const auto& e : ir["edges"]) out.insert(e["from"].get<std::string>() + " -> " + e["to"].get<std::string>());
    return out;
  };
  const auto open = edges("CL_Open");
  EXPECT_EQ(open, edges("CL_Named"));
  EXPECT_TRUE(open.count("shop@front@web::Page -> shop@back@store::Ledger"));  // the subject, not its module
  EXPECT_FALSE(open.count("shop@front@web -> shop@back@store::Ledger"));
  EXPECT_TRUE(open.count("shop@front -> shop@back@orders"));  // the declared @consumes
}

}  // namespace
