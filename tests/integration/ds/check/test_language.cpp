#include <gtest/gtest.h>

#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// What each package and module is written in (REQ-00084-1): written once, held by what it holds,
/// said again only where it changes; one ds does not know is a finding.
class TestLanguage : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void load(const std::string& text) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    const auto file = parser.parseText("l.ds", text);
    ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
    binder.bind(file);
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
  }

  std::string languageOf(const std::string& id) const {
    const ds::model::Entity* e = model.find(id);
    if (!e) return "?";
    const auto it = e->fields.find("language");
    return it == e->fields.end() ? "" : it->second;
  }
};

TEST_F(TestLanguage, WhatABoxHoldsIsWrittenInItsLanguageUnlessItSaysOtherwise) {
  load(
      "package shop {\n  language: cpp\n  module store {\n    class Cart { }\n  }\n"
      "  module web {\n    language: python\n    module views { }\n  }\n}\n");
  EXPECT_EQ(languageOf("shop"), "cpp");
  EXPECT_EQ(languageOf("shop@store"), "cpp");          // held
  EXPECT_EQ(languageOf("shop@web"), "python");         // said otherwise
  EXPECT_EQ(languageOf("shop@web::views"), "python");  // held from where it changed
  EXPECT_TRUE(model.findings().empty()) << model.findings().front().message;
}

TEST_F(TestLanguage, ABoxThatSaysNothingAndHoldsNothingThatSaysHasNoLanguage) {
  load("package shop {\n  module store { }\n}\n");
  EXPECT_EQ(languageOf("shop@store"), "");
  EXPECT_TRUE(model.findings().empty());
}

TEST_F(TestLanguage, ALanguageDsDoesNotKnowOrTwoAtOnceIsAFinding) {
  load(
      "package shop {\n  module old {\n    language: cobol\n  }\n  module twice {\n    language: cpp\n  }\n"
      "  module twice {\n    language: python\n  }\n}\n");
  int unknown = 0;
  for (const auto& f : model.findings()) unknown += f.code == "W-LANGUAGE-UNKNOWN" ? 1 : 0;
  EXPECT_EQ(unknown, 2);
}

}  // namespace
