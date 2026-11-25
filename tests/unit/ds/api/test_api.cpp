#include <gtest/gtest.h>

#include <algorithm>
#include <nlohmann/json.hpp>
#include <string>

#include "ds/api/Api.hpp"
#include "ds/bind/Binder.hpp"
#include "ds/check/Selection.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// The engine in process (REQ-00085-2): a workspace as text in, the JSON of the command line out.
class TestApi : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  const ds::api::Files files{{"a.ds", "package p {\n  module m {\n    class C {\n      + go() -> none\n    }\n  }\n}\n"}};
};

TEST_F(TestApi, ACleanWorkspaceChecksWithNoFindingAndItsCounts) {
  const auto out = nlohmann::json::parse(ds::api::Api(metamodel).check(files));
  EXPECT_TRUE(out["findings"].empty());
  EXPECT_EQ(out["errors"], 0);
  EXPECT_GT(out["entities"].get<int>(), 3);
}

TEST_F(TestApi, TheModelExportsItsEntitiesByIdentity) {
  const auto out = nlohmann::json::parse(ds::api::Api(metamodel).exportModel(files));
  bool found = false;
  for (const auto& e : out["entities"]) found |= e["id"] == "p@m::C";
  EXPECT_TRUE(found);
}

TEST_F(TestApi, WhatTheGrammarTakesInAModuleIsWhatAModuleHolds) {
  const auto words = nlohmann::json::parse(ds::api::Api::expectedAt("package p {\n  module m {\n    \n  }\n}\n", 2, 4));
  EXPECT_NE(std::find(words.begin(), words.end(), "class"), words.end());
  EXPECT_NE(std::find(words.begin(), words.end(), "function"), words.end());
}

/// The steps of the check a load runs, chosen by the Api for what the load is to give (DS-376).
class TestApiSteps : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model bound{metamodel};
  const std::vector<ds::check::Step> every{
      ds::check::Step::Linker,        ds::check::Step::Behaviour, ds::check::Step::Derivation,   ds::check::Step::Failures,
      ds::check::Step::Interfaces,    ds::check::Step::UseCases,  ds::check::Step::Diagrams,     ds::check::Step::Scenarios,
      ds::check::Step::FailureChecks, ds::check::Step::Structure, ds::check::Step::Dependencies, ds::check::Step::Rules,
      ds::check::Step::Levels,        ds::check::Step::States,    ds::check::Step::Order,        ds::check::Step::Walkthroughs};

  /// A workspace with a class diagram and a table written as rows and columns, bound and not yet checked.
  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(bound, metamodel);
    binder.bind(parser.parseText("a.ds",
                                 "package p {\n  module m {\n    class C {\n      + go() -> none\n    }\n  }\n}\n"
                                 "diagram CL_M {\n  kind: class\n  include: p@m::*\n}\n"
                                 "diagram T_Classes {\n  kind: table\n  for class c in p\n  column class: c.name\n}\n"));
  }
  std::vector<ds::check::Step> chosen(ds::api::Output output, const std::string& diagram) const {
    const ds::check::Selection selection = ds::api::Api::stepsFor(output, bound, diagram);
    std::vector<ds::check::Step> out;
    for (const auto step : every)
      if (selection.has(step)) out.push_back(step);
    return out;
  }
};

TEST_F(TestApiSteps, TheFindingsTheModelTheAnswersAndTheViewerRunEveryStep) {
  // ds check, ds export (and a workspace kept for many questions), ds query, ds render -f html or json
  for (const auto output : {ds::api::Output::Findings, ds::api::Output::Model, ds::api::Output::Answers, ds::api::Output::Viewer}) {
    EXPECT_EQ(chosen(output, ""), every);
    EXPECT_EQ(chosen(output, "CL_M"), every);  // the diagram a viewer opens framed on changes nothing
  }
}

TEST_F(TestApiSteps, ADrawingRunsOnlyTheStepsThatWriteWhatItShows) {
  // ds render -f svg, text, cells, ir or layout: the relations, the calls and couplings, the stereotypes
  const std::vector<ds::check::Step> drawing{ds::check::Step::Linker, ds::check::Step::Behaviour, ds::check::Step::Derivation};
  EXPECT_EQ(chosen(ds::api::Output::Drawing, "CL_M"), drawing);
  EXPECT_EQ(chosen(ds::api::Output::Drawing, "diagram:CL_M"), drawing);
  EXPECT_EQ(chosen(ds::api::Output::Drawing, "CL_Nothing"), drawing);  // no such diagram: nothing to draw, nothing more
}

TEST_F(TestApiSteps, ATableWrittenAsRowsAndColumnsIsDrawnWithTheRules) {
  const std::vector<ds::check::Step> table{ds::check::Step::Linker, ds::check::Step::Behaviour, ds::check::Step::Derivation,
                                           ds::check::Step::Rules};
  EXPECT_EQ(chosen(ds::api::Output::Drawing, "T_Classes"), table);
  EXPECT_EQ(chosen(ds::api::Output::Drawing, "diagram:T_Classes"), table);
}

TEST_F(TestApiSteps, ADrawingLoadedKeepsNoFindingOfAStepItSkips) {
  // a field its kind does not declare is a finding of the structure, which a drawing skips and never prints
  const ds::api::Files files{{"a.ds", "package p {\n  colour: red\n  module m {\n    class C {\n    }\n  }\n}\n"}};
  ds::parse::Parser parser;
  std::vector<ds::parse::ParsedFile> parsed;
  for (const auto& f : files) parsed.push_back(parser.parseText(f.first, f.second));
  const ds::api::Api api(metamodel);
  const auto codes = [](const ds::model::Model& model) {
    std::vector<std::string> out;
    for (const auto& f : model.findings()) out.push_back(f.code);
    return out;
  };
  const auto whole = codes(api.load(parsed, ds::api::Output::Findings));
  const auto drawn = codes(api.load(parsed, ds::api::Output::Drawing, "CL_M"));
  EXPECT_EQ(whole, std::vector<std::string>{"W-FIELD-UNKNOWN"});
  EXPECT_TRUE(drawn.empty());
  EXPECT_EQ(codes(api.load(parsed)), whole);  // with nothing said, the whole check
}

}  // namespace
