#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "ds/model/Manual.hpp"
#include "ds/model/Metamodel.hpp"

namespace {

using ds::model::Kind;

/// One manual, made from one metamodel, as ds makes both where it starts (DS-341, DS-342).
class WithAManual : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  const ds::model::Manual manual{metamodel};

  /// The line of a page that starts with `start`; "" when none does.
  static std::string lineStarting(const std::string& page, const std::string& start) {
    std::istringstream lines(page);
    for (std::string line; std::getline(lines, line);)
      if (line.rfind(start, 0) == 0) return line;
    return "";
  }
};

/// The manual's members (REQ-00079-1, REQ-00160-2): one fixture each, asked what whoever shows the manual asks.
class TestManual : public WithAManual {};
class TestPage : public WithAManual {};
class TestTopics : public WithAManual {};
class TestGuides : public WithAManual {};
class TestCommands : public WithAManual {};
class TestCommandOf : public WithAManual {};
class TestCommandSpec : public WithAManual {};

TEST_F(TestManual, BuildsItsTablesOnceAndWritesThePagesOfTheMetamodelItIsMadeWith) {
  EXPECT_EQ(&manual.commands(), &manual.commands());
  EXPECT_EQ(&manual.guides(), &manual.guides());
  const ds::model::Metamodel another;  // another metamodel says the same language, so its manual is the same
  const ds::model::Manual other(another);
  EXPECT_EQ(other.topics(), manual.topics());
  for (const char* topic : {"", "class", "grammar", "codes"}) EXPECT_EQ(other.page(topic), manual.page(topic)) << topic;
}

TEST_F(TestPage, TheIndexListsTheKindsAndAnUnknownTopicHasNoPage) {
  const std::string index = manual.page("");
  for (const auto& k : metamodel.kinds()) EXPECT_NE(index.find(metamodel.nameOf(k.kind)), std::string::npos);
  EXPECT_EQ(manual.page("weather"), "");
}

TEST_F(TestPage, ACodeSaysItsSeverityWhatItMeansAndWhatToDo) {
  const auto* code = metamodel.codeOf("E-SYNTAX");
  ASSERT_NE(code, nullptr);
  EXPECT_EQ(manual.page("E-SYNTAX"), "E-SYNTAX (error)\n\n" + code->meaning + "\n\nWhat to do: " + code->fix + "\n");
}

TEST_F(TestPage, AGuideIsItsTextAsWritten) {
  for (const auto& guide : manual.guides()) EXPECT_EQ(manual.page(guide.first), guide.second) << guide.first;
}

TEST_F(TestPage, TheCommandsSayHowEachIsTypedAndEachExitCode) {
  const std::string page = manual.page("commands");
  for (const auto& c : manual.commands()) {
    EXPECT_NE(page.find(c.synopsis + "\n  " + c.does + "\n"), std::string::npos) << c.name;
    for (const auto& e : c.exits) EXPECT_NE(page.find("  exit " + std::to_string(e.first) + "  " + e.second + "\n"), std::string::npos);
  }
}

TEST_F(TestTopics, TheTopicsAreTheGuidesTheTablesTheKindsAndTheCodes) {
  const auto topics = manual.topics();
  const auto& guides = manual.guides();  // the guides written in doc/manual come first (DS-272)
  EXPECT_EQ(topics.size(), guides.size() + 4 + metamodel.kinds().size() + metamodel.codes().size());
  EXPECT_EQ(topics[guides.size()], "grammar");
  for (std::size_t i = 0; i < guides.size(); ++i) EXPECT_EQ(topics[i], guides[i].first);
  for (const auto& topic : topics) EXPECT_FALSE(manual.page(topic).empty()) << topic;
}

TEST_F(TestGuides, EachGuideHasATopicOfItsOwnAndStartsWithItsTitle) {
  const auto& guides = manual.guides();
  ASSERT_FALSE(guides.empty());
  std::set<std::string> topics;
  for (const auto& guide : guides) {
    EXPECT_TRUE(topics.insert(guide.first).second) << guide.first << " is there twice";
    EXPECT_EQ(guide.first.find_first_of("0123456789"), std::string::npos) << guide.first << " keeps the number of its file";
    EXPECT_EQ(guide.second.rfind("# ", 0), 0U) << guide.first;
  }
  EXPECT_EQ(guides.front().first, "intent");  // in the order of their files
}

TEST_F(TestCommands, EveryCommandSaysHowItIsTypedAndWhatItDoes) {
  std::set<std::string> seen;
  for (const auto& c : manual.commands()) {
    EXPECT_TRUE(seen.insert(c.name).second) << c.name;
    EXPECT_EQ(c.synopsis.rfind("ds " + c.name, 0), 0u) << c.name;
    EXPECT_FALSE(c.does.empty()) << c.name;
  }
  EXPECT_TRUE(seen.count("check"));
}

TEST_F(TestCommandOf, ACommandIsTheRowNamedAndNothingForAnother) {
  for (const auto& c : manual.commands()) EXPECT_EQ(manual.commandOf(c.name), &c);
  EXPECT_EQ(manual.commandOf("frobnicate"), nullptr);
}

TEST_F(TestCommandSpec, EveryCommandSaysWhatEachOfItsExitCodesMeans) {
  for (const char* name : {"check", "export", "render", "query", "manual"}) {
    const auto* command = manual.commandOf(name);
    ASSERT_NE(command, nullptr) << name;
    ASSERT_GE(command->exits.size(), 2u) << name;
    EXPECT_EQ(command->exits.front().first, 0) << name;
  }
}

