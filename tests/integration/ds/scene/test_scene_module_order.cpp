#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/scene/Scene.hpp"

namespace {

/// In a package, a module whose classes are inherited from comes before the modules that inherit from them
/// (REQ-00076-1), however long the chain and whatever the order the modules come in; modules that inherit nothing
/// of each other keep that order, and so do modules that inherit from each other.
class TestSceneOrderOfModules : public ::testing::Test {
 protected:
  /// The modules of package p as the scene places them, for a design written as text.
  static std::vector<std::string> modules(const std::string& text) {
    const ds::model::Metamodel metamodel;
    ds::model::Model model{metamodel};
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    binder.bind(parser.parseText("order.ds", text));
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
    const ds::scene::SceneDocument doc = ds::scene::Scene(model, metamodel).build();
    std::vector<std::string> out;
    for (const auto& n : doc.nodes)
      if (n.parent == "p" && n.kind == "module") out.push_back(n.id);
    return out;
  }
};

TEST_F(TestSceneOrderOfModules, AChainOfBasesIsPlacedBaseFirstThoughItsHeirsComeFirst) {
  const std::vector<std::string> placed = modules(
      "package p {\n"
      "  module a {\n"
      "    class A : b::B { + f() -> void }\n"
      "  }\n"
      "  module b {\n"
      "    class B : c::C { + g() -> void }\n"
      "  }\n"
      "  module c {\n"
      "    class C { + h() -> void }\n"
      "  }\n"
      "}\n");
  EXPECT_EQ(placed, (std::vector<std::string>{"p@c", "p@b", "p@a"}));
}

TEST_F(TestSceneOrderOfModules, OnlyABaseMovesAModuleThatInheritsNothingKeepsItsPlace) {
  const std::vector<std::string> placed = modules(
      "package p {\n"
      "  module m {\n"
      "    class M { + f() -> void }\n"
      "  }\n"
      "  module n {\n"
      "    class N : o::O { + g() -> void }\n"
      "  }\n"
      "  module o {\n"
      "    class O { + h() -> void }\n"
      "  }\n"
      "}\n");
  EXPECT_EQ(placed, (std::vector<std::string>{"p@m", "p@o", "p@n"}));
}

TEST_F(TestSceneOrderOfModules, ModulesThatInheritFromEachOtherKeepTheirOrderAfterTheirBase) {
  const std::vector<std::string> placed = modules(
      "package p {\n"
      "  module a {\n"
      "    class A : b::B { + f() -> void }\n"
      "    class D : c::C { + g() -> void }\n"
      "  }\n"
      "  module b {\n"
      "    class B : c::C { + h() -> void }\n"
      "    class E : a::D { + i() -> void }\n"
      "  }\n"
      "  module c {\n"
      "    class C { + j() -> void }\n"
      "  }\n"
      "}\n");
  EXPECT_EQ(placed, (std::vector<std::string>{"p@c", "p@a", "p@b"}));
}

}  // namespace
