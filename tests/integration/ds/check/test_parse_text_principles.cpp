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

/// The principles of parse and text, as rules of ds's design (DS-374): the design of ds keeps every one, and the same
/// design with one deliberate violation written into it is told which rule it breaks.
class TestParseTextPrinciples : public ::testing::Test {
 protected:
  /// A line to write into one file of ds's design, after the line that starts with `after`.
  struct Edit {
    std::string file;   ///< the file's path under design/, as ds/parse.ds
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
    const auto checkSteps = ds::check::Steps::of();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
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

  const std::string noModel = "driftspec@ds@parse::parse-knows-no-model";
  const std::string parseClasses = "driftspec@ds@parse::parse-operations-belong-to-classes";
  const std::string textAlone = "driftspec@ds@text::text-depends-on-no-engine-module";
  const std::string textStateless = "driftspec@ds@text::text-holds-no-state";
};

TEST_F(TestParseTextPrinciples, TheDesignOfDsKeepsTheRulesOfParseAndText) {
  const std::vector<std::string> rules = broken({});
  for (const std::string& rule : {noModel, parseClasses, textAlone, textStateless}) EXPECT_FALSE(has(rules, rule)) << rule;
}

TEST_F(TestParseTextPrinciples, AClassOfParseThatKnowsTheModelIsTold) {
  const std::vector<std::string> rules = broken({{"ds/parse.ds", "    class TreeReader {", "      - refs model_: model::Model\n"}});
  EXPECT_TRUE(has(rules, noModel));
  EXPECT_FALSE(has(rules, parseClasses));
}

TEST_F(TestParseTextPrinciples, AFunctionOfParseIsTold) {
  const std::vector<std::string> rules =
      broken({{"ds/parse.ds", "  module parse {", "    function loose(text: std::string) -> std::string\n"}});
  EXPECT_TRUE(has(rules, parseClasses));
  EXPECT_FALSE(has(rules, noModel));
}

TEST_F(TestParseTextPrinciples, AFunctionOfTextThatKnowsTheModelIsTold) {
  const std::vector<std::string> rules =
      broken({{"ds/text.ds", "  module text {", "    function describe(model: model::Model) -> std::string\n"}});
  EXPECT_TRUE(has(rules, textAlone));
  EXPECT_FALSE(has(rules, textStateless));
}

TEST_F(TestParseTextPrinciples, StateInTextIsTold) {
  const std::vector<std::string> rules =
      broken({{"ds/text.ds", "  module text {", "    dataclass LastSplit {\n      + items: std::vector<std::string>\n    }\n"}});
  EXPECT_TRUE(has(rules, textStateless));
  EXPECT_FALSE(has(rules, textAlone));
}

}  // namespace
