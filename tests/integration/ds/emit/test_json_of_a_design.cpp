#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "IrJson.hpp"
#include "ds/api/Api.hpp"
#include "ds/api/Browser.hpp"
#include "ds/api/Workspace.hpp"
#include "ds/check/Report.hpp"
#include "ds/emit/JsonSink.hpp"
#include "ds/export/JsonExport.hpp"
#include "ds/format/DescriptionFactory.hpp"
#include "ds/format/FormatFactory.hpp"
#include "ds/model/Identity.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/query/Query.hpp"
#include "ds/render/IrExport.hpp"
#include "ds/scene/Scene.hpp"
#include "ds/theme/Theme.hpp"

namespace {

/// The style the drawings are written in, made once for the file (DS-393).
// built on first use, not when the executable starts: a test reaches the theme only if it uses it (DS-473)
const ds::theme::Theme& theme() {
  static const ds::theme::Theme built;
  return built;
}

/// Every JSON document ds writes, over designs of every size (DS-397): each writer lent a checked sink, and each
/// door that builds its own, writes its fields in byte order and the bytes nlohmann writes for the same document,
/// which is what ds wrote before the sink: parsed and written again by nlohmann, it comes out the same.
class TestJsonOfADesign : public ::testing::Test {
 protected:
  /// A workspace of the repository, as text: ds's own design and the examples.
  struct Loaded {
    std::string name;
    ds::api::Files files;
    std::unique_ptr<ds::api::Workspace> workspace;
  };

  static void SetUpTestSuite() {
    for (const std::string dir : {"design", "examples/hola", "examples/shop", "examples/states"}) {
      Loaded loaded{dir, {}, nullptr};
      ds::parse::Parser parser;
      for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_SOURCE_DIR) + "/" + dir)) loaded.files[f.path] = f.text;
      loaded.workspace = std::make_unique<ds::api::Workspace>(ds::api::Workspace::madeFor(metamodel(), loaded.files));
      workspaces().push_back(std::move(loaded));
    }
  }
  static void TearDownTestSuite() { workspaces().clear(); }
  static const ds::model::Metamodel& metamodel() {
    static const ds::model::Metamodel m;
    return m;
  }
  static std::vector<Loaded>& workspaces() {
    static std::vector<Loaded> all;
    return all;
  }

  /// The document reads back as nlohmann writes it: compact, or indented by two.
  static void asNlohmannWrites(const std::string& written, bool pretty, const std::string& what) {
    nlohmann::json tree;
    ASSERT_NO_THROW(tree = nlohmann::json::parse(written)) << what;
    EXPECT_EQ(pretty ? tree.dump(2) : tree.dump(), written) << what;
  }
};

TEST_F(TestJsonOfADesign, EveryWriterLentACheckedSinkWritesWhatNlohmannWrote) {
  for (const Loaded& loaded : workspaces()) {
    const ds::model::Model& model = loaded.workspace->model();
    const ds::exporter::JsonExport exporter(metamodel());
    for (bool pretty : {false, true}) {
      ds::emit::JsonSink sink(pretty, true);
      exporter.render(model, sink);
      asNlohmannWrites(sink.result(), pretty, loaded.name + " export");
      const ds::scene::SceneDocument scene = ds::scene::Scene(model, metamodel()).build();
      ds::format::DescriptionFactory::scene(scene)->describe(sink);
      asNlohmannWrites(sink.result(), pretty, loaded.name + " scene");
      const ds::check::Report report = ds::check::Report::of(model, metamodel());
      ds::format::DescriptionFactory::report(report)->describe(sink);
      asNlohmannWrites(sink.result(), pretty, loaded.name + " report");
    }
    ds::emit::JsonSink sink(false, true);
    int written = 0;
    for (const auto& e : model.entities()) {
      if (++written > 300) break;  // enough of every kind, in the order of identities; the export above holds them all
      exporter.entity(e.second, sink);
      asNlohmannWrites(sink.result(), false, e.first);
      exporter.relations(model.index().touching(e.first), sink);
      asNlohmannWrites(sink.result(), false, e.first + " touching");
    }
    for (const auto& q : ds::query::Query::questions()) {
      const auto answers = ds::query::Query(model, metamodel()).ask(q.name, "");
      ds::format::DescriptionFactory::answers(answers)->describe(sink);
      asNlohmannWrites(sink.result(), false, loaded.name + " " + q.name);
    }
    const ds::render::IrExport ir(model, metamodel());
    for (const auto& id : ir.diagrams()) {
      if (ds::test::irJson(ir, id).size() > 64 * 1024) continue;  // a large drawing lays out slowly in a debug build; the goldens hold one
      const ds::render::IrDocument diagram = *ir.document(id);
      for (const char* format : {"layout", "cells"}) {
        (std::string(format) == "layout" ? ds::format::DescriptionFactory::layout(diagram, theme(), 0)
                                         : ds::format::DescriptionFactory::cells(diagram, theme(), 0))
            ->describe(sink);
        const std::string drawn = sink.result();
        ASSERT_FALSE(drawn.empty());
        asNlohmannWrites(drawn, false, id + " " + format);
      }
    }
    ds::format::FormatOptions options;
    options.focus = {"no::such", "a \"quoted\" </script> name"};
    EXPECT_NE(
        ds::format::FormatFactory::scene().at("html")->write(ds::scene::Scene(model, metamodel()).build(), options, sink).find("<html"),
        std::string::npos);
  }
}

