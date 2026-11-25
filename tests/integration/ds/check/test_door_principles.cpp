#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// The doors of ds, the command line and the api, as rules of ds's design (DS-410, decisions 4 and 5; DS-474): the design
/// of ds keeps both, and the same design with one deliberate violation written into it is told which rule it breaks.
class TestDoorPrinciples : public ::testing::Test {
 protected:
  /// A line to write into one file of ds's design, after the line that starts with `after`.
  struct Edit {
    std::string file;   ///< the file's path under design/, as ds/emit.ds
    std::string after;  ///< the start of the line it goes after; it must occur once
    std::string line;   ///< what is written, ending in a newline
  };

  /// The rules of ds's design that a model breaks: the rule's identity, once per finding.
  static std::vector<std::string> broken(const std::vector<Edit>& edits) {
    const ds::model::Metamodel metamodel;
    ds::model::Model model{metamodel};
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (auto file : parser.parseDirectory(std::string(DRIFTSPEC_SOURCE_DIR) + "/design")) {
      for (const Edit& edit : edits) {
        const std::string suffix = "/design/" + edit.file;
        if (file.path.size() < suffix.size() || file.path.compare(file.path.size() - suffix.size(), suffix.size(), suffix) != 0) continue;
        const std::size_t at = file.text.find("\n" + edit.after);
        EXPECT_NE(at, std::string::npos) << edit.after;
        EXPECT_EQ(file.text.find("\n" + edit.after, at + 1), std::string::npos) << edit.after << " occurs twice";
        const std::size_t end = file.text.find('\n', at + 1) + 1;
        file = parser.parseText(file.path, file.text.substr(0, end) + edit.line + file.text.substr(end));
      }
      binder.bind(file);
    }
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
    std::vector<std::string> out;
    for (const auto& f : model.findings())
      if (f.code == "E-RULE-BROKEN") out.push_back(f.message.substr(0, f.message.find(' ')));
    return out;
  }
  static bool has(const std::vector<std::string>& rules, const std::string& rule) {
    for (const auto& r : rules)
      if (r == rule) return true;
    return false;
  }

  const std::string knowNoModel = "driftspec@ds@app::doors-know-no-model";
  const std::string throughWorkspace = "driftspec@ds@app::the-command-line-reaches-the-engine-through-the-workspace";
};

TEST_F(TestDoorPrinciples, TheDesignOfDsKeepsTheRulesOfTheDoors) {
  const std::vector<std::string> rules = broken({});
  for (const std::string& rule : {knowNoModel, throughWorkspace}) EXPECT_FALSE(has(rules, rule)) << rule;
}

/// A door holds no model and asks no workspace for one: a new class of the api that refers to it, a command that asks
/// the workspace for its model, and the command line owning one are told.
TEST_F(TestDoorPrinciples, ADoorThatHoldsOrAsksForTheModelIsTold) {
  const std::vector<std::string> peek = broken({{"ds/api.ds", "  module api {",
                                                 "    class Peek {\n"
                                                 "      @requirement REQ-00107-2\n"
                                                 "      - refs model_: model::Model\n"
                                                 "    }\n"}});
  EXPECT_TRUE(has(peek, knowNoModel));
  EXPECT_FALSE(has(peek, throughWorkspace));  // the api is no command line
  const std::vector<std::string> asked =
      broken({{"ds/app.ds", "    sequence RenderCommand::run {", "      local held: api::Workspace\n      -> held.model() -> model\n"}});
  EXPECT_TRUE(has(asked, knowNoModel));
  EXPECT_FALSE(has(asked, throughWorkspace));  // the workspace is what the command line reaches; its model is what a door may not
  const std::vector<std::string> owned = broken({{"ds/app.ds", "    class CommandLine {", "      - owns model_: model::Model\n"}});
  EXPECT_TRUE(has(owned, knowNoModel));
  EXPECT_TRUE(has(owned, throughWorkspace));
}

