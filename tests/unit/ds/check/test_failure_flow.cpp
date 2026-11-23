#include <gtest/gtest.h>

#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

ds::model::Model load(const ds::model::Metamodel& metamodel, const std::string& text) {
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  const auto file = parser.parseText("a.ds", text);
  EXPECT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
  binder.bind(file);
  const auto steps = ds::check::steps();
  ds::check::Checker(model, metamodel, steps).run(binder);
  return model;
}

/// The step that computes what failures escape each operation (DS-188), on workspaces of its own.

TEST(TestFailureFlow, AFailureInsideAnAttemptThatHandlesItEscapesNothing) {
  const ds::model::Metamodel metamodel;
  const auto model = load(
      metamodel,
      "package p {\n  module m {\n    class A {\n      + run() -> int\n    }\n"
      "    sequence A::run {\n      attempt {\n        fail Busy\n      } on Busy {\n        return 0\n      }\n      return 1\n    }\n"
      "  }\n}\n");
  EXPECT_EQ(model.find("p@m::A::run")->field("escapes"), "");
}

TEST(TestFailureFlow, AHandlersOwnFailureIsNotHandledByTheAttemptThatHoldsIt) {
  const ds::model::Metamodel metamodel;
  const auto model = load(
      metamodel,
      "package p {\n  module m {\n    class A {\n      + run() -> int\n    }\n"
      "    sequence A::run {\n      attempt {\n        fail Busy\n      } on Busy {\n        fail Busy\n      }\n      return 1\n    }\n"
      "  }\n}\n");
  EXPECT_EQ(model.find("p@m::A::run")->field("escapes"), "Busy");  // raised again from its handler: it goes on
}

TEST(TestFailureFlow, CallsThatCallEachOtherEndWithWhatEitherLetsEscape) {
  const ds::model::Metamodel metamodel;
  const auto model = load(
      metamodel,
      "package p {\n  module m {\n    class A {\n      - b: B\n      + run() -> int\n    }\n"
      "    class B {\n      - a: A\n      + walk() -> int\n    }\n"
      "    sequence A::run {\n      self -> b.walk() -> n\n      opt it is lost / lost {\n        fail Lost\n      }\n      return n\n    "
      "}\n"
      "    sequence B::walk {\n      self -> a.run() -> n\n      return n\n    }\n"
      "  }\n}\n");
  EXPECT_EQ(model.find("p@m::A::run")->field("escapes"), "Lost");
  EXPECT_EQ(model.find("p@m::B::walk")->field("escapes"), "Lost");
  EXPECT_EQ(model.find("p@m::B::walk")->field("escape.Lost"), "p@m::B::walk -> p@m::A::run from p@m::A::run::sequence/lost");
}

}  // namespace
