#include <gtest/gtest.h>

#include <map>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/Ir.hpp"
#include "ds/render/IrExport.hpp"
#include "ds/render/Layout.hpp"
#include "ds/render/Svg.hpp"
#include "ds/theme/Theme.hpp"

namespace {

/// The style the drawings are written in, made once for the file (DS-393).
const ds::theme::Theme kTheme;

using ds::render::IrDocument;
using ds::render::LayoutResult;
using ds::render::Placed;
using ds::render::Rect;

/// The engine's own layout over the diagrams of the static corpus: structural
/// properties every drawing must hold, whatever the aesthetics.
class TestLayout : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/static")) binder.bind(f);
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
  }

  IrDocument document(const std::string& diagram) {
    return IrDocument::parse(ds::render::IrExport(model, metamodel).render("diagram:" + diagram, false)).bundled();
  }

  static bool overlap(const Rect& a, const Rect& b) { return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h; }

  static bool inside(const Rect& inner, const Rect& outer) {
    return inner.x >= outer.x && inner.y >= outer.y && inner.x + inner.w <= outer.x + outer.w && inner.y + inner.h <= outer.y + outer.h;
  }

  static std::map<std::string, const Placed*> byId(const LayoutResult& lay) {
    std::map<std::string, const Placed*> out;
    for (const auto& p : lay.placed) out[p.node->id] = &p;
    return out;
  }
};

TEST_F(TestLayout, EveryNodeIsPlacedAndNoTwoLeavesOverlap) {
  for (const char* name : {"CL_Model", "CL_Values", "CL_Templates", "CL_Package_ds"}) {
    const IrDocument doc = document(name);
    const LayoutResult lay = ds::render::Layout(ds::render::Metrics::svg(kTheme.measures(ds::theme::Sink::svg))).place(doc);
    EXPECT_EQ(lay.placed.size(), doc.nodes.size()) << name;
    for (size_t i = 0; i < lay.placed.size(); ++i) {
      if (lay.placed[i].node->kind == "container") continue;
      EXPECT_GT(lay.placed[i].rect.w, 0) << name;
      for (size_t j = i + 1; j < lay.placed.size(); ++j) {
        if (lay.placed[j].node->kind == "container") continue;
        EXPECT_FALSE(overlap(lay.placed[i].rect, lay.placed[j].rect))
            << name << ": " << lay.placed[i].node->id << " over " << lay.placed[j].node->id;
      }
    }
  }
}

TEST_F(TestLayout, ChildrenSitInsideTheirContainer) {
  const IrDocument doc = document("CL_Package_ds");
  const LayoutResult lay = ds::render::Layout(ds::render::Metrics::svg(kTheme.measures(ds::theme::Sink::svg))).place(doc);
  const auto placed = byId(lay);
  for (const auto& p : lay.placed) {
    if (p.node->parent.empty()) continue;
    EXPECT_TRUE(inside(p.rect, placed.at(p.node->parent)->rect)) << p.node->id << " outside " << p.node->parent;
  }
  EXPECT_FALSE(overlap(placed.at("ds@ds")->rect, placed.at("ds@lsp")->rect));
}

TEST_F(TestLayout, BasesAreDrawnAboveSubtypesAndWholesAboveParts) {
  const IrDocument doc = document("CL_Model");
  const LayoutResult lay = ds::render::Layout(ds::render::Metrics::svg(kTheme.measures(ds::theme::Sink::svg)))
                               .place(doc);  // kept: what byId points at lives in it
  const auto placed = byId(lay);
  EXPECT_LT(placed.at("ds@ds::Check")->rect.y + placed.at("ds@ds::Check")->rect.h, placed.at("ds@ds::ScopedCheck")->rect.y);
  EXPECT_LT(placed.at("ds@ds::Finding")->rect.y + placed.at("ds@ds::Finding")->rect.h, placed.at("ds@ds::Location")->rect.y);
}

