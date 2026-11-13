#include <gtest/gtest.h>

#include "ds/check/Resolver.hpp"
#include "ds/model/Model.hpp"

namespace {

using ds::model::Entity;
using ds::model::Kind;

/// What a name means where it is written, asked of the model alone, with no checker (DS-125).
class TestResolver : public ::testing::Test {
 protected:
  ds::model::Model model;

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

}  // namespace
