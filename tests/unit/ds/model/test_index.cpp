#include <gtest/gtest.h>

#include "ds/model/Model.hpp"

namespace {

using ds::model::Entity;
using ds::model::Kind;
using ds::model::Model;
using ds::model::Relation;

Entity entity(Kind kind, const std::string& id, const std::string& scope = "", bool expected = false) {
  Entity e;
  e.kind = kind;
  e.id = id;
  e.name = id;
  e.scope = scope;
  e.expected = expected;
  return e;
}

std::vector<std::string> ends(const std::vector<Relation>& relations, bool to) {
  std::vector<std::string> out;
  for (const auto& r : relations) out.push_back(r.kind + " " + (to ? r.to : r.from));
  return out;
}

/// The questions the model answers by identity, with no pass over it (REQ-00001-1, DS-171).
class TestIndex : public ::testing::Test {
 protected:
  Model model;

  void SetUp() override {
    model.declare(entity(Kind::Package, "p"));
    model.declare(entity(Kind::Module, "p@m", "p@"));
    model.declare(entity(Kind::Class, "p@m::A", "p@m"));
    model.declare(entity(Kind::Class, "p@m::B", "p@m"));
    model.declare(entity(Kind::Method, "p@m::A::run", "p@m::A"));
    model.relate({"contains", "p", "p@m", {}, ""});
    model.relate({"inherits", "p@m::B", "p@m::A", {}, ""});
    model.relate({"uses", "p@m::A::run", "p@m::B", {}, ""});
    model.relate({"uses", "p@m::B", "p@m::A", {}, ""});
  }
};

TEST_F(TestIndex, WhatLeavesAndWhatReachesAnEntityComeInTheOrderStated) {
  const std::vector<std::string> leaves = {"inherits p@m::A", "uses p@m::A"};
  EXPECT_EQ(ends(model.index().from("p@m::B"), true), leaves);
  const std::vector<std::string> reaches = {"inherits p@m::B", "uses p@m::B"};
  EXPECT_EQ(ends(model.index().to("p@m::A"), false), reaches);
  EXPECT_TRUE(model.index().from("p@m::Nobody").empty());
}

TEST_F(TestIndex, EveryRelationOfAKindComesInTheOrderStated) {
  const std::vector<std::string> uses = {"uses p@m::B", "uses p@m::A"};
  EXPECT_EQ(ends(model.index().ofKind("uses"), true), uses);
  EXPECT_TRUE(model.index().ofKind("owns").empty());
}

TEST_F(TestIndex, WhatTouchesAnEntityComesOnceInTheOrderStated) {
  model.relate({"uses", "p@m::B", "p@m::B", {}, ""});  // from an entity to itself: once
  std::vector<std::string> ends;
  for (const auto& r : model.index().touching("p@m::B")) ends.push_back(r.kind + " " + r.from + " " + r.to);
  const std::vector<std::string> expected = {"inherits p@m::B p@m::A", "uses p@m::A::run p@m::B", "uses p@m::B p@m::A",
                                             "uses p@m::B p@m::B"};
  EXPECT_EQ(ends, expected);
  EXPECT_TRUE(model.index().touching("p@m::Nobody").empty());
}

TEST_F(TestIndex, SeveralKindsAndTheWholeListComeInTheOrderStated) {
  const std::vector<std::string> kinds = {"contains p@m", "inherits p@m::A", "uses p@m::B", "uses p@m::A"};
  EXPECT_EQ(ends(model.index().ofKinds({"uses", "inherits", "contains"}), true), kinds);  // the order asked does not matter
  EXPECT_EQ(ends(model.index().all(), true), kinds);
  EXPECT_EQ(model.index().count(), 4u);
  EXPECT_TRUE(model.index().ofKinds({"owns", "refs"}).empty());
}

TEST_F(TestIndex, AnAnswerIsACopyThatALaterChangeLeavesAsItWas) {
  const std::vector<Relation> held = model.index().all();
  for (int i = 0; i < 1000; ++i) model.relate({"uses", "p@m::A", "p@m::B", {}, ""});  // the list grows, and moves
  ASSERT_EQ(held.size(), 4u);
  EXPECT_EQ(held[1].kind, "inherits");
  EXPECT_EQ(held[3].to, "p@m::A");
  EXPECT_EQ(model.index().count(), 1004u);
}

TEST_F(TestIndex, ARelationStatedLaterIsAnsweredAtOnce) {
  model.relate({"uses", "p@m::B", "p@m::A::run", {}, ""});
  EXPECT_EQ(model.index().from("p@m::B").size(), 3u);
  EXPECT_EQ(model.index().to("p@m::A::run").size(), 1u);
}

TEST_F(TestIndex, AScopeNamesWhatItDeclaresExpectedOnesIncluded) {
  model.declare(entity(Kind::Class, "p@m::Later", "p@m", true));
  const std::vector<std::string> members = {"p@m::A", "p@m::B", "p@m::Later"};
  EXPECT_EQ(model.index().members("p@m"), members);
  EXPECT_EQ(model.index().members("p@m::A"), std::vector<std::string>{"p@m::A::run"});
  EXPECT_TRUE(model.index().members("p@none").empty());
}

TEST_F(TestIndex, AnExpectedNodeThatIsDeclaredMovesToTheScopeOfItsDeclaration) {
  model.declare(entity(Kind::Class, "p@m::C", "p@", true));  // referenced from the package
  model.declare(entity(Kind::Class, "p@m::C", "p@m"));       // declared in the module
  EXPECT_TRUE(model.index().members("p@").size() == 1u);     // the module only
  EXPECT_EQ(model.index().members("p@m").back(), "p@m::C");
}

TEST_F(TestIndex, OnlyTheRelationsOfOneKindAreAnsweredInTheOrderStated) {
  // DS-345: a reader that wants one kind gets only those, copied, not every relation to filter
  Model m;
  m.relate({"inherits", "a", "b", {}, ""});
  m.relate({"owns", "a", "c", {}, ""});
  m.relate({"inherits", "a", "d", {}, ""});
  m.relate({"inherits", "e", "b", {}, ""});
  EXPECT_EQ(ends(m.index().fromOfKind("a", "inherits"), true), (std::vector<std::string>{"inherits b", "inherits d"}));
  EXPECT_EQ(ends(m.index().toOfKind("b", "inherits"), false), (std::vector<std::string>{"inherits a", "inherits e"}));
  EXPECT_TRUE(m.index().fromOfKind("a", "uses").empty());
  EXPECT_TRUE(m.index().toOfKind("nobody", "inherits").empty());
}

TEST_F(TestIndex, TheDeclaredClassesOfANameAreKeptByPackageThroughARename) {
  // DS-336: what the resolver asks of a bare name, kept in step with declare and rename
  Model m;
  Entity plan = entity(Kind::Class, "p@a::Plan", "p@a");
  plan.name = "Plan";
  m.declare(plan);
  Entity other = entity(Kind::Class, "q@a::Plan", "q@a");
  other.name = "Plan";
  m.declare(other);
  EXPECT_EQ(m.index().named("p@", "Plan"), (std::vector<std::string>{"p@a::Plan"}));
  EXPECT_TRUE(m.index().named("p@", "Nobody").empty());
  ASSERT_TRUE(m.rename("p@a::Plan", "p@a::Plan2"));
  EXPECT_EQ(m.index().named("p@", "Plan"), (std::vector<std::string>{"p@a::Plan2"}));  // the entity keeps its name
}

TEST_F(TestIndex, WhatHoldsWhatIsATreeFromContainsAndThenFromTheScope) {
  EXPECT_EQ(model.index().parent("p@m"), "p");       // the contains relation
  EXPECT_EQ(model.index().parent("p@m::A"), "p@m");  // its scope, declared
  EXPECT_EQ(model.index().parent("p@m::A::run"), "p@m::A");
  EXPECT_EQ(model.index().parent("p"), "");
  const std::vector<std::string> held = {"p@m::A", "p@m::B"};
  EXPECT_EQ(model.index().children("p@m"), held);
}

TEST_F(TestIndex, ALinkThatWouldCloseACycleIsLeftOutOfTheTree) {
  model.relate({"contains", "p@m", "p", {}, ""});  // a half-bound model: m holds p, p holds m
  EXPECT_EQ(model.index().parent("p@m"), "p");
  EXPECT_EQ(model.index().parent("p"), "");
}

TEST_F(TestIndex, TheTreeFollowsWhatIsDeclaredAfterItWasAsked) {
  EXPECT_TRUE(model.index().children("p@m::B").empty());
  model.declare(entity(Kind::Method, "p@m::B::stop", "p@m::B"));
  EXPECT_EQ(model.index().children("p@m::B"), std::vector<std::string>{"p@m::B::stop"});
}

TEST_F(TestIndex, AnArtefactIsFoundByAnyRevisionOrNone) {
  model.declare(entity(Kind::Requirement, "REQ-00001-2"));
  EXPECT_EQ(model.index().revisionOf("REQ-00001-1"), "REQ-00001-2");
  EXPECT_EQ(model.index().revisionOf("REQ-00001"), "REQ-00001-2");
  EXPECT_EQ(model.index().revisionOf("REQ-00009-1"), "");
  model.declare(entity(Kind::Requirement, "REQ-00002-1", "", true));  // only referenced
  EXPECT_EQ(model.index().revisionOf("REQ-00002-1"), "");
}

TEST_F(TestIndex, ARenameMovesEverythingIndexedByTheOldIdentity) {
  ASSERT_TRUE(model.rename("p@m::A", "p@m::Z"));
  EXPECT_TRUE(model.index().from("p@m::A::run").empty());
  EXPECT_EQ(model.index().from("p@m::Z::run").size(), 1u);
  EXPECT_EQ(model.index().to("p@m::Z").size(), 2u);
  EXPECT_EQ(model.index().members("p@m::Z"), std::vector<std::string>{"p@m::Z::run"});
  EXPECT_EQ(model.index().parent("p@m::Z::run"), "p@m::Z");
}

TEST_F(TestIndex, ACopyOfTheModelAnswersForItself) {
  Model copy = model;
  copy.relate({"uses", "p@m::A", "p@m::B", {}, ""});
  EXPECT_EQ(copy.index().from("p@m::A").size(), 1u);
  EXPECT_TRUE(model.index().from("p@m::A").empty());
}

}  // namespace
