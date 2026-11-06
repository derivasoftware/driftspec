#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/scene/Scene.hpp"

namespace {

/// The scene of ds's own design (REQ-00069-1): every box of the model has one place,
/// inside its parent, apart from its siblings, with and without private members.
class TestSceneOfTheDesign : public ::testing::Test {
 protected:
  ds::model::Model model;
  std::vector<ds::scene::Node> nodes;
  std::map<std::string, const ds::scene::Node*> byId;

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_SOURCE_DIR) + "/design")) binder.bind(f);
    ds::check::Checker(model).run(binder);
    nodes = ds::scene::Scene(model).build().nodes;
    for (const auto& n : nodes) byId[n.id] = &n;
  }

  static bool inside(const ds::scene::Box& in, const ds::scene::Box& out) {
    const double e = 0.01;
    return in.x >= out.x - e && in.y >= out.y - e && in.x + in.w <= out.x + out.w + e && in.y + in.h <= out.y + out.h + e;
  }
  static bool apart(const ds::scene::Box& a, const ds::scene::Box& b) {
    return a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y;
  }

  void nestsAndKeepsApart(bool lean) {
    std::map<std::string, std::vector<const ds::scene::Node*>> siblings;
    for (const auto& n : nodes) {
      if (lean && !n.lean) continue;
      const ds::scene::Box& box = lean ? n.leanBox : n.full;
      if (!n.parent.empty() && n.kind != "interface") {  // an interface sits on its package's border, as a port
        const ds::scene::Node* p = byId.at(n.parent);
        EXPECT_TRUE(inside(box, lean ? p->leanBox : p->full)) << n.id << (lean ? " (lean)" : "");
      }
      siblings[n.parent].push_back(&n);
    }
    for (const auto& s : siblings) {
      for (size_t i = 0; i < s.second.size(); ++i)
        for (size_t j = i + 1; j < s.second.size(); ++j) {
          const auto* a = s.second[i];
          const auto* b = s.second[j];
          EXPECT_TRUE(apart(lean ? a->leanBox : a->full, lean ? b->leanBox : b->full)) << a->id << " and " << b->id;
        }
    }
  }
};

TEST_F(TestSceneOfTheDesign, EveryClassFunctionAndMemberOfTheModelHasAPlace) {
  int units = 0, rows = 0;
  for (const auto& pair : model.entities()) {
    const auto& e = pair.second;
    if (e.expected || e.has("extern")) continue;
    const ds::model::Entity* scope = model.find(e.scope);
    if (scope && scope->has("extern")) continue;
    if (e.kind == ds::model::Kind::Class || e.kind == ds::model::Kind::Function || e.kind == ds::model::Kind::Enum) {
      ++units;
      EXPECT_EQ(byId.count(e.id), 1u) << e.id;
    }
    if (e.kind == ds::model::Kind::Method || e.kind == ds::model::Kind::Attribute) {
      ++rows;
      EXPECT_EQ(byId.count(e.id), 1u) << e.id;
    }
  }
  EXPECT_GT(units, 60);
  EXPECT_GT(rows, 200);
}

TEST_F(TestSceneOfTheDesign, EveryBoxNestsInItsParentApartFromItsSiblings) { nestsAndKeepsApart(false); }

TEST_F(TestSceneOfTheDesign, WithPrivateMembersHiddenEveryBoxStillNestsAndKeepsApart) {
  int hidden = 0;
  for (const auto& n : nodes) hidden += n.lean ? 0 : 1;
  EXPECT_GT(hidden, 30);
  nestsAndKeepsApart(true);
}

}  // namespace
