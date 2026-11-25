#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "IrJson.hpp"
#include "ds/api/Api.hpp"
#include "ds/api/Workspace.hpp"
#include "ds/check/Report.hpp"
#include "ds/emit/NotImplemented.hpp"
#include "ds/emit/SinkFactory.hpp"
#include "ds/format/DescriptionFactory.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/query/Query.hpp"
#include "ds/render/IrExport.hpp"
#include "ds/scene/Scene.hpp"
#include "ds/theme/Theme.hpp"

/// Every document ds writes as text, over designs of every size (DS-399): the report and the answers described once
/// and written by the text sink are what ds check and ds query printed before, when each had a writer of its own,
/// which lives on here as what the text is compared with; every other document handed to the text sink fails
/// NotImplemented, naming it; and the JSON sink writes none of the presentation the descriptions say.
class TestTextOfADesign : public ::testing::Test {
 protected:
  /// A workspace of the repository: ds's own design and the examples.
  struct Loaded {
    std::string name;
    std::unique_ptr<ds::api::Workspace> workspace;
  };

  static void SetUpTestSuite() {
    for (const std::string dir :
         {"design", "examples/hola", "examples/shop", "examples/states", "tests/corpus/diag", "tests/corpus/failures"}) {
      ds::api::Files files;
      ds::parse::Parser parser;
      for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_SOURCE_DIR) + "/" + dir)) files[f.path] = f.text;
      if (files.empty()) continue;
      workspaces().push_back({dir, std::make_unique<ds::api::Workspace>(ds::api::Workspace::madeFor(metamodel(), files))});
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
  static const ds::theme::Theme& theme() {
    static const ds::theme::Theme t;
    return t;
  }

  /// What ds check printed before the text sink (ReportText, DS-431, DS-435).
  static std::string printedReport(const ds::check::Report& report, const std::string& workspace) {
    std::string out;
    for (const auto& f : report.findings)
      out += f.location.file + ":" + std::to_string(f.location.line) + ":" + std::to_string(f.location.column) + "  " + f.code + "  " +
             f.message + "\n";
    const std::size_t open = report.coverage.size();
    out += counted(report.entities, "entity", "entities") + " · " + std::to_string(report.expected) + " expected · " +
           counted(report.relations, "relation", "relations") + " · " + counted(report.findings.size(), "finding", "findings") +
           (open ? " · " + std::to_string(open) + " open" : "") + "\n";
    if (open)
      out += std::to_string(open) + (open == 1 ? " open item" : " open items") + " of coverage: ds query " +
             (workspace.empty() ? "<workspace>" : workspace) + " coverage lists them\n";
    return out;
  }
  static std::string counted(std::size_t n, const char* one, const char* many) { return std::to_string(n) + " " + (n == 1 ? one : many); }

  /// What ds query printed before the text sink (AnswersText).
  static std::string printedAnswers(const std::vector<ds::query::Answer>& answers) {
    std::string out;
    for (const auto& a : answers) out += a.kind + "  " + a.id + (a.detail.empty() ? "" : "  " + a.detail) + "\n";
    return out;
  }

  /// The document written by the text sink, found by its name.
  static std::string asText(const ds::format::ADescription& description) {
    const auto sink = ds::emit::SinkFactory::of("text", false);
    description.describe(*sink);
    return sink->result();
  }
};

TEST_F(TestTextOfADesign, TheReportOfEveryDesignAsTextIsWhatDsCheckPrinted) {
  ASSERT_GE(workspaces().size(), 4u);
  bool open = false, found = false;
  for (const Loaded& loaded : workspaces()) {
    const ds::check::Report report = ds::check::Report::of(loaded.workspace->model(), metamodel());
    open = open || !report.coverage.empty();
    found = found || !report.findings.empty();
    for (const std::string& workspace : {loaded.name, std::string()})
      EXPECT_EQ(asText(*ds::format::DescriptionFactory::report(report, workspace)), printedReport(report, workspace)) << loaded.name;
  }
  EXPECT_TRUE(open);   // a design with open coverage, which says where it is listed
  EXPECT_TRUE(found);  // and one with findings, a line each
}