/// What the manual's private members make (DS-342), read through the pages it writes.
class TestBuildCommands : public WithAManual {};
class TestBuildGuides : public WithAManual {};
class TestIndent : public WithAManual {};
class TestPad : public WithAManual {};
class TestNames : public WithAManual {};
class TestKindPage : public WithAManual {};
class TestIndexPage : public WithAManual {};
class TestGrammarPage : public WithAManual {};

TEST_F(TestBuildCommands, AreTheFiveCommandsOfDsInTheOrderTheHelpListsThem) {
  std::vector<std::string> names;
  for (const auto& c : manual.commands()) names.push_back(c.name);
  EXPECT_EQ(names, (std::vector<std::string>{"check", "export", "render", "query", "manual"}));
}

TEST_F(TestBuildGuides, AreTheFilesOfDocManualInTheOrderOfTheirNumberEachAsWritten) {
  std::vector<std::filesystem::path> files;
  for (const auto& entry : std::filesystem::directory_iterator(std::string(DRIFTSPEC_SOURCE_DIR) + "/doc/manual"))
    if (entry.path().extension() == ".md") files.push_back(entry.path());
  std::sort(files.begin(), files.end());
  const auto& guides = manual.guides();
  ASSERT_EQ(guides.size(), files.size());
  for (std::size_t i = 0; i < files.size(); ++i) {
    const std::string stem = files[i].stem().string();  // NN-topic
    EXPECT_EQ(guides[i].first, stem.substr(stem.find('-') + 1));
    std::ifstream in(files[i], std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    EXPECT_EQ(guides[i].second, text.str()) << files[i];
  }
}

TEST_F(TestIndent, EveryLineOfHowAKindIsWrittenAndOfItsExampleComesAfterTwoSpaces) {
  const auto& spec = metamodel.specOf(Kind::Class);
  const std::string page = manual.page("class");
  std::string syntax, example;
  std::istringstream s(spec.syntax), e(spec.example);
  for (std::string line; std::getline(s, line);) syntax += "  " + line + "\n";
  for (std::string line; std::getline(e, line);) example += "  " + line + "\n";
  EXPECT_NE(page.find("How it is written\n" + syntax + "\n"), std::string::npos);
  EXPECT_EQ(page.substr(page.size() - example.size()), example);  // the page ends with its example, each line ending
}

TEST_F(TestPad, FillsAColumnToItsWidthOrLeavesOneSpaceAfterALongerText) {
  const std::string codes = manual.page("codes");
  EXPECT_EQ(lineStarting(codes, "  E-SYNTAX ").substr(0, 2 + 26 + 9), "  E-SYNTAX" + std::string(26 - 8, ' ') + "error    ");
  EXPECT_EQ(lineStarting(codes, "  W-USECASE-OUTCOME-UNHANDLED").substr(0, 2 + 27 + 1 + 9), "  W-USECASE-OUTCOME-UNHANDLED coverage ");
  const std::string annotations = manual.page("annotations");
  EXPECT_EQ(lineStarting(annotations, "  @requirement").substr(0, 2 + 20 + 16),
            "  @requirement" + std::string(20 - 12, ' ') + "satisfies       ");
}

TEST_F(TestNames, AKindIsDeclaredInItsParentsNamedAsTheLanguageWritesThem) {
  const auto& parents = metamodel.specOf(Kind::Rule).parents;
  ASSERT_GE(parents.size(), 2u);
  std::string named;
  for (std::size_t i = 0; i < parents.size(); ++i) named += (i ? ", " : "") + metamodel.nameOf(parents[i]);
  EXPECT_NE(manual.page("rule").find("declared in: " + named + "\n"), std::string::npos);
  EXPECT_NE(manual.page("state").find("declared in: package, module\n"), std::string::npos);
  EXPECT_NE(manual.page("package").find("declared in: package, or at the top\n"), std::string::npos);
}

TEST_F(TestKindPage, SaysWhatAKindIsAndEndsWithItsExample) {
  const std::string page = manual.page("class");
  EXPECT_EQ(page.rfind("class — " + metamodel.specOf(Kind::Class).meaning + "\n", 0), 0u);
  EXPECT_NE(page.find("How it is written"), std::string::npos);
  EXPECT_NE(page.find("Fields\n"), std::string::npos);
  EXPECT_NE(page.find("Example (a workspace that checks clean)"), std::string::npos);
}

TEST_F(TestIndexPage, IsTheManualWithNoTopicAndListsTheCommandsAndTheGuides) {
  const std::string index = manual.page("");
  EXPECT_EQ(index.rfind("ds — a software design as text", 0), 0u);
  for (const auto& c : manual.commands()) EXPECT_NE(index.find(c.does), std::string::npos) << c.name;
  for (const auto& guide : manual.guides()) EXPECT_NE(index.find(guide.first + "  "), std::string::npos) << guide.first;
}

TEST_F(TestGrammarPage, SaysHowAWorkspaceIsWrittenAndWhatGoesInsideWhat) {
  const std::string grammar = manual.page("grammar");
  EXPECT_EQ(grammar.rfind("How a workspace is written", 0), 0u);
  EXPECT_NE(grammar.find("What goes inside what\n"), std::string::npos);
  EXPECT_NE(grammar.find("  sequence      message, frame\n"), std::string::npos);
}

}  // namespace
