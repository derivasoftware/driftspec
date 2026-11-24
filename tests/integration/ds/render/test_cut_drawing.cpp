#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <utility>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/query/Filter.hpp"
#include "ds/render/IrExport.hpp"

namespace {

/// The cut of a diagram as it is drawn and as it is asked (DS-229), over tests/corpus/cut: one shop
/// through several cuts.
class TestCutDrawing : public ::testing::Test {
 protected:
  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/cut")) binder.bind(f);
    const auto checkSteps = ds::check::Steps::of();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  }

  nlohmann::json diagram(const std::string& name) {
    return nlohmann::json::parse(ds::render::IrExport(model, metamodel).render("diagram:" + name, false));
  }

  static std::string kindOf(const nlohmann::json& ir, const std::string& id) {
    for (const auto& n : ir["nodes"]) {
      if (n["id"] == id) return n["kind"];
    }
    return "";
  }

  static bool edge(const nlohmann::json& ir, const std::string& from, const std::string& to) {
    for (const auto& e : ir["edges"]) {
      if (e["from"] == from && e["to"] == to) return true;
    }
    return false;
  }

  static std::set<std::string> ids(const nlohmann::json& ir) {
    std::set<std::string> out;
    for (const auto& n : ir["nodes"]) out.insert(n["id"].get<std::string>());
    return out;
  }

  int findings(const std::string& code, const std::string& about) const {
    int found = 0;
    for (const auto& f : model.findings()) found += f.code == code && f.message.find(about) != std::string::npos;
    return found;
  }

  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
};

TEST_F(TestCutDrawing, CloseClosesTheBoxItNamesAndTheRestStaysOpen) {
  const auto ir = diagram("HLD_Close");
  EXPECT_EQ(kindOf(ir, "shop@warehouse"), "box");
  EXPECT_EQ(kindOf(ir, "shop@warehouse@stock"), "");  // what a closed box holds is not drawn
  EXPECT_EQ(kindOf(ir, "shop@sales"), "container");
  EXPECT_EQ(kindOf(ir, "shop@sales@desk"), "container");
  EXPECT_EQ(kindOf(ir, "shop@sales@desk::Desk"), "box");
}

TEST_F(TestCutDrawing, AnEdgeThatLosesAnEndToAClosedBoxIsLiftedToIt) {
  const auto ir = diagram("HLD_Close");
  EXPECT_TRUE(edge(ir, "shop@sales@desk::Desk", "shop@warehouse"));  // the desk refers to the counter, inside the warehouse
}

TEST_F(TestCutDrawing, AKindClosesTheBoxesOfThatKindWhereverTheyAre) {
  const auto ir = diagram("HLD_Modules");
  for (const char* module : {"shop@sales@desk", "shop@sales@till", "shop@warehouse@stock", "shop@warehouse@supply"})
    EXPECT_EQ(kindOf(ir, module), "box") << module;
  for (const char* package : {"shop@sales", "shop@warehouse", "shop@bank"}) EXPECT_EQ(kindOf(ir, package), "container") << package;
  EXPECT_EQ(kindOf(ir, "shop@warehouse@counter"), "box");  // an interface is no module: drawn as it is
  const auto packages = diagram("HLD_Packages");
  EXPECT_EQ(ids(packages), (std::set<std::string>{"shop@bank", "shop@sales", "shop@shell", "shop@warehouse"}));
}

TEST_F(TestCutDrawing, OpenGoesThroughWhatCloseCloses) {
  const auto ir = diagram("HLD_Till");
  EXPECT_EQ(kindOf(ir, "shop@sales@till"), "container");
  EXPECT_EQ(kindOf(ir, "shop@sales@till::Till"), "box");
  EXPECT_EQ(kindOf(ir, "shop@sales@desk"), "box");  // the other modules stay closed
  EXPECT_EQ(kindOf(ir, "shop@warehouse@stock"), "box");
}

TEST_F(TestCutDrawing, OpenAloneStillClosesEveryOtherBox) {
  const auto ir = diagram("HLD_Open");
  EXPECT_EQ(ids(ir),
            (std::set<std::string>{"shop@bank", "shop@sales", "shop@sales@desk", "shop@sales@till", "shop@shell", "shop@warehouse"}));
  EXPECT_EQ(kindOf(ir, "shop@sales"), "container");
  EXPECT_EQ(kindOf(ir, "shop@sales@desk"), "box");
}

