#include <gtest/gtest.h>

#include <cstddef>
#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/emit/JsonSink.hpp"
#include "ds/format/SceneFormats.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/scene/Scene.hpp"

namespace {

/// The scene writes each behaviour once (REQ-00074-2), so what a caller adds to the page is its own call, not the steps
/// it replays: over a design made for it, one behaviour of `steps` steps called from `callers` places. The scene is
/// measured as the page carries it before it is packed, since packing would hide a copy of the steps as well as it
/// hides their repetition.
class TestScenePageOfACalledBehaviour : public ::testing::Test {
 protected:
  static std::string design(int callers, int steps) {
    std::string text = "package p {\n  module m {\n    class Leaf {\n      + work() -> none\n      + step() -> none\n    }\n";
    for (int i = 1; i <= callers; ++i) {
      const std::string c = "C" + std::to_string(i);
      text += "    class " + c + " {\n      - leaf: Leaf\n      + run() -> none\n    }\n";
      text += "    sequence " + c + "::run {\n      self -> leaf.work()\n    }\n";
    }
    text += "    sequence Leaf::work {\n";
    for (int s = 0; s < steps; ++s) text += "      self -> step()\n";
    text += "    }\n  }\n}\n";
    return text;
  }
  /// The size of the scene of that design, as the page carries it.
  static std::size_t page(int callers, int steps) {
    const ds::model::Metamodel metamodel;
    ds::model::Model model{metamodel};
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    binder.bind(parser.parseText("p.ds", design(callers, steps)));
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
    ds::emit::JsonSink sink(false, true);
    return ds::format::SceneJson().write(ds::scene::Scene(model, metamodel).build(), {}, sink).size();
  }
};

TEST_F(TestScenePageOfACalledBehaviour, CallersAddTheirCallsToThePageNotTheStepsTheyReplay) {
  const std::size_t fewSteps = 2, manySteps = 40;
  const long callersAtFew = static_cast<long>(page(10, fewSteps)) - static_cast<long>(page(1, fewSteps));
  const long callersAtMany = static_cast<long>(page(10, manySteps)) - static_cast<long>(page(1, manySteps));
  const long stepsOnce = static_cast<long>(page(1, manySteps)) - static_cast<long>(page(1, fewSteps));
  ASSERT_GT(stepsOnce, 0);
  // nine more callers of a behaviour with 38 more steps cost less than those steps written once: had every caller's
  // tour carried them, it would be nine times as much (measured at DS-452: 0 bytes more, against 3,079 for the steps)
  EXPECT_LT(callersAtMany - callersAtFew, stepsOnce);
}

}  // namespace
