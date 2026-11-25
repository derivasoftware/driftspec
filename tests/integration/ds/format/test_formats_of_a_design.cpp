#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>
#include <sstream>
#include <string>

#include "ds/api/Api.hpp"
#include "ds/api/Workspace.hpp"
#include "ds/check/Report.hpp"
#include "ds/emit/JsonSink.hpp"
#include "ds/emit/SinkFactory.hpp"
#include "ds/format/DescriptionFactory.hpp"
#include "ds/format/FormatFactory.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/query/Query.hpp"
#include "ds/scene/Scene.hpp"
#include "ds/theme/Theme.hpp"

namespace {

/// The style the drawings are written in, made once for the file (DS-393).
const ds::theme::Theme kTheme;

/// Every output of ds is a document (REQ-00106-4), here over examples/hola: the document comes from the design; it
/// is described into the sink of the technology named, or written by the writer named, and neither reads the model.
class TestFormatsOfADesign : public ::testing::Test {
 protected:
  ds::api::Files files;
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void SetUp() override {
    const std::string root = std::string(DRIFTSPEC_EXAMPLES_DIR) + "/hola";
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
      if (entry.path().extension() != ".ds") continue;
      std::ifstream in(entry.path());
      std::stringstream text;
      text << in.rdbuf();
      files[entry.path().string()] = text.str();
    }
    ds::parse::Parser parser;
    model = ds::api::Api(metamodel).load(parser.parseDirectory(root));
  }
};

/// The scene of the design, written by each of its formats.
class TestSceneFormatsOfADesign : public TestFormatsOfADesign {};

TEST_F(TestSceneFormatsOfADesign, EachFormatFoundByItsNameWritesTheSceneOfTheDesign) {
  const ds::scene::SceneDocument scene = ds::scene::Scene(model, metamodel).build();
  const auto sink = ds::emit::SinkFactory::of("json", false);  // the technology found by its name (DS-398)
  ds::format::DescriptionFactory::scene(scene)->describe(*sink);
  const std::string written = sink->result();
  EXPECT_EQ(nlohmann::json::parse(written)["nodes"].size(), scene.nodes.size());
  EXPECT_NE(written.find("hola@core::Greeter"), std::string::npos);
  const auto formats = ds::format::FormatFactory::scene();  // the page, found by its name
  ASSERT_EQ(formats.size(), 1u);
  EXPECT_NE(formats.at("html")->write(scene, {}, *sink).find("<html"), std::string::npos);
}

TEST_F(TestSceneFormatsOfADesign, ThePageOfTheDesignIsAFrameCarryingItWholeWhenAskedToEmbed) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  ds::format::FormatOptions options;
  options.title = "hola";
  options.embed = true;
  options.height = 300;
  const std::string frame = ds::format::FormatFactory::scene().at("html")->write(ds::scene::Scene(model, metamodel).build(), options, sink);
  EXPECT_EQ(frame.rfind("<iframe", 0), 0u);
  EXPECT_NE(frame.find("height:300px"), std::string::npos);
  EXPECT_NE(frame.find("srcdoc=\""), std::string::npos);
}

/// A diagram the design declares, in each of its five formats: two writers and three descriptions (DS-398, DS-458).
class TestDiagramFormatsOfADesign : public TestFormatsOfADesign {};

TEST_F(TestDiagramFormatsOfADesign, EveryFormatFoundByItsNameDrawsTheDiagramTheDesignDeclares) {
  ASSERT_EQ(ds::format::FormatFactory::diagram(kTheme).size(), 2u);  // svg, text; the IR, layout and cells are descriptions
  const ds::api::Workspace workspace = ds::api::Workspace::madeFor(metamodel, files);
  std::set<std::string> drawn;
  for (const char* format : {"cells", "ir", "layout", "svg", "text"}) {
    // the engine draws the format it is asked for, as the workspace draws it, never a text drawing instead (DS-453)
    const std::string drawing = workspace.render("CL_Hola", format, 120);
    EXPECT_FALSE(drawing.empty()) << format;
    EXPECT_EQ(ds::api::Api(metamodel).render(files, "CL_Hola", format, 120), drawing) << format;
    EXPECT_EQ(ds::api::Api(metamodel).render(files, "NoSuchDiagram", format, 120), "") << format;
    drawn.insert(drawing);
  }
  EXPECT_EQ(drawn.size(), 5u);  // five documents, none of them another's
}

