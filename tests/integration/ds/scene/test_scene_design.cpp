#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/format/SceneFormats.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/scene/Scene.hpp"

namespace {

/// The scene of ds's own design (REQ-00069-2): every box of the model has one place,
/// inside its parent, apart from its siblings, with and without private members.
class TestSceneOfTheDesign : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  std::vector<ds::scene::Node> nodes;
  std::map<std::string, const ds::scene::Node*> byId;

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_SOURCE_DIR) + "/design")) binder.bind(f);
    const auto checkSteps = ds::check::Steps::of();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
    nodes = ds::scene::Scene(model, metamodel).build().nodes;
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

  static std::string packOf(const std::string& page) {
    const auto at = page.find("const PACKED='") + std::string("const PACKED='").size();
    return page.substr(at, page.find('\'', at) - at);
  }

  static std::string packedIn(const std::string& page) { return ds::format::common::unpack(packOf(page)); }
};

TEST_F(TestSceneOfTheDesign, EveryClassFunctionAndMemberOfTheModelHasAPlace) {
  int units = 0, rows = 0;
  for (const auto& pair : model.entities()) {
    const auto& e = pair.second;
    if (e.expected) continue;
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
  const std::string page = ds::format::SceneHtml().write(ds::scene::Scene(model, metamodel).build(), {false, "design"});
  const std::string scene = packedIn(page);
  for (const auto& n : nodes) EXPECT_NE(scene.find("\"" + n.id + "\""), std::string::npos) << n.id;
  EXPECT_EQ(page.find("</script>"), page.rfind("</script>"));  // the scene inside never ends the viewer's script
  // REQ-00075-5: what the tool adds to a scene is bounded, the scene grows with the design it draws
  EXPECT_LT(page.size() - packOf(page).size(), 64u * 1024u);
  EXPECT_LT(packOf(page).size(), 128u * nodes.size());  // each node with its relations, artefacts and behaviours
}

TEST_F(TestSceneOfTheDesign, TheDesignEmbedsAsOneFrameCarryingTheWholePage) {
  const std::string page = ds::format::SceneHtml().write(ds::scene::Scene(model, metamodel).build(), {false, "design"});
  const std::string frame = ds::format::SceneHtml().write(ds::scene::Scene(model, metamodel).build(), {false, "design", "", {}, true, 500});
  EXPECT_EQ(frame.rfind("<iframe ", 0), 0u);
  EXPECT_NE(frame.find("height:500px"), std::string::npos);
  EXPECT_EQ(frame.find('<', 1), frame.rfind("</iframe>"));  // the page travels escaped: no tag of it opens inside the host
  EXPECT_NE(frame.find("&lt;canvas"), std::string::npos);
  const std::string scene = packedIn(frame);  // base64 needs no escaping in an attribute: the pack travels as it is
  for (const auto& n : nodes) EXPECT_NE(scene.find("\"" + n.id + "\""), std::string::npos) << n.id;
  EXPECT_LT(frame.size(), page.size() + 16u * 1024u);  // escaped for the attribute, the page grows little
}

TEST_F(TestSceneOfTheDesign, AnEmbeddingPageSteersTheViewerAndHearsTheReader) {
  const std::string page = ds::format::SceneHtml().write(ds::scene::Scene(model, metamodel).build(), {false, "design"});
  const auto at = page.find("const VIEWER='") + std::string("const VIEWER='").size();
  const std::string viewer = ds::format::common::unpack(page.substr(at, page.find('\'', at) - at));
  EXPECT_NE(viewer.find("window.DS_VIEW="), std::string::npos);
  for (const char* member :
       {"setCam(", "select(id)", "show(id)", "frameAny(", "mark(ids)", "highlight(list)", "notes(list)", "isolate(ids)"})
    EXPECT_NE(viewer.find(member), std::string::npos) << member;
  EXPECT_NE(viewer.find("{ds:'ready'}"), std::string::npos);                                    // when a page can steer it
  EXPECT_NE(viewer.find("{ds:'select',id:"), std::string::npos);                                // what the reader selects
  EXPECT_NE(viewer.find("parent.postMessage({ds:'fullscreen'}"), std::string::npos);            // the page goes fullscreen whole (DS-141)
  EXPECT_NE(viewer.find("parent.postMessage({ds:'note',ids,act:h.act}"), std::string::npos);    // a note folded, opened or seen (DS-102)
  EXPECT_NE(viewer.find("parent.postMessage({ds:'isolate',ids:[...ISO]}"), std::string::npos);  // what the reader isolates (DS-139)
  EXPECT_NE(page.find("id=\"iso\""), std::string::npos);                                        // the isolate button, in the toolbar
}

TEST_F(TestSceneOfTheDesign, AReadersStylesheetLandsAfterTheViewersAndCannotCloseIt) {
  const std::string page = ds::format::SceneHtml().write(ds::scene::Scene(model, metamodel).build(),
                                                         {false, "design", ":root{--k-class:#e06c75}</style><script>bad()</script>"});
  const auto theme = page.find("--k-class:#e06c75");
  ASSERT_NE(theme, std::string::npos);
  // the viewer's own styles travel packed and go in before the theme, which wins
  const auto at = page.find("const STYLE='") + std::string("const STYLE='").size();
  EXPECT_NE(ds::format::common::unpack(page.substr(at, page.find('\'', at) - at)).find("--k-class:#3567a0"), std::string::npos);
  EXPECT_NE(page.find("insertBefore(st,document.getElementById('theme'))"), std::string::npos);
  EXPECT_LT(page.find("<style id=\"theme\">"), theme);
  EXPECT_NE(page.find("<\\/style><script>bad()"), std::string::npos);  // left as text inside the style, never a tag
  EXPECT_EQ(page.find("</style>", theme), page.find("</style>\n</head>"));
}

TEST_F(TestSceneOfTheDesign, EveryLinkJoinsTwoNodesOfTheSceneInOneOfTheKindsItDraws) {
  const auto doc = ds::scene::Scene(model, metamodel).build();
  const std::set<std::string> kinds{"inherits", "realises", "owns", "shares", "refs", "uses", "consumes", "calls"};
  bool checkerCallsModel = false;  // the step that types behaviour calls the model (DS-138)
  for (const auto& l : doc.links) {
    EXPECT_EQ(byId.count(l.from), 1u) << l.from;
    EXPECT_EQ(byId.count(l.to), 1u) << l.to;
    EXPECT_TRUE(kinds.count(l.kind)) << l.kind;
    EXPECT_GT(l.count, 0);
    if (l.kind == "calls" && l.from.rfind("driftspec@ds@check::BehaviourTyping::", 0) == 0 &&
        l.to.rfind("driftspec@ds@model::Model::", 0) == 0) {
      checkerCallsModel = true;
      EXPECT_FALSE(l.sequences.empty());
    }
  }
  EXPECT_TRUE(checkerCallsModel);
  EXPECT_GT(doc.links.size(), 200u);
}

TEST_F(TestSceneOfTheDesign, EveryDiagramListsTheNodesItShowsAndEveryRequirementHasItsTitle) {
  const auto doc = ds::scene::Scene(model, metamodel).build();
  bool checkNamespace = false;
  for (const auto& d : doc.diagrams) {
    for (const auto& n : d.nodes) EXPECT_EQ(byId.count(n), 1u) << d.name << " " << n;
    if (d.name == "CL_CheckNamespace")
      checkNamespace = std::find(d.nodes.begin(), d.nodes.end(), "driftspec@ds@check::Checker") != d.nodes.end();
  }
  EXPECT_TRUE(checkNamespace);
  for (const auto& n : doc.nodes)
    for (const auto& r : n.requirements) EXPECT_TRUE(doc.titles.count(r)) << r;
}

TEST_F(TestSceneOfTheDesign, ThePageStaysSmallBecauseEachBehaviourIsWrittenOnce) {
  const auto doc = ds::scene::Scene(model, metamodel).build();
  std::set<std::string> seen;
  size_t calls = 0;
  for (const auto& b : doc.behaviours) {
    EXPECT_TRUE(seen.insert(b.id).second) << b.id;
    calls += b.calls.size();
  }
  EXPECT_GT(calls, 0u);  // how many more steps the tours replay: tests/integration/viewer/tours.test.ts (DS-130)
  EXPECT_LT(ds::format::SceneHtml().write(ds::scene::Scene(model, metamodel).build(), {false, "design"}).size(),
            250u * 1024u);  // the whole viewer of ds's design
}

TEST_F(TestSceneOfTheDesign, HidingPrivateMembersMakesNoBoxWider) {
  for (const auto& n : nodes) {
    if (!n.lean || n.kind == "row") continue;
    EXPECT_LE(n.leanBox.w, n.full.w + 0.01) << n.id;
  }
}

TEST_F(TestSceneOfTheDesign, EveryBoxNestsInItsParentApartFromItsSiblings) { nestsAndKeepsApart(false); }

TEST_F(TestSceneOfTheDesign, WithPrivateMembersHiddenEveryBoxStillNestsAndKeepsApart) {
  int hidden = 0;
  for (const auto& n : nodes) hidden += n.lean ? 0 : 1;
  EXPECT_GT(hidden, 30);
  nestsAndKeepsApart(true);
}

}  // namespace
