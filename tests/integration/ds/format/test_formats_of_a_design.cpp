#include <gtest/gtest.h>

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

/// Every output of ds is a document (REQ-00106-3), here over examples/hola: the document comes from the design; it
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

/// A diagram the design declares, in each of its five formats: three writers and two descriptions (DS-398).
class TestDiagramFormatsOfADesign : public TestFormatsOfADesign {};

TEST_F(TestDiagramFormatsOfADesign, EveryFormatFoundByItsNameDrawsTheDiagramTheDesignDeclares) {
  ASSERT_EQ(ds::format::FormatFactory::diagram(kTheme).size(), 3u);  // ir, svg, text; layout and cells are descriptions
  const ds::api::Workspace workspace(metamodel, files);
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
  const ds::render::IrText diagram{text};
  const auto formats = ds::format::FormatFactory::diagram(kTheme);
  EXPECT_EQ(formats.at("ir")->write(diagram, {}, sink), text);  // as it came
  ds::format::FormatOptions indented;
  indented.pretty = true;
  EXPECT_EQ(formats.at("ir")->write(diagram, indented, sink), ir.dump(2));
  const std::string svg = formats.at("svg")->write(diagram, {}, sink);
  EXPECT_NE(svg.find("<svg"), std::string::npos);
  EXPECT_NE(svg.find("Greeter"), std::string::npos);
  EXPECT_NE(formats.at("text")->write(diagram, {}, sink).find("Greeter"), std::string::npos);
  EXPECT_EQ(svg, ds::api::Api(metamodel).render(files, "CL_Hola", "svg", 0));  // what ds render writes
}

/// The answers of a query and the report of a check on the design, written as text or described in JSON.
class TestTextFormatsOfADesign : public TestFormatsOfADesign {};

TEST_F(TestTextFormatsOfADesign, TheAnswersOfAQueryAreWrittenAsTextOrJsonByName) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const auto answers = ds::query::Query(model, metamodel).ask("entities", "hola@core");
  ASSERT_FALSE(answers.empty());
  const auto formats = ds::format::FormatFactory::answers();
  ASSERT_EQ(formats.size(), 1u);
  ds::format::DescriptionFactory::answers(answers)->describe(sink);
  const auto json = nlohmann::json::parse(sink.result());
  ASSERT_EQ(json.size(), answers.size());
  EXPECT_EQ(json[0]["id"], answers[0].id);
  const std::string text = formats.at("text")->write(answers, {}, sink);
  for (const auto& a : answers) EXPECT_NE(text.find(a.id), std::string::npos) << a.id;
}

TEST_F(TestTextFormatsOfADesign, TheReportOfTheCheckIsBuiltOnceAndReadsTheSameAsTextAndAsJson) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const ds::check::Report report = ds::check::Report::of(model, metamodel);
  EXPECT_GT(report.entities, 0);
  EXPECT_EQ(report.errors, 0);
  const auto formats = ds::format::FormatFactory::report();
  ASSERT_EQ(formats.size(), 1u);
  ds::format::DescriptionFactory::report(report)->describe(sink);
  const auto json = nlohmann::json::parse(sink.result());
  EXPECT_EQ(json["entities"], report.entities);
  EXPECT_EQ(json["relations"], report.relations);
  EXPECT_EQ(json["findings"].size(), report.findings.size());
  const std::string text = formats.at("text")->write(report, {}, sink);
  EXPECT_NE(text.find(std::to_string(report.entities) + " entities"), std::string::npos) << text;
  EXPECT_NE(text.find(std::to_string(report.relations) + " relations"), std::string::npos) << text;
  EXPECT_EQ(json, nlohmann::json::parse(ds::api::Api(metamodel).check(files)));  // what ds check prints
}

}  // namespace
