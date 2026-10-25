#include <gtest/gtest.h>

#include <map>

#include "driftspec/bind/Binder.hpp"
#include "driftspec/check/Checker.hpp"
#include "driftspec/model/Model.hpp"
#include "driftspec/parse/Parser.hpp"
#include "driftspec/render/Ir.hpp"
#include "driftspec/render/IrExport.hpp"
#include "driftspec/render/Layout.hpp"
#include "driftspec/render/Svg.hpp"

namespace {

using driftspec::render::IrDocument;
using driftspec::render::LayoutResult;
using driftspec::render::Placed;
using driftspec::render::Rect;

/// The engine's own layout over the diagrams of the static corpus: structural
/// properties every drawing must hold, whatever the aesthetics.
class TestLayout : public ::testing::Test {
 protected:
  driftspec::model::Model model;

  void SetUp() override {
    driftspec::parse::Parser parser;
    driftspec::bind::Binder binder(model);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/static")) binder.bind(f);
    driftspec::check::Checker(model).run(binder);
  }

  IrDocument document(const std::string& diagram) {
    return IrDocument::parse(driftspec::render::IrExport(model).render("diagram:" + diagram, false)).bundled();
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
    const LayoutResult lay = driftspec::render::layout(doc);
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
  const LayoutResult lay = driftspec::render::layout(doc);
  const auto placed = byId(lay);
  for (const auto& p : lay.placed) {
    if (p.node->parent.empty()) continue;
    EXPECT_TRUE(inside(p.rect, placed.at(p.node->parent)->rect)) << p.node->id << " outside " << p.node->parent;
  }
  EXPECT_FALSE(overlap(placed.at("ds@ds")->rect, placed.at("ds@lsp")->rect));
}

TEST_F(TestLayout, BasesAreDrawnAboveSubtypesAndWholesAboveParts) {
  const IrDocument doc = document("CL_Model");
  const auto placed = byId(driftspec::render::layout(doc));
  EXPECT_LT(placed.at("ds@ds::Check")->rect.y + placed.at("ds@ds::Check")->rect.h, placed.at("ds@ds::ScopedCheck")->rect.y);
  EXPECT_LT(placed.at("ds@ds::Finding")->rect.y + placed.at("ds@ds::Finding")->rect.h, placed.at("ds@ds::Location")->rect.y);
}

TEST_F(TestLayout, EveryEdgeBetweenPlacedNodesGetsAnOrthogonalRoute) {
  const IrDocument doc = document("CL_Package_ds");
  const LayoutResult lay = driftspec::render::layout(doc);
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
  const LayoutResult lay = driftspec::render::layout(doc);
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

TEST_F(TestLayout, ParallelRelationsAreBundledWithTheirLabelsStacked) {
  const IrDocument raw = IrDocument::parse(driftspec::render::IrExport(model).render("diagram:CL_Templates", false));
  const IrDocument doc = raw.bundled();
  EXPECT_LT(doc.edges.size(), raw.edges.size());
  bool stacked = false;
  for (const auto& e : doc.edges)
    stacked |= e.from == "ds@ds::Index" && e.to == "ds@ds::Repository<1>" && e.label == "requirements 1\nrelations 1";
  EXPECT_TRUE(stacked);
}

TEST_F(TestLayout, TheSvgIsDeterministicAndCarriesStableIds) {
  const IrDocument doc = document("CL_Values");
  const std::string once = driftspec::render::Svg().render(doc);
  const std::string twice = driftspec::render::Svg().render(doc);
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
  driftspec::model::Model model;

  void SetUp() override {
    driftspec::parse::Parser parser;
    driftspec::bind::Binder binder(model);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/behaviour")) binder.bind(f);
    driftspec::check::Checker(model).run(binder);
  }

  std::string svg(const std::string& diagram) {
    return driftspec::render::Svg().render(IrDocument::parse(driftspec::render::IrExport(model).render("diagram:" + diagram, false)));
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
