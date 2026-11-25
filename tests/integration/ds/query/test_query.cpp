#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <set>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/emit/JsonSink.hpp"
#include "ds/format/TextFormats.hpp"
#include "ds/model/Manual.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/query/Pattern.hpp"
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
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
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

TEST_F(TestQuery, EachQuestionTakesTheSubjectItSaysAndAnswersAnotherKindWithAnError) {
  const ds::query::Query query(model, metamodel);
  const auto refused = [&query](const std::string& question, const std::string& subject) {
    const auto answers = query.ask(question, subject);
    return answers.size() == 1 && answers.front().kind == "error" ? answers.front().id + ": " + answers.front().detail : std::string();
  };
  // a subject a question would ignore, or one of a kind it does not take: said, never an empty answer (DS-436)
  EXPECT_EQ(refused("diagrams", "iris"), "iris: diagrams takes no subject: ds query <workspace> diagrams");
  EXPECT_EQ(refused("coupling", "iris"), "iris: coupling takes no subject: ds query <workspace> coupling");
  EXPECT_EQ(refused("artefacts", "REQ-00001-2"),
            "REQ-00001-2: REQ-00001-2 is a requirement: artefacts takes a design element: what realises, satisfies or obeys an "
            "artefact; ds query <workspace> relations REQ-00001-2 says what it links");
  EXPECT_EQ(refused("failures", "iris@projection::Projection"),
            "iris@projection::Projection: iris@projection::Projection is a class: failures takes what a failure may escape");
  EXPECT_EQ(refused("behaviours", "VER-00050-1"), "VER-00050-1: VER-00050-1 is a verification: behaviours takes what has a behaviour");
  EXPECT_EQ(refused("entities", "diagram:REQ_Tracker"),
            "diagram:REQ_Tracker: diagram:REQ_Tracker is a diagram: entities takes a scope: something others are declared in; with * "
            "after it (shop@*), all it holds at any depth");
  EXPECT_NE(refused("entities", "artefacts.ds"), "");  // a file is no scope: only the findings and the coverage take one
  // what each takes is answered as before
  EXPECT_EQ(refused("artefacts", "iris@projection::Projection"), "");
  EXPECT_EQ(refused("entities", "iris"), "");
  EXPECT_EQ(refused("relations", "REQ-00001-2"), "");
  EXPECT_EQ(refused("findings", "artefacts.ds"), "");
  // every question says the subject it takes, and the usage lists them so
  for (const auto& q : ds::query::Query::questions()) EXPECT_FALSE(q.takes.empty()) << q.name;
  EXPECT_EQ(ds::query::Query::usage().rfind("diagrams · entities [scope] · relations <identity>", 0), 0u);
}

TEST_F(TestQuery, FindingsAndCoverageTakeAFileOrAnEntityAndEverythingItHolds) {
  const ds::query::Query query(model, metamodel);
  const auto codes = [&query](const std::string& question, const std::string& subject) {
    std::multiset<std::string> out;
    for (const auto& a : query.ask(question, subject)) out.insert(a.kind);
    return out;
  };
  // an identity: what lies on the entity, here the two findings written inside REQ-00003-1 (DS-436)
  EXPECT_EQ(codes("findings", "REQ-00003-1"), (std::multiset<std::string>{"E-REF-UNPINNED", "W-PARENT-IGNORED"}));
  // and on anything it holds: the package holds the requirements, so its findings include theirs
  const auto package = codes("findings", "iris");
  EXPECT_EQ(package.count("E-REF-UNPINNED"), 1u);
  EXPECT_EQ(package.count("W-REVISION-DRIFT"), codes("findings", "REQ-00002-1").count("W-REVISION-DRIFT"));
  // the coverage of an identity: what names it, as before, and what lies on what it holds
  EXPECT_EQ(codes("coverage", "SREQ-00002-1"), (std::multiset<std::string>{"W-REQ-NO-VERIFICATION", "W-REQ-UNREALISED"}));
  EXPECT_GE(codes("coverage", "iris").size(), codes("coverage", "SREQ-00002-1").size());
  // a file, for both
  EXPECT_EQ(codes("coverage", "artefacts.ds"), codes("coverage", ""));
  EXPECT_EQ(codes("findings", "artefacts.ds"), codes("findings", ""));
  // an identity the model does not declare is said, as for the other questions
  EXPECT_EQ(query.ask("coverage", "nope").front().kind, "error");
}

TEST_F(TestQuery, TheManualListsTheQuestionsAsTheQueryDoes) {
  // the model may not ask the query: the manual writes the usage itself, held equal to the table here (DS-436)
  const ds::model::Manual manual(metamodel);
  bool found = false;
  for (const auto& c : manual.commands())
    if (c.name == "query") {
      found = true;
      EXPECT_EQ(c.does, "ask the model: " + ds::query::Query::usage());
    }
  EXPECT_TRUE(found);
}

TEST_F(TestQuery, TheEntitiesOfAPatternAreAllItTakesAtAnyDepth) {
  const ds::query::Query query(model, metamodel);
  // the include syntax (DS-444): shop@* is all the package holds, at any depth, one line each
  const auto deep = query.ask("entities", "iris@*");
  const auto level = query.ask("entities", "iris");
  EXPECT_TRUE(names(deep, "iris@projection", "module"));
  EXPECT_TRUE(names(deep, "iris@projection::Projection", "class"));  // two levels down: not among the package's own
  EXPECT_FALSE(names(level, "iris@projection::Projection"));
  EXPECT_GT(deep.size(), level.size());
  // what it takes is what a diagram's include takes
  const ds::query::Pattern pattern(model, "iris@projection::*");
  for (const auto& a : query.ask("entities", "iris@projection::*")) EXPECT_TRUE(pattern.takes(*model.find(a.id))) << a.id;
  // a pattern that takes nothing is said, as a scope the model does not declare is
  EXPECT_EQ(query.ask("entities", "nope@*").front().kind, "error");
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
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
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
  ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
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
