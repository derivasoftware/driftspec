#include <gtest/gtest.h>

#include <set>
#include <string>

#include "ds/model/Metamodel.hpp"

namespace {

using ds::model::Kind;

const Kind kAllKinds[] = {Kind::Package,   Kind::Module,     Kind::Requirement, Kind::Verification, Kind::Interface, Kind::Operation,
                          Kind::View,      Kind::Constraint, Kind::Class,       Kind::Enum,         Kind::Function,  Kind::Method,
                          Kind::Attribute, Kind::Operator,   Kind::Sequence,    Kind::Message,      Kind::Frame,     Kind::State,
                          Kind::Diagram,   Kind::UseCase,    Kind::Outcome};

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

}  // namespace
