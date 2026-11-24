#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <set>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/emit/JsonSink.hpp"
#include "ds/format/TextFormats.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/query/Query.hpp"

namespace {

using ds::query::Answer;

/// Questions about the model answered by identity, over the artefacts corpus (iris).
class TestQuery : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/artefacts")) binder.bind(f);
    const auto checkSteps = ds::check::StepFactory::of();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  }

  static bool names(const std::vector<Answer>& answers, const std::string& id, const std::string& kind = "") {
    for (const auto& a : answers)
      if (a.id == id && (kind.empty() || a.kind == kind)) return true;
    return false;
  }
};

TEST_F(TestQuery, TheDiagramsAWorkspaceDeclaresWithTheirKind) {
  const auto answers = ds::query::Query(model, metamodel).ask("diagrams", "");
  EXPECT_TRUE(names(answers, "REQ_Tracker", "diagram"));
  EXPECT_TRUE(names(answers, "CL_Projection", "diagram"));
  for (const auto& a : answers)
    if (a.id == "REQ_Tracker") {
      EXPECT_EQ(a.detail, "requirement");
    }
}

TEST_F(TestQuery, TheEntitiesOfAScope) {
  const auto top = ds::query::Query(model, metamodel).ask("entities", "");
  EXPECT_TRUE(names(top, "iris", "package"));
  const auto package = ds::query::Query(model, metamodel).ask("entities", "iris");  // the package named plainly
  EXPECT_TRUE(names(package, "iris@projection", "module"));
  EXPECT_TRUE(names(package, "SREQ-00001-2", "requirement"));
  const auto module = ds::query::Query(model, metamodel).ask("entities", "iris@projection");
  EXPECT_TRUE(names(module, "iris@projection::Projection", "class"));
  EXPECT_FALSE(names(module, "iris", "package"));
}

TEST_F(TestQuery, TheRelationsOfAnEntityInBothDirections) {
  const auto answers = ds::query::Query(model, metamodel).ask("relations", "SREQ-00001-2");
  bool refinedBy = false, verifiedBy = false;
  for (const auto& a : answers) {
    refinedBy |= a.kind == "refines" && a.detail == "<-";
    verifiedBy |= a.kind == "verifies" && a.detail == "<-";
  }
  EXPECT_TRUE(refinedBy);
  EXPECT_TRUE(verifiedBy);
}

TEST_F(TestQuery, WhatAClassRealisesAndWhatProvesIt) {
  const auto answers = ds::query::Query(model, metamodel).ask("artefacts", "iris@projection::Projection");
  EXPECT_TRUE(names(answers, "REQ-00001-2", "satisfies"));
  bool proof = false;
  for (const auto& a : answers) proof |= a.kind == "verified-by" && a.detail == "REQ-00001-2";
  EXPECT_TRUE(proof);
}

TEST_F(TestQuery, WhatADiagramShows) {
  const auto answers = ds::query::Query(model, metamodel).ask("shows", "REQ_Tracker");
  EXPECT_TRUE(names(answers, "SREQ-00001-2", "requirement"));
  EXPECT_TRUE(names(answers, "iris@projection::Projection", "class"));
  EXPECT_EQ(ds::query::Query(model, metamodel).ask("shows", "nope").front().kind, "error");
}

TEST_F(TestQuery, TheFindingsOfAFile) {
  const auto all = ds::query::Query(model, metamodel).ask("findings", "");
  const auto file = ds::query::Query(model, metamodel).ask("findings", "artefacts.ds");
  EXPECT_FALSE(all.empty());
  EXPECT_EQ(all.size(), file.size());
  EXPECT_TRUE(ds::query::Query(model, metamodel).ask("findings", "other.ds").empty());
}

TEST_F(TestQuery, AnUnknownQuestionIsAnError) {
  const auto answers = ds::query::Query(model, metamodel).ask("weather", "");
  ASSERT_EQ(answers.size(), 1u);
  EXPECT_EQ(answers.front().kind, "error");
}

TEST_F(TestQuery, AnswersAsTextAndAsJson) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const std::vector<Answer> answers = {{"class", "a@b::C", "x"}, {"package", "a", ""}};
  EXPECT_EQ(ds::format::AnswersText().write(answers, {}, sink), "class  a@b::C  x\npackage  a\n");
  const auto parsed = nlohmann::json::parse(ds::format::AnswersJson().write(answers, {}, sink));
  EXPECT_EQ(parsed.size(), 2u);
  EXPECT_EQ(parsed[0]["id"], "a@b::C");
}

/// The same questions over ds's own design, whose behaviours cross modules.
class TestQueryOfTheDesign : public TestQuery {
 protected:
  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_SOURCE_DIR) + "/design")) binder.bind(f);
    const auto checkSteps = ds::check::StepFactory::of();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  }
};

TEST_F(TestQueryOfTheDesign, CouplingAnswersWhatEachBehaviourTouchesOutsideItsModule) {
  const auto answers = ds::query::Query(model, metamodel).ask("coupling", "");
  ASSERT_FALSE(answers.empty());
  for (const auto& a : answers) {
    EXPECT_TRUE(a.kind == "couples-transient" || a.kind == "couples-structural") << a.kind;
    EXPECT_NE(a.detail.find(" in "), std::string::npos) << a.detail;  // the behaviour and the module it lives in
  }
  // load asks the factory for the checker and holds it as the IChecker it is (DS-404): it names no concrete checker
  EXPECT_TRUE(names(answers, "driftspec@ds@check::CheckerFactory", "couples-transient"));
  EXPECT_FALSE(names(answers, "driftspec@ds@check::Checker", "couples-transient"));
}

}  // namespace

namespace {

/// DS-53: a class and its operations, one with a sequence, the class with a state machine.
ds::model::Model aMachine(const ds::model::Metamodel& metamodel) {
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  binder.bind(parser.parseText("door.ds",
                               "package app {\n"
                               "  module shop {\n"
                               "    class Door {\n"
                               "      + open() -> none\n"
                               "      + close() -> none\n"
                               "    }\n"
                               "    sequence Door::open {\n"
                               "      -> close()\n"
                               "    }\n"
                               "    state Door {\n"
                               "      initial Shut\n"
                               "      Shut -> Open on open\n"
                               "      Open -> Shut on close\n"
                               "    }\n"
                               "  }\n"
                               "}\n"));
  const auto checkSteps = ds::check::StepFactory::of();
  ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  return model;
}

}  // namespace

TEST(TestQueryOfAClass, TheBehavioursOfAClassAreItsMachineAndTheSequencesOfItsOperations) {
  const ds::model::Metamodel metamodel;
  const ds::model::Model model = aMachine(metamodel);
  const auto answers = ds::query::Query(model, metamodel).ask("behaviours", "app@shop::Door");
  std::set<std::string> ids;
  for (const auto& a : answers) ids.insert(a.id);
  EXPECT_TRUE(ids.count("app@shop::Door::state"));
  EXPECT_TRUE(ids.count("app@shop::Door::open::sequence"));
  EXPECT_EQ(answers.size(), 2U);
  const auto ofMethod = ds::query::Query(model, metamodel).ask("behaviours", "app@shop::Door::open");
  ASSERT_EQ(ofMethod.size(), 1U);  // a method answers its own, as before
  EXPECT_EQ(ofMethod[0].id, "app@shop::Door::open::sequence");
}
