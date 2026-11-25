#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "ds/api/Api.hpp"
#include "ds/emit/NotImplemented.hpp"
#include "ds/emit/SinkFactory.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/service/UseCases.hpp"

namespace {

namespace fs = std::filesystem;
using ds::service::Request;

const std::string kGolden = std::string(DRIFTSPEC_SOURCE_DIR) + "/tests/system/python/golden";

/// Whether two selections hold the same steps: Selection says which it has, step by step.
bool same(const ds::check::Selection& a, const ds::check::Selection& b) {
  using ds::check::Step;
  for (const Step step :
       {Step::Linker, Step::Behaviour, Step::Derivation, Step::Failures, Step::Interfaces, Step::UseCases, Step::Diagrams, Step::Scenarios,
        Step::FailureChecks, Step::Structure, Step::Dependencies, Step::Rules, Step::Levels, Step::States, Step::Order})
    if (a.has(step) != b.has(step)) return false;
  return true;
}

std::string read(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::stringstream text;
  text << in.rdbuf();
  return text.str();
}

/// The use cases over the workspaces the goldens pin (DS-407): each describes into a sink what its door writes today.
class TestUseCases : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;

  /// A golden workspace loaded as ds loads it for a command: from the workspaces' folder, by its name, so every path
  /// reads as the golden files wrote it.
  ds::model::Model load(const std::string& ws, ds::api::Output output, const std::string& diagram = "") const {
    const fs::path here = fs::current_path();
    fs::current_path(kGolden + "/workspaces");
    ds::parse::Parser parser;
    ds::model::Model model = ds::api::Api(metamodel).load(parser.parseDirectory(ws), output, diagram);
    fs::current_path(here);
    return model;
  }

  /// What a use case describes into the JSON sink its door uses, with the line the door ends it with, the version as
  /// the goldens mask it.
  std::string described(const std::string& name, const ds::model::Model& model, const Request& request, bool pretty) const {
    const auto sink = ds::emit::SinkFactory::json(pretty);
    ds::service::UseCaseFactory(metamodel).of(name)->run(model, request, *sink);
    const std::string text = sink->result() + "\n";
    const std::string version = metamodel.version();
    std::string masked = text;
    for (std::size_t at = masked.find(version); !version.empty() && at != std::string::npos; at = masked.find(version, at + 9))
      masked.replace(at, version.size(), "<version>");
    return masked;
  }

  ds::service::Outcome outcome(const std::string& name, const ds::model::Model& model, const Request& request) const {
    const auto sink = ds::emit::SinkFactory::json(false);
    return ds::service::UseCaseFactory(metamodel).of(name)->run(model, request, *sink);
  }
};

TEST_F(TestUseCases, EachUseCaseDescribesWhatItsDoorWritesByteForByte) {
  // the questions the goldens ask of each workspace (test_golden.py's QUESTIONS)
  const std::map<std::string, std::vector<std::pair<std::string, std::string>>> questions = {
      {"design",
       {{"diagrams", ""},
        {"entities", "driftspec@ds@check"},
        {"relations", "driftspec@ds@check::Checker"},
        {"artefacts", "driftspec@ds@check::Checker"},
        {"behaviours", "driftspec@ds@check::Checker"},
        {"shows", "CL_CheckNamespace"},
        {"findings", ""},
        {"coverage", ""},
        {"coupling", ""}}},
      {"kinds",
       {{"diagrams", ""},
        {"entities", "hola"},
        {"relations", "hola@core::Greeter"},
        {"artefacts", "hola@core::Greeter"},
        {"behaviours", "hola@core::Greeter"},
        {"shows", "CL_Hola"},
        {"findings", ""},
        {"coverage", ""},
        {"coupling", ""},
        {"failures", "hola@core::Greeter::greet"}}},
      {"small",
       {{"diagrams", ""},
        {"entities", "p"},
        {"relations", "p@m::Leaf"},
        {"artefacts", "p@m::Leaf"},
        {"behaviours", "p@m::Leaf"},
        {"shows", "CL_Small"},
        {"findings", ""},
        {"coverage", ""},
        {"coupling", ""}}},
  };
  std::size_t compared = 0;
  // a golden file kept whole (one over 256 KiB is pinned by its hash, and left to test_golden.py)
  const auto against = [&compared](const fs::path& file, const std::string& got) {
    if (!fs::exists(file)) return;
    ++compared;
    EXPECT_EQ(got, read(file)) << file;
  };
  for (const auto& [ws, asked] : questions) {
    const fs::path expected = kGolden + "/expected/" + ws;
    against(expected / "report.json", described("check-design", load(ws, ds::api::Output::Findings), {}, true));
    against(expected / "export.json", described("hand-model", load(ws, ds::api::Output::Model), {}, true));
    against(expected / "scene.json", described("see-design", load(ws, ds::api::Output::Viewer), {{{"format", "json"}}}, true));
    const ds::model::Model answers = load(ws, ds::api::Output::Answers);
    for (const auto& [question, subject] : asked) {
      const std::string file = question + (subject.empty() ? "" : "-" + std::regex_replace(subject, std::regex("[^A-Za-z0-9_.-]+"), "_"));
      Request request{{{"question", question}}};
      if (!subject.empty()) request.arguments["subject"] = subject;
      against(expected / "answers" / (file + ".json"), described("ask-model", answers, request, false));
    }
    for (const auto& entry : fs::directory_iterator(expected / "diagrams")) {
      const std::string name = entry.path().filename().string();
      for (const std::string format : {"layout", "cells"}) {
        const std::string suffix = format == "layout" ? ".layout.json" : ".cells.txt";
        if (name.size() <= suffix.size() || name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0) continue;
        const std::string diagram = name.substr(0, name.size() - suffix.size());
        against(entry.path(),
                described("see-design", load(ws, ds::api::Output::Drawing, diagram), {{{"format", format}, {"diagram", diagram}}}, false));
      }
    }
  }
  EXPECT_GT(compared, 40u);  // every document kept whole, of the three workspaces
}

