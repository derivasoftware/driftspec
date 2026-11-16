#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// Six small defects the model audit of 2026-11 found (DS-224), each held to the requirement that owns it.
class TestAuditDefects : public ::testing::Test {
 protected:
  ds::model::Model model;
  bool parsed = true;

  void load(const std::string& text) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    const auto file = parser.parseText("a.ds", text);
    parsed = !file.hasSyntaxErrors;
    binder.bind(file);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, checkSteps).run(binder);
  }

  std::vector<std::string> said(const std::string& code) const {
    std::vector<std::string> out;
    for (const auto& f : model.findings())
      if (f.code == code) out.push_back(f.message);
    return out;
  }
};

TEST_F(TestAuditDefects, ARuleOfAnAttributeIsNamedUnderTheAttribute) {
  load(
      "package shop {\n"
      "  requirement REQ-00001-1: Amounts shall be positive\n"
      "  module store {\n"
      "    class Cart {\n"
      "      - cents: int {\n"
      "        rule positive {\n"
      "          @requirement REQ-00001-1\n"
      "          for class c in shop@store\n"
      "          holds c in shop@store\n"
      "        }\n"
      "      }\n"
      "      - items: int {\n"
      "        rule positive {\n"
      "          @requirement REQ-00001-1\n"
      "          for class c in shop@store\n"
      "          holds c in shop@store\n"
      "        }\n"
      "      }\n"
      "    }\n"
      "  }\n"
      "}\n");
  EXPECT_TRUE(said("E-DUPLICATE-IDENTITY").empty());
  EXPECT_NE(model.find("shop@store::Cart::cents::positive"), nullptr);
  EXPECT_NE(model.find("shop@store::Cart::items::positive"), nullptr);
}

TEST_F(TestAuditDefects, ABareNameFindsTheClassBesideItBeforeARootPackageOfItsName) {
  load(
      "package Cart {\n"
      "  module store {\n"
      "    class Cart {\n"
      "      + total() -> int\n"
      "    }\n"
      "    class Shop {\n"
      "      - cart: Cart\n"
      "      + open() -> none\n"
      "    }\n"
      "    sequence Shop::open {\n"
      "      self -> cart.total() -> t\n"
      "    }\n"
      "  }\n"
      "}\n");
  EXPECT_TRUE(model.findings().empty()) << model.findings().front().message;
}

TEST_F(TestAuditDefects, ASlashFollowedByTextStaysInTheProseOfACondition) {
  load(
      "package shop {\n"
      "  module store {\n"
      "    class Cart {\n"
      "      + add(item: string) -> bool\n"
      "    }\n"
      "    sequence Cart::add {\n"
      "      opt the path is /tmp or the ratio a/b is low / odd {\n"
      "        return false\n"
      "      }\n"
      "      return true\n"
      "    }\n"
      "  }\n"
      "}\n");
  EXPECT_TRUE(parsed);
  bool named = false;
  for (const auto& pair : model.entities())
    named = named || (pair.second.kind == ds::model::Kind::Frame && pair.second.field("scenario") == "odd" &&
                      pair.second.field("condition") == "the path is /tmp or the ratio a/b is low");
  EXPECT_TRUE(named);
}

TEST_F(TestAuditDefects, AnAttributeThatOwnsAnInterfaceIsToldToWriteRefs) {
  load(
      "package shop {\n"
      "  interface orders {\n"
      "    kind: library\n"
      "    operation find(id: string) -> string\n"
      "  }\n"
      "  module web {\n"
      "    class Page {\n"
      "      - api: shop@orders\n"
      "    }\n"
      "  }\n"
      "}\n");
  const auto kind = said("E-RELATION-KIND");
  ASSERT_EQ(kind.size(), 1u);
  EXPECT_NE(kind[0].find("write - refs api"), std::string::npos);
}

TEST_F(TestAuditDefects, AProtectedMemberIsHiddenFromAClassThatDoesNotDeriveFromIt) {
  load(
      "package shop {\n"
      "  module store {\n"
      "    class Vault {\n"
      "      # secret() -> int\n"
      "    }\n"
      "    class Thief {\n"
      "      - vault: Vault\n"
      "      + steal() -> none\n"
      "    }\n"
      "    sequence Thief::steal {\n"
      "      self -> vault.secret() -> a\n"
      "    }\n"
      "  }\n"
      "}\n");
  const auto hidden = said("W-NOT-VISIBLE");
  ASSERT_EQ(hidden.size(), 1u);
  EXPECT_NE(hidden[0].find("which is protected"), std::string::npos);
}

TEST_F(TestAuditDefects, AHandlerOfAFailureNoStepRaisesHandlesNothing) {
  load(
      "package shop {\n"
      "  module store {\n"
      "    class Disk {\n"
      "      + read(path: string) -> string fails IoError\n"
      "    }\n"
      "    class Book {\n"
      "      - disk: Disk\n"
      "      + load(path: string) -> string fails nothing\n"
      "    }\n"
      "    sequence Book::load {\n"
      "      attempt {\n"
      "        self -> disk.read(path) -> text\n"
      "      } on IoError {\n"
      "        return \"empty\"\n"
      "      } on IoErorr {\n"
      "        return \"typo\"\n"
      "      }\n"
      "      return text\n"
      "    }\n"
      "  }\n"
      "}\n");
  const auto dead = said("W-HANDLER-UNREACHED");
  ASSERT_EQ(dead.size(), 1u);
  EXPECT_NE(dead[0].find("IoErorr"), std::string::npos);
  EXPECT_TRUE(said("W-FAILURE-UNDECLARED").empty());  // IoError is handled
}

}  // namespace
