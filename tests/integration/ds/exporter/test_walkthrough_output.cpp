#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "ds/api/Api.hpp"
#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/query/Query.hpp"

namespace {

const char* const kShop =
    "package shop {\n"
    "  module desk {\n"
    "    class Desk {\n"
    "      + sell(item: string) -> int\n"
    "    }\n"
    "  }\n"
    "}\n"
    "diagram CL_Shop {\n"
    "  kind: class\n"
    "  include: shop@*\n"
    "}\n";

const char* const kWalk =
    "diagram WALK_Shop {\n"
    "  kind: walkthrough\n"
    "  title: The shop\n"
    "  step Who sells {\n"
    "    show: CL_Shop\n"
    "    focus: shop@desk::Desk\n"
    "    highlight: shop@desk::Desk::sell\n"
    "    at: shop@desk::Desk::sell\n"
    "    text: The desk sells.\n"
    "  }\n"
    "  step Again {\n"
    "    show: CL_Shop\n"
    "    text: Once more.\n"
    "  }\n"
    "}\n";

/// A walkthrough as each document gives it (REQ-00243-1, DS-471): built once, written by the export by identity and
/// by the scene by index, answered by the query, and absent from a workspace that writes none.
class TestWalkthroughOutput : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void load() {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    binder.bind(parser.parseText("shop.ds", kShop));
    binder.bind(parser.parseText("walk.ds", kWalk));
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
  }
};

TEST_F(TestWalkthroughOutput, TheExportGivesEachStepByIdentityInTheOrderWritten) {
  const auto doc = nlohmann::json::parse(ds::api::Api(metamodel).exportModel({{"shop.ds", kShop}, {"walk.ds", kWalk}}));
  ASSERT_TRUE(doc.contains("walkthroughs"));
  const auto& w = doc["walkthroughs"].at(0);
  EXPECT_EQ(w["id"], "diagram:WALK_Shop");
  EXPECT_EQ(w["title"], "The shop");
  ASSERT_EQ(w["steps"].size(), 2u);
  const auto& first = w["steps"][0];
  EXPECT_EQ(first["n"], 1);
  EXPECT_EQ(first["title"], "Who sells");
  EXPECT_EQ(first["diagram"], "diagram:CL_Shop");
  EXPECT_EQ(first["focus"], nlohmann::json::array({"shop@desk::Desk"}));
  EXPECT_EQ(first["highlight"], nlohmann::json::array({"shop@desk::Desk::sell"}));
  EXPECT_EQ(first["at"], "shop@desk::Desk::sell");
  EXPECT_EQ(first["text"], "The desk sells.");
  EXPECT_EQ(w["steps"][1]["n"], 2);
}

TEST_F(TestWalkthroughOutput, TheSceneGivesEachStepByTheNamesAndNodesItDraws) {  // its diagram by name, what it lights by identity
  const auto doc = nlohmann::json::parse(ds::api::Api(metamodel).described("see-design", {{"shop.ds", kShop}, {"walk.ds", kWalk}}));
  ASSERT_TRUE(doc.contains("walkthroughs"));
  EXPECT_EQ(doc["fields"]["walkthrough"], nlohmann::json::array({"name", "title", "steps"}));
  EXPECT_EQ(doc["fields"]["step"], nlohmann::json::array({"title", "diagram", "behaviour", "focus", "highlight", "at", "text"}));
  const auto& w = doc["walkthroughs"].at(0);
  EXPECT_EQ(w[0], "WALK_Shop");
  const auto& step = w[2].at(0);
  EXPECT_EQ(step[1], "CL_Shop");                                   // as diagrams names it
  EXPECT_EQ(step[2], -1);                                          // a class diagram replays no behaviour
  EXPECT_EQ(step[3], nlohmann::json::array({"shop@desk::Desk"}));  // by identity, as its drawing names it (DS-470)
  EXPECT_EQ(step[4], nlohmann::json::array({"shop@desk::Desk::sell"}));
  EXPECT_EQ(step[5], "shop@desk::Desk::sell");  // the callout on what it highlights
}

TEST_F(TestWalkthroughOutput, AWorkspaceWithNoWalkthroughIsWrittenAsItWas) {
  const ds::api::Api api(metamodel);
  EXPECT_FALSE(nlohmann::json::parse(api.exportModel({{"shop.ds", kShop}})).contains("walkthroughs"));
  const auto scene = nlohmann::json::parse(api.described("see-design", {{"shop.ds", kShop}}));
  EXPECT_FALSE(scene.contains("walkthroughs"));
  EXPECT_FALSE(scene["fields"].contains("step"));
  EXPECT_FALSE(scene["fields"].contains("walkthrough"));
}

TEST_F(TestWalkthroughOutput, TheQueryListsTheWalkthroughsAndTheStepsOfOne) {
  load();
  const ds::query::Query query(model, metamodel);
  const auto all = query.ask("walkthroughs", "");
  ASSERT_EQ(all.size(), 1u);
  EXPECT_EQ(all[0].id, "diagram:WALK_Shop");
  EXPECT_EQ(all[0].detail, "The shop");
  const auto steps = query.ask("walkthroughs", "WALK_Shop");
  ASSERT_EQ(steps.size(), 2u);
  EXPECT_EQ(steps[0].id, "diagram:WALK_Shop::1");
  EXPECT_EQ(steps[0].detail, "Who sells → CL_Shop");
  EXPECT_EQ(query.ask("walkthroughs", "WALK_None").at(0).kind, "error");
}

}  // namespace
