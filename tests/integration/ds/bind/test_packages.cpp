#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/IrExport.hpp"

namespace {

/// Nested packages: the HLD nests with the conventions of every other entity (tests/corpus/packages).
class TestPackages : public ::testing::Test {
 protected:
  ds::model::Model model;

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/packages")) {
      EXPECT_FALSE(f.hasSyntaxErrors) << f.path;
      binder.bind(f);
    }
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, checkSteps).run(binder);
  }

  bool related(const std::string& kind, const std::string& from, const std::string& to) const {
    for (const auto& r : model.index().all())
      if (r.kind == kind && r.from == from && r.to == to) return true;
    return false;
  }

  bool reported(const std::string& code, const std::string& fragment) const {
    for (const auto& f : model.findings())
      if (f.code == code && f.message.find(fragment) != std::string::npos) return true;
    return false;
  }
};

TEST_F(TestPackages, EveryPackageLevelAddsItsNameWithAt) {
  const auto* core = model.find("app@core");
  ASSERT_NE(core, nullptr);
  EXPECT_EQ(core->kind, ds::model::Kind::Package);
  EXPECT_EQ(core->scope, "app@");
  const auto* entity = model.find("app@core@model::Entity");
  ASSERT_NE(entity, nullptr);
  EXPECT_EQ(entity->scope, "app@core@model");
  EXPECT_TRUE(related("contains", "app", "app@core"));
  EXPECT_TRUE(related("contains", "app@core", "app@core@model"));
}

TEST_F(TestPackages, TheFlatFormIsTheSameIdentityAsNesting) {
  const auto* render = model.find("app@render");
  ASSERT_NE(render, nullptr);
  EXPECT_EQ(render->scope, "app@");
  EXPECT_TRUE(related("contains", "app", "app@render"));                                 // reopened by path: the outer package contains it
  EXPECT_TRUE(related("owns", "app@render@svg::Svg::model", "app@core@model::Entity"));  // resolved across packages
  EXPECT_TRUE(related("owns", "app@render@svg::Svg::cache", "app@core@model::Entity"));  // std::map passes ownership to its value
}

TEST_F(TestPackages, AnOuterPackageNobodyDeclaresIsExpected) {
  const auto* missing = model.find("app@missing");
  ASSERT_NE(missing, nullptr);
  EXPECT_TRUE(missing->expected);
  EXPECT_TRUE(reported("W-REF-UNDECLARED", "app@missing"));
  EXPECT_TRUE(related("contains", "app@missing", "app@missing@deep"));
}

TEST_F(TestPackages, ABoxRealisesRequirementsLikeAClass) {
  EXPECT_TRUE(related("requires", "app@core", "SREQ-00001-1"));  // @requirement in the package body
}

TEST_F(TestPackages, AModifierIsAWordOnlyInFrontOfAName) {
  const auto* flag = model.find("app@render@svg::Svg::abstract");
  ASSERT_NE(flag, nullptr);  // an attribute named abstract
  EXPECT_EQ(flag->kind, ds::model::Kind::Attribute);
  EXPECT_FALSE(flag->has("abstract"));
  const auto* run = model.find("app@render@svg::Svg::run");
  ASSERT_NE(run, nullptr);
  EXPECT_TRUE(run->has("static"));  // the same word, modifying
}

TEST_F(TestPackages, ABareNameFallsBackToTheExternCatalogue) {
  EXPECT_TRUE(related("owns", "app@render@svg::Svg::node", "app@render@tree_sitter::TSNode"));  // resolved by simple name
  EXPECT_TRUE(reported("E-REF-AMBIGUOUS", "Dup"));                                              // two externs, no winner
  EXPECT_FALSE(related("owns", "app@render@svg::Svg::dup", "app@render@tree_sitter::Dup"));
}

TEST_F(TestPackages, AGenericUseFindsTheExternOfItsSimpleName) {
  // an extern's template arguments are not the design's to check: the use's arity, and the declaration's, do not matter
  EXPECT_TRUE(related("owns", "app@render@svg::Svg::ends", "app@render@std::pair<2>"));
  EXPECT_TRUE(related("owns", "app@render@svg::Svg::span", "app@render@python::tuple"));
  EXPECT_FALSE(reported("W-REF-UNDECLARED", "pair<2>"));
  EXPECT_FALSE(reported("W-REF-UNDECLARED", "tuple<3>"));
  EXPECT_FALSE(reported("E-REF-AMBIGUOUS", "pair<2>"));  // std::pair<2> and python::pair<1>: the arity of the use decides
}

TEST_F(TestPackages, AnEnumRealisesRequirementsLikeAClass) {
  EXPECT_TRUE(related("requires", "app@render@svg::Mode", "SREQ-00001-1"));
  EXPECT_EQ(model.find("app@render@svg::Mode")->fields.at("values"), "Fast, Slow");
}

