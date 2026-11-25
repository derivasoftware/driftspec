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

  /// What see-design hands the text sink the command line lends it for a drawing or the page, the version masked.
  std::string seen(const ds::model::Model& model, const Request& request) const {
    const auto sink = ds::emit::SinkFactory::text();
    ds::service::UseCaseFactory(metamodel).of("see-design")->run(model, request, *sink);
    std::string text = sink->result();
    const std::string version = metamodel.version();
    for (std::size_t at = text.find(version); !version.empty() && at != std::string::npos; at = text.find(version, at + 9))
      text.replace(at, version.size(), "<version>");
    return text;
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
    // an end that is not the default says what happened, which its door words (DS-409)
    if (end.name != "default") EXPECT_FALSE(end.message.empty()) << name << " ended in " << end.name << " and says nothing";
    seen.insert(name + ":" + end.name);
  }
  // each end the use case can reach of itself is reached here
  for (const std::string reached :
       {"check-design:default", "check-design:errors", "see-design:no-diagram", "ask-model:unknown-question", "learn-language:no-topic"})
    EXPECT_TRUE(seen.count(reached)) << reached;
}

TEST_F(TestUseCases, TheFactoryNamesEveryUseCaseAndAnyOtherNameIsNotImplemented) {
  const auto names = ds::service::UseCaseFactory::names();
  EXPECT_EQ(names.size(), 7u);  // play-design, the playground's, since DS-460
  for (const auto& name : names) EXPECT_EQ(ds::service::UseCaseFactory(metamodel).of(name)->name(), name);
  EXPECT_THROW(ds::service::UseCaseFactory(metamodel).of("teleport"), ds::emit::NotImplemented);
}

TEST_F(TestUseCases, ARequestThatIsNotTheUseCasesIsABadRequestAndADrawingTakesOnlyWhatItReads) {
  const ds::model::Model small = load("small", ds::api::Output::Drawing, "CL_Small");
  EXPECT_THROW(outcome("ask-model", small, {}), ds::service::BadRequest);  // a question is needed
  EXPECT_THROW(outcome("edit-design", small, {{{"text", ""}, {"line", "one"}, {"column", "0"}}}), ds::service::BadRequest);
  EXPECT_THROW(outcome("see-design", small, {{{"format", "svg"}, {"diagram", "CL_Small"}}}), ds::emit::NotImplemented);  // no JSON (DS-474)
  // what each reads of a model: the viewer all a check derives, a drawing its three steps, the manual nothing
  const auto see = ds::service::UseCaseFactory(metamodel).of("see-design");
  EXPECT_TRUE(same(see->needs(small, {{{"format", "layout"}, {"diagram", "CL_Small"}}}),
                   ds::api::Api::stepsFor(ds::api::Output::Drawing, small, "CL_Small")));
  EXPECT_TRUE(same(see->needs(small, {{{"format", "json"}}}), ds::check::Selection::every()));
  EXPECT_TRUE(same(ds::service::UseCaseFactory(metamodel).of("learn-language")->needs(small, {}), ds::check::Selection::of({})));
}

/// see-design draws every diagram (DS-474): each golden workspace's drawings in svg and text, as the text sink the
/// command line lends writes them, and its IR, as the JSON sink indented for a reader writes it, are what ds render wrote.
TEST_F(TestUseCases, SeeDesignDrawsEveryDiagramAsDsRenderWroteIt) {
  std::size_t compared = 0;
  for (const std::string ws : {"design", "kinds", "small"}) {
    for (const auto& entry : fs::directory_iterator(kGolden + "/expected/" + ws + "/diagrams")) {
      const std::string name = entry.path().filename().string();
      const auto ends = [&name](const std::string& suffix) {
        return name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
      };
      std::string format, diagram;
      if (ends(".svg")) format = "svg", diagram = name.substr(0, name.size() - 4);
      if (ends(".txt") && !ends(".cells.txt")) format = "text", diagram = name.substr(0, name.size() - 4);
      if (ends(".ir.json")) format = "ir", diagram = name.substr(0, name.size() - 8);
      if (format.empty()) continue;
      // asked as the command line asks: whole, naming the workspace, in a terminal that shows no colour
      const Request request{
          {{"format", format}, {"diagram", diagram}, {"width", "0"}, {"color", "false"}, {"workspace", ws}, {"whole", "true"}}};
      const ds::model::Model model = load(ws, ds::api::Output::Drawing, diagram);
      const std::string got = format == "ir" ? described("see-design", model, request, true) : seen(model, request);
      EXPECT_EQ(got, read(entry.path())) << ws << " " << name;
      ++compared;
    }
  }
  EXPECT_GT(compared, 100u);  // every drawing and every IR of the three workspaces
}

