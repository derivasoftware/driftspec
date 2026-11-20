#include <gtest/gtest.h>

#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// A type is written as the code writes it, and only what the design declares relates (DS-18):
/// REQ-00024-3, REQ-00026-2, REQ-00028-2, REQ-00029-2, REQ-00169-1, REQ-00170-1.
class TestTypesAsText : public ::testing::Test {
 protected:
  ds::model::Model model;

  void load(const std::string& text) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    const auto file = parser.parseText("a.ds", text);
    ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
    binder.bind(file);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, checkSteps).run(binder);
  }

  bool related(const std::string& kind, const std::string& from, const std::string& to, const std::string& multiplicity = "?") const {
    for (const auto& r : model.index().from(from))
      if (r.kind == kind && r.to == to && (multiplicity == "?" || r.multiplicity == multiplicity)) return true;
    return false;
  }

  int findings(const std::string& code) const {
    int n = 0;
    for (const auto& f : model.findings()) n += f.code == code;
    return n;
  }
};

TEST_F(TestTypesAsText, OnlyTheNamesTheDesignDeclaresRelateTheRestIsText) {
  load(
      "package p {\n"
      "  module m {\n"
      "    class IStep\n"
      "    class Pool\n"
      "    class Error : Exception\n"
      "    class Holder {\n"
      "      - steps: std::map<std::string, std::unique_ptr<IStep>>\n"
      "      - pool: Pool*\n"
      "      - name: Optional[str]\n"
      "      - cb: Callable[[str], int]\n"
      "      - n: long long\n"
      "      + go(*args: str, **kwargs: Any) -> None\n"
      "    }\n"
      "    function main() -> int\n"
      "    sequence main {\n"
      "      -> subprocess::run(argv)\n"
      "      return 0\n"
      "    }\n"
      "  }\n"
      "}\n");
  EXPECT_TRUE(related("owns", "p@m::Holder::steps", "p@m::IStep"));
  EXPECT_TRUE(related("owns", "p@m::Holder::pool", "p@m::Pool"));
  for (const char* id : {"p@m::Holder::name", "p@m::Holder::cb", "p@m::Holder::n", "p@m::Error"})
    for (const auto& r : model.index().from(id))
      if (r.kind != "contains") ADD_FAILURE() << id << " " << r.kind << " " << r.to;
  EXPECT_TRUE(model.findings().empty()) << model.findings().front().code;  // Exception, std::, str and subprocess are text
  EXPECT_EQ(model.find("p@m::Holder::go")->params.at(0).name, "*args");
}

TEST_F(TestTypesAsText, MultiplicityIsWrittenInFrontOfTheName) {
  load(
      "package p {\n"
      "  module m {\n"
      "    class Watcher\n"
      "    class Item\n"
      "    class Holder {\n"
      "      - many items: std::vector<Item>\n"
      "      - refs many watchers: std::set<Watcher*>\n"
      "      - optional parent: Holder | None\n"
      "      - one: Item\n"
      "      - shares optional many: int\n"
      "    }\n"
      "  }\n"
      "}\n");
  EXPECT_TRUE(related("owns", "p@m::Holder::items", "p@m::Item", "*"));
  EXPECT_TRUE(related("refs", "p@m::Holder::watchers", "p@m::Watcher", "*"));
  EXPECT_TRUE(related("owns", "p@m::Holder::parent", "p@m::Holder", "0..1"));
  EXPECT_TRUE(related("owns", "p@m::Holder::one", "p@m::Item", "1"));
  EXPECT_NE(model.find("p@m::Holder::many"), nullptr);  // a modifier is a word only in front of a name (CST-00005-1)
  EXPECT_TRUE(model.findings().empty()) << model.findings().front().message;
}

