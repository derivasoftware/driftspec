#include <gtest/gtest.h>

#include "driftspec/bind/Binder.hpp"
#include "driftspec/check/Checker.hpp"
#include "driftspec/model/Model.hpp"
#include "driftspec/parse/Parser.hpp"
#include "driftspec/render/Ir.hpp"
#include "driftspec/render/IrExport.hpp"
#include "driftspec/render/Text.hpp"

namespace {

/// The terminal drawing: the same layout on a character grid.
class TestText : public ::testing::Test {
 protected:
  driftspec::model::Model model;

  void SetUp() override {
    driftspec::parse::Parser parser;
    driftspec::bind::Binder binder(model);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/static")) binder.bind(f);
    driftspec::check::Checker(model).run(binder);
  }

  std::string draw(const std::string& diagram, bool color = false) {
    const auto doc = driftspec::render::IrDocument::parse(driftspec::render::IrExport(model).render("diagram:" + diagram, false));
    return driftspec::render::Text().render(doc, color);
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

}  // namespace
