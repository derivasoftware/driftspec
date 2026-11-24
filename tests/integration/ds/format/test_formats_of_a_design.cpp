#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

#include "ds/api/Api.hpp"
#include "ds/check/Report.hpp"
#include "ds/emit/JsonSink.hpp"
#include "ds/format/DiagramFormats.hpp"
#include "ds/format/SceneFormats.hpp"
#include "ds/format/TextFormats.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/query/Query.hpp"
#include "ds/scene/Scene.hpp"

namespace {

/// Every output of ds is a document written by a format found by its name (REQ-00070-1), here over
/// examples/hola: the document comes from the design, the format only writes it.
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
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const ds::scene::SceneDocument scene = ds::scene::Scene(model, metamodel).build();
  const auto formats = ds::format::sceneFormats();
  ASSERT_EQ(formats.size(), 2u);
  const std::string written = formats.at("json")->write(scene, {}, sink);
  EXPECT_EQ(nlohmann::json::parse(written)["nodes"].size(), scene.nodes.size());
  EXPECT_NE(written.find("hola@core::Greeter"), std::string::npos);
  EXPECT_NE(formats.at("html")->write(scene, {}, sink).find("<html"), std::string::npos);
}

TEST_F(TestSceneFormatsOfADesign, ThePageOfTheDesignIsAFrameCarryingItWholeWhenAskedToEmbed) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  ds::format::FormatOptions options;
  options.title = "hola";
  options.embed = true;
  options.height = 300;
  const std::string frame = ds::format::sceneFormats().at("html")->write(ds::scene::Scene(model, metamodel).build(), options, sink);
  EXPECT_EQ(frame.rfind("<iframe", 0), 0u);
  EXPECT_NE(frame.find("height:300px"), std::string::npos);
  EXPECT_NE(frame.find("srcdoc=\""), std::string::npos);
}

/// A diagram the design declares, written by each of the five formats of a diagram.
class TestDiagramFormatsOfADesign : public TestFormatsOfADesign {};

TEST_F(TestDiagramFormatsOfADesign, EveryFormatFoundByItsNameDrawsTheDiagramTheDesignDeclares) {
  const auto formats = ds::format::diagramFormats();
  ASSERT_EQ(formats.size(), 5u);
  for (const auto& f : formats) {
    EXPECT_FALSE(ds::api::Api(metamodel).render(files, "CL_Hola", f.first, 120).empty()) << f.first;
    EXPECT_EQ(ds::api::Api(metamodel).render(files, "NoSuchDiagram", f.first, 120), "") << f.first;
  }
}

TEST_F(TestDiagramFormatsOfADesign, EachDrawingFormatDrawsTheSameDiagramItsIrDescribes) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const std::string text = ds::api::Api(metamodel).render(files, "CL_Hola", "ir", 0);
  const auto ir = nlohmann::json::parse(text);
  ASSERT_FALSE(ir["nodes"].empty());
  const ds::render::IrText diagram{text};
  const auto formats = ds::format::diagramFormats();
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

/// The answers of a query and the report of a check on the design, written as text or as JSON.
class TestTextFormatsOfADesign : public TestFormatsOfADesign {};

TEST_F(TestTextFormatsOfADesign, TheAnswersOfAQueryAreWrittenAsTextOrJsonByName) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const auto answers = ds::query::Query(model, metamodel).ask("entities", "hola@core");
  ASSERT_FALSE(answers.empty());
  const auto formats = ds::format::answerFormats();
  ASSERT_EQ(formats.size(), 2u);
  const auto json = nlohmann::json::parse(formats.at("json")->write(answers, {}, sink));
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
  const auto formats = ds::format::reportFormats();
  ASSERT_EQ(formats.size(), 2u);
  const auto json = nlohmann::json::parse(formats.at("json")->write(report, {}, sink));
  EXPECT_EQ(json["entities"], report.entities);
  EXPECT_EQ(json["relations"], report.relations);
  EXPECT_EQ(json["findings"].size(), report.findings.size());
  const std::string text = formats.at("text")->write(report, {}, sink);
  EXPECT_NE(text.find(std::to_string(report.entities) + " entities"), std::string::npos) << text;
  EXPECT_NE(text.find(std::to_string(report.relations) + " relations"), std::string::npos) << text;
  EXPECT_EQ(json, nlohmann::json::parse(ds::api::Api(metamodel).check(files)));  // what ds check prints
}

}  // namespace
