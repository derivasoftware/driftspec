#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "ds/api/Workspace.hpp"

namespace {

/// What escapes an operation, asked of a loaded workspace (REQ-00104-1): the texts go in, and the
/// answer is read on the operation as any reader of the model reads it.
class TestFailureEscapes : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  static std::string field(const ds::api::Workspace& ws, const std::string& id, const std::string& name) {
    const auto entity = nlohmann::json::parse(ws.find(id));
    if (!entity.is_object() || !entity["fields"].contains(name)) return "";
    return entity["fields"][name].get<std::string>();
  }

  static std::string design(const std::string& behaviours) {
    return "package p {\n  module m {\n    class A {\n      - b: B\n      + run() -> int\n    }\n"
           "    class B {\n      - a: A\n      + walk() -> int\n    }\n" +
           behaviours + "  }\n}\n";
  }
};

TEST_F(TestFailureEscapes, AFailureInsideAnAttemptThatHandlesItEscapesNothing) {
  const ds::api::Workspace ws = ds::api::Workspace::madeFor(
      metamodel,
      {{"a.ds", design("    sequence A::run {\n      attempt {\n        fail Busy\n      } on Busy {\n        return 0\n      }\n"
                       "      return 1\n    }\n")}});
  EXPECT_EQ(field(ws, "p@m::A::run", "escapes"), "");
}

TEST_F(TestFailureEscapes, AHandlersOwnFailureIsNotHandledByTheAttemptThatHoldsIt) {
  const ds::api::Workspace ws = ds::api::Workspace::madeFor(
      metamodel,
      {{"a.ds", design("    sequence A::run {\n      attempt {\n        fail Busy\n      } on Busy {\n        fail Busy\n      }\n"
                       "      return 1\n    }\n")}});
  EXPECT_EQ(field(ws, "p@m::A::run", "escapes"), "Busy");  // raised again from its handler: it goes on
}

TEST_F(TestFailureEscapes, CallsThatCallEachOtherEndWithWhatEitherLetsEscape) {
  const ds::api::Workspace ws = ds::api::Workspace::madeFor(
      metamodel, {{"a.ds", design("    sequence A::run {\n      self -> b.walk() -> n\n      opt it is lost / lost {\n        "
                                  "fail Lost\n      }\n      return n\n    }\n"
                                  "    sequence B::walk {\n      self -> a.run() -> n\n      return n\n    }\n")}});
  EXPECT_EQ(field(ws, "p@m::A::run", "escapes"), "Lost");
  EXPECT_EQ(field(ws, "p@m::B::walk", "escapes"), "Lost");
  EXPECT_EQ(field(ws, "p@m::B::walk", "escape.Lost"), "p@m::B::walk -> p@m::A::run from p@m::A::run::sequence/lost");
}

}  // namespace
