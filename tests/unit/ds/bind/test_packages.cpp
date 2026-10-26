#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/IrExport.hpp"

namespace {

/// Nested packages: the HLD nests with the conventions of every other entity (examples/packages).
class TestPackages : public ::testing::Test {
 protected:
  ds::model::Model model;

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/packages")) {
      EXPECT_FALSE(f.hasSyntaxErrors) << f.path;
      binder.bind(f);
    }
    ds::check::Checker(model).run(binder);
  }

  bool related(const std::string& kind, const std::string& from, const std::string& to) const {
    for (const auto& r : model.relations())
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

TEST_F(TestPackages, LevelByPlacementThroughTheLevels) {
  EXPECT_EQ(model.find("SREQ-00001-1")->scope, "app@");      // the outermost package: system
  EXPECT_EQ(model.find("REQ-00001-1")->scope, "app@core@");  // the inner package: its own level
  EXPECT_TRUE(ds::model::isSystemScope("app@"));
  EXPECT_FALSE(ds::model::isSystemScope("app@core@"));
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