TEST_F(TestLayout, EveryEdgeBetweenPlacedNodesGetsAnOrthogonalRoute) {
  const IrDocument doc = document("CL_Package_ds");
  const LayoutResult lay = ds::render::Layout(ds::render::Metrics::svg(kTheme.measures(ds::theme::Sink::svg))).place(doc);
  EXPECT_EQ(lay.routes.size(), doc.edges.size());
  for (const auto& r : lay.routes) {
    ASSERT_GE(r.second.points.size(), 2u);
    for (size_t i = 1; i < r.second.points.size(); ++i) {
      const auto& a = r.second.points[i - 1];
      const auto& b = r.second.points[i];
      EXPECT_TRUE(a.x == b.x || a.y == b.y) << "edge " << r.first << " has a diagonal segment";
    }
  }
}

TEST_F(TestLayout, LongEdgesStepAroundTheNodesTheyCross) {
  // Check -> Finding spans the layer of ScopedCheck: the route must not cut through it
  const IrDocument doc = document("CL_Model");
  const LayoutResult lay = ds::render::Layout(ds::render::Metrics::svg(kTheme.measures(ds::theme::Sink::svg))).place(doc);
  const auto placed = byId(lay);
  const Rect scoped = placed.at("ds@ds::ScopedCheck")->rect;
  for (size_t i = 0; i < doc.edges.size(); ++i) {
    if (doc.edges[i].from != "ds@ds::Check" || doc.edges[i].to != "ds@ds::Finding") continue;
    for (size_t k = 1; k < lay.routes.at(i).points.size(); ++k) {
      const auto& a = lay.routes.at(i).points[k - 1];
      const auto& b = lay.routes.at(i).points[k];
      const Rect segment{std::min(a.x, b.x), std::min(a.y, b.y), std::max(1, std::abs(a.x - b.x)), std::max(1, std::abs(a.y - b.y))};
      EXPECT_FALSE(overlap(segment, scoped)) << "segment " << k << " crosses ScopedCheck";
    }
  }
}

// DS-350: a gap keeps a lane only for an edge that turns in it. A -> Dn turns where it leaves A and where it reaches Dn,
// and runs straight down its channel through the gap between B and C: that gap is as tall as without it
TEST(TestLayoutLanes, AStraightLongEdgeReservesNoLaneRow) {
  const auto gapBetweenBAndC = [](const std::string& extra) {
    const ds::model::Metamodel metamodel;
    ds::model::Model model{metamodel};
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    binder.bind(
        parser.parseText("chain.ds",
                         "package p {\n  module m {\n"
                         "    class A {\n      - owns b: B\n" +
                             extra +
                             "    }\n"
                             "    class B {\n      - owns c: C\n    }\n"
                             "    class C {\n      - owns d1: D1\n      - owns d2: D2\n      - owns d3: D3\n      - owns d4: D4\n    }\n"
                             "    class D1 { }\n    class D2 { }\n    class D3 { }\n    class D4 { }\n"
                             "  }\n}\n"
                             "diagram CL_Chain {\n  kind: class\n  include: p@*\n}\n"));
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
    const IrDocument doc = IrDocument::parse(ds::render::IrExport(model, metamodel).render("diagram:CL_Chain", false)).bundled();
    const LayoutResult lay = ds::render::Layout(ds::render::Metrics::svg(kTheme.measures(ds::theme::Sink::svg))).place(doc);
    int bottomOfB = -1, topOfC = -1;
    for (const auto& p : lay.placed) {
      if (p.node->id == "p@m::B") bottomOfB = p.rect.y + p.rect.h;
      if (p.node->id == "p@m::C") topOfC = p.rect.y;
    }
    return bottomOfB < 0 || topOfC < 0 ? -1 : topOfC - bottomOfB;
  };
  const int without = gapBetweenBAndC("");
  // four long edges, so their lanes would outgrow the gap's least height (GAP_Y) if they were kept
  const int with = gapBetweenBAndC("      - refs f1: D1\n      - refs f2: D2\n      - refs f3: D3\n      - refs f4: D4\n");
  ASSERT_GT(without, 0);
  EXPECT_EQ(with, without) << "the long edge A -> D reserved a lane in the gap it only crosses";
}

TEST_F(TestLayout, ParallelRelationsAreBundledWithTheirLabelsStacked) {
  const IrDocument raw = IrDocument::parse(ds::render::IrExport(model, metamodel).render("diagram:CL_Templates", false));
  const IrDocument doc = raw.bundled();
  EXPECT_LT(doc.edges.size(), raw.edges.size());
  bool stacked = false;
  for (const auto& e : doc.edges)
    stacked |= e.from == "ds@ds::Index" && e.to == "ds@ds::Repository<1>" && e.label == "requirements 1\nrelations 1";
  EXPECT_TRUE(stacked);
}

