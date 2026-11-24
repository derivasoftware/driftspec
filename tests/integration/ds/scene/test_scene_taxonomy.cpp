#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/emit/JsonSink.hpp"
#include "ds/format/SceneFormats.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/scene/Scene.hpp"

namespace {

/// The taxonomy shapes the scene (REQ-00076-1) and the viewer lights relations up instead of
/// drawing them (REQ-00072-2), over examples/hola: Formal and Casual realise the interface
/// Style inside core; Console (app) and Response (server) inherit the abstract Channel.
class TestSceneOfTheTaxonomy : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::scene::SceneDocument doc;
  std::map<std::string, const ds::scene::Node*> byId;
  std::map<std::string, size_t> order;

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/hola")) binder.bind(f);
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
    doc = ds::scene::Scene(model, metamodel).build();
    for (size_t i = 0; i < doc.nodes.size(); ++i) {
      byId[doc.nodes[i].id] = &doc.nodes[i];
      order[doc.nodes[i].id] = i;
    }
  }

  const ds::scene::Node& at(const std::string& id) const {
    const auto it = byId.find(id);
    if (it == byId.end()) throw std::runtime_error("no node " + id);
    return *it->second;
  }

  std::string kindOf(const std::string& from, const std::string& to) const {
    for (const auto& l : doc.links)
      if (l.from == from && l.to == to && (l.kind == "inherits" || l.kind == "realises")) return l.kind;
    return "";
  }

  // a base above the row of its heirs, centred over it, in one variant
  static void baseOverItsHeirs(const ds::layout::Box& base, const std::vector<ds::layout::Box>& heirs) {
    double left = heirs.front().x, right = heirs.front().x + heirs.front().w;
    for (const auto& h : heirs) {
      EXPECT_GT(h.y, base.y + base.h);         // below the base, with room for the trunk
      EXPECT_DOUBLE_EQ(h.y, heirs.front().y);  // in one row
      left = std::min(left, h.x);
      right = std::max(right, h.x + h.w);
    }
    EXPECT_NEAR(base.x + base.w / 2, (left + right) / 2, 0.5);  // centred over the row
  }
};

TEST_F(TestSceneOfTheTaxonomy, HeirsSitInARowBelowTheirBaseCentredOverThem) {
  const auto& style = at("hola@core::Style");
  const auto& formal = at("hola@core::Formal");
  const auto& casual = at("hola@core::Casual");
  baseOverItsHeirs(style.full, {formal.full, casual.full});
  ASSERT_TRUE(style.lean && formal.lean && casual.lean);
  baseOverItsHeirs(style.leanBox, {formal.leanBox, casual.leanBox});
  EXPECT_EQ(formal.parent, "hola@core");  // the tree only groups: the heirs stay children of their module
}

TEST_F(TestSceneOfTheTaxonomy, AbstractIsTheStereotypeTheCheckerDerives) {
  EXPECT_EQ(at("hola@core::Style").stereotype, "abstract");  // every method abstract (DS-301)
  EXPECT_EQ(at("hola@core::Channel").stereotype, "abstract");
  EXPECT_EQ(at("hola@core::Greeter").stereotype, "class");
  EXPECT_EQ(at("hola@core::Locale").stereotype, "dataclass");
}

TEST_F(TestSceneOfTheTaxonomy, AClassThatInheritsIsDrawnInheritingWhateverItsBase) {
  // a base whose methods are all abstract is an abstract class: inheriting from it is inheritance (DS-301)
  EXPECT_EQ(kindOf("hola@core::Formal", "hola@core::Style"), "inherits");
  EXPECT_EQ(kindOf("hola@core::Casual", "hola@core::Style"), "inherits");
  EXPECT_EQ(kindOf("hola@app::Console", "hola@core::Channel"), "inherits");
  EXPECT_EQ(kindOf("hola@server::Response", "hola@core::Channel"), "inherits");
}

TEST_F(TestSceneOfTheTaxonomy, TheModuleThatHoldsTheBasesComesBeforeTheModulesThatInherit) {
  EXPECT_LT(order.at("hola@core"), order.at("hola@app"));
  EXPECT_LT(order.at("hola@core"), order.at("hola@server"));
  const auto& core = at("hola@core").full;
  for (const auto* heir : {&at("hola@app").full, &at("hola@server").full})
    EXPECT_TRUE(heir->y > core.y || (heir->y == core.y && heir->x > core.x));  // read after it
}

TEST_F(TestSceneOfTheTaxonomy, AHeaderThatNamesABaseInAnotherModuleIsWideEnoughForIt) {
  // Console ◁ Channel: the box counts the base's name as part of its title
  const auto& console = at("hola@app::Console").full;
  const auto& plain = at("hola@app::main").full;
  EXPECT_GT(console.w, plain.w);
}

TEST_F(TestSceneOfTheTaxonomy, TheViewerLightsRelationsUpAndDocksItsPanels) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const std::string page = ds::format::SceneHtml().write(ds::scene::Scene(model, metamodel).build(), {false, "hola"}, sink);
  // only inheritance is a family of lines; the rest of the relations light up in their colour
  const auto at = page.find("const VIEWER='") + std::string("const VIEWER='").size();
  const std::string code = ds::format::common::unpack(page.substr(at, page.find('\'', at) - at));  // the viewer travels packed
  EXPECT_NE(code.find("const FAMILY={inherits:'inheritance',realises:'inheritance'}"), std::string::npos);
  for (const char* colour : {"--r-owns", "--r-agg", "--r-uses", "--r-calls"}) EXPECT_NE(page.find(colour), std::string::npos) << colour;
  // the card docks on the right, the tour plays along the bottom, and on a phone the tour hides the card
  const auto from = page.find("const STYLE='") + std::string("const STYLE='").size();
  const std::string style = ds::format::common::unpack(page.substr(from, page.find('\'', from) - from));  // the styles travel packed
  EXPECT_NE(style.find("body.carded .stage{right:380px}"), std::string::npos);
  EXPECT_NE(style.find(".tour{position:fixed;left:0;right:0;bottom:0"), std::string::npos);
  EXPECT_NE(style.find("body.carded.touring .card{display:none}"), std::string::npos);
  EXPECT_NE(page.find("<html lang=\"en\">"), std::string::npos);  // the viewer speaks English
}

}  // namespace
