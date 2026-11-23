#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

#include "ds/model/Detail.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/model/SequenceTree.hpp"

// The model package's internal helpers (DS-332), each held to what the public functions it serves rely on.

namespace {

using ds::model::Entity;
using ds::model::Kind;
using ds::model::Model;
namespace detail = ds::model::detail;

const std::string kSeq = "p@m::C::run::sequence";

/// One behaviour: m0, an `opt / stop` holding m1, then m2; each step contained by what encloses it.
Model behaviour(const ds::model::Metamodel& metamodel) {
  Model model{metamodel};
  Entity s;
  s.kind = Kind::Sequence;
  s.id = kSeq;
  model.declare(s);
  auto message = [&](int order, const std::string& holder) {
    Entity m;
    m.kind = Kind::Message;
    m.id = kSeq + "::m" + std::to_string(order);
    m.scope = kSeq;
    model.declare(m);
    model.relate({"contains", holder, m.id, {}, ""});
  };
  message(0, kSeq);
  Entity f;
  f.kind = Kind::Frame;
  f.id = kSeq + "::f0";
  f.scope = kSeq;
  f.fields["kind"] = "opt";
  f.fields["scenario"] = "stop";
  model.declare(f);
  model.relate({"contains", kSeq, f.id, {}, ""});
  message(1, f.id);
  message(2, kSeq);
  return model;
}

// ---- Identity.cpp ------------------------------------------------------------------------------

class TestOuterOf : public ::testing::Test {};

TEST_F(TestOuterOf, StepsOutOfAMemberAPackageAndTheSystem) {
  EXPECT_EQ(detail::outerOf("p@m::C"), "p@m");
  EXPECT_EQ(detail::outerOf("p@m"), "p@");
  EXPECT_EQ(detail::outerOf("p@"), "");
  EXPECT_EQ(detail::outerOf("app@core@"), "app@");
}

// ---- Model.cpp ---------------------------------------------------------------------------------

class TestFieldIn : public ::testing::Test {};

TEST_F(TestFieldIn, ReadsAFieldAndNothingForOneNotCarried) {
  Entity e;
  e.fields["kind"] = "loop";
  EXPECT_EQ(detail::fieldIn(e, "kind"), "loop");
  EXPECT_EQ(detail::fieldIn(e, "scenario"), "");
}

class TestBranches : public ::testing::Test {};

TEST_F(TestBranches, AnOptNamesItsScenarioOnOneSideOnly) {
  const ds::model::Metamodel metamodel;
  const Model model = behaviour(metamodel);
  const ds::model::SequenceTree tree(model, kSeq);
  const detail::Branches branches{tree};
  const Entity& opt = *model.find(kSeq + "::f0");
  EXPECT_EQ(branches.names(opt, false), (std::set<std::string>{"stop"}));
  EXPECT_TRUE(branches.names(opt, true).empty());
}

TEST_F(TestBranches, ThePathThatNamesAnOptTakesItAndTheDefaultSkipsIt) {
  const ds::model::Metamodel metamodel;
  const Model model = behaviour(metamodel);
  const ds::model::SequenceTree tree(model, kSeq);
  const detail::Branches branches{tree};
  const Entity& opt = *model.find(kSeq + "::f0");
  EXPECT_EQ(branches.decide(opt, "stop"), detail::Branches::Way::Then);
  EXPECT_EQ(branches.decide(opt, ""), detail::Branches::Way::Skip);
}

// The metamodel's builders are its private members (DS-341): tests/unit/ds/model/test_metamodel.cpp holds them
// through what the class answers.

// The manual's page builders are its private members (DS-342): tests/unit/ds/model/test_manual_pages.cpp holds
// them through the pages the class writes.

}  // namespace
