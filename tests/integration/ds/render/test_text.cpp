#include <gtest/gtest.h>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/Ir.hpp"
#include "ds/render/IrExport.hpp"
#include "ds/render/Text.hpp"

namespace {

/// The terminal drawing: the same layout on a character grid.
class TestText : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/static")) binder.bind(f);
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
  }

  std::string draw(const std::string& diagram, bool color = false) {
    const auto doc = ds::render::IrDocument::parse(ds::render::IrExport(model, metamodel).render("diagram:" + diagram, false));
    return ds::render::Text().render(doc, color);
  }
};

TEST_F(TestText, DrawsEveryBoxWithItsNameAndMembers) {
  const std::string out = draw("CL_Values");
  EXPECT_NE(out.find("D Finding"), std::string::npos);
  EXPECT_NE(out.find("E Severity"), std::string::npos);
  EXPECT_NE(out.find("+ column: int = 0"), std::string::npos);
  EXPECT_NE(out.find("\xE2\x95\xAD"), std::string::npos);  // ╭ a rounded box
  EXPECT_NE(out.find("\xE2\x97\x86"), std::string::npos);  // ◆ the composition
  EXPECT_NE(out.find(" ds "), std::string::npos);          // the container label
}

/// A diff's change on a relation or a message is drawn as on a box (DS-419): + added, - removed, > renamed, ~ changed,
/// each in its own colour; a changed relation is not drawn as removed.
TEST_F(TestText, MarksAChangeOnARelationOrAMessageAsOnABox) {
  // one relation per drawing, so no other route crosses its label
  const std::pair<const char*, const char*> changes[] = {
      {"added", "+ uses"}, {"removed", "- uses"}, {"renamed", "> uses"}, {"changed", "~ uses"}};
  for (const auto& [change, marked] : changes) {
    const auto doc = ds::render::IrDocument::parse(
        std::string(
            R"J({"ir": 1, "title": "diff", "nodes": [{"id": "A", "kind": "box", "label": "A"}, {"id": "B", "kind": "box", "label": "B"}],
    "edges": [{"from": "A", "to": "B", "kind": "dependency", "label": "uses", "refs": {"change": ")J") +
        change + R"J("}}]})J");
    const std::string plain = ds::render::Text().render(doc, false);
    EXPECT_NE(plain.find(marked), std::string::npos) << change << "\n" << plain;
    EXPECT_EQ(plain.find("- uses") != std::string::npos, std::string(change) == "removed") << change;
  }
  // the line of the relation in its change's colour: added, removed, renamed, changed
  const std::string colours[] = {"\x1b[32m", "\x1b[31m", "\x1b[34m", "\x1b[33m"};
  for (std::size_t k = 0; k < 4; ++k) {
    const auto doc = ds::render::IrDocument::parse(
        std::string(
            R"J({"ir": 1, "title": "diff", "nodes": [{"id": "A", "kind": "box", "label": "A"}, {"id": "B", "kind": "box", "label": "B"}],
    "edges": [{"from": "A", "to": "B", "kind": "dependency", "label": "uses", "refs": {"change": ")J") +
        changes[k].first + R"J("}}]})J");
    EXPECT_NE(ds::render::Text().render(doc, true).find(colours[k]), std::string::npos) << changes[k].first;
  }

  const std::string messages = R"J({"ir": 1, "title": "diff", "nodes": [{"id": "a", "kind": "lifeline", "label": "a"},
    {"id": "b", "kind": "lifeline", "label": "b"}],
    "edges": [{"from": "a", "to": "b", "kind": "message", "label": "run()", "order": 0, "refs": {"change": "renamed"}},
              {"from": "b", "to": "a", "kind": "message", "label": "done()", "order": 1, "refs": {"change": "changed"}}]})J";
  const std::string sequence = ds::render::Text().render(ds::render::IrDocument::parse(messages), false);
  EXPECT_NE(sequence.find("> run()"), std::string::npos) << sequence;
  EXPECT_NE(sequence.find("~ done()"), std::string::npos) << sequence;
}

TEST_F(TestText, IsDeterministicAndPlainWithoutColor) {
  const std::string once = draw("CL_Model");
  EXPECT_EQ(once, draw("CL_Model"));
  EXPECT_EQ(once.find("\x1b["), std::string::npos);
  EXPECT_NE(draw("CL_Model", true).find("\x1b["), std::string::npos);
}

TEST_F(TestText, BoxesDoNotCollideOnTheGrid) {
  // every line of a box interior starts and ends with a border glyph on the same row
  const std::string out = draw("CL_Templates");
  EXPECT_EQ(out.find("\xE2\x96\xB3"), std::string::npos);  // △: templates have no inheritance
  EXPECT_NE(out.find("T: Entity"), std::string::npos);     // the bound travels as a label
}

TEST_F(TestText, FramesOpeningAtTheSameMessageStackTheirHeaders) {
  // DS-13: a loop whose first step is a loop drew both headers on one row (┌ l┌ loop N)
  ds::model::Model nested{metamodel};  // a workspace of its own, not the static corpus
  ds::parse::Parser parser;
  ds::bind::Binder binder(nested, metamodel);
  binder.bind(parser.parseText("l.ds",
                               "package app {\n"
                               "  module m {\n"
                               "    class A {\n"
                               "      + run()\n"
                               "      + step()\n"
                               "    }\n"
                               "    class B {\n"
                               "      + put(x: int)\n"
                               "    }\n"
                               "    sequence A::run / nested {\n"
                               "      local b: B\n"
                               "      loop N {\n"
                               "        loop M {\n"
                               "          self -> b.put(x)\n"
                               "        }\n"
                               "        -> step()\n"
                               "      }\n"
                               "    }\n"
                               "  }\n"
                               "}\n"
                               "diagram SEQ_Nested {\n"
                               "  kind: sequence\n"
                               "  include: app@m::A::run::sequence/nested\n"
                               "}\n"));
  ds::check::CheckerFactory::make(nested, metamodel)->run(binder.notes());
  const auto doc = ds::render::IrDocument::parse(ds::render::IrExport(nested, metamodel).render("diagram:SEQ_Nested", false));
  const std::string out = ds::render::Text().render(doc, false);
  const auto outer = out.find("loop N"), inner = out.find("loop M");
  ASSERT_NE(outer, std::string::npos);
  ASSERT_NE(inner, std::string::npos);
  EXPECT_LT(out.rfind('\n', outer), out.rfind('\n', inner));  // each header on a row of its own, the outer first
}

}  // namespace
