#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "ds/api/Api.hpp"
#include "ds/model/Metamodel.hpp"

namespace {

/// A file that does not parse is said once, where it breaks, and what only echoes the cut is not
/// reported until it parses (REQ-00167-1, DS-304).
class TestCutFile : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  std::vector<nlohmann::json> findings(const ds::api::Files& files) const {
    const auto report = nlohmann::json::parse(ds::api::Api(metamodel).check(files));
    return {report["findings"].begin(), report["findings"].end()};
  }
};

TEST_F(TestCutFile, ALostBraceIsReportedAtTheBlockItLeavesOpen) {
  const std::string text =
      "package p {\n  module m {\n    class A {\n      + run() -> int\n\n    class B {\n      + stop() -> int\n    }\n  }\n}\n";
  const auto all = findings({{"a.ds", text}});
  ASSERT_EQ(all.size(), 1u);
  EXPECT_EQ(all[0]["code"], "E-SYNTAX");
  EXPECT_EQ(all[0]["location"]["line"], 3);  // where class A opens
  EXPECT_NE(all[0]["message"].get<std::string>().find("`class A {` is not closed; line 6"), std::string::npos) << all[0]["message"];
}

TEST_F(TestCutFile, WhatOnlyEchoesTheCutGoesAndTheOtherFilesKeepTheirFindings) {
  // a.ds loses the rest of its module to the lost brace; b.ds names what was cut, and breaks a rule of its own
  const std::string cut = "package p {\n  module m {\n    class A {\n      + run() -> int\n\n    class B { }\n  }\n}\n";
  const std::string other = "package q {\n  module n {\n    class C {\n      - refs b: p@m::B\n      bogus: field\n    }\n  }\n}\n";
  bool syntax = false, own = false, echo = false;
  for (const auto& f : findings({{"a.ds", cut}, {"b.ds", other}})) {
    syntax |= f["code"] == "E-SYNTAX" && f["location"]["file"] == "a.ds";
    own |= f["code"] == "W-FIELD-UNKNOWN" && f["location"]["file"] == "b.ds";
    echo |= f["code"] == "W-REF-UNDECLARED" || (f["location"]["file"] == "a.ds" && f["code"] != "E-SYNTAX");
  }
  EXPECT_TRUE(syntax);
  EXPECT_TRUE(own);  // a finding of an intact file stays
  EXPECT_FALSE(echo);
}

}  // namespace
