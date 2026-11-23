#include <gtest/gtest.h>

#include "ds/check/Resolver.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace {

using ds::model::Entity;
using ds::model::Kind;

/// What a name means where it is written, asked of the model alone, with no checker (DS-125).
class TestResolver : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void SetUp() override {
    for (const auto& [kind, id, scope] : std::vector<std::tuple<Kind, std::string, std::string>>{{Kind::Package, "p", ""},
                                                                                                 {Kind::Module, "p@a", "p@"},
                                                                                                 {Kind::Class, "p@a::X", "p@a"},
                                                                                                 {Kind::Module, "p@a::b", "p@a"},
                                                                                                 {Kind::Class, "p@a::b::Y", "p@a::b"},
                                                                                                 {Kind::Method, "p@a::X::run", "p@a::X"}}) {
      Entity e;
      e.kind = kind;
      e.id = id;
      e.name = ds::model::Identity(id).lastName();
      e.scope = scope;
      model.declare(e);
    }
    model.relate({"inherits", "p@a::b::Y", "p@a::X", {}, ""});
  }
};

TEST_F(TestResolver, ANameResolvesFromTheInnermostScopeOutward) {
  const ds::check::Resolver resolver(model);
  EXPECT_EQ(resolver.resolve("X", "p@a::b"), "p@a::X");
  EXPECT_EQ(resolver.resolve("Nobody", "p@a::b"), "");
}

TEST_F(TestResolver, ATypeHasWhatItInherits) {
  const ds::check::Resolver resolver(model);
  EXPECT_TRUE(resolver.derives("p@a::b::Y", "p@a::X"));
  ASSERT_NE(resolver.member("p@a::b::Y", "run"), nullptr);
  EXPECT_EQ(resolver.member("p@a::b::Y", "run")->id, "p@a::X::run");
  EXPECT_EQ(resolver.resolveTypeText("list<X>", "p@a::b"), "p@a::X");
}

/// A bare name no scope on the way declares is the one class or enum of that name in its package, found through
/// the model's index of names, not a pass over the model (DS-336).
class TestNamesakes : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void SetUp() override {
    for (const auto& [kind, id, scope] : std::vector<std::tuple<Kind, std::string, std::string>>{{Kind::Package, "p", ""},
                                                                                                 {Kind::Module, "p@a", "p@"},
                                                                                                 {Kind::Module, "p@b", "p@"},
                                                                                                 {Kind::Module, "p@c", "p@"},
                                                                                                 {Kind::Package, "q", ""},
                                                                                                 {Kind::Module, "q@a", "q@"}})
      add(kind, id, scope);
  }

  void add(Kind kind, const std::string& id, const std::string& scope, const std::string& name = "", bool expected = false) {
    Entity e;
    e.kind = kind;
    e.id = id;
    e.name = name.empty() ? ds::model::Identity(id).lastName() : name;
    e.scope = scope;
    e.expected = expected;
    model.declare(e);
  }
};

TEST_F(TestNamesakes, TheOneClassOfTheNameInThePackageIsFound) {
  add(Kind::Class, "p@a::Plan", "p@a");
  const ds::check::Resolver resolver(model);
  EXPECT_EQ(resolver.resolve("Plan", "p@c"), "p@a::Plan");
  EXPECT_TRUE(resolver.ambiguous("Plan", "p@c").empty());
}

TEST_F(TestNamesakes, TwoClassesOfTheNameInThePackageAreAmbiguous) {
  add(Kind::Class, "p@a::Plan", "p@a");
  add(Kind::Enum, "p@b::Plan", "p@b");
  const ds::check::Resolver resolver(model);
  EXPECT_EQ(resolver.resolve("Plan", "p@c"), "");  // it relates nothing (W-NAME-AMBIGUOUS, DS-325)
  EXPECT_EQ(resolver.ambiguous("Plan", "p@c"), (std::vector<std::string>{"p@a::Plan", "p@b::Plan"}));
}

TEST_F(TestNamesakes, TheArityWrittenChoosesTheTemplate) {
  add(Kind::Class, "p@a::Box<1>", "p@a", "Box");
  add(Kind::Class, "p@b::Box", "p@b");
  const ds::check::Resolver resolver(model);
  EXPECT_EQ(resolver.resolve("Box<1>", "p@c"), "p@a::Box<1>");
  EXPECT_EQ(resolver.resolve("Box", "p@c"), "p@b::Box");
  EXPECT_EQ(resolver.resolve("Box<2>", "p@c"), "");
}

TEST_F(TestNamesakes, TheSameNameInAnotherPackageDoesNotCount) {
  add(Kind::Class, "p@a::Plan", "p@a");
  add(Kind::Class, "q@a::Plan", "q@a");
  const ds::check::Resolver resolver(model);
  EXPECT_EQ(resolver.resolve("Plan", "p@c"), "p@a::Plan");
  EXPECT_TRUE(resolver.ambiguous("Plan", "p@c").empty());
}

TEST_F(TestNamesakes, AnExpectedClassIsNotCountedUntilItIsDeclared) {
  add(Kind::Class, "p@a::Plan", "p@a");
  add(Kind::Class, "p@b::Plan", "p@b", "", true);  // referenced, declared nowhere yet
  const ds::check::Resolver resolver(model);
  EXPECT_EQ(resolver.resolve("Plan", "p@c"), "p@a::Plan");
  add(Kind::Class, "p@b::Plan", "p@b");  // its declaration arrives: the index is kept in step
  EXPECT_EQ(resolver.ambiguous("Plan", "p@c"), (std::vector<std::string>{"p@a::Plan", "p@b::Plan"}));
}

}  // namespace
