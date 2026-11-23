#include <gtest/gtest.h>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/model/SequenceTree.hpp"

namespace {

using ds::model::Entity;
using ds::model::Kind;
using ds::model::Model;
using ds::model::SequenceTree;

const std::string kSeq = "p@m::C::run::sequence";

/// m0; alt (its first side holds m1 and a loop holding m2; its else / refused holds m3); an empty opt; m4.
/// Each step contained by what encloses it, in the order written, as the binder keeps it (DS-172).
Model behaviour(const ds::model::Metamodel& metamodel) {
  Model model{metamodel};
  Entity s;
  s.kind = Kind::Sequence;
  s.id = kSeq;
  model.declare(s);
  auto step = [&](Kind kind, const std::string& name, const std::string& holder, const std::string& frameKind = "",
                  const std::string& scenario = "") {
    Entity e;
    e.kind = kind;
    e.id = kSeq + "::" + name;
    e.scope = kSeq;
    if (!frameKind.empty()) e.fields["kind"] = frameKind;
    if (!scenario.empty()) e.fields["scenario"] = scenario;
    model.declare(e);
    model.relate({"contains", holder, e.id, {}, ""});
    return e.id;
  };
  step(Kind::Message, "m0", kSeq);
  const std::string alt = step(Kind::Frame, "f0", kSeq, "alt");
  const std::string then = step(Kind::Frame, "f0::b0", alt, "branch");
  step(Kind::Message, "m1", then);
  const std::string loop = step(Kind::Frame, "f1", then, "loop");
  step(Kind::Message, "m2", loop);
  const std::string otherwise = step(Kind::Frame, "f0::b1", alt, "branch", "refused");
  step(Kind::Message, "m3", otherwise);
  step(Kind::Frame, "f2", kSeq, "opt");
  step(Kind::Message, "m4", kSeq);
  return model;
}

/// One reading of a behaviour's tree, the same for every reader (REQ-00049-2, DS-172).
class TestSequenceTree : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  Model model = behaviour(metamodel);
  SequenceTree tree{model, kSeq};
};

TEST_F(TestSequenceTree, TheMessagesComeInTheOrderWrittenAtAnyDepth) {
  std::vector<std::string> ids;
  for (const auto& m : tree.messages()) ids.push_back(m.substr(kSeq.size() + 2));
  EXPECT_EQ(ids, (std::vector<std::string>{"m0", "m1", "m2", "m3", "m4"}));
  EXPECT_EQ(tree.orderOf(kSeq + "::m3"), 3);
  EXPECT_EQ(tree.orderOf("p@m::C::other::sequence::m0"), -1);
}

TEST_F(TestSequenceTree, TheFramesAreTheLoopsAltsAndOptsNeverTheBranches) {
  std::vector<std::string> ids;
  for (const auto& f : tree.frames()) ids.push_back(f.substr(kSeq.size() + 2));
  EXPECT_EQ(ids, (std::vector<std::string>{"f0", "f1", "f2"}));
}

TEST_F(TestSequenceTree, TheFrameAroundAStepIsTheInnermostLoopAltOrOpt) {
  EXPECT_EQ(tree.frameOf(kSeq + "::m0"), "");
  EXPECT_EQ(tree.frameOf(kSeq + "::m1"), kSeq + "::f0");  // a branch is a side of its alt
  EXPECT_EQ(tree.frameOf(kSeq + "::m2"), kSeq + "::f1");
  EXPECT_EQ(tree.frameOf(kSeq + "::f1"), kSeq + "::f0");
  EXPECT_EQ(tree.frameOf(kSeq + "::m3"), kSeq + "::f0");
}

TEST_F(TestSequenceTree, AFrameKnowsWhereItOpensAndWhatItHoldsFirst) {
  EXPECT_EQ(tree.at(kSeq + "::f0"), 1);
  EXPECT_EQ(tree.first(kSeq + "::f0"), kSeq + "::m1");
  EXPECT_EQ(tree.first(kSeq + "::f1"), kSeq + "::m2");
  EXPECT_EQ(tree.at(kSeq + "::f2"), 4);  // after m3, before m4
  EXPECT_EQ(tree.first(kSeq + "::f2"), "");
}

