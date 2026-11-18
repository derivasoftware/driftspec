#include <gtest/gtest.h>

#include <set>
#include <string>

#include "ds/model/Metamodel.hpp"

namespace {

using ds::model::Kind;

const Kind kAllKinds[] = {Kind::Package,   Kind::Module,     Kind::Requirement, Kind::Verification, Kind::Interface, Kind::Operation,
                          Kind::View,      Kind::Constraint, Kind::Class,       Kind::Enum,         Kind::Function,  Kind::Method,
                          Kind::Attribute, Kind::Operator,   Kind::Sequence,    Kind::Message,      Kind::Frame,     Kind::State,
                          Kind::Diagram,   Kind::UseCase,    Kind::Outcome,     Kind::Rule,         Kind::Definition};

/// The metamodel declared once (REQ-00066-1). One fixture per unit of ds::model it tests:
/// the kinds, the fields, the relations and joins.
class TestKindSpec : public ::testing::Test {};
class TestFieldSpec : public ::testing::Test {};
class TestRelationSpec : public ::testing::Test {};
class TestJoins : public ::testing::Test {};

TEST_F(TestKindSpec, EveryKindOfTheModelIsDeclaredOnce) {
  std::set<std::string> seen;
  for (const auto& k : ds::model::kinds()) EXPECT_TRUE(seen.insert(ds::model::kindName(k.kind)).second) << ds::model::kindName(k.kind);
  for (Kind k : kAllKinds) EXPECT_EQ(ds::model::specOf(k).kind, k) << ds::model::kindName(k);
  EXPECT_EQ(seen.size(), sizeof(kAllKinds) / sizeof(kAllKinds[0]));
}

TEST_F(TestFieldSpec, AFieldIsFoundByItsKindAndAFamilyByAnySuffix) {
  const auto* title = ds::model::fieldOf(Kind::Requirement, "title");
  ASSERT_NE(title, nullptr);
  EXPECT_EQ(title->source, ds::model::FieldSource::Written);
  EXPECT_NE(ds::model::fieldOf(Kind::Sequence, "local.parser"), nullptr);
  EXPECT_NE(ds::model::fieldOf(Kind::Operation, "exit 2"), nullptr);
  EXPECT_EQ(ds::model::fieldOf(Kind::Sequence, "local."), nullptr);  // a family needs a suffix
  EXPECT_EQ(ds::model::fieldOf(Kind::Diagram, "layout"), nullptr);
  EXPECT_EQ(ds::model::fieldOf(Kind::Class, "title"), nullptr);  // a field of another kind is not this kind's
}

TEST_F(TestRelationSpec, ARelationIsFoundByItsKindAndAnUnknownOneIsNot) {
  const auto* verifies = ds::model::relationOf("verifies");
  ASSERT_NE(verifies, nullptr);
  EXPECT_EQ(verifies->from.size(), 1u);
  EXPECT_EQ(verifies->to.size(), 2u);
  EXPECT_EQ(ds::model::relationOf("depends-on"), nullptr);
}

TEST_F(TestJoins, ARelationJoinsOnlyTheKindsItDeclares) {
  const auto* verifies = ds::model::relationOf("verifies");
  ASSERT_NE(verifies, nullptr);
  EXPECT_TRUE(ds::model::joins(*verifies, Kind::Verification, Kind::Requirement));
  EXPECT_TRUE(ds::model::joins(*verifies, Kind::Verification, Kind::Constraint));
  EXPECT_FALSE(ds::model::joins(*verifies, Kind::Requirement, Kind::Verification));
}

TEST_F(TestJoins, ContainsJoinsAParentOnlyWithTheChildrenItMayHold) {
  const auto* contains = ds::model::relationOf("contains");
  ASSERT_NE(contains, nullptr);
  EXPECT_TRUE(ds::model::joins(*contains, Kind::Class, Kind::Method));
  EXPECT_TRUE(ds::model::joins(*contains, Kind::Package, Kind::Package));
  EXPECT_FALSE(ds::model::joins(*contains, Kind::Class, Kind::Package));  // both ends are kinds contains knows, not as a pair
  EXPECT_FALSE(ds::model::joins(*contains, Kind::Sequence, Kind::Method));
}

TEST_F(TestJoins, ARelationWithNoEndsJoinsAnyKind) {
  const auto* refers = ds::model::relationOf("refers");
  ASSERT_NE(refers, nullptr);
  EXPECT_TRUE(ds::model::joins(*refers, Kind::Class, Kind::Diagram));
}

TEST_F(TestKindSpec, OnlyPackagesAndDiagramsStandAtTheTop) {
  for (const auto& k : ds::model::kinds()) {
    EXPECT_EQ(k.top, k.kind == Kind::Package || k.kind == Kind::Diagram) << ds::model::kindName(k.kind);
    if (!k.top) {
      EXPECT_FALSE(k.parents.empty()) << ds::model::kindName(k.kind);
    }
  }
}

/// The manual's tables (REQ-00079-1): a code, an annotation, a command, each found by its name.
class TestCodeSpec : public ::testing::Test {};
class TestAnnotationSpec : public ::testing::Test {};
class TestCommandSpec : public ::testing::Test {};

TEST_F(TestCodeSpec, ACodeIsFoundWithItsSeverityItsMeaningAndWhatToDo) {
  const auto* code = ds::model::codeOf("E-SYNTAX");
  ASSERT_NE(code, nullptr);
  EXPECT_EQ(code->severity, "error");
  EXPECT_FALSE(code->fix.empty());
  EXPECT_EQ(ds::model::codeOf("E-NOTHING"), nullptr);
}

TEST_F(TestAnnotationSpec, EveryAnnotationThatMeansSomethingNamesTheRelationItMakes) {
  for (const auto& a : ds::model::annotations()) {
    if (a.relation == "refers") continue;  // any other word: a link with no meaning
    EXPECT_NE(ds::model::relationOf(a.relation), nullptr) << a.name;
  }
}

TEST_F(TestCommandSpec, EveryCommandSaysWhatEachOfItsExitCodesMeans) {
  for (const char* name : {"check", "export", "render", "query", "manual"}) {
    const auto* command = ds::model::commandOf(name);
    ASSERT_NE(command, nullptr) << name;
    ASSERT_GE(command->exits.size(), 2u) << name;
    EXPECT_EQ(command->exits.front().first, 0) << name;
  }
}

/// The manual's functions (REQ-00079-1), one fixture each.
class TestSyntaxOf : public ::testing::Test {};
class TestExampleOf : public ::testing::Test {};
class TestManualPage : public ::testing::Test {};
class TestManualTopics : public ::testing::Test {};

TEST_F(TestSyntaxOf, AKindSaysHowItIsWritten) {
  EXPECT_NE(ds::model::syntaxOf(Kind::Sequence).find("sequence <Class>::<method>"), std::string::npos);
  EXPECT_EQ(ds::model::specOf(Kind::Sequence).syntax, ds::model::syntaxOf(Kind::Sequence));  // the metamodel carries it
}

TEST_F(TestExampleOf, AKindsExampleDeclaresOneOfItsKind) {
  EXPECT_NE(ds::model::exampleOf(Kind::Enum).find("enum "), std::string::npos);
  EXPECT_NE(ds::model::exampleOf(Kind::Diagram).find("diagram "), std::string::npos);
}

TEST_F(TestManualPage, TheIndexListsTheKindsAndAnUnknownTopicHasNoPage) {
  const std::string index = ds::model::manualPage("");
  for (const auto& k : ds::model::kinds()) EXPECT_NE(index.find(ds::model::kindName(k.kind)), std::string::npos);
  EXPECT_EQ(ds::model::manualPage("weather"), "");
}

TEST_F(TestManualTopics, TheTopicsAreTheTablesTheKindsAndTheCodes) {
  const auto topics = ds::model::manualTopics();
  const auto& guides = ds::model::manualGuides();  // the guides written in doc/manual come first (DS-272)
  EXPECT_EQ(topics.size(), guides.size() + 4 + ds::model::kinds().size() + ds::model::codes().size());
  EXPECT_EQ(topics[guides.size()], "grammar");
  for (std::size_t i = 0; i < guides.size(); ++i) EXPECT_EQ(topics[i], guides[i].first);
}

/// The guides of the manual, embedded from doc/manual when ds is built (DS-272).
class TestManualGuides : public ::testing::Test {};

TEST_F(TestManualGuides, EachGuideHasATopicOfItsOwnAndStartsWithItsTitle) {
  const auto& guides = ds::model::manualGuides();
  ASSERT_FALSE(guides.empty());
  std::set<std::string> topics;
  for (const auto& guide : guides) {
    EXPECT_TRUE(topics.insert(guide.first).second) << guide.first << " is there twice";
    EXPECT_EQ(guide.first.find_first_of("0123456789"), std::string::npos) << guide.first << " keeps the number of its file";
    EXPECT_EQ(guide.second.rfind("# ", 0), 0U) << guide.first;
  }
  EXPECT_EQ(guides.front().first, "intent");  // in the order of their files
}

}  // namespace
