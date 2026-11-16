#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/query/Filter.hpp"
#include "ds/render/IrExport.hpp"

namespace {

/// Behaviour written at the high level, in one block between the boxes (DS-232), over examples/levels: the
/// purchase of a shop written as a scenario of its use case, beside the same purchase in detail.
class TestLevels : public ::testing::Test {
 protected:
  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/levels")) binder.bind(f);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, checkSteps).run(binder);
  }

  /// The message of a scenario that calls `op`, the first one written.
  const ds::model::Entity* message(const std::string& scenario, const std::string& op) const {
    for (const auto& pair : model.entities()) {
      const ds::model::Entity& m = pair.second;
      if (m.kind == ds::model::Kind::Message && m.scope == scenario && m.field("op") == op) return &m;
    }
    return nullptr;
  }

  bool related(const std::string& kind, const std::string& from, const std::string& to) const {
    for (const auto& r : model.index().from(from))
      if (r.kind == kind && r.to == to) return true;
    return false;
  }

  int found(const std::vector<ds::model::Finding>& in, const std::string& code, const std::string& about) const {
    int n = 0;
    for (const auto& f : in) n += f.code == code && f.message.find(about) != std::string::npos;
    return n;
  }

  nlohmann::json diagram(const std::string& name) {
    return nlohmann::json::parse(ds::render::IrExport(model).render("diagram:" + name, false));
  }

  /// The calls drawn, in order, as who sends to whom: a lifeline is named by what it stands for.
  static std::vector<std::pair<std::string, std::string>> crossings(const nlohmann::json& ir) {
    const auto of = [](const std::string& line) { return line.substr(0, line.find('|')); };
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto& e : ir["edges"])
      if (!e.value("dashed", false)) out.push_back({of(e["from"]), of(e["to"])});
    return out;
  }

  ds::model::Model model;
};

TEST_F(TestLevels, AUseCaseOrABoxIsTheSubjectOfAScenario) {
  EXPECT_TRUE(related("behaviour-of", "UC-00001-1::sequence", "UC-00001-1"));
  EXPECT_TRUE(related("behaviour-of", "shop@sales::sequence", "shop@sales"));
  EXPECT_EQ(found(model.findings(), "E-RELATION-KIND", "behaviour-of"), 0);
}

TEST_F(TestLevels, ABoxTakesPartByBeingNamed) {
  const ds::model::Entity* sell = message("UC-00001-1::sequence", "sell");
  ASSERT_NE(sell, nullptr);
  EXPECT_EQ(sell->field("type"), "shop@sales");  // a package, named from the scope the use case is declared in
  const ds::model::Entity* charge = message("shop@sales::sequence", "charge");
  ASSERT_NE(charge, nullptr);
  EXPECT_EQ(charge->field("type"), "shop@sales@till");  // a module, named from inside the box the scenario is of
  EXPECT_EQ(found(model.findings(), "W-PARTICIPANT-UNDECLARED", "sales"), 0);
}

TEST_F(TestLevels, TheParticipantWrittenBeforeTheArrowSendsTheMessage) {
  const ds::model::Entity* take = message("UC-00001-1::sequence", "take");
  ASSERT_NE(take, nullptr);
  EXPECT_EQ(take->field("from-type"), "shop@sales");
  const auto drawn = crossings(diagram("SEQ_Written"));
  ASSERT_EQ(drawn.size(), 5U);
  EXPECT_EQ(drawn[0], (std::make_pair(std::string("caller"), std::string("shop@shell"))));  // nobody written: whoever asks
  EXPECT_EQ(drawn[2], (std::make_pair(std::string("shop@sales"), std::string("shop@warehouse"))));
  EXPECT_EQ(drawn[3], (std::make_pair(std::string("shop@warehouse"), std::string("shop@bank"))));
}

TEST_F(TestLevels, AMessageToABoxIsBoundToTheOperationTheBoxOffers) {
  const ds::model::Entity* take = message("UC-00001-1::sequence", "take");
  ASSERT_NE(take, nullptr);
  EXPECT_TRUE(related("calls", take->id, "shop@warehouse@counter::take"));      // the counter is the interface of the warehouse
  EXPECT_EQ(take->field("expands"), "shop@warehouse@counter::take::sequence");  // and the detail is one step below
  EXPECT_EQ(take->field("type"), "shop@warehouse");                             // the box stays the participant
}

TEST_F(TestLevels, AMessageNoOperationAnswersIsALabelListedAsCoverage) {
  EXPECT_EQ(found(model.coverage(), "W-MESSAGE-NO-OPERATION", "sends sell to shop@sales"), 1);
  EXPECT_EQ(found(model.coverage(), "W-MESSAGE-NO-OPERATION", "sends charge to shop@sales@till"), 1);  // a module offers its functions
  EXPECT_EQ(found(model.findings(), "W-MESSAGE-NO-OPERATION", ""), 0);
  EXPECT_EQ(found(model.findings(), "W-MSG-UNDECLARED", "sell"), 0);
}

TEST_F(TestLevels, WhatClashesWithTheLevelBelowIsAFinding) {
  EXPECT_EQ(found(model.findings(), "E-MSG-ARITY", "shop@warehouse@counter::take takes 1 parameter, 2 given"), 1);
  EXPECT_EQ(found(model.findings(), "W-USES-UNDECLARED", "shop@bank calls shop@warehouse@counter::take"), 1);  // the sender is who calls
  EXPECT_EQ(found(model.findings(), "W-MSG-UNDECLARED", "shop@shell::refund"), 1);  // an interface is written: what it lacks, it lacks
  EXPECT_EQ(found(model.findings(), "W-PARTICIPANT-UNDECLARED", "'courier' sends a message"), 1);
  EXPECT_EQ(model.findings().size(), 4U);  // and nothing else: the use case that asks from outside declares no use
}

TEST_F(TestLevels, TheWrittenLevelAndTheDetailLiftedOntoItsBoxesDrawTheSameCrossings) {
  EXPECT_EQ(crossings(diagram("SEQ_Written")), crossings(diagram("SEQ_Lifted")));
}

TEST_F(TestLevels, AScenarioOfABoxIsDrawnBetweenWhatItNames) {
  const auto ir = diagram("SEQ_Sales");
  std::vector<std::string> lifelines;
  for (const auto& n : ir["nodes"])
    if (n["kind"] == "lifeline") lifelines.push_back(n["label"]);
  EXPECT_EQ(lifelines, (std::vector<std::string>{"desk", "till"}));  // the sales themselves send nothing: no lifeline
}

TEST_F(TestLevels, TheQueryShowsWhoTakesPartInAWrittenScenario) {
  const auto filter = ds::query::filterFor(*model.find("diagram:SEQ_Written"));
  EXPECT_EQ(filter->select(model), (std::set<std::string>{"shop@bank", "shop@sales", "shop@shell", "shop@warehouse"}));
}

}  // namespace
