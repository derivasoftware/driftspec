#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <set>
#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/query/Filter.hpp"
#include "ds/render/IrExport.hpp"

namespace {

/// The cut of a diagram as it is drawn and as it is asked (DS-229), over examples/cut: one shop
/// through several cuts.
class TestCutDrawing : public ::testing::Test {
 protected:
  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/cut")) binder.bind(f);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, checkSteps).run(binder);
  }

  nlohmann::json diagram(const std::string& name) {
    return nlohmann::json::parse(ds::render::IrExport(model).render("diagram:" + name, false));
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

  ds::model::Model model;
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
    const auto filter = ds::query::filterFor(*model.find(std::string("diagram:") + name));
    EXPECT_EQ(filter->select(model), ids(diagram(name))) << name;
  }
}

}  // namespace
