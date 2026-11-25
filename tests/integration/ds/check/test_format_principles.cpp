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

/// The principles of format, as rules of ds's design (DS-398): the design of ds keeps every one, and the same design
/// with one deliberate violation written into it is told which rule it breaks.
class TestFormatPrinciples : public ::testing::Test {
 protected:
  /// A line to write into one file of ds's design, after the line that starts with `after`.
  struct Edit {
    std::string file;   ///< the file's path under design/, as ds/format.ds
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

  const std::string readsNoModel = "driftspec@ds@format::formats-read-no-model";
  const std::string siblings = "driftspec@ds@format::no-format-uses-another";
  const std::string commonAlone = "driftspec@ds@format::common-classes-know-no-format";
  const std::string noTechnology = "driftspec@ds@format::descriptions-know-no-technology";
  const std::string descriptionsByFactory = "driftspec@ds@format::descriptions-are-named-by-their-factory";
  const std::string writersByFactory = "driftspec@ds@format::writers-are-named-by-their-factory";
  const std::string inClasses = "driftspec@ds@format::format-operations-belong-to-classes";
};

TEST_F(TestFormatPrinciples, TheDesignOfDsKeepsTheRulesOfFormat) {
  const std::vector<std::string> rules = broken({});
  for (const std::string& rule : {readsNoModel, siblings, commonAlone, noTechnology, descriptionsByFactory, writersByFactory, inClasses})
    EXPECT_FALSE(has(rules, rule)) << rule;
}

TEST_F(TestFormatPrinciples, ADescriptionThatNamesTheJsonSinkIsTold) {
  const std::vector<std::string> rules =
      broken({{"ds/format.ds", "    class SceneDescription : ADescription {", "      + dump(out_sink: emit::JsonSink) -> void\n"}});
  EXPECT_TRUE(has(rules, noTechnology));
}

TEST_F(TestFormatPrinciples, ADescriptionThatMakesItsOwnSinkIsTold) {
  // the factory of the sinks is no sink, so only format's rule tells it: a description is lent its sink
  const std::vector<std::string> rules =
      broken({{"ds/format.ds", "    class AnswersDescription : ADescription {", "      - owns factory_: emit::SinkFactory\n"}});
  EXPECT_TRUE(has(rules, noTechnology));
  EXPECT_FALSE(has(rules, "driftspec@ds@emit::sinks-are-reached-through-their-interface"));
}

TEST_F(TestFormatPrinciples, ADoorThatNamesADescriptionIsTold) {
  // a verb of the command line that names the report's description is told by format's rule (DS-409)
  const std::vector<std::string> rules =
      broken({{"ds/app.ds", "    sequence CheckCommand::run {", "      local described: format::ReportDescription\n"}});
  EXPECT_TRUE(has(rules, descriptionsByFactory));
  EXPECT_FALSE(has(rules, writersByFactory));
}

TEST_F(TestFormatPrinciples, AnotherFactoryThatNamesADescriptionIsTold) {
  // every factory is exempt from the convention of the families by its name (DS-451); a description has one factory
  const std::vector<std::string> rules =
      broken({{"ds/query.ds", "    sequence QueryFactory::of {", "      local described: format::SceneDescription\n"}});
  EXPECT_TRUE(has(rules, descriptionsByFactory));
  EXPECT_FALSE(has(rules, "driftspec@ds@families-are-reached-through-their-interface"));
}

TEST_F(TestFormatPrinciples, AClassThatNamesAWriterIsTold) {
  const std::vector<std::string> api = broken({{"ds/api.ds", "    class Workspace {", "      - owns page_: format::ViewerPage\n"}});
  EXPECT_TRUE(has(api, writersByFactory));
  const std::vector<std::string> app =
      broken({{"ds/app.ds", "    sequence RenderCommand::page {", "      local page: format::ViewerPage\n"}});
  EXPECT_TRUE(has(app, writersByFactory));
  EXPECT_FALSE(has(app, descriptionsByFactory));
}

TEST_F(TestFormatPrinciples, AFunctionInFormatIsTold) {
  const std::vector<std::string> rules =
      broken({{"ds/format.ds", "    module common {", "      function pack(text: std::string) -> std::string\n"}});
  EXPECT_TRUE(has(rules, inClasses));
}

TEST_F(TestFormatPrinciples, ADescriptionThatUsesAnotherIsTold) {
  const std::vector<std::string> rules =
      broken({{"ds/format.ds", "    class CellsDescription : ADescription {", "      - owns layout_: LayoutDescription\n"}});
  EXPECT_TRUE(has(rules, siblings));
  EXPECT_TRUE(has(rules, descriptionsByFactory));
}

TEST_F(TestFormatPrinciples, AWriterThatReadsTheModelIsTold) {
  const std::vector<std::string> rules =
      broken({{"ds/format.ds", "    class DiagramText : IFormat<render::IrDocument> {", "      + read(model: model::Model) -> void\n"}});
  EXPECT_TRUE(has(rules, readsNoModel));
  const std::vector<std::string> shared = broken({{"ds/format.ds", "      class Packing {", "        - owns page_: ViewerPage\n"}});
  EXPECT_TRUE(has(shared, commonAlone));
}

}  // namespace