/// A door exports no render-IR, asks no question of the query and names no writer of format: a command that calls the
/// IR's export, one that asks the query, and one that names the viewer's page are told.
TEST_F(TestDoorPrinciples, ADoorThatExportsTheIrAsksTheQueryOrNamesAWriterIsTold) {
  const std::vector<std::string> exported = broken(
      {{"ds/app.ds", "    sequence RenderCommand::request {", "      local ir: render::IrExport\n      -> ir.document(id) -> document\n"}});
  EXPECT_TRUE(has(exported, knowNoModel));
  EXPECT_TRUE(has(exported, throughWorkspace));
  // a call alone tells it, with no local naming the class: the factory of the questions asked by its operation
  const std::vector<std::string> asked =
      broken({{"ds/app.ds", "    sequence CheckCommand::run {", "      -> query::QueryFactory::of(model, metamodel_) -> questions\n"}});
  EXPECT_TRUE(has(asked, knowNoModel));
  EXPECT_TRUE(has(asked, throughWorkspace));
  const std::vector<std::string> page =
      broken({{"ds/app.ds", "    sequence RenderCommand::run {", "      local page: format::ViewerPage\n"}});
  EXPECT_TRUE(has(page, knowNoModel));
  const std::vector<std::string> factory =
      broken({{"ds/app.ds", "    sequence ManualCommand::run {", "      -> format::FormatFactory::scene() -> writers\n"}});
  EXPECT_TRUE(has(factory, knowNoModel));
}

/// The exemptions cover exactly the loading machinery, api's Workspace, Api and Bound, for good, and api's Browser until
/// DS-460: the model held by each of them is not told; held by any other class of the api or the command line, it is.
TEST_F(TestDoorPrinciples, TheExemptionsCoverTheLoadingMachineryAlone) {
  const std::string held = "      - refs held_: model::Model\n";
  const std::vector<std::string> exempt = broken({{"ds/api.ds", "    class Workspace {", held},
                                                  {"ds/api.ds", "    class Api {", held},
                                                  {"ds/api.ds", "    dataclass Bound {", "      + held: model::Model\n"}});
  EXPECT_FALSE(has(exempt, knowNoModel));
  // the playground's door answers through play-design (DS-460): it is held to the rule like every door
  const std::vector<std::string> browser = broken({{"ds/api.ds", "    class Browser {", held}});
  EXPECT_TRUE(has(browser, knowNoModel));
  const std::vector<std::string> identity = broken({{"ds/api.ds", "    class Identity {", held}});
  EXPECT_TRUE(has(identity, knowNoModel));
  const std::vector<std::string> disk = broken({{"ds/app.ds", "    class Disk {", held}});
  EXPECT_TRUE(has(disk, knowNoModel));
}

/// The command line reaches the engine through the workspace and nothing else of it: a command that names the scene or
/// asks the check is told, though neither is the model.
TEST_F(TestDoorPrinciples, ACommandThatReachesTheEngineAroundTheWorkspaceIsTold) {
  const std::vector<std::string> scene = broken({{"ds/app.ds", "    sequence RenderCommand::run {", "      local scene: scene::IScene\n"}});
  EXPECT_TRUE(has(scene, throughWorkspace));
  EXPECT_FALSE(has(scene, knowNoModel));
  const std::vector<std::string> check =
      broken({{"ds/app.ds", "    sequence QueryCommand::run {", "      -> check::Report::of(model, metamodel_) -> report\n"}});
  EXPECT_TRUE(has(check, throughWorkspace));
  EXPECT_FALSE(has(check, knowNoModel));
}

/// What the command line draws itself is a render-IR document someone else produced, which has no workspace: it parses
/// it and draws it through render's Svg and Text, or describes its layout and cells, as the clean design says. Any other
/// part of render is told, the layout's engine among them.
TEST_F(TestDoorPrinciples, TheDrawingOfARenderIrDocumentIsTheCommandLinesOwn) {
  EXPECT_FALSE(has(broken({}), throughWorkspace));
  const std::vector<std::string> laid =
      broken({{"ds/app.ds", "    sequence RenderCommand::draw {", "      local placed: render::Layout\n"}});
  EXPECT_TRUE(has(laid, throughWorkspace));
  EXPECT_FALSE(has(laid, knowNoModel));
}

}  // namespace
