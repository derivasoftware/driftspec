#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace {

using ds::model::Entity;
using ds::model::Kind;
using ds::model::Model;

/// One behaviour built by hand, as the binder keeps it (DS-172): m0, an `opt / stop` holding m1 and a
/// return (m2), then m3 and a return (m4); each step contained by what encloses it, in the order written.
Model behaviour(const ds::model::Metamodel& metamodel) {
  Model model{metamodel};
  const std::string seq = "p@m::C::run::sequence";
  Entity s;
  s.kind = Kind::Sequence;
  s.id = seq;
  model.declare(s);
  auto message = [&](int order, const std::string& op, const std::string& holder, bool isReturn) {
    Entity m;
    m.kind = Kind::Message;
    m.id = seq + "::m" + std::to_string(order);
    m.scope = seq;
    m.fields["op"] = op;
    if (isReturn) {
      m.fields["return"] = "true";
      m.fields["result"] = op;
    }
    model.declare(m);
    model.relate({"contains", holder, m.id, {}, ""});
  };
  message(0, "a", seq, false);
  Entity f;
  f.kind = Kind::Frame;
  f.id = seq + "::f0";
  f.scope = seq;
  f.fields["kind"] = "opt";
  f.fields["scenario"] = "stop";
  model.declare(f);
  model.relate({"contains", seq, f.id, {}, ""});
  message(1, "b", f.id, false);
  message(2, "1", f.id, true);
  message(3, "c", seq, false);
  message(4, "0", seq, true);
  return model;
}

std::vector<std::string> ops(const std::vector<const Entity*>& path) {
  std::vector<std::string> out;
  for (const Entity* m : path) out.push_back(m->fields.at("op"));
  return out;
}

class TestHasScenario : public ::testing::Test {};

TEST_F(TestHasScenario, AnswersForANamedBranchOnly) {
  const ds::model::Metamodel metamodel;
  const Model model = behaviour(metamodel);
  EXPECT_TRUE(ds::model::hasScenario(model, "p@m::C::run::sequence", "stop"));
  EXPECT_FALSE(ds::model::hasScenario(model, "p@m::C::run::sequence", "other"));
  EXPECT_FALSE(ds::model::hasScenario(model, "p@m::C::other::sequence", "stop"));
}

class TestPathOf : public ::testing::Test {};

TEST_F(TestPathOf, TheNamedPathTakesItsBranchAndEndsAtItsReturn) {
  const ds::model::Metamodel metamodel;
  const Model model = behaviour(metamodel);
  EXPECT_EQ(ops(ds::model::pathOf(model, "p@m::C::run::sequence", "stop", nullptr)), (std::vector<std::string>{"a", "b", "1"}));
}

TEST_F(TestPathOf, TheDefaultPathSkipsTheNamedBranch) {
  const ds::model::Metamodel metamodel;
  const Model model = behaviour(metamodel);
  std::set<std::string> kept;
  EXPECT_EQ(ops(ds::model::pathOf(model, "p@m::C::run::sequence", "", &kept)), (std::vector<std::string>{"a", "c", "0"}));
  EXPECT_TRUE(kept.empty());
}

/// A loop holding a named opt: m0 in the loop, the opt / found holding m1 and a return (m2); then m3.
/// Each step contained by what encloses it, in the order written (DS-172).
Model loopBehaviour(const ds::model::Metamodel& metamodel) {
  Model model{metamodel};
  const std::string seq = "p@m::C::find::sequence";
  Entity s;
  s.kind = Kind::Sequence;
  s.id = seq;
  model.declare(s);
  auto frame = [&](const std::string& id, const std::string& kind, const std::string& scenario, const std::string& holder) {
    Entity f;
    f.kind = Kind::Frame;
    f.id = id;
    f.scope = seq;
    f.fields["kind"] = kind;
    if (!scenario.empty()) f.fields["scenario"] = scenario;
    model.declare(f);
    model.relate({"contains", holder, id, {}, ""});
  };
  auto message = [&](int order, const std::string& op, const std::string& holder, bool isReturn) {
    Entity m;
    m.kind = Kind::Message;
    m.id = seq + "::m" + std::to_string(order);
    m.scope = seq;
    m.fields["op"] = op;
    if (isReturn) {
      m.fields["return"] = "true";
      m.fields["result"] = op;
    }
    model.declare(m);
    model.relate({"contains", holder, m.id, {}, ""});
  };
  frame(seq + "::f0", "loop", "", seq);
  message(0, "next", seq + "::f0", false);
  frame(seq + "::f1", "opt", "found", seq + "::f0");
  message(1, "take", seq + "::f1", false);
  message(2, "1", seq + "::f1", true);
  message(3, "after", seq, false);
  return model;
}

TEST_F(TestPathOf, ALoopWithANamedBranchInsideStaysOnTheDefaultPath) {
  const ds::model::Metamodel metamodel;
  const Model model = loopBehaviour(metamodel);
  std::set<std::string> kept;
  EXPECT_EQ(ops(ds::model::pathOf(model, "p@m::C::find::sequence", "", &kept)), (std::vector<std::string>{"next", "after"}));
  EXPECT_TRUE(kept.count("p@m::C::find::sequence::f0"));  // drawn as the loop it is
}

TEST_F(TestPathOf, APathThatNamesABranchInsideALoopGoesIntoItAndEndsThere) {
  const ds::model::Metamodel metamodel;
  const Model model = loopBehaviour(metamodel);
  EXPECT_EQ(ops(ds::model::pathOf(model, "p@m::C::find::sequence", "found", nullptr)), (std::vector<std::string>{"next", "take", "1"}));
}

class TestHolderOf : public ::testing::Test {};

TEST_F(TestHolderOf, AMembersBehaviourRunsOnWhatHoldsItAndAClassesOnTheClass) {
  const ds::model::Metamodel metamodel;
  Model model{metamodel};
  Entity c;
  c.kind = Kind::Class;
  c.id = "p@m::C";
  c.scope = "p@m";
  model.declare(c);
  Entity run;
  run.kind = Kind::Method;
  run.id = "p@m::C::run";
  run.scope = "p@m::C";
  model.declare(run);
  EXPECT_EQ(ds::model::holderOf(model, "p@m::C::run::sequence"), "p@m::C");
  EXPECT_EQ(ds::model::holderOf(model, "p@m::C::run::sequence/out"), "p@m::C");
  EXPECT_EQ(ds::model::holderOf(model, "p@m::C::sequence"), "p@m::C");
  EXPECT_EQ(ds::model::holderOf(model, "p@m::D::sequence"), "");
}

}  // namespace
