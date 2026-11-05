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

/// A loop holding a named opt: m0 in the loop, the opt / found holding m1 and a return (m2); then m3.
Model loopBehaviour() {
  Model model;
  const std::string seq = "p@m::C::find::sequence";
  Entity loop;
  loop.kind = Kind::Frame;
  loop.id = seq + "::f0";
  loop.scope = seq;
  loop.fields["kind"] = "loop";
  loop.fields["first"] = seq + "::m0";
  model.declare(loop);
  Entity opt;
  opt.kind = Kind::Frame;
  opt.id = seq + "::f1";
  opt.scope = seq;
  opt.fields["kind"] = "opt";
  opt.fields["scenario"] = "found";
  opt.fields["parent"] = loop.id;
  opt.fields["first"] = seq + "::m1";
  model.declare(opt);
  auto message = [&](int order, const std::string& op, const std::string& frame, bool isReturn) {
    Entity m;
    m.kind = Kind::Message;
    m.id = seq + "::m" + std::to_string(order);
    m.scope = seq;
    m.fields["order"] = std::to_string(order);
    m.fields["op"] = op;
    m.fields["frame"] = frame;
    if (isReturn) {
      m.fields["return"] = "true";
      m.fields["result"] = op;
    }
    model.declare(m);
  };
  message(0, "next", loop.id, false);
  message(1, "take", opt.id, false);
  message(2, "1", opt.id, true);
  message(3, "after", "", false);
  model.find(seq + "::m3")->fields.erase("frame");
  return model;
}

TEST_F(TestPathOf, ALoopWithANamedBranchInsideStaysOnTheDefaultPath) {
  const Model model = loopBehaviour();
  std::set<std::string> kept;
  EXPECT_EQ(ops(ds::model::pathOf(model, "p@m::C::find::sequence", "", &kept)), (std::vector<std::string>{"next", "after"}));
  EXPECT_TRUE(kept.count("p@m::C::find::sequence::f0"));  // drawn as the loop it is
}

TEST_F(TestPathOf, APathThatNamesABranchInsideALoopGoesIntoItAndEndsThere) {
  const Model model = loopBehaviour();
  EXPECT_EQ(ops(ds::model::pathOf(model, "p@m::C::find::sequence", "found", nullptr)), (std::vector<std::string>{"next", "take", "1"}));
}

}  // namespace
