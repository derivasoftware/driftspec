#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Report.hpp"
#include "ds/check/Steps.hpp"
#include "ds/format/TextFormats.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// The principles of findings, as rules of ds's design (DS-379): the design of ds keeps every one, and the same
/// design with one deliberate violation written into it is told which rule it breaks.
class TestFindingPrinciples : public ::testing::Test {
 protected:
  /// A line to write into one file of ds's design, after the line that starts with `after`.
  struct Edit {
    std::string file;   ///< the file's path under design/, as ds/check.ds
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
    const auto checkSteps = ds::check::steps();
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

  /// The corpus of the audit checked: errors, warnings and coverage, each of many codes.
  void checkAudit() {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& file : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/audit")) binder.bind(file);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  }

  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  const std::string fromCatalogue = "driftspec@ds@model::findings-come-from-the-catalogue";
  const std::string askedOfTable = "driftspec@ds@model::severities-are-asked-of-their-table";
};

TEST_F(TestFindingPrinciples, TheDesignOfDsKeepsTheRulesOfFindings) {
  const std::vector<std::string> rules = broken({});
  for (const std::string& rule : {fromCatalogue, askedOfTable}) EXPECT_FALSE(has(rules, rule)) << rule;
}

TEST_F(TestFindingPrinciples, AClassOutsideTheModelThatReadsTheCatalogueIsTold) {
  const std::vector<std::string> rules = broken({{"ds/check.ds", "    class Linker", "      - refs codes_: model::CodeSpec\n"}});
  EXPECT_TRUE(has(rules, fromCatalogue));
  EXPECT_FALSE(has(rules, askedOfTable));
}

TEST_F(TestFindingPrinciples, AClassThatHoldsASeverityRowIsTold) {
  const std::vector<std::string> rules = broken({{"ds/check.ds", "    class Linker", "      - refs weight_: model::SeveritySpec\n"}});
  EXPECT_TRUE(has(rules, askedOfTable));
  EXPECT_FALSE(has(rules, fromCatalogue));
}

/// Every finding a design gets carries the severity the catalogue gives its code (DS-379): made from its code, none
/// is written by hand, and a coverage finding is born coverage.
TEST_F(TestFindingPrinciples, EveryFindingOfADesignCarriesTheSeverityOfItsCode) {
  checkAudit();
  ASSERT_FALSE(model.findings().empty());
  ASSERT_FALSE(model.coverage().empty());
  for (const auto* list : {&model.findings(), &model.coverage()}) {
    for (const auto& f : *list) {
      const ds::model::CodeSpec* spec = metamodel.codeOf(f.code);
      ASSERT_NE(spec, nullptr) << f.code;
      EXPECT_EQ(f.severity, spec->severity) << f.code;
      EXPECT_EQ(metamodel.severityOf(f.severity).keptApart, list == &model.coverage()) << f.code;
    }
  }
}

/// The report counts what fails, and its JSON prints each severity by the name of its row (DS-379).
TEST_F(TestFindingPrinciples, TheReportCountsWhatFailsAndPrintsEachSeverityByName) {
  checkAudit();
  const ds::check::Report report = ds::check::Report::of(model, metamodel);
  int fails = 0;
  for (const auto& f : report.findings) fails += metamodel.severityOf(f.severity).fails ? 1 : 0;
  EXPECT_GT(fails, 0);
  EXPECT_EQ(report.errors, fails);
  const auto json = nlohmann::json::parse(ds::format::ReportJson().write(report, {}));
  ASSERT_EQ(json["findings"].size(), report.findings.size());
  for (std::size_t i = 0; i < report.findings.size(); ++i)
    EXPECT_EQ(json["findings"][i]["severity"], metamodel.severityOf(report.findings[i].severity).name);
  for (const auto& c : json["coverage"]) EXPECT_EQ(c["severity"], "coverage");
}

}  // namespace