TEST_F(TestTypesAsText, TwoOwnershipsOrTwoMultiplicitiesAreAConflict) {
  load(
      "package p {\n"
      "  module m {\n"
      "    class Pool\n"
      "    class Holder {\n"
      "      - refs shares a: Pool\n"
      "      - many optional b: std::vector<Pool>\n"
      "      - refs refs c: Pool\n"
      "      - refs many d: std::vector<Pool>\n"
      "    }\n"
      "  }\n"
      "}\n");
  EXPECT_EQ(findings("E-MODIFIER-CONFLICT"), 3);
  EXPECT_TRUE(related("refs", "p@m::Holder::a", "p@m::Pool"));  // the first written is kept
  EXPECT_TRUE(related("owns", "p@m::Holder::b", "p@m::Pool", "*"));
}

TEST_F(TestTypesAsText, ABareNameNoScopeDeclaresIsTheOneClassOfThePackage) {
  load(
      "package shop {\n"
      "  module plan {\n"
      "    class Plan\n"
      "    class Twice\n"
      "  }\n"
      "  module other {\n"
      "    class Twice\n"
      "  }\n"
      "  module cli {\n"
      "    class Runner {\n"
      "      - plan: Plan\n"
      "      - twice: Twice\n"
      "    }\n"
      "  }\n"
      "}\n"
      "package elsewhere {\n"
      "  module m {\n"
      "    class User {\n"
      "      - plan: Plan\n"
      "    }\n"
      "  }\n"
      "}\n");
  EXPECT_TRUE(related("owns", "shop@cli::Runner::plan", "shop@plan::Plan"));
  for (const auto& r : model.index().from("shop@cli::Runner::twice")) ADD_FAILURE() << "two Twice, related to " << r.to;
  for (const auto& r : model.index().from("elsewhere@m::User::plan")) ADD_FAILURE() << "another package, related to " << r.to;
  EXPECT_TRUE(model.findings().empty()) << model.findings().front().message;
}

TEST_F(TestTypesAsText, ABaseAndAMethodCarryTheirTemplateArgumentsAsWritten) {
  load(
      "package p {\n"
      "  module m {\n"
      "    class Entity\n"
      "    class T\n"
      "    class Scene\n"
      "    class IFormat<D> {\n"
      "      + abstract write(document: D) -> std::string\n"
      "    }\n"
      "    class SceneJson : IFormat<Scene> {\n"
      "      + write(document: Scene) -> std::string\n"
      "      + each<F>(f: F) -> void\n"
      "    }\n"
      "    class Base<X: Entity>\n"
      "    class Holder<T> : Base<T>\n"
      "    class IntBox : Box[int]\n"
      "  }\n"
      "}\n");
  EXPECT_TRUE(related("inherits", "p@m::SceneJson", "p@m::IFormat<1>"));
  EXPECT_EQ(model.find("p@m::SceneJson")->field("bases"), "IFormat<Scene>");
  EXPECT_EQ(model.find("p@m::SceneJson::each")->field("template"), "F");
  EXPECT_TRUE(related("inherits", "p@m::Holder<1>", "p@m::Base<1>"));
  EXPECT_EQ(findings("E-TEMPLATE-BOUND"), 0);  // Holder's own T, not the class T, is the argument
  EXPECT_TRUE(model.findings().empty()) << model.findings().front().message;
}

TEST_F(TestTypesAsText, ALiteralDiffersOnlyFromADesignedClass) {
  load(
      "package p {\n"
      "  module m {\n"
      "    class Item\n"
      "    class Pool {\n"
      "      + take(count: int) -> Item\n"
      "      + give(item: Item) -> void\n"
      "    }\n"
      "    class Holder {\n"
      "      - pool: Pool\n"
      "      + go() -> Item\n"
      "    }\n"
      "    sequence Holder::go {\n"
      "      -> pool.take(\"three\") -> item\n"
      "      -> pool.give(\"x\")\n"
      "      -> pool.give(item)\n"
      "      return 0\n"
      "    }\n"
      "  }\n"
      "}\n");
  EXPECT_EQ(findings("W-ARG-TYPE"), 1);     // "x" where an Item is expected; "three" against int is text
  EXPECT_EQ(findings("W-RETURN-TYPE"), 1);  // 0 where an Item is promised
}

}  // namespace
