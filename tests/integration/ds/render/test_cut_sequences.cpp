#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/query/Filter.hpp"
#include "ds/render/IrExport.hpp"

namespace {

/// A sequence and a requirement diagram on the cut of a class diagram (DS-231), over tests/corpus/cut: one
/// purchase that goes from the shell through the sales, the warehouse and the bank.
class TestCutSequences : public ::testing::Test {
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

  /// The labels of the lifelines, in the order drawn.
  static std::vector<std::string> lifelines(const nlohmann::json& ir) {
    std::vector<std::string> out;
    for (const auto& n : ir["nodes"])
      if (n["kind"] == "lifeline") out.push_back(n["label"]);
    return out;
  }

  /// The calls drawn, in order: the arrows that are not an answer.
  static std::vector<std::string> calls(const nlohmann::json& ir) {
    std::vector<std::string> out;
    for (const auto& e : ir["edges"])
      if (!e.value("dashed", false)) out.push_back(e["label"]);
    return out;
  }

  static bool message(const nlohmann::json& ir, const std::string& from, const std::string& to, const std::string& label) {
    for (const auto& e : ir["edges"])
      if (e["from"] == from && e["to"] == to && e["label"] == label) return true;
    return false;
  }

  /// What the lifelines stand for: the box of a closed one, the type of an object, without the caller.
  static std::set<std::string> participants(const nlohmann::json& ir) {
    std::set<std::string> out;
    for (const auto& n : ir["nodes"])
      if (n["kind"] == "lifeline" && n.contains("title")) out.insert(n["title"].get<std::string>());
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

using Labels = std::vector<std::string>;

TEST_F(TestCutSequences, WithNoCutEveryObjectIsALifelineAndDepthSaysHowFar) {
  const auto ir = diagram("SEQ_Classes");
  EXPECT_EQ(lifelines(ir).size(), 10U);
  EXPECT_EQ(calls(ir).size(), 10U);  // every call of the purchase, down to the ledger
}

TEST_F(TestCutSequences, EachClosedBoxIsOneLifelineForEverythingInsideIt) {
  const auto ir = diagram("SEQ_Open");  // open: shop@sales
  EXPECT_EQ(lifelines(ir), (Labels{"caller", "self: shell", "desk", "warehouse", "bank", "till"}));
}

TEST_F(TestCutSequences, AMessageInsideAClosedBoxIsNotDrawnAndOneThatCrossesSaysWhoTakesIt) {
  const auto ir = diagram("SEQ_Open");
  EXPECT_EQ(calls(ir), (Labels{"buy(item: string)", "Desk::sell(item)", "counter::take(item)", "gateway::pay(cost)", "Till::charge(price)",
                               "gateway::pay(amount)"}));  // no priceOf, pick, reorder nor record: each stays inside a box
  EXPECT_TRUE(message(ir, "shop@sales@desk", "shop@warehouse", "counter::take(item)"));
  EXPECT_TRUE(message(ir, "shop@warehouse", "shop@bank", "gateway::pay(cost)"));  // three calls down, and it crosses
}

TEST_F(TestCutSequences, AFrameIsKeptAroundWhatCrossesInsideIt) {
  const auto ir = diagram("SEQ_Open");
  int frames = 0;
  for (const auto& n : ir["nodes"]) {
    if (n["kind"] != "frame") continue;
    ++frames;
    EXPECT_EQ(n["label"], "opt it was the last one");
    EXPECT_EQ(n["span"][0], n["span"][1].get<int>() - 1);  // the payment and its answer
  }
  EXPECT_EQ(frames, 1);
}

TEST_F(TestCutSequences, TheCutRunsAtAKindOfBoxAndWhatIsAboveItStaysOpen) {
  const auto ir = diagram("SEQ_Modules");  // close: module
  EXPECT_EQ(lifelines(ir), (Labels{"caller", "self: shell", "desk", "counter", "stock", "supply", "bank: gateway", "till"}));
  EXPECT_TRUE(message(ir, "shop@warehouse@counter|counter", "shop@warehouse@stock", "Shelf::pick(item)"));
}

TEST_F(TestCutSequences, ASequenceTakesTheCutOfTheBoxesDrawnAndStandsOnIt) {
  const auto ir = diagram("SEQ_Borrowed");  // cut: HLD_Till
  const Labels drawn = lifelines(ir);
  EXPECT_NE(std::find(drawn.begin(), drawn.end(), "till: Till"), drawn.end());  // the till is open there: its objects
  EXPECT_NE(std::find(drawn.begin(), drawn.end(), "desk"), drawn.end());        // the desk is closed there: the box
  const auto filter = ds::query::Filters::of(metamodel, *model.find("diagram:HLD_Till"));
  for (const auto& id : participants(ir)) EXPECT_EQ(filter->select(model).count(id), 1U) << id;  // each is a box of that drawing
}

TEST_F(TestCutSequences, UnfoldAloneFoldsEveryOtherCall) {
  EXPECT_EQ(calls(diagram("SEQ_AsWritten")), (Labels{"buy(item: string)", "Desk::sell(item)"}));  // unfold: with nothing after it
  EXPECT_EQ(calls(diagram("SEQ_UnfoldSales")), (Labels{"buy(item: string)", "Desk::sell(item)", "take(item)", "Till::charge(price)",
                                                       "pay(amount)"}));  // not what the warehouse does
}

TEST_F(TestCutSequences, FoldStopsAtWhatItNamesAndEveryOtherCallIsFollowed) {
  EXPECT_EQ(calls(diagram("SEQ_FoldSupply")), (Labels{"buy(item: string)", "Desk::sell(item)", "take(item)", "Shelf::pick(item)",
                                                      "Buyer::reorder(item)", "Till::charge(price)", "pay(amount)"}));  // no pay(cost)
}

TEST_F(TestCutSequences, UnfoldGoesThroughWhatFoldFolds) {
  // fold: shop@sales, unfold: shop@sales@desk: the desk is followed, the till is not
  EXPECT_EQ(calls(diagram("SEQ_Through")), (Labels{"buy(item: string)", "Desk::sell(item)", "take(item)", "Shelf::pick(item)",
                                                   "Buyer::reorder(item)", "pay(cost)", "Till::charge(price)"}));
}

TEST_F(TestCutSequences, DepthIsTheUniformCaseOnACutToo) {
  EXPECT_EQ(calls(diagram("SEQ_OneLevel")), (Labels{"buy(item: string)", "Desk::sell(item)", "take(item)", "Till::charge(price)"}));
}

TEST_F(TestCutSequences, TheQueryShowsWhoTakesPartOnTheCut) {
  for (const char* name : {"SEQ_Classes", "SEQ_Open", "SEQ_Modules", "SEQ_Borrowed", "SEQ_AsWritten", "SEQ_UnfoldSales", "SEQ_FoldSupply",
                           "SEQ_Through", "SEQ_OneLevel"}) {
    const auto filter = ds::query::Filters::of(metamodel, *model.find(std::string("diagram:") + name));
    EXPECT_EQ(filter->select(model), participants(diagram(name))) << name;
  }
}

TEST_F(TestCutSequences, ACallPatternThatNamesNothingAndTheRetiredLevelAreReported) {
  EXPECT_EQ(findings("W-DIAGRAM-UNDECLARED", "SEQ_Cellar unfolds shop@cellar"), 1);
  EXPECT_EQ(findings("W-FIELD-UNKNOWN", "SEQ_Level"), 1);  // level: module is written close: module
}

TEST_F(TestCutSequences, WhatAnswersToARequirementIsTheBoxThatStandsForIt) {
  const auto classes = diagram("REQ_Classes");
  const auto modules = diagram("REQ_Modules");  // close: module
  const auto has = [](const nlohmann::json& ir, const std::string& id) {
    for (const auto& n : ir["nodes"])
      if (n["id"] == id) return true;
    return false;
  };
  EXPECT_TRUE(has(classes, "shop@sales@desk::Desk"));
  EXPECT_TRUE(has(classes, "shop@warehouse@stock::Shelf"));  // a method answers through its class
  EXPECT_TRUE(has(modules, "shop@sales@desk"));
  EXPECT_FALSE(has(modules, "shop@sales@desk::Desk"));
  EXPECT_TRUE(message(modules, "REQ-00002-1", "shop@warehouse@stock", "satisfied by"));
  const auto filter = ds::query::Filters::of(metamodel, *model.find("diagram:REQ_Modules"));
  std::set<std::string> drawn;
  for (const auto& n : modules["nodes"]) drawn.insert(n["id"].get<std::string>());
  EXPECT_EQ(filter->select(model), drawn);
}

}  // namespace
