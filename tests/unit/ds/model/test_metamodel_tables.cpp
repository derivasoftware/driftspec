#include <gtest/gtest.h>

#include <set>
#include <string>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace {

using ds::model::FieldSource;
using ds::model::FieldType;
using ds::model::Kind;

/// One metamodel, made for each test as the engine makes one where it starts (DS-341).
class Metamodelled : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
};

/// The tables of the metamodel and what finds a row in each (REQ-00066-1, REQ-00079-1): one
/// fixture per member of ds::model::Metamodel, each asked what a reader of the language asks of it.
class TestKinds : public Metamodelled {};
class TestSpecOf : public Metamodelled {};
class TestFieldOf : public Metamodelled {};
class TestRelations : public Metamodelled {};
class TestRelationOf : public Metamodelled {};
class TestCodes : public Metamodelled {};
class TestCodeOf : public Metamodelled {};
class TestAnnotations : public Metamodelled {};
class TestAnnotationOf : public Metamodelled {};
class TestNameOf : public Metamodelled {};
class TestFieldTypeName : public Metamodelled {};
class TestFieldSourceName : public Metamodelled {};

TEST_F(TestKinds, NoKindIsDeclaredTwiceAndEachSaysWhatItIs) {
  std::set<Kind> seen;
  for (const auto& k : metamodel.kinds()) {
    EXPECT_TRUE(seen.insert(k.kind).second) << metamodel.nameOf(k.kind);
    EXPECT_FALSE(k.meaning.empty()) << metamodel.nameOf(k.kind);
  }
  EXPECT_TRUE(seen.count(Kind::Class));
  EXPECT_TRUE(seen.count(Kind::Requirement));
}

TEST_F(TestSpecOf, TheSpecOfAKindIsTheRowOfTheTableForIt) {
  for (const auto& k : metamodel.kinds()) EXPECT_EQ(&metamodel.specOf(k.kind), &k);
}

TEST_F(TestFieldOf, AFieldIsOfItsKindOnly) {
  ASSERT_NE(metamodel.fieldOf(Kind::Requirement, "status"), nullptr);
  EXPECT_EQ(metamodel.fieldOf(Kind::Requirement, "status")->name, "status");
  EXPECT_EQ(metamodel.fieldOf(Kind::Attribute, "status"), nullptr);
  EXPECT_EQ(metamodel.fieldOf(Kind::Requirement, "no-such-field"), nullptr);
}

TEST_F(TestRelations, NoRelationIsDeclaredTwiceAndContainmentIsOne) {
  std::set<std::string> seen;
  for (const auto& r : metamodel.relations()) EXPECT_TRUE(seen.insert(r.kind).second) << r.kind;
  EXPECT_TRUE(seen.count("contains"));
  EXPECT_TRUE(seen.count("inherits"));
}

TEST_F(TestRelationOf, ARelationIsTheRowNamedAndNothingForAnotherWord) {
  for (const auto& r : metamodel.relations()) EXPECT_EQ(metamodel.relationOf(r.kind), &r);
  EXPECT_EQ(metamodel.relationOf("befriends"), nullptr);
}

TEST_F(TestCodes, EveryCodeIsAnErrorOrAWarningWithAMeaning) {
  std::set<std::string> seen;
  for (const auto& c : metamodel.codes()) {
    EXPECT_TRUE(seen.insert(c.code).second) << c.code;
    EXPECT_TRUE(c.severity == "error" || c.severity == "warning" || c.severity == "coverage") << c.code;
    EXPECT_EQ(c.code.rfind(c.severity == "error" ? "E-" : "W-", 0), 0u) << c.code;
    EXPECT_FALSE(c.meaning.empty()) << c.code;
  }
}

TEST_F(TestCodeOf, ACodeIsTheRowNamedAndNothingForAnother) {
  for (const auto& c : metamodel.codes()) EXPECT_EQ(metamodel.codeOf(c.code), &c);
  EXPECT_EQ(metamodel.codeOf("E-NO-SUCH-CODE"), nullptr);
}

TEST_F(TestAnnotations, EveryAnnotationSaysHowItIsWrittenAndWhatItMeans) {
  std::set<std::string> seen;
  for (const auto& a : metamodel.annotations()) {
    EXPECT_TRUE(seen.insert(a.name).second) << a.name;
    EXPECT_FALSE(a.meaning.empty()) << a.name;
    EXPECT_FALSE(a.relation.empty()) << a.name;
  }
  EXPECT_TRUE(seen.count("@requirement"));
  EXPECT_TRUE(seen.count("@<any other word>"));
}

TEST_F(TestAnnotationOf, AWordTheLanguageKnowsMakesItsRelationAndAnyOtherOnlyRefers) {
  EXPECT_EQ(metamodel.annotationOf("requirement").name, "@requirement");
  EXPECT_NE(metamodel.annotationOf("requirement").relation, "refers");
  EXPECT_EQ(metamodel.annotationOf("whatever").relation, "refers");
}

TEST_F(TestNameOf, AKindIsNamedAsTheLanguageWritesIt) {
  EXPECT_EQ(metamodel.nameOf(Kind::Requirement), "requirement");
  EXPECT_EQ(metamodel.nameOf(Kind::Class), "class");
  std::set<std::string> seen;
  for (const auto& k : metamodel.kinds()) EXPECT_TRUE(seen.insert(metamodel.nameOf(k.kind)).second);
}

TEST_F(TestFieldTypeName, ATypeOfFieldIsNamedInLowerCase) {
  EXPECT_EQ(metamodel.fieldTypeName(FieldType::Text), "text");
  EXPECT_EQ(metamodel.fieldTypeName(FieldType::Name), "name");
  EXPECT_NE(metamodel.fieldTypeName(FieldType::Names), metamodel.fieldTypeName(FieldType::Name));
}

TEST_F(TestFieldSourceName, WhereAFieldComesFromIsNamed) {
  EXPECT_EQ(metamodel.fieldSourceName(FieldSource::Written), "written");
  std::set<std::string> seen;
  for (FieldSource s : {FieldSource::Written, FieldSource::Syntax, FieldSource::Derived})
    EXPECT_TRUE(seen.insert(metamodel.fieldSourceName(s)).second);
}

}  // namespace
