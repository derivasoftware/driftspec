#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

#include "ds/model/Model.hpp"

namespace {

using ds::model::Entity;
using ds::model::Kind;
using ds::model::Model;

/// One behaviour built by hand: m0, an `opt / stop` holding m1 and a return (m2), then m3 and a return (m4).
Model behaviour() {
  Model model;
  const std::string seq = "p@m::C::run::sequence";
  auto message = [&](int order, const std::string& op, const std::string& frame, bool isReturn) {
    Entity m;
    m.kind = Kind::Message;
    m.id = seq + "::m" + std::to_string(order);
    m.scope = seq;
    m.fields["order"] = std::to_string(order);
    m.fields["op"] = op;
    if (!frame.empty()) m.fields["frame"] = frame;
    if (isReturn) {
      m.fields["return"] = "true";
      m.fields["result"] = op;
    }
    model.declare(m);
  };
  Entity f;
  f.kind = Kind::Frame;
  f.id = seq + "::f0";
  f.scope = seq;
  f.fields["kind"] = "opt";
  f.fields["scenario"] = "stop";
  f.fields["first"] = seq + "::m1";
  f.fields["last"] = seq + "::m2";
  model.declare(f);
  message(0, "a", "", false);
  message(1, "b", f.id, false);
  message(2, "1", f.id, true);
  message(3, "c", "", false);
  message(4, "0", "", true);
  return model;
}

std::vector<std::string> ops(const std::vector<const Entity*>& path) {
  std::vector<std::string> out;
  for (const Entity* m : path) out.push_back(m->fields.at("op"));
  return out;
}

class TestHasScenario : public ::testing::Test {};

TEST_F(TestHasScenario, AnswersForANamedBranchOnly) {
  const Model model = behaviour();
  EXPECT_TRUE(ds::model::hasScenario(model, "p@m::C::run::sequence", "stop"));
  EXPECT_FALSE(ds::model::hasScenario(model, "p@m::C::run::sequence", "other"));
  EXPECT_FALSE(ds::model::hasScenario(model, "p@m::C::other::sequence", "stop"));
}

class TestPathOf : public ::testing::Test {};

TEST_F(TestPathOf, TheNamedPathTakesItsBranchAndEndsAtItsReturn) {
  const Model model = behaviour();
  EXPECT_EQ(ops(ds::model::pathOf(model, "p@m::C::run::sequence", "stop", nullptr)), (std::vector<std::string>{"a", "b", "1"}));
}

TEST_F(TestPathOf, TheDefaultPathSkipsTheNamedBranch) {
  const Model model = behaviour();
  std::set<std::string> kept;
  EXPECT_EQ(ops(ds::model::pathOf(model, "p@m::C::run::sequence", "", &kept)), (std::vector<std::string>{"a", "c", "0"}));
  EXPECT_TRUE(kept.empty());
}

}  // namespace