TEST_F(TestPackages, AnExtensionAddsRelationsWithoutRedefining) {
  EXPECT_EQ(model.find("REQ-00002-1")->fields.at("title"), "The render shall draw the model");  // defined once, in packages.ds
  EXPECT_TRUE(related("refines", "REQ-00002-1", "SREQ-00001-1"));                               // wired in wiring.ds
  EXPECT_TRUE(related("requires", "app@render@svg::Svg", "REQ-00002-1"));
  EXPECT_FALSE(reported("E-DUPLICATE-IDENTITY", "REQ-00002-1"));
  EXPECT_FALSE(reported("E-DUPLICATE-IDENTITY", "Svg"));
  EXPECT_FALSE(model.find("app@render@svg::Svg")->expected);
}

TEST_F(TestPackages, AnExtensionOfNothingIsExpected) {
  ASSERT_NE(model.find("REQ-00099-1"), nullptr);
  EXPECT_TRUE(model.find("REQ-00099-1")->expected);
  EXPECT_TRUE(reported("W-REF-UNDECLARED", "REQ-00099-1"));
  EXPECT_TRUE(related("refines", "REQ-00099-1", "SREQ-00001-1"));  // the relation waits for the definition
}

TEST_F(TestPackages, LevelByPlacementThroughTheLevels) {
  EXPECT_EQ(model.find("SREQ-00001-1")->scope, "app@");      // the outermost package: system
  EXPECT_EQ(model.find("REQ-00001-1")->scope, "app@core@");  // the inner package: its own level
  EXPECT_TRUE(ds::model::Scope("app@").isSystem());
  EXPECT_FALSE(ds::model::Scope("app@core@").isSystem());
  EXPECT_TRUE(related("refines", "REQ-00001-1", "SREQ-00001-1"));
}

TEST_F(TestPackages, NestedPackagesAreContainersOfTheDrawing) {
  const auto doc = nlohmann::json::parse(ds::render::IrExport(model).render("diagram:CL_App", false));
  std::map<std::string, nlohmann::json> by;
  for (const auto& n : doc["nodes"]) by[n["id"]] = n;
  ASSERT_TRUE(by.count("app@core"));
  EXPECT_EQ(by["app@core"]["kind"], "container");
  EXPECT_EQ(by["app@core"]["classifier"], "package");
  EXPECT_EQ(by["app@core"]["label"], "core");
  EXPECT_FALSE(by["app@core"].contains("parent"));  // the outermost package is the drawing itself
  EXPECT_EQ(by["app@core@model"]["parent"], "app@core");
  EXPECT_EQ(by["app@core@model"]["label"], "model");
  EXPECT_EQ(by["app@core@model::Entity"]["parent"], "app@core@model");
  EXPECT_EQ(by["app@render@svg::Svg"]["parent"], "app@render@svg");
}

}  // namespace

namespace {

/// DS-20: a class written with its requirements and no members, where no other block defines it.
ds::model::Model loneClass() {
  ds::model::Model model;
  ds::parse::Parser parser;
  ds::bind::Binder binder(model);
  binder.bind(parser.parseText("errors.ds",
                               "package app {\n"
                               "  requirement REQ-00001-1 {\n"
                               "    title: The app shall refuse what it does not allow\n"
                               "  }\n"
                               "  module errors {\n"
                               "    class NotAllowed {\n"
                               "      @requirement REQ-00001-1\n"
                               "    }\n"
                               "    class Store {\n"
                               "      + put(key: string)\n"
                               "    }\n"
                               "  }\n"
                               "}\n"));
  binder.bind(parser.parseText("wiring.ds",
                               "package app {\n"
                               "  module errors {\n"
                               "    class Store {\n"
                               "      @requirement REQ-00001-1\n"
                               "    }\n"
                               "  }\n"
                               "}\n"));
  const auto checkSteps = ds::check::steps();
  ds::check::Checker(model, checkSteps).run(binder);
  return model;
}

}  // namespace

TEST(TestLoneExtension, AClassWithRequirementsAndNoMembersIsADefinitionWhenNothingElseDefinesIt) {
  const ds::model::Model model = loneClass();
  const auto* lone = model.find("app@errors::NotAllowed");
  ASSERT_NE(lone, nullptr);
  EXPECT_FALSE(lone->expected);
  EXPECT_FALSE(lone->has("extension"));
  bool contained = false, required = false;
  for (const auto& r : model.index().all()) {
    contained |= r.kind == "contains" && r.from == "app@errors" && r.to == "app@errors::NotAllowed";
    required |= r.kind == "requires" && r.from == "app@errors::NotAllowed" && r.to == "REQ-00001-1";
  }
  EXPECT_TRUE(contained);
  EXPECT_TRUE(required);
  for (const auto& f : model.findings()) EXPECT_NE(f.code, "W-REF-UNDECLARED") << f.message;
  // a block of annotations of a class defined elsewhere stays an extension: one definition, with its member
  const auto* store = model.find("app@errors::Store");
  ASSERT_NE(store, nullptr);
  EXPECT_FALSE(store->expected);
  EXPECT_NE(model.find("app@errors::Store::put"), nullptr);
}