/// Every change a diff marks on a relation has its style in the SVG (DS-419): a changed or renamed relation is drawn in
/// its colour, not as one that did not change.
TEST_F(TestLayout, EveryChangeOfARelationHasItsStyleInTheSvg) {
  const IrDocument doc = IrDocument::parse(R"({"ir": 1, "title": "diff", "nodes": [{"id": "A", "kind": "box", "label": "A"},
    {"id": "B", "kind": "box", "label": "B"}],
    "edges": [{"from": "A", "to": "B", "kind": "dependency", "label": "alters", "refs": {"change": "changed"}}]})");
  const std::string svg = ds::render::Svg(kTheme).render(doc);
  EXPECT_NE(svg.find("pr-edge-change-changed"), std::string::npos);
  for (const char* change : {"added", "removed", "changed", "renamed"})
    EXPECT_NE(svg.find(std::string(".pr-diagram .pr-edge-change-") + change + " {"), std::string::npos) << change;
}

TEST_F(TestLayout, TheSvgIsDeterministicAndCarriesStableIds) {
  const IrDocument doc = document("CL_Values");
  const std::string once = ds::render::Svg(kTheme).render(doc);
  const std::string twice = ds::render::Svg(kTheme).render(doc);
  EXPECT_EQ(once, twice);
  EXPECT_NE(once.find("data-id=\"ds@ds::Finding\""), std::string::npos);
  EXPECT_NE(once.find("class=\"pr-edge pr-edge-composition\""), std::string::npos);
  EXPECT_NE(once.find("<title>CL_Values</title>"), std::string::npos);
  EXPECT_EQ(once.find("NaN"), std::string::npos);
}

}  // namespace

namespace {

/// Sequences drawn by the engine carry their own interaction: stable ids, links, spans, a script.
class TestSequenceSvg : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/behaviour")) binder.bind(f);
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
  }

  std::string svg(const std::string& diagram) {
    return ds::render::Svg(kTheme).render(IrDocument::parse(ds::render::IrExport(model, metamodel).render("diagram:" + diagram, false)));
  }
};

TEST_F(TestSequenceSvg, DrawsLifelinesRowsFramesAndTheCaller) {
  const std::string out = svg("SEQ_Run");
  EXPECT_NE(out.find("class=\"pr-lifeline pr-actor\" data-id=\"caller\""), std::string::npos);
  EXPECT_NE(out.find("data-id=\"ds@ds::Checker\""), std::string::npos);
  EXPECT_NE(out.find("class=\"pr-frame pr-frame-loop\""), std::string::npos);
  EXPECT_NE(out.find("class=\"pr-frame pr-frame-alt\""), std::string::npos);
  EXPECT_NE(out.find("run(binder: Binder)"), std::string::npos);
  EXPECT_EQ(out.find("NaN"), std::string::npos);
}

TEST_F(TestSequenceSvg, CarriesLinksSpansLocationsAndTheScript) {
  const std::string deep = svg("SEQ_Run");
  EXPECT_NE(deep.find("data-ref-span=\"4-9\""), std::string::npos);  // resolveReference expanded into rows 4..9
  EXPECT_NE(deep.find("data-ref-expands=\"ds@ds::Checker::resolveReference::sequence\""), std::string::npos);
  EXPECT_NE(deep.find("behaviour.ds:"), std::string::npos);  // data-ref-location carries file:line (absolute under the test runner)
  EXPECT_NE(deep.find("<script><![CDATA["), std::string::npos);
  EXPECT_NE(deep.find("<a href=\"#ds@ds::Model\""), std::string::npos);  // a lifeline links to its class
  const std::string shallow = svg("SEQ_Run_Overview");
  EXPECT_NE(shallow.find("<a href=\"#ds@ds::Checker::resolveReference::sequence\""), std::string::npos);  // a folded child is a link
  EXPECT_EQ(shallow.find("data-ref-span"), std::string::npos);
}

TEST_F(TestSequenceSvg, IsDeterministic) { EXPECT_EQ(svg("SEQ_Publish"), svg("SEQ_Publish")); }

}  // namespace
