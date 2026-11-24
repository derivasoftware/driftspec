#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/format/SceneFormats.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/scene/Scene.hpp"

namespace {

/// The rules in the viewer (REQ-00098-1): each placed in the node it is written in, with what breaks
/// it placed on the map, so the logic perspective draws them without reading a finding.
class TestSceneRules : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::scene::SceneDocument doc;

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    const auto file = parser.parseText("a.ds",
                                       "package p {\n"
                                       "  requirement REQ-00001-1: The core shall not depend on render\n"
                                       "  module render {\n"
                                       "    class Canvas { }\n"
                                       "  }\n"
                                       "  module core {\n"
                                       "    class Model {\n"
                                       "      + canvas: p@render::Canvas\n"
                                       "    }\n"
                                       "    class Link {\n"
                                       "      + send(msg: int) -> bool {\n"
                                       "        rule in-range {\n"
                                       "          @requirement REQ-00001-1\n"
                                       "          for message k\n"
                                       "          where k calls p@core::Link::send, arg(k, msg, v)\n"
                                       "          holds v < 100\n"
                                       "        }\n"
                                       "      }\n"
                                       "    }\n"
                                       "    class Client {\n"
                                       "      - refs link: Link\n"
                                       "      + run() -> none\n"
                                       "    }\n"
                                       "    sequence Client::run {\n"
                                       "      -> link.send(150)\n"
                                       "    }\n"
                                       "    definition owns-in(a, b) {\n"
                                       "      if m in a, m owns b\n"
                                       "    }\n"
                                       "    rule blind {\n"
                                       "      title: the core owns nothing of render\n"
                                       "      @requirement REQ-00001-1\n"
                                       "      for class c in p@core\n"
                                       "      never owns-in(c, x), x in p@render\n"
                                       "    }\n"
                                       "  }\n"
                                       "}\n");
    ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
    binder.bind(file);
    const auto checkSteps = ds::check::Steps::of();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
    doc = ds::scene::Scene(model, metamodel).build();
  }

  const ds::scene::RuleView& rule(const std::string& id) const {
    for (const auto& r : doc.rules)
      if (r.id == id) return r;
    throw std::runtime_error("no rule " + id);
  }
};

TEST_F(TestSceneRules, ARuleIsPlacedWhereItIsWrittenWithWhatItSaysAndAnswersTo) {
  const auto& r = rule("p@core::blind");
  EXPECT_EQ(r.kind, "rule");
  EXPECT_EQ(r.name, "blind");
  EXPECT_EQ(r.title, "the core owns nothing of render");
  EXPECT_EQ(r.scope, "p@core");
  ASSERT_EQ(r.lines.size(), 2u);
  EXPECT_EQ(r.lines[0], (std::pair<std::string, std::string>{"for", "class c in p@core"}));
  EXPECT_EQ(r.lines[1].first, "never");
  EXPECT_EQ(r.requirements, (std::vector<std::string>{"REQ-00001-1"}));
  EXPECT_EQ(rule("p@core::owns-in").kind, "definition");
  EXPECT_EQ(rule("p@core::owns-in").lines[0].first, "if");
}

TEST_F(TestSceneRules, WhatBreaksARuleIsPlacedOnTheMapWithTheAnswerThatBreaksIt) {
  const auto& r = rule("p@core::blind");
  ASSERT_EQ(r.breaches.size(), 1u);
  EXPECT_EQ(r.breaches[0].node, "p@core::Model");
  EXPECT_EQ(r.breaches[0].to, "p@render::Canvas");  // the answer, drawn as a line from the breach
  EXPECT_EQ(r.breaches[0].line, 7);
  // a call that breaks a member's rule lands on the member whose behaviour writes it
  const auto& call = rule("p@core::Link::send::in-range");
  ASSERT_EQ(call.breaches.size(), 1u);
  EXPECT_EQ(call.breaches[0].node, "p@core::Client::run");
  EXPECT_EQ(call.breaches[0].to, "");
  EXPECT_TRUE(rule("p@core::owns-in").breaches.empty());
}

TEST_F(TestSceneRules, TheDocumentCarriesTheRulesAndThePageItsThreePerspectives) {
  const std::string json = ds::format::SceneJson().write(ds::scene::Scene(model, metamodel).build(), {});
  EXPECT_NE(json.find("\"rule\":[\"id\",\"kind\",\"name\",\"title\",\"scope\",\"lines\",\"requirements\",\"breaches\",\"file\",\"line\"]"),
            std::string::npos);
  EXPECT_NE(json.find("\"breach\":[\"node\",\"to\",\"bindings\",\"witness\",\"file\",\"line\"]"), std::string::npos);
  const std::string page = ds::format::SceneHtml().write(ds::scene::Scene(model, metamodel).build(), {false, "rules"});
  for (const char* p : {"data-p=\"structure\"", "data-p=\"behaviour\"", "data-p=\"logic\""})
    EXPECT_NE(page.find(p), std::string::npos) << p;
  const auto at = page.find("const VIEWER='") + std::string("const VIEWER='").size();
  const std::string viewer = ds::format::common::unpack(page.substr(at, page.find('\'', at) - at));
  for (const char* part : {"function setPersp(", "function route(", "function spreadLanes(", "function drawLogic(", "lightRule(id){"})
    EXPECT_NE(viewer.find(part), std::string::npos) << part;  // the strokes of each perspective, and a host lights one rule
}

}  // namespace
