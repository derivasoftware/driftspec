#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Manual.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// The manual ds embeds (REQ-00079-1) held to the language: every kind is written down with an
/// example that checks clean, and the catalogue of codes is exactly the codes the engine reports.
class TestManual : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  const ds::model::Manual manual{metamodel};
  // the findings of one example read as a workspace of its own; a syntax error is one of them
  static std::vector<std::string> findingsOf(const std::string& text) {
    const ds::model::Metamodel metamodel;
    ds::model::Model model{metamodel};
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    const ds::parse::ParsedFile file = parser.parseText("example.ds", text);
    std::vector<std::string> out;
    if (file.hasSyntaxErrors) out.push_back("E-SYNTAX at " + std::to_string(file.errorLine) + ":" + std::to_string(file.errorColumn));
    binder.bind(file);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, metamodel, checkSteps).run(binder);
    for (const auto& f : model.findings()) out.push_back(f.code + " " + f.message);
    return out;
  }

  // every finding code written in the engine's source, as a literal
  static std::set<std::string> codesInTheSource() {
    std::set<std::string> out;
    const std::regex literal("\"([EW]-[A-Z0-9-]+)\"");
    for (const char* root : {"/src", "/app"}) {
      for (const auto& entry : std::filesystem::recursive_directory_iterator(std::string(DRIFTSPEC_SOURCE_DIR) + root)) {
        if (!entry.is_regular_file() || entry.path().filename() == "Codes.cpp") continue;  // the catalogue itself
        std::ifstream in(entry.path());
        std::stringstream text;
        text << in.rdbuf();
        const std::string s = text.str();
        for (std::sregex_iterator it(s.begin(), s.end(), literal), end; it != end; ++it) out.insert((*it)[1]);
      }
    }
    return out;
  }
};

TEST_F(TestManual, EveryKindIsWrittenDownAndItsExampleChecksClean) {
  for (const auto& k : metamodel.kinds()) {
    const std::string name = metamodel.nameOf(k.kind);
    EXPECT_FALSE(k.syntax.empty()) << name << " says nothing of how it is written";
    ASSERT_FALSE(k.example.empty()) << name << " has no example";
    const auto findings = findingsOf(k.example);
    EXPECT_TRUE(findings.empty()) << name << ": " << (findings.empty() ? "" : findings.front());
  }
}

TEST_F(TestManual, EveryExampleOfAGuideChecksCleanAndOneMarkedWrongHasItsFinding) {
  ASSERT_FALSE(manual.guides().empty());
  const std::regex example("```ds( wrong)?\n([^`]*)```");
  std::size_t examples = 0;
  for (const auto& guide : manual.guides()) {
    EXPECT_EQ(guide.second.rfind("# ", 0), 0U) << guide.first << " does not start with its title";
    EXPECT_EQ(manual.page(guide.first), guide.second);
    for (std::sregex_iterator it(guide.second.begin(), guide.second.end(), example), end; it != end; ++it) {
      ++examples;
      const auto findings = findingsOf((*it)[2].str());
      if ((*it)[1].matched)
        EXPECT_FALSE(findings.empty()) << guide.first << ": an example marked wrong checks clean\n" << (*it)[2].str();
      else
        EXPECT_TRUE(findings.empty()) << guide.first << ": " << (findings.empty() ? "" : findings.front()) << "\n" << (*it)[2].str();
    }
  }
  EXPECT_GT(examples, 0U);
}

TEST_F(TestManual, TheCatalogueHoldsExactlyTheCodesTheEngineReports) {
  std::set<std::string> catalogued;
  for (const auto& c : metamodel.codes()) {
    EXPECT_TRUE(catalogued.insert(c.code).second) << c.code << " is catalogued twice";
    // a coverage code is a W- that names something not yet written (DS-216)
    EXPECT_EQ(c.severity == "coverage" ? "warning" : c.severity, c.code[0] == 'E' ? "error" : "warning") << c.code;
    EXPECT_FALSE(c.meaning.empty() || c.fix.empty()) << c.code;
  }
  for (const auto& code : codesInTheSource()) EXPECT_TRUE(catalogued.count(code)) << code << " is reported but not in the manual";
  const auto reported = codesInTheSource();
  for (const auto& code : catalogued) EXPECT_TRUE(reported.count(code)) << code << " is in the manual but nothing reports it";
}

TEST_F(TestManual, EveryTopicHasAPageAndAnUnknownOneHasNone) {
  const std::string index = manual.page("");
  for (const auto& c : manual.commands()) EXPECT_NE(index.find(c.does), std::string::npos) << c.name;
  for (const auto& topic : manual.topics()) EXPECT_FALSE(manual.page(topic).empty()) << topic;
  const std::string classPage = manual.page("class");
  EXPECT_NE(classPage.find("How it is written"), std::string::npos);
  EXPECT_NE(classPage.find("Example (a workspace that checks clean)"), std::string::npos);
  EXPECT_NE(manual.page("W-REF-UNDECLARED").find("What to do:"), std::string::npos);
  EXPECT_NE(manual.page("annotations").find("@requirement"), std::string::npos);
  const std::string grammar = manual.page("grammar");  // what goes inside what, from the metamodel
  EXPECT_NE(grammar.find("What goes inside what"), std::string::npos);
  EXPECT_NE(grammar.find("class         requirement, constraint, method, attribute, operator, rule, definition"), std::string::npos);
  EXPECT_NE(grammar.find("method        requirement, constraint, rule"), std::string::npos);  // as the grammar lets it (DS-121)
  EXPECT_TRUE(manual.page("weather").empty());
}

}  // namespace
