#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/model/SequenceTree.hpp"
#include "ds/parse/Parser.hpp"

namespace {

const std::string kSeq = "p@m::C::run::sequence";

/// One reading of a behaviour's tree, over the tree the binder leaves of a behaviour as it is
/// written (REQ-00065-1, REQ-00049-2): m0; an alt whose first side holds m1 and a loop with m2 and
/// whose else, refused, holds m3; an opt with m4; m5.
class TestSequenceTreeOfADesign : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    const auto file = parser.parseText("p.ds",
                                       "package p {\n  module m {\n"
                                       "    class B {\n      + go() -> int\n    }\n"
                                       "    class C {\n      - b: B\n      + run() -> int\n    }\n"
                                       "    sequence C::run {\n"
                                       "      self -> b.go() -> a\n"
                                       "      alt there is some {\n"
                                       "        self -> b.go() -> c\n"
                                       "        loop each one {\n"
                                       "          self -> b.go() -> d\n"
                                       "        }\n"
                                       "      } else / refused {\n"
                                       "        self -> b.go() -> e\n"
                                       "      }\n"
                                       "      opt it is asked {\n"
                                       "        self -> b.go() -> f\n"
                                       "      }\n"
                                       "      return a\n"
                                       "    }\n"
                                       "  }\n}\n");
    ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
    binder.bind(file);
    const auto steps = ds::check::steps();
    ds::check::Checker(model, metamodel, steps).run(binder);
  }

  static std::vector<std::string> names(const std::vector<std::string>& ids) {
    std::vector<std::string> out;
    for (const auto& id : ids) out.push_back(id.substr(kSeq.size() + 2));
    return out;
  }
};

TEST_F(TestSequenceTreeOfADesign, TheMessagesComeInTheOrderWrittenAtAnyDepth) {
  const ds::model::SequenceTree tree(model, kSeq);
  EXPECT_EQ(names(tree.messages()), (std::vector<std::string>{"m0", "m1", "m2", "m3", "m4", "m5"}));
  EXPECT_EQ(tree.orderOf(kSeq + "::m3"), 3);
  EXPECT_EQ(tree.orderOf("p@m::C::other::sequence::m0"), -1);
}

TEST_F(TestSequenceTreeOfADesign, TheFramesAreTheLoopsAltsAndOptsAndEachStepKnowsTheInnermostAroundIt) {
  const ds::model::SequenceTree tree(model, kSeq);
  EXPECT_EQ(names(tree.frames()), (std::vector<std::string>{"f0", "f1", "f2"}));  // never the branches of the alt
  EXPECT_EQ(tree.frameOf(kSeq + "::m0"), "");
  EXPECT_EQ(tree.frameOf(kSeq + "::m1"), kSeq + "::f0");  // a branch is a side of its alt
  EXPECT_EQ(tree.frameOf(kSeq + "::m2"), kSeq + "::f1");
  EXPECT_EQ(tree.frameOf(kSeq + "::f1"), kSeq + "::f0");
  EXPECT_EQ(tree.frameOf(kSeq + "::m3"), kSeq + "::f0");
  EXPECT_EQ(tree.frameOf(kSeq + "::m4"), kSeq + "::f2");
}

TEST_F(TestSequenceTreeOfADesign, AFrameKnowsWhereItOpensAndWhatItHoldsFirst) {
  const ds::model::SequenceTree tree(model, kSeq);
  EXPECT_EQ(tree.at(kSeq + "::f0"), 1);
  EXPECT_EQ(tree.first(kSeq + "::f0"), kSeq + "::m1");
  EXPECT_EQ(tree.first(kSeq + "::f1"), kSeq + "::m2");
  EXPECT_EQ(tree.at(kSeq + "::f2"), 4);  // after m3
  EXPECT_EQ(tree.first(kSeq + "::f2"), kSeq + "::m4");
}

TEST_F(TestSequenceTreeOfADesign, AnAltKnowsItsElseSideAndWhatEachSideNames) {
  const ds::model::SequenceTree tree(model, kSeq);
  EXPECT_EQ(tree.elseOf(kSeq + "::f0"), kSeq + "::m3");
  EXPECT_TRUE(tree.twoSides(kSeq + "::f0"));
  EXPECT_TRUE(tree.onElse(kSeq + "::f0", kSeq + "::m3"));
  EXPECT_FALSE(tree.onElse(kSeq + "::f0", kSeq + "::m2"));
  EXPECT_EQ(tree.scenario(kSeq + "::f0", true), "refused");
  EXPECT_EQ(tree.scenario(kSeq + "::f0", false), "");
  EXPECT_EQ(tree.inside(kSeq + "::f0", false), (std::vector<std::string>{kSeq + "::f1"}));
  EXPECT_TRUE(tree.inside(kSeq + "::f0", true).empty());
  EXPECT_FALSE(tree.twoSides(kSeq + "::f2"));  // an opt has one side
}

}  // namespace