TEST_F(TestCutDrawing, ADiagramDrawsTheCutItTakesFromAnotherAndAddsItsOwn) {
  EXPECT_EQ(ids(diagram("HLD_Borrowed")), ids(diagram("HLD_Till")));
  const auto more = diagram("HLD_BorrowedAndMore");
  EXPECT_EQ(kindOf(more, "shop@sales@desk"), "box");  // the modules close, as in the diagram it takes
  EXPECT_EQ(kindOf(more, "shop@warehouse@stock"), "container");
  EXPECT_EQ(kindOf(more, "shop@warehouse@stock::Shelf"), "box");
}

TEST_F(TestCutDrawing, ACloseThatNamesNoBoxIsAFindingAndAKindIsNone) {
  EXPECT_EQ(findings("W-DIAGRAM-UNDECLARED", "HLD_Cellar closes shop@cellar"), 1);
  EXPECT_EQ(findings("W-DIAGRAM-UNDECLARED", "module"), 0);  // a kind of box names no box, and is no broken reference
  EXPECT_EQ(findings("W-DIAGRAM-UNDECLARED", "package"), 0);
}

TEST_F(TestCutDrawing, ACutTakenFromADiagramNobodyDeclaresIsAFindingAndStillDraws) {
  EXPECT_EQ(findings("W-DIAGRAM-UNDECLARED", "HLD_Lost takes the cut of HLD_Nowhere"), 1);
  EXPECT_EQ(ids(diagram("HLD_Lost")), ids(diagram("CL_Everything")));
}

TEST_F(TestCutDrawing, TheQueryShowsWhatTheDrawingDrawsUnderEveryCut) {
  for (const char* name :
       {"CL_Everything", "HLD_Open", "HLD_Close", "HLD_Modules", "HLD_Till", "HLD_Packages", "HLD_Borrowed", "HLD_BorrowedAndMore"}) {
    const auto filter = ds::query::Filters::of(metamodel, *model.find(std::string("diagram:") + name));
    EXPECT_EQ(filter->select(model), ids(diagram(name))) << name;
  }
}

TEST_F(TestCutDrawing, ARelationLiftedOntoClosedBoxesIsOneOfTwoBondsAndOneArrowOfEachAtMost) {
  const auto ir = diagram("HLD_Packages");  // close: package
  std::multiset<std::string> salesToWarehouse, shellToSales;
  for (const auto& e : ir["edges"]) {
    if (e["from"] == "shop@sales" && e["to"] == "shop@warehouse") salesToWarehouse.insert(e["kind"].get<std::string>());
    if (e["from"] == "shop@shell" && e["to"] == "shop@sales") shellToSales.insert(e["kind"].get<std::string>());
  }
  // the desk keeps a reference to the counter, and calls it: a structural bond and a use, once each
  EXPECT_EQ(salesToWarehouse, (std::multiset<std::string>{"association", "dependency"}));
  EXPECT_EQ(shellToSales, (std::multiset<std::string>{"dependency"}));  // the shell only calls the desk
  for (const auto& e : ir["edges"]) EXPECT_TRUE(e["refs"]["bond"] == "structural" || e["refs"]["bond"] == "uses");
}

TEST_F(TestCutDrawing, AUseBetweenTwoBoxesSaysTheInterfacesItGoesThrough) {
  ds::parse::Parser parser;
  ds::model::Model other{metamodel};
  ds::bind::Binder binder(other, metamodel);
  binder.bind(parser.parseText("a.ds",
                               "package p {\n"
                               "  package tool {\n"
                               "    interface door {\n      kind: library\n      operation ask(question: string) -> string\n    }\n"
                               "    sequence door::ask {\n      return answer\n    }\n"
                               "    module core {\n      class Core {\n        + run() -> none\n      }\n    }\n"
                               "  }\n"
                               "  package app {\n"
                               "    module ui {\n"
                               "      class Screen {\n        @consumes p@tool@door\n        + show(question: string) -> string\n      }\n"
                               "      sequence Screen::show {\n        local door: p@tool@door\n        -> door.ask(question) -> answer\n  "
                               "      return answer\n      }\n"
                               "      class Plain {\n        + refs core: p@tool@core::Core\n      }\n"
                               "    }\n"
                               "  }\n"
                               "}\n"
                               "diagram HLD {\n  kind: class\n  include: p@*\n  close: package\n}\n"));
  const auto checkSteps = ds::check::Steps::of();
  ds::check::Checker(other, metamodel, checkSteps).run(binder.notes());
  const auto ir = nlohmann::json::parse(ds::render::IrExport(other, metamodel).render("diagram:HLD", false));
  std::string use, structural = "none";
  for (const auto& e : ir["edges"]) {
    if (e["from"] != "p@app" || e["to"] != "p@tool") continue;
    if (e["refs"]["bond"] == "uses") use = e.value("title", "");
    if (e["refs"]["bond"] == "structural") structural = e.value("title", "");
  }
  EXPECT_EQ(use, "door: ask");  // the use goes through the door, and asks
  EXPECT_EQ(structural, "");    // the reference to the core is a bond: it says nothing
}

