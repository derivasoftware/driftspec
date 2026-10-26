#include <gtest/gtest.h>

#include "ds/bind/Binder.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

class TestBinder : public ::testing::Test {
 protected:
  ds::model::Model model;
  ds::parse::Parser parser;

  void bindText(const std::string& text) {
    ds::bind::Binder binder(model);
    binder.bind(parser.parseText("t.ds", text));
  }
};

TEST_F(TestBinder, PackagesQualifyWithAtAndModulesWithColons) {
  bindText("package ds { module lsp { class LspClient { + send(msg: int) -> bool } } }");
  EXPECT_NE(model.find("ds@lsp::LspClient"), nullptr);
  EXPECT_NE(model.find("ds@lsp::LspClient::send"), nullptr);
  EXPECT_EQ(model.find("ds@ds::lsp::LspClient"), nullptr);
}

TEST_F(TestBinder, ModulesCanBeReopenedAcrossFilesButClassesCannot) {
  ds::bind::Binder binder(model);
  binder.bind(parser.parseText("a.ds", "package ds { module m { class A { } } }"));
  binder.bind(parser.parseText("b.ds", "package ds { module m { class B { } } }"));
  binder.bind(parser.parseText("c.ds", "package ds { module m { class A { } } }"));
  EXPECT_NE(model.find("ds@m::A"), nullptr);
  EXPECT_NE(model.find("ds@m::B"), nullptr);
  ASSERT_EQ(model.findings().size(), 1u);
  EXPECT_EQ(model.findings()[0].code, "E-DUPLICATE-IDENTITY");
}

TEST_F(TestBinder, NestingGivesTheRelation) {
  // a field value runs to the end of its line, so closing braces take their own line
  bindText("package ds {\n requirement R-1 {\n title: t\n verification V-1 {\n test: x\n }\n }\n}\n");
  bool verifies = false;
  for (const auto& r : model.relations()) verifies |= (r.kind == "verifies" && r.from == "V-1" && r.to == "R-1");
  EXPECT_TRUE(verifies);
  ASSERT_NE(model.find("R-1"), nullptr);
  EXPECT_EQ(model.find("R-1")->fields.at("title"), "t");
}

}  // namespace
