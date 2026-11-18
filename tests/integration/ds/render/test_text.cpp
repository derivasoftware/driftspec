#include <gtest/gtest.h>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/Ir.hpp"
#include "ds/render/IrExport.hpp"
#include "ds/render/Text.hpp"

namespace {

/// The terminal drawing: the same layout on a character grid.
class TestText : public ::testing::Test {
 protected:
  ds::model::Model model;

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/static")) binder.bind(f);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, checkSteps).run(binder);
  }

  std::string draw(const std::string& diagram, bool color = false) {
    const auto doc = ds::render::IrDocument::parse(ds::render::IrExport(model).render("diagram:" + diagram, false));
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
  ds::model::Model nested;  // a workspace of its own, not the static corpus
  ds::parse::Parser parser;
  ds::bind::Binder binder(nested);
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
  const auto checkSteps = ds::check::steps();
  ds::check::Checker(nested, checkSteps).run(binder);
  const auto doc = ds::render::IrDocument::parse(ds::render::IrExport(nested).render("diagram:SEQ_Nested", false));
  const std::string out = ds::render::Text().render(doc, false);
  const auto outer = out.find("loop N"), inner = out.find("loop M");
  ASSERT_NE(outer, std::string::npos);
  ASSERT_NE(inner, std::string::npos);
  EXPECT_LT(out.rfind('\n', outer), out.rfind('\n', inner));  // each header on a row of its own, the outer first
}

}  // namespace