TEST_F(TestCutDrawing, WithTheBoxesOpenEveryRelationKeepsItsOwnKind) {
  const auto ir = diagram("CL_Everything");
  std::set<std::string> kinds;
  for (const auto& e : ir["edges"]) kinds.insert(e["kind"].get<std::string>());
  EXPECT_EQ(kinds.count("composition"), 1U);  // the desk owns its price list
  EXPECT_EQ(kinds.count("aggregation"), 1U);  // and refers to the counter
  EXPECT_EQ(kinds.count("association"), 0U);  // nothing is lifted
}

TEST_F(TestCutDrawing, AClosedBoxListsWhatItOffersByKindEachMarkedWithWhatItIs) {
  const auto modules = diagram("HLD_Modules");    // close: module
  const auto packages = diagram("HLD_Packages");  // close: package
  const auto listed = [](const nlohmann::json& ir, const std::string& id) {
    for (const auto& n : ir["nodes"])
      if (n["id"] == id) return std::make_pair(n["sections"].dump(), n["sectionKinds"].dump());
    return std::make_pair(std::string(), std::string());
  };
  // a module: its classes, then its dataclasses, a compartment each
  EXPECT_EQ(listed(modules, "shop@warehouse@stock"),
            (std::make_pair(std::string(R"([["Shelf"],["Box"]])"), std::string(R"(["class","dataclass"])"))));
  // each compartment by name
  EXPECT_EQ(listed(modules, "shop@sales@till"), (std::make_pair(std::string(R"([["Ledger","Till"]])"), std::string(R"(["class"])"))));
  // a package: its modules, then its interfaces
  EXPECT_EQ(listed(packages, "shop@warehouse"),
            (std::make_pair(std::string(R"([["stock","supply"],["counter"]])"), std::string(R"(["namespace","interface"])"))));
}

/// A design with only its high level: three boxes and one scenario between them (DS-230).
TEST(TestWrittenLevel, AMessageWrittenBetweenTwoBoxesConnectsThemInTheDrawingOfTheBoxes) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  binder.bind(parser.parseText("shop.ds",
                               "package shop {\n"
                               "  usecase UC-00001-1 buy(item: string) -> int {\n"
                               "    title: Buy an item\n"
                               "    outcome default: bought\n"
                               "  }\n"
                               "  package sales { }\n"
                               "  package warehouse { }\n"
                               "  package bank { }\n"
                               "  sequence UC-00001-1 {\n"
                               "    -> sales.sell(item)\n"
                               "    sales -> warehouse.take(item)\n"
                               "    sales -> bank.pay(price)\n"
                               "  }\n"
                               "}\n"
                               "diagram HLD {\n  kind: class\n  include: shop@*\n}\n"));
  const auto steps = ds::check::Steps::of();
  ds::check::Checker(model, metamodel, steps).run(binder.notes());
  const auto ir = nlohmann::json::parse(ds::render::IrExport(model, metamodel).render("diagram:HLD", false));
  std::set<std::pair<std::string, std::string>> arrows;
  for (const auto& e : ir["edges"]) {
    EXPECT_EQ(e["kind"], "dependency");  // a message is a use
    arrows.insert({e["from"].get<std::string>(), e["to"].get<std::string>()});
  }
  // whoever asks for the use case is outside the design: no arrow into the sales from nowhere
  EXPECT_EQ(arrows, (std::set<std::pair<std::string, std::string>>{{"shop@sales", "shop@warehouse"}, {"shop@sales", "shop@bank"}}));
}

}  // namespace
