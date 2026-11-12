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
