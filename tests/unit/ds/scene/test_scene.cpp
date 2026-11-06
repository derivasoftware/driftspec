#include <gtest/gtest.h>

#include <map>
#include <nlohmann/json.hpp>
#include <string>

#include "ds/model/Model.hpp"
#include "ds/scene/Scene.hpp"

namespace {

using ds::model::Entity;
using ds::model::Kind;

Entity entity(Kind kind, const std::string& id, const std::string& name, const std::string& scope) {
  Entity e;
  e.kind = kind;
  e.id = id;
  e.name = name;
  e.scope = scope;
  return e;
}

/// The structural scene of a small model: a package, a module, a class with a public and a
/// private member, a function (REQ-00069-1).
class TestScene : public ::testing::Test {
 protected:
  ds::model::Model model;
  std::map<std::string, ds::scene::Node> byId;

  void SetUp() override {
    model.declare(entity(Kind::Package, "p", "p", ""));
    model.declare(entity(Kind::Module, "p@m", "m", "p@"));
    model.declare(entity(Kind::Class, "p@m::A", "A", "p@m"));
    Entity run = entity(Kind::Method, "p@m::A::run", "run", "p@m::A");
    run.params = {{"name", "string"}};
    run.fields["returns"] = "std::vector<model::Entity>";
    run.location.line = 2;
    model.declare(run);
    Entity helper = entity(Kind::Method, "p@m::A::helper", "helper", "p@m::A");
    helper.fields["visibility"] = "-";
    helper.location.line = 3;
    model.declare(helper);
    Entity f = entity(Kind::Function, "p@m::load", "load", "p@m");
    f.params = {{"dir", "path"}};
    f.fields["returns"] = "Model";
    model.declare(f);
    model.relate({"requires", "p@m::A", "REQ-00001-1", {}, ""});
    for (const auto& n : ds::scene::Scene(model).build().nodes) byId[n.id] = n;
  }

  static bool inside(const ds::scene::Box& in, const ds::scene::Box& out) {
    return in.x >= out.x && in.y >= out.y && in.x + in.w <= out.x + out.w && in.y + in.h <= out.y + out.h;
  }
};

TEST_F(TestScene, EveryBoxSitsInsideItsParent) {
  ASSERT_EQ(byId.count("p@m::A::run"), 1u);
  for (const auto& pair : byId) {
    const auto& n = pair.second;
    if (n.parent.empty()) continue;
    EXPECT_TRUE(inside(n.full, byId.at(n.parent).full)) << n.id;
  }
  EXPECT_EQ(byId.at("p@m::A").kind, "unit");
  EXPECT_EQ(byId.at("p@m").kind, "module");
  EXPECT_EQ(byId.at("p@m::A").requirements.size(), 1u);
}

TEST_F(TestScene, AMemberReadsAsItsSignatureUnqualified) {
  EXPECT_EQ(byId.at("p@m::A::run").label, "+ run(name) → vector<Entity>");
  EXPECT_EQ(byId.at("p@m::load#sig").label, "load(dir) → Model");
  EXPECT_EQ(byId.at("p@m::load").stereotype, "function");
}

/// One fixture per unit of ds::scene the tests look at: a node, its box, the document.
class TestNode : public TestScene {};
class TestBox : public TestScene {};
class TestSceneDocument : public TestScene {};

TEST_F(TestNode, APrivateMemberHasNoLeanPlaceAndItsClassShrinks) {
  EXPECT_FALSE(byId.at("p@m::A::helper").lean);
  EXPECT_TRUE(byId.at("p@m::A::run").lean);
  EXPECT_LT(byId.at("p@m::A").leanBox.h, byId.at("p@m::A").full.h);
}

TEST_F(TestBox, ARowIsAsWideAsItsUnitAndOneRowTall) {
  const auto& row = byId.at("p@m::A::run").full;
  const auto& unit = byId.at("p@m::A").full;
  EXPECT_DOUBLE_EQ(row.w, unit.w);
  EXPECT_GT(row.h, 0);
  EXPECT_LT(row.h, unit.h);
}

TEST_F(TestSceneDocument, TheWorldHoldsEveryNodeInBothVariants) {
  const auto doc = ds::scene::Scene(model).build();
  for (const auto& n : doc.nodes) {
    EXPECT_TRUE(inside(n.full, doc.world)) << n.id;
    if (n.lean) {
      EXPECT_TRUE(inside(n.leanBox, doc.leanWorld)) << n.id;
    }
  }
}

TEST_F(TestScene, TheDocumentCarriesBothPlacesOfEveryNode) {
  const auto doc = nlohmann::json::parse(ds::scene::Scene(model).render(false));
  ASSERT_FALSE(doc["nodes"].empty());
  for (const auto& n : doc["nodes"]) {
    EXPECT_EQ(n["full"].size(), 4u) << n["id"];
    if (n["id"] == "p@m::A::helper") {
      EXPECT_TRUE(n["lean"].is_null());
    }
  }
  EXPECT_EQ(doc["world"].size(), 4u);
  EXPECT_EQ(doc["lean_world"].size(), 4u);
}

TEST_F(TestScene, ThePageIsTheViewerWithTheSceneAndTheTitleInside) {
  const std::string page = ds::scene::Scene(model).page("a<b");
  EXPECT_EQ(page.rfind("<!doctype html>", 0), 0u);
  EXPECT_NE(page.find("<canvas"), std::string::npos);
  EXPECT_NE(page.find("\"p@m::A::run\""), std::string::npos);
  EXPECT_NE(page.find("a&lt;b \u00b7 DriftSpec"), std::string::npos);
  EXPECT_EQ(page.find("/*SCENE*/"), std::string::npos);
  EXPECT_EQ(page.find("/*TITLE*/"), std::string::npos);
}

}  // namespace