TEST_F(TestDiagramFormatsOfADesign, EachDrawingFormatDrawsTheSameDiagramItsIrDescribes) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const std::string text = ds::api::Api(metamodel).render(files, "CL_Hola", "ir", 0);
  const auto ir = nlohmann::json::parse(text);
  ASSERT_FALSE(ir["nodes"].empty());
  const ds::render::IrDocument diagram = ds::render::IrDocument::parse(text);  // read once, as a door reads an IR file
  const auto formats = ds::format::FormatFactory::diagram(kTheme);
  const std::string svg = formats.at("svg")->write(diagram, {}, sink);
  EXPECT_NE(svg.find("<svg"), std::string::npos);
  EXPECT_NE(svg.find("Greeter"), std::string::npos);
  EXPECT_NE(formats.at("text")->write(diagram, {}, sink).find("Greeter"), std::string::npos);
  EXPECT_EQ(svg, ds::api::Api(metamodel).render(files, "CL_Hola", "svg", 0));  // what ds render writes
}

/// The answers of a query and the report of a check on the design, each described once into the sink of the
/// technology named: text or JSON (DS-399).
class TestTextFormatsOfADesign : public TestFormatsOfADesign {};

TEST_F(TestTextFormatsOfADesign, TheAnswersOfAQueryAreWrittenAsTextOrJsonByName) {
  const auto answers = ds::query::Query(model, metamodel).ask("entities", "hola@core");
  ASSERT_FALSE(answers.empty());
  const auto description = ds::format::DescriptionFactory::answers(answers);
  const auto sink = ds::emit::SinkFactory::of("json", false);  // the technology found by its name (DS-398)
  description->describe(*sink);
  const auto json = nlohmann::json::parse(sink->result());
  ASSERT_EQ(json.size(), answers.size());
  EXPECT_EQ(json[0]["id"], answers[0].id);
  const auto text = ds::emit::SinkFactory::of("text", false);
  description->describe(*text);
  const std::string written = text->result();
  EXPECT_EQ(static_cast<std::size_t>(std::count(written.begin(), written.end(), '\n')), answers.size());  // a line each
  for (const auto& a : answers) EXPECT_NE(written.find(a.kind + "  " + a.id), std::string::npos) << a.id;
}

TEST_F(TestTextFormatsOfADesign, TheReportOfTheCheckIsBuiltOnceAndReadsTheSameAsTextAndAsJson) {
  const ds::check::Report report = ds::check::Report::of(model, metamodel);
  EXPECT_GT(report.entities, 0);
  EXPECT_EQ(report.errors, 0);
  const auto description = ds::format::DescriptionFactory::report(report, "examples/hola");
  const auto sink = ds::emit::SinkFactory::of("json", false);
  description->describe(*sink);
  const auto json = nlohmann::json::parse(sink->result());
  EXPECT_EQ(json["entities"], report.entities);
  EXPECT_EQ(json["relations"], report.relations);
  EXPECT_EQ(json["findings"].size(), report.findings.size());
  const auto text = ds::emit::SinkFactory::of("text", false);
  description->describe(*text);
  const std::string written = text->result();
  EXPECT_NE(written.find(std::to_string(report.entities) + " entities"), std::string::npos) << written;
  EXPECT_NE(written.find(std::to_string(report.relations) + " relations"), std::string::npos) << written;
  EXPECT_EQ(json, nlohmann::json::parse(ds::api::Api(metamodel).check(files)));  // what ds check prints
}

}  // namespace
