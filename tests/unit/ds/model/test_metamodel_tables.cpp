#include <gtest/gtest.h>

#include <set>
#include <string>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace {

using ds::model::FieldSource;
using ds::model::FieldType;
using ds::model::Kind;

/// The tables of the metamodel and what finds a row in each (REQ-00066-1, REQ-00079-1): one
/// fixture per function of ds::model, each asked what a reader of the language asks of it.
class TestKinds : public ::testing::Test {};
class TestSpecOf : public ::testing::Test {};
class TestFieldOf : public ::testing::Test {};
class TestRelations : public ::testing::Test {};
class TestRelationOf : public ::testing::Test {};
class TestCodes : public ::testing::Test {};
class TestCodeOf : public ::testing::Test {};
class TestAnnotations : public ::testing::Test {};
class TestAnnotationOf : public ::testing::Test {};
class TestCommands : public ::testing::Test {};
class TestCommandOf : public ::testing::Test {};
class TestKindName : public ::testing::Test {};
class TestFieldTypeName : public ::testing::Test {};
class TestFieldSourceName : public ::testing::Test {};

TEST_F(TestKinds, NoKindIsDeclaredTwiceAndEachSaysWhatItIs) {
  std::set<Kind> seen;
  for (const auto& k : ds::model::kinds()) {
    EXPECT_TRUE(seen.insert(k.kind).second) << ds::model::kindName(k.kind);
    EXPECT_FALSE(k.meaning.empty()) << ds::model::kindName(k.kind);
  }
  EXPECT_TRUE(seen.count(Kind::Class));
  EXPECT_TRUE(seen.count(Kind::Requirement));
}

TEST_F(TestSpecOf, TheSpecOfAKindIsTheRowOfTheTableForIt) {
  for (const auto& k : ds::model::kinds()) EXPECT_EQ(&ds::model::specOf(k.kind), &k);
}

TEST_F(TestFieldOf, AFieldIsOfItsKindOnly) {
  ASSERT_NE(ds::model::fieldOf(Kind::Requirement, "status"), nullptr);
  EXPECT_EQ(ds::model::fieldOf(Kind::Requirement, "status")->name, "status");
  EXPECT_EQ(ds::model::fieldOf(Kind::Attribute, "status"), nullptr);
  EXPECT_EQ(ds::model::fieldOf(Kind::Requirement, "no-such-field"), nullptr);
}

TEST_F(TestRelations, NoRelationIsDeclaredTwiceAndContainmentIsOne) {
  std::set<std::string> seen;
  for (const auto& r : ds::model::relations()) EXPECT_TRUE(seen.insert(r.kind).second) << r.kind;
  EXPECT_TRUE(seen.count("contains"));
  EXPECT_TRUE(seen.count("inherits"));
}

TEST_F(TestRelationOf, ARelationIsTheRowNamedAndNothingForAnotherWord) {
  for (const auto& r : ds::model::relations()) EXPECT_EQ(ds::model::relationOf(r.kind), &r);
  EXPECT_EQ(ds::model::relationOf("befriends"), nullptr);
}

TEST_F(TestCodes, EveryCodeIsAnErrorOrAWarningWithAMeaning) {
  std::set<std::string> seen;
  for (const auto& c : ds::model::codes()) {
    EXPECT_TRUE(seen.insert(c.code).second) << c.code;
    EXPECT_TRUE(c.severity == "error" || c.severity == "warning" || c.severity == "coverage") << c.code;
    EXPECT_EQ(c.code.rfind(c.severity == "error" ? "E-" : "W-", 0), 0u) << c.code;
    EXPECT_FALSE(c.meaning.empty()) << c.code;
  }
}

TEST_F(TestCodeOf, ACodeIsTheRowNamedAndNothingForAnother) {
  for (const auto& c : ds::model::codes()) EXPECT_EQ(ds::model::codeOf(c.code), &c);
  EXPECT_EQ(ds::model::codeOf("E-NO-SUCH-CODE"), nullptr);
}

TEST_F(TestAnnotations, EveryAnnotationSaysHowItIsWrittenAndWhatItMeans) {
  std::set<std::string> seen;
  for (const auto& a : ds::model::annotations()) {
    EXPECT_TRUE(seen.insert(a.name).second) << a.name;
    EXPECT_FALSE(a.meaning.empty()) << a.name;
    EXPECT_FALSE(a.relation.empty()) << a.name;
  }
  EXPECT_TRUE(seen.count("@requirement"));
  EXPECT_TRUE(seen.count("@<any other word>"));
}

TEST_F(TestAnnotationOf, AWordTheLanguageKnowsMakesItsRelationAndAnyOtherOnlyRefers) {
  EXPECT_EQ(ds::model::annotationOf("requirement").name, "@requirement");
  EXPECT_NE(ds::model::annotationOf("requirement").relation, "refers");
  EXPECT_EQ(ds::model::annotationOf("whatever").relation, "refers");
}

TEST_F(TestCommands, EveryCommandSaysHowItIsTypedAndWhatItDoes) {
  std::set<std::string> seen;
  for (const auto& c : ds::model::commands()) {
    EXPECT_TRUE(seen.insert(c.name).second) << c.name;
    EXPECT_EQ(c.synopsis.rfind("ds " + c.name, 0), 0u) << c.name;
    EXPECT_FALSE(c.does.empty()) << c.name;
  }
  EXPECT_TRUE(seen.count("check"));
}

TEST_F(TestCommandOf, ACommandIsTheRowNamedAndNothingForAnother) {
  for (const auto& c : ds::model::commands()) EXPECT_EQ(ds::model::commandOf(c.name), &c);
  EXPECT_EQ(ds::model::commandOf("frobnicate"), nullptr);
}

TEST_F(TestKindName, AKindIsNamedAsTheLanguageWritesIt) {
  EXPECT_STREQ(ds::model::kindName(Kind::Requirement), "requirement");
  EXPECT_STREQ(ds::model::kindName(Kind::Class), "class");
  std::set<std::string> seen;
  for (const auto& k : ds::model::kinds()) EXPECT_TRUE(seen.insert(ds::model::kindName(k.kind)).second);
}

TEST_F(TestFieldTypeName, ATypeOfFieldIsNamedInLowerCase) {
  EXPECT_STREQ(ds::model::fieldTypeName(FieldType::Text), "text");
  EXPECT_STREQ(ds::model::fieldTypeName(FieldType::Name), "name");
  EXPECT_STRNE(ds::model::fieldTypeName(FieldType::Names), ds::model::fieldTypeName(FieldType::Name));
}

TEST_F(TestFieldSourceName, WhereAFieldComesFromIsNamed) {
  EXPECT_STREQ(ds::model::fieldSourceName(FieldSource::Written), "written");
  std::set<std::string> seen;
  for (FieldSource s : {FieldSource::Written, FieldSource::Syntax, FieldSource::Derived})
    EXPECT_TRUE(seen.insert(ds::model::fieldSourceName(s)).second);
}

}  // namespace
