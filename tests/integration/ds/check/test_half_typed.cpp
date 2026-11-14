#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "ds/api/Api.hpp"

namespace {

/// A file being typed is checked to the end (REQ-00096-1, DS-127): a syntax error leaves the model
/// half bound, and the rules stated on it are still decided instead of walked for ever.
class TestHalfTypedFile : public ::testing::Test {
 protected:
  static std::string rules() {
    return "    rule inside {\n      for class c in p@m\n      never x in c, c in y\n    }\n"
           "    rule reached {\n      for class c in p@m\n      holds c in p@m\n    }\n";
  }
};

TEST_F(TestHalfTypedFile, TheRulesOfAFileThatStopsParsingAreDecidedAndTheCheckEnds) {
  const std::string text = "package p {\n  module m {\n    class A {\n      + run( -> none\n    }\n    class B : A {\n" + rules() + "  }\n";
  const auto report = nlohmann::json::parse(ds::api::Api::check({{"typing.ds", text}}));
  bool syntax = false;
  for (const auto& f : report["findings"]) syntax |= f["code"] == "E-SYNTAX";
  EXPECT_TRUE(syntax);
  EXPECT_GE(report["errors"].get<int>(), 1);
}

TEST_F(TestHalfTypedFile, TheSameRulesAreDecidedOnceTheFileParses) {
  const std::string text =
      "package p {\n  module m {\n    class A {\n      + run() -> none\n    }\n    class B : A { }\n" + rules() + "  }\n}\n";
  const auto report = nlohmann::json::parse(ds::api::Api::check({{"typed.ds", text}}));
  for (const auto& f : report["findings"]) EXPECT_NE(f["code"], "E-SYNTAX") << f["message"];
}

}  // namespace