TEST_F(TestJsonOfADesign, EveryAnswerOfTheDoorsReadsBackAsNlohmannWritesIt) {
  for (const Loaded& loaded : workspaces()) {
    const ds::api::Workspace& ws = *loaded.workspace;
    asNlohmannWrites(ws.exportModel(), false, loaded.name + " export");
    asNlohmannWrites(ws.report(), false, loaded.name + " report");
    asNlohmannWrites(ds::api::Api(metamodel()).report(ws.model(), true), true, loaded.name + " report, indented");
    EXPECT_EQ(ws.find("no@such::thing"), "null");
    int asked = 0;
    for (const auto& e : ws.model().entities()) {
      if (++asked > 300) break;  // enough of every kind, in the order of identities
      const std::string& id = e.first;
      for (const std::string& answer :
           {ws.find(id), ws.from(id), ws.to(id), ws.touching(id), ws.members(id), ws.parent(id), ws.children(id), ws.revisionOf(id)})
        asNlohmannWrites(answer, false, id);
      if (e.second.kind == ds::model::Kind::Sequence) {
        asNlohmannWrites(ws.tree(id), false, id + " tree");
        asNlohmannWrites(ws.steps(id, "*"), false, id + " steps");
        asNlohmannWrites(ws.walk(id, "", 3), false, id + " walk");
      }
      if (e.second.kind == ds::model::Kind::Diagram) {
        const std::string name = ds::model::Identity(id).diagramName();
        asNlohmannWrites(ws.shows(name), false, id + " shows");
        for (const char* format : {"layout", "cells"}) {
          std::string drawn = ws.render(name, format, 0);
          if (drawn.empty()) continue;
          drawn.pop_back();
          asNlohmannWrites(drawn, false, id + " " + format);
        }
      }
    }
    for (const auto& q : ds::query::Query::questions()) asNlohmannWrites(ws.ask(q.name, ""), false, loaded.name + " " + q.name);
    nlohmann::json request{{"files", loaded.files}, {"width", 0}};
    const std::string answer = ds::api::Browser(metamodel()).answer(request.dump());
    EXPECT_EQ(answer.find("{\"error\""), std::string::npos) << answer.substr(0, 200);
    asNlohmannWrites(answer, false, loaded.name + " browser");
  }
  const ds::api::Browser browser(metamodel());
  for (const char* topic : {"", "class", "E-RULE-BROKEN", "commands", "no such topic"})
    asNlohmannWrites(browser.answer(nlohmann::json{{"manual", topic}}.dump()), false, std::string("manual ") + topic);
  asNlohmannWrites(browser.answer("not json"), false, "an error");
  asNlohmannWrites(ds::api::Api::expectedAt("package p {\n  \n}\n", 1, 2), false, "expected");
}

TEST_F(TestJsonOfADesign, AWriteThatFailsHalfWayWritesNothingOfTheDocument) {
  // ds's whole design, and one file whose path is not UTF-8: its entity is written near the end of the export, after
  // megabytes of it, and the sink refuses the path there. The door gives nothing of the document: it raises.
  ds::api::Files files = workspaces().front().files;
  files["z\xe9.ds"] = "package zz {\n}\n";
  const ds::api::Workspace ws = ds::api::Workspace::madeFor(metamodel(), files);
  EXPECT_THROW((void)ws.exportModel(), std::runtime_error);
  EXPECT_THROW((void)ws.find("zz"), std::runtime_error);
  asNlohmannWrites(ws.find(workspaces().front().workspace->model().entities().begin()->first), false, "a sound entity");
  // the same document written into a sink: the sink holds what was written before the refusal, and nobody takes it
  ds::emit::JsonSink sink(true, true);
  EXPECT_THROW(ds::exporter::JsonExport(metamodel()).render(ws.model(), sink), std::runtime_error);
}

}  // namespace