TEST_F(TestUseCases, EachUseCaseEndsInAnOutcomeItsUseCaseDeclares) {
  // the outcomes design/usecases.ds declares, by use case; the door's own (bad-usage, no-workspace) are left to it
  ds::parse::Parser parser;
  const ds::model::Model design =
      ds::api::Api(metamodel).load(parser.parseDirectory(std::string(DRIFTSPEC_SOURCE_DIR) + "/design"), ds::api::Output::Model);
  const std::map<std::string, std::string> uc = {{"check-design", "UC-00001-1"},   {"see-design", "UC-00002-1"},
                                                 {"ask-model", "UC-00004-1"},      {"hand-model", "UC-00005-1"},
                                                 {"learn-language", "UC-00006-1"}, {"edit-design", "UC-00007-1"}};
  const auto declared = [&](const std::string& name) {
    std::set<std::string> out;
    for (const auto& member : design.index().members(uc.at(name))) {
      const ds::model::Entity* e = design.find(member);
      if (e && e->kind == ds::model::Kind::Outcome) out.insert(e->name);
    }
    return out;
  };
  const ds::model::Model small = load("small", ds::api::Output::Model);
  const ds::model::Model clean = load("kinds", ds::api::Output::Findings);  // it checks clean; small has an error
  const ds::model::Model broken = load("small", ds::api::Output::Findings);
  const std::vector<std::pair<std::string, ds::service::Outcome>> ended = {
      {"check-design", outcome("check-design", clean, {})},
      {"check-design", outcome("check-design", broken, {})},
      {"see-design", outcome("see-design", small, {{{"format", "layout"}, {"diagram", "CL_Small"}}})},
      {"see-design", outcome("see-design", small, {{{"format", "layout"}, {"diagram", "nope"}}})},
      {"ask-model", outcome("ask-model", small, {{{"question", "diagrams"}}})},
      {"ask-model", outcome("ask-model", small, {{{"question", "weather"}}})},
      {"hand-model", outcome("hand-model", small, {})},
      {"learn-language", outcome("learn-language", small, {})},
      {"learn-language", outcome("learn-language", small, {{{"topic", "nope"}}})},
      {"edit-design", outcome("edit-design", small, {{{"text", "package p {\n}\n"}, {"line", "1"}, {"column", "0"}}})},
  };
  std::set<std::string> seen;
  for (const auto& [name, end] : ended) {
    EXPECT_TRUE(declared(name).count(end.name)) << name << " ended in " << end.name << ", which " << uc.at(name) << " does not declare";
    seen.insert(name + ":" + end.name);
  }
  // each end the use case can reach of itself is reached here
  for (const std::string reached :
       {"check-design:default", "check-design:errors", "see-design:no-diagram", "ask-model:unknown-question", "learn-language:no-topic"})
    EXPECT_TRUE(seen.count(reached)) << reached;
}

TEST_F(TestUseCases, TheFactoryNamesEveryUseCaseAndAnyOtherNameIsNotImplemented) {
  const auto names = ds::service::UseCaseFactory::names();
  EXPECT_EQ(names.size(), 6u);
  for (const auto& name : names) EXPECT_EQ(ds::service::UseCaseFactory(metamodel).of(name)->name(), name);
  EXPECT_THROW(ds::service::UseCaseFactory(metamodel).of("teleport"), ds::emit::NotImplemented);
}

TEST_F(TestUseCases, ARequestThatIsNotTheUseCasesIsABadRequestAndADrawingTakesOnlyWhatItReads) {
  const ds::model::Model small = load("small", ds::api::Output::Drawing, "CL_Small");
  EXPECT_THROW(outcome("ask-model", small, {}), ds::service::BadRequest);  // a question is needed
  EXPECT_THROW(outcome("edit-design", small, {{{"text", ""}, {"line", "one"}, {"column", "0"}}}), ds::service::BadRequest);
  EXPECT_THROW(outcome("see-design", small, {{{"format", "svg"}, {"diagram", "CL_Small"}}}), ds::emit::NotImplemented);  // DS-399
  // what each reads of a model: the viewer all a check derives, a drawing its three steps, the manual nothing
  const auto see = ds::service::UseCaseFactory(metamodel).of("see-design");
  EXPECT_TRUE(same(see->needs(small, {{{"format", "layout"}, {"diagram", "CL_Small"}}}),
                   ds::api::Api::stepsFor(ds::api::Output::Drawing, small, "CL_Small")));
  EXPECT_TRUE(same(see->needs(small, {{{"format", "json"}}}), ds::check::Selection::every()));
  EXPECT_TRUE(same(ds::service::UseCaseFactory(metamodel).of("learn-language")->needs(small, {}), ds::check::Selection::of({})));
}

}  // namespace
