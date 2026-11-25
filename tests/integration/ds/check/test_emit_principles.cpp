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

/// The principles of emit, as rules of ds's design (DS-397): the design of ds keeps every one, and the same design
/// with one deliberate violation written into it is told which rule it breaks.
class TestEmitPrinciples : public ::testing::Test {
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

  const std::string throughInterface = "driftspec@ds@emit::sinks-are-reached-through-their-interface";
  const std::string emitAlone = "driftspec@ds@emit::emit-depends-on-no-engine-module";
};

TEST_F(TestEmitPrinciples, TheDesignOfDsKeepsTheRulesOfEmit) {
  const std::vector<std::string> rules = broken({});
  for (const std::string& rule : {throughInterface, emitAlone}) EXPECT_FALSE(has(rules, rule)) << rule;
}

TEST_F(TestEmitPrinciples, AFormatLentTheJsonSinkIsTold) {
  const std::vector<std::string> rules =
      broken({{"ds/format.ds", "    class SceneDescription : ADescription {", "      + dump(out_sink: emit::JsonSink) -> void\n"}});
  EXPECT_TRUE(has(rules, throughInterface));
  EXPECT_FALSE(has(rules, emitAlone));
}

TEST_F(TestEmitPrinciples, AnExporterThatHoldsTheJsonSinkIsTold) {
  const std::vector<std::string> rules = broken({{"ds/exporter.ds", "    class JsonExport {", "      - owns sink_: emit::JsonSink\n"}});
  EXPECT_TRUE(has(rules, throughInterface));
}

TEST_F(TestEmitPrinciples, ABehaviourOfTheRenderThatBuildsTheJsonSinkIsTold) {
  const std::vector<std::string> rules = broken({{"ds/render.ds", "    sequence Text::cells {", "      local sink: emit::JsonSink\n"}});
  EXPECT_TRUE(has(rules, throughInterface));
}

TEST_F(TestEmitPrinciples, ADoorThatBuildsTheJsonSinkIsTold) {
  // the api and the command line ask SinkFactory for the sink and hold an ISink: a member of the api lent the JsonSink,
  // or a behaviour of the command line that builds it, is told
  const std::vector<std::string> api = broken({{"ds/api.ds", "    class Workspace {", "      - lend(out_sink: emit::JsonSink) -> void\n"}});
  EXPECT_TRUE(has(api, throughInterface));
  const std::vector<std::string> app = broken({{"ds/app.ds", "    sequence check {", "      local json: emit::JsonSink\n"}});
  EXPECT_TRUE(has(app, throughInterface));
}

TEST_F(TestEmitPrinciples, ASinkThatKnowsTheModelIsTold) {
  const std::vector<std::string> rules = broken({{"ds/emit.ds", "    class JsonSink : ISink {", "      - refs model_: model::Model\n"}});
  EXPECT_TRUE(has(rules, emitAlone));
  EXPECT_FALSE(has(rules, throughInterface));
}

}  // namespace
