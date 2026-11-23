#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/model/Paths.hpp"

namespace {

using ds::model::Entity;
using ds::model::Kind;
using ds::model::Model;
using ds::model::Paths;

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

class TestPaths : public ::testing::Test {};

TEST_F(TestPaths, AnswersTheModelItIsMadeOverAsTheModelStandsWhenAsked) {
  // it keeps the model by reference: a frame declared after it is made is one it answers for
  const ds::model::Metamodel metamodel;
  Model model = behaviour(metamodel);
  const Paths paths(model);
  EXPECT_FALSE(paths.hasScenario("p@m::C::run::sequence", "late"));
  Entity f;
  f.kind = Kind::Frame;
  f.id = "p@m::C::run::sequence::f1";
  f.scope = "p@m::C::run::sequence";
  f.fields["kind"] = "opt";
  f.fields["scenario"] = "late";
  model.declare(f);
  model.relate({"contains", "p@m::C::run::sequence", f.id, {}, ""});
  EXPECT_TRUE(paths.hasScenario("p@m::C::run::sequence", "late"));
}

class TestHasScenario : public ::testing::Test {};

TEST_F(TestHasScenario, AnswersForANamedBranchOnly) {
  const ds::model::Metamodel metamodel;
  const Model model = behaviour(metamodel);
  const Paths paths(model);
  EXPECT_TRUE(paths.hasScenario("p@m::C::run::sequence", "stop"));
  EXPECT_FALSE(paths.hasScenario("p@m::C::run::sequence", "other"));
  EXPECT_FALSE(paths.hasScenario("p@m::C::other::sequence", "stop"));
}

class TestPathOf : public ::testing::Test {};

TEST_F(TestPathOf, TheNamedPathTakesItsBranchAndEndsAtItsReturn) {
  const ds::model::Metamodel metamodel;
  const Model model = behaviour(metamodel);
  EXPECT_EQ(ops(Paths(model).pathOf("p@m::C::run::sequence", "stop", nullptr)), (std::vector<std::string>{"a", "b", "1"}));
}

TEST_F(TestPathOf, TheDefaultPathSkipsTheNamedBranch) {
  const ds::model::Metamodel metamodel;
  const Model model = behaviour(metamodel);
  std::set<std::string> kept;
  EXPECT_EQ(ops(Paths(model).pathOf("p@m::C::run::sequence", "", &kept)), (std::vector<std::string>{"a", "c", "0"}));
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
  EXPECT_EQ(ops(Paths(model).pathOf("p@m::C::find::sequence", "", &kept)), (std::vector<std::string>{"next", "after"}));
  EXPECT_TRUE(kept.count("p@m::C::find::sequence::f0"));  // drawn as the loop it is
}

TEST_F(TestPathOf, APathThatNamesABranchInsideALoopGoesIntoItAndEndsThere) {
  const ds::model::Metamodel metamodel;
  const Model model = loopBehaviour(metamodel);
  EXPECT_EQ(ops(Paths(model).pathOf("p@m::C::find::sequence", "found", nullptr)), (std::vector<std::string>{"next", "take", "1"}));
}

/// m0; an alt whose first side, / refused, holds m1 and a return (m2) and whose else holds m3; then an opt
/// that names no scenario holding a return (m4); then m5. Each step contained by what encloses it (DS-172).
Model altBehaviour(const ds::model::Metamodel& metamodel) {
  Model model{metamodel};
  const std::string seq = "p@m::C::pay::sequence";
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
  message(0, "ask", seq, false);
  frame(seq + "::f0", "alt", "", seq);
  frame(seq + "::f0::b0", "branch", "refused", seq + "::f0");
  message(1, "refuse", seq + "::f0::b0", false);
  message(2, "1", seq + "::f0::b0", true);
  frame(seq + "::f0::b1", "branch", "", seq + "::f0");
  message(3, "take", seq + "::f0::b1", false);
  frame(seq + "::f1", "opt", "", seq);
  message(4, "2", seq + "::f1", true);
  message(5, "after", seq, false);
  return model;
}

// How a path goes at each frame is Paths's own (DS-343): the branches of a behaviour are held here through the
// paths they give. That the side of an opt naming a scenario is taken by that path only, and skipped by the
// default one, TestPathOf holds.
class TestBranches : public ::testing::Test {};

TEST_F(TestBranches, OfAnAltWithOneNamedSideTheDefaultPathTakesTheOther) {
  const ds::model::Metamodel metamodel;
  const Model model = altBehaviour(metamodel);
  const Paths paths(model);
  EXPECT_EQ(ops(paths.pathOf("p@m::C::pay::sequence", "refused", nullptr)), (std::vector<std::string>{"ask", "refuse", "1"}));
  EXPECT_EQ(ops(paths.pathOf("p@m::C::pay::sequence", "", nullptr)), (std::vector<std::string>{"ask", "take", "2", "after"}));
}

TEST_F(TestBranches, AFrameThatNamesNoScenarioIsKeptAndItsReturnEndsNoPath) {
  const ds::model::Metamodel metamodel;
  const Model model = altBehaviour(metamodel);
  std::set<std::string> kept;
  Paths(model).pathOf("p@m::C::pay::sequence", "", &kept);
  EXPECT_EQ(kept, (std::set<std::string>{"p@m::C::pay::sequence::f1"}));
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
  const Paths paths(model);
  EXPECT_EQ(paths.holderOf("p@m::C::run::sequence"), "p@m::C");
  EXPECT_EQ(paths.holderOf("p@m::C::run::sequence/out"), "p@m::C");
  EXPECT_EQ(paths.holderOf("p@m::C::sequence"), "p@m::C");
  EXPECT_EQ(paths.holderOf("p@m::D::sequence"), "");
}

}  // namespace