TEST_F(TestTextOfADesign, TheAnswersOfEveryQuestionAsTextAreWhatDsQueryPrinted) {
  for (const Loaded& loaded : workspaces()) {
    const ds::query::Query query(loaded.workspace->model(), metamodel());
    std::vector<std::string> subjects{""};
    int taken = 0;
    for (const auto& e : loaded.workspace->model().entities()) {
      if (++taken > 40) break;  // a spread of identities, for the questions about one entity
      subjects.push_back(e.first);
    }
    for (const auto& q : ds::query::Query::questions())
      for (const auto& subject : subjects) {
        const auto answers = query.ask(q.name, subject);
        if (!answers.empty() && answers.front().kind == "error") continue;  // said on stderr by the command line, never as text
        EXPECT_EQ(asText(*ds::format::DescriptionFactory::answers(answers)), printedAnswers(answers)) << q.name << " " << subject;
      }
  }
  EXPECT_EQ(asText(*ds::format::DescriptionFactory::answers({})), "");  // no answers: nothing, as ds query printed
}

TEST_F(TestTextOfADesign, EveryOtherDocumentOfADesignHandedToTheTextSinkFailsNamingIt) {
  const Loaded& loaded = workspaces().front();
  const ds::scene::SceneDocument scene = ds::scene::Scene(loaded.workspace->model(), metamodel()).build();
  const ds::render::IrExport ir(loaded.workspace->model(), metamodel());
  ASSERT_FALSE(ir.diagrams().empty());
  const ds::render::IrDocument diagram = *ir.document(ir.diagrams().front());  // the IR as a document (DS-458)
  std::vector<std::unique_ptr<ds::format::ADescription>> others;
  others.push_back(ds::format::DescriptionFactory::scene(scene));
  others.push_back(ds::format::DescriptionFactory::layout(diagram, theme(), 0));
  others.push_back(ds::format::DescriptionFactory::cells(diagram, theme(), 0));
  for (const auto& description : others) {
    try {
      (void)asText(*description);
      FAIL() << "the " << description->name() << " was handed over as text it says nothing of";
    } catch (const ds::emit::NotImplemented& e) {
      EXPECT_EQ(e.document(), description->name());
      EXPECT_EQ(e.technology(), "text");
      EXPECT_EQ(std::string(e.what()), "the " + description->name() + " is not written in text");
    }
  }
}

TEST_F(TestTextOfADesign, TheJsonSinkSkipsThePresentationOfEveryDocument) {
  // the report and the answers say their presentation too; the JSON written is what the engine's doors answer
  for (const Loaded& loaded : workspaces()) {
    const ds::api::Workspace& ws = *loaded.workspace;
    const ds::check::Report report = ds::check::Report::of(ws.model(), metamodel());
    for (bool pretty : {false, true}) {
      const auto sink = ds::emit::SinkFactory::of("json", pretty);
      ds::format::DescriptionFactory::report(report, loaded.name)->describe(*sink);
      const std::string written = sink->result();
      EXPECT_EQ(written, ds::api::Api(metamodel()).report(ws.model(), pretty)) << loaded.name;
      EXPECT_EQ(written.find(" entities · "), std::string::npos) << loaded.name;  // the counts said as prose: not in it
      EXPECT_EQ(written.find("coverage lists them"), std::string::npos) << loaded.name;
    }
    const auto answers = ds::query::Query(ws.model(), metamodel()).ask("entities", "");
    const auto sink = ds::emit::SinkFactory::of("json", false);
    ds::format::DescriptionFactory::answers(answers)->describe(*sink);
    const std::string written = sink->result();
    EXPECT_EQ(written, ws.ask("entities", "")) << loaded.name;
    if (!answers.empty()) EXPECT_EQ(written.find(answers.front().kind + "  " + answers.front().id), std::string::npos);  // no row
  }
}
