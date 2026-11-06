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

TEST_F(TestSceneOfTheDesign, ThePageOfTheDesignCarriesEveryNodeAndNothingClosesItsScript) {
  const std::string page = ds::scene::Scene(model).page("design");
  for (const auto& n : nodes) EXPECT_NE(page.find("\"" + n.id + "\""), std::string::npos) << n.id;
  EXPECT_EQ(page.find("</script>"), page.rfind("</script>"));  // the scene inside never ends the viewer's script
}

TEST_F(TestSceneOfTheDesign, TheDesignEmbedsAsOneFrameCarryingTheWholePage) {
  const std::string page = ds::scene::Scene(model).page("design");
  const std::string frame = ds::scene::Scene::frame(page, "design", 500);
  EXPECT_EQ(frame.rfind("<iframe ", 0), 0u);
  EXPECT_NE(frame.find("height:500px"), std::string::npos);
  EXPECT_EQ(frame.find('<', 1), frame.rfind("</iframe>"));  // the page travels escaped: no tag of it opens inside the host
  EXPECT_NE(frame.find("&lt;canvas"), std::string::npos);
  for (const auto& n : nodes) EXPECT_NE(frame.find("&quot;" + n.id + "&quot;"), std::string::npos) << n.id;
}

TEST_F(TestSceneOfTheDesign, AReadersStylesheetLandsAfterTheViewersAndCannotCloseIt) {
  const std::string page = ds::scene::Scene(model).page("design", ":root{--k-class:#e06c75}</style><script>bad()</script>");
  const auto theme = page.find("--k-class:#e06c75");
  ASSERT_NE(theme, std::string::npos);
  EXPECT_GT(theme, page.find("--k-class:#3567a0"));                    // after the viewer's own value, so it wins
  EXPECT_NE(page.find("<\\/style><script>bad()"), std::string::npos);  // left as text inside the style, never a tag
  EXPECT_EQ(page.find("</style>", theme), page.find("</style>\n</head>"));
}

TEST_F(TestSceneOfTheDesign, EveryBoxNestsInItsParentApartFromItsSiblings) { nestsAndKeepsApart(false); }

TEST_F(TestSceneOfTheDesign, WithPrivateMembersHiddenEveryBoxStillNestsAndKeepsApart) {
  int hidden = 0;
  for (const auto& n : nodes) hidden += n.lean ? 0 : 1;
  EXPECT_GT(hidden, 30);
  nestsAndKeepsApart(true);
}

}  // namespace