TEST_F(TestSequenceTree, AnAltKnowsItsElseSideAndWhatEachSideNames) {
  EXPECT_EQ(tree.elseOf(kSeq + "::f0"), kSeq + "::m3");
  EXPECT_TRUE(tree.twoSides(kSeq + "::f0"));
  EXPECT_TRUE(tree.onElse(kSeq + "::f0", kSeq + "::m3"));
  EXPECT_FALSE(tree.onElse(kSeq + "::f0", kSeq + "::m2"));
  EXPECT_EQ(tree.scenario(kSeq + "::f0", true), "refused");
  EXPECT_EQ(tree.scenario(kSeq + "::f0", false), "");
  EXPECT_EQ(tree.inside(kSeq + "::f0", false), (std::vector<std::string>{kSeq + "::f1"}));
  EXPECT_TRUE(tree.inside(kSeq + "::f0", true).empty());
}

TEST(TestSequenceTreeSides, WhatLiesDeeperOnAnElseSideIsOnItAndAnInnerElseIsNotTheOuters) {
  // DS-345: the sides are told by the walk once; nested frames keep the answers a walk up gave
  // f0 alt { b0: m1, f1 alt { b0: m2 | b1: m3 } | b1: f2 loop { m4 } }
  const ds::model::Metamodel metamodel;
  Model model{metamodel};
  Entity s;
  s.kind = Kind::Sequence;
  s.id = kSeq;
  model.declare(s);
  auto step = [&](Kind kind, const std::string& name, const std::string& holder, const std::string& frameKind = "") {
    Entity e;
    e.kind = kind;
    e.id = kSeq + "::" + name;
    e.scope = kSeq;
    if (!frameKind.empty()) e.fields["kind"] = frameKind;
    model.declare(e);
    model.relate({"contains", holder, e.id, {}, ""});
    return e.id;
  };
  const std::string f0 = step(Kind::Frame, "f0", kSeq, "alt");
  const std::string first = step(Kind::Frame, "f0::b0", f0, "branch");
  step(Kind::Message, "m1", first);
  const std::string f1 = step(Kind::Frame, "f1", first, "alt");
  step(Kind::Message, "m2", step(Kind::Frame, "f1::b0", f1, "branch"));
  step(Kind::Message, "m3", step(Kind::Frame, "f1::b1", f1, "branch"));
  const std::string otherwise = step(Kind::Frame, "f0::b1", f0, "branch");
  const std::string f2 = step(Kind::Frame, "f2", otherwise, "loop");
  step(Kind::Message, "m4", f2);
  const SequenceTree tree(model, kSeq);
  EXPECT_TRUE(tree.onElse(f0, kSeq + "::m4"));  // inside a loop on the else side
  EXPECT_TRUE(tree.onElse(f0, f2));
  EXPECT_FALSE(tree.onElse(f0, otherwise));  // the side itself is the alt's, not on it
  EXPECT_TRUE(tree.onElse(f1, kSeq + "::m3"));
  EXPECT_FALSE(tree.onElse(f0, kSeq + "::m3"));  // the inner alt's else lies on the outer's first side
  EXPECT_EQ(tree.elseOf(f0), kSeq + "::m4");
  EXPECT_EQ(tree.elseOf(f1), kSeq + "::m3");
  EXPECT_EQ(tree.elseOf(kSeq + "::nothing"), "");
}

TEST_F(TestSequenceTree, AnAltWithAnEmptyElseHasOneSide) {
  const ds::model::Metamodel metamodel;
  Model one{metamodel};
  Entity s;
  s.kind = Kind::Sequence;
  s.id = kSeq;
  one.declare(s);
  for (const auto& [id, kind, holder] : std::vector<std::tuple<std::string, std::string, std::string>>{
           {"f0", "alt", kSeq}, {"f0::b0", "branch", kSeq + "::f0"}, {"f0::b1", "branch", kSeq + "::f0"}}) {
    Entity f;
    f.kind = Kind::Frame;
    f.id = kSeq + "::" + id;
    f.scope = kSeq;
    f.fields["kind"] = kind;
    one.declare(f);
    one.relate({"contains", holder, f.id, {}, ""});
  }
  EXPECT_FALSE(SequenceTree(one, kSeq).twoSides(kSeq + "::f0"));
}

}  // namespace