/// The viewer's page is see-design's (DS-474): what ds render writes of each golden workspace, and framed on a diagram,
/// what it shows is where it opens; a diagram the workspace lacks is no-diagram.
TEST_F(TestUseCases, SeeDesignWritesTheViewersPageFramedOnADiagram) {
  for (const std::string ws : {"design", "kinds", "small"}) {
    const ds::model::Model model = load(ws, ds::api::Output::Viewer);
    // titled after the workspace, as the command line titles it
    EXPECT_EQ(seen(model, {{{"format", "html"}, {"title", ws}, {"embed", "false"}, {"height", "640"}}}),
              read(kGolden + "/expected/" + ws + "/scene.html"))
        << ws;
  }
  const ds::model::Model kinds = load("kinds", ds::api::Output::Viewer);
  const std::string framed = seen(kinds, {{{"format", "html"}, {"title", "kinds"}, {"diagram", "CL_Hola"}}});
  EXPECT_NE(framed.find("hola@core::Greeter"), std::string::npos);  // in the focus, which travels outside the pack
  EXPECT_EQ(outcome("see-design", kinds, {{{"format", "html"}, {"diagram", "CL_Nope"}}}).name, "no-diagram");
}

/// Asked whole, as the command line asks (DS-433): a diagram that reads a file that did not read whole is said, finding
/// by finding, and not drawn; one that is missing while files do not read whole names them all. Not asked whole, as an
/// editor asks, it draws what it can.
TEST_F(TestUseCases, SeeDesignAskedWholeSaysADiagramThatDidNotReadWhole) {
  ds::parse::Parser parser;
  const std::vector<ds::parse::ParsedFile> files = {
      parser.parseText("broken/a.ds",
                       "package p {\n  module m {\n    class A {\n    }\n  }\n}\ndiagram CL_A {\n  kind: class\n  include: p@m::*\n}\n"),
      parser.parseText("broken/b.ds", "package p {\n  module m {\n    class B {\n"),
      parser.parseText("broken/c.ds", "package q {\n  class C {\n"),
      parser.parseText("broken/d.ds", "diagram CL_D {\n  kind: class\n  include: p@m::*\n}\npackage z {\n  class Y {\n")};
  const ds::model::Model model = ds::api::Api(metamodel).load(files, ds::api::Output::Drawing, "CL_A");
  const Request whole{{{"format", "text"}, {"workspace", "broken"}, {"whole", "true"}}};
  const auto asked = [&whole](const std::string& diagram) {
    Request r = whole;
    r.arguments["diagram"] = diagram;
    return r;
  };
  // how it ended, lent the text sink a drawing in text goes into
  const auto ended = [this, &model](const Request& request) {
    const auto sink = ds::emit::SinkFactory::text();
    return ds::service::UseCaseFactory(metamodel).of("see-design")->run(model, request, *sink);
  };
  // a broken file it does not read stops nothing
  EXPECT_EQ(ended(asked("CL_A")).name, "default");
  EXPECT_FALSE(seen(model, asked("CL_A")).empty());
  // its own file does not parse: that finding, then why
  const ds::service::Outcome own = ended(asked("CL_D"));
  EXPECT_EQ(own.name, "no-diagram");
  EXPECT_EQ(
      own.message,
      "broken/d.ds:5:1  E-SYNTAX  the file does not parse: unexpected `package z {` at the top of the file; it takes diagram, package\n"
      "CL_D is not drawn: a file it reads does not read whole");
  // missing, while files do not read whole: each of them, as ds check says it, then what may be why
  const ds::service::Outcome gone = ended(asked("CL_Gone"));
  EXPECT_EQ(gone.name, "no-diagram");
  EXPECT_EQ(gone.message.rfind("broken/b.ds:1:1  E-SYNTAX  the file does not parse", 0), 0u);
  EXPECT_NE(gone.message.find("\nbroken/c.ds:1:1  E-SYNTAX"), std::string::npos);
  const std::string tail = "CL_Gone is not found, and 3 files of broken do not read whole: what did not parse may be why";
  EXPECT_EQ(gone.message.substr(gone.message.size() - tail.size()), tail);
  // not asked whole: drawn of what there is, and a missing one is only missing
  EXPECT_EQ(ended({{{"format", "text"}, {"diagram", "CL_D"}}}).name, "default");
  const ds::service::Outcome missing = ended({{{"format", "svg"}, {"diagram", "CL_Gone"}}});
  EXPECT_EQ(missing.name, "no-diagram");
  EXPECT_EQ(missing.message, "no diagram named CL_Gone");
}

}  // namespace
