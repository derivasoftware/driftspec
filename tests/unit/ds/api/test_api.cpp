#include <gtest/gtest.h>

#include <algorithm>
#include <nlohmann/json.hpp>
#include <string>

#include "ds/api/Api.hpp"
#include "ds/model/Metamodel.hpp"

namespace {

/// The engine in process (REQ-00085-1): a workspace as text in, the JSON of the command line out.
class TestApi : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  const ds::api::Files files{{"a.ds", "package p {\n  module m {\n    class C {\n      + go() -> none\n    }\n  }\n}\n"}};
};

TEST_F(TestApi, ACleanWorkspaceChecksWithNoFindingAndItsCounts) {
  const auto out = nlohmann::json::parse(ds::api::Api(metamodel).check(files));
  EXPECT_TRUE(out["findings"].empty());
  EXPECT_EQ(out["errors"], 0);
  EXPECT_GT(out["entities"].get<int>(), 3);
}

TEST_F(TestApi, TheModelExportsItsEntitiesByIdentity) {
  const auto out = nlohmann::json::parse(ds::api::Api(metamodel).exportModel(files));
  bool found = false;
  for (const auto& e : out["entities"]) found |= e["id"] == "p@m::C";
  EXPECT_TRUE(found);
}

TEST_F(TestApi, WhatTheGrammarTakesInAModuleIsWhatAModuleHolds) {
  const auto words = nlohmann::json::parse(ds::api::Api::expectedAt("package p {\n  module m {\n    \n  }\n}\n", 2, 4));
  EXPECT_NE(std::find(words.begin(), words.end(), "class"), words.end());
  EXPECT_NE(std::find(words.begin(), words.end(), "function"), words.end());
}

}  // namespace
