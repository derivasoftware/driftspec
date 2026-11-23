#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "ds/api/Workspace.hpp"

namespace {

/// The workspace alone, over the smallest design there is. What it answers of a whole design is
/// held by tests/integration/ds/api/test_workspace.cpp.
class TestWorkspace : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::api::Workspace ws{metamodel, {{"one.ds", "package p {\n  module m {\n    class A { }\n  }\n}\n"}}};
};

TEST_F(TestWorkspace, ItFindsWhatItsTextDeclaresAndNothingElse) {
  EXPECT_EQ(nlohmann::json::parse(ws.find("p@m::A"))["kind"], "class");
  EXPECT_EQ(ws.find("p@m::B"), "null");
  EXPECT_EQ(nlohmann::json::parse(ws.parent("p@m::A")), "p@m");
}

TEST_F(TestWorkspace, WithNoTextItAnswersNothing) {
  const ds::api::Workspace empty{metamodel, {}};
  EXPECT_EQ(empty.find("p"), "null");
  EXPECT_TRUE(nlohmann::json::parse(empty.members("p@")).empty());
}

TEST_F(TestWorkspace, AnIdentityIsQualifiedAndTakenApart) {
  EXPECT_EQ(ds::api::Identity::qualify("p@m", "C"), "p@m::C");
  EXPECT_EQ(ds::api::Identity::outer("p@m::C"), "p@m");
  EXPECT_EQ(ds::api::Identity::withoutRevision("REQ-00001-2"), "REQ-00001");
}

}  // namespace
