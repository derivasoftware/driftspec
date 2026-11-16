#include <gtest/gtest.h>

#include <string>

#include "ds/axis/Cut.hpp"
#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// The cut of a diagram (DS-229), over a shop of two packages: the sales hold a desk and a till, the
/// warehouse a stock; each diagram writes one way of saying what closes and what opens.
class TestCut : public ::testing::Test {
 protected:
  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    const auto file = parser.parseText(
        "shop.ds",
        "package shop {\n"
        "  package sales {\n"
        "    module desk {\n"
        "      class Desk { }\n"
        "    }\n"
        "    module till {\n"
        "      class Till { }\n"
        "    }\n"
        "  }\n"
        "  package warehouse {\n"
        "    module stock {\n"
        "      class Shelf { }\n"
        "    }\n"
        "  }\n"
        "}\n"
        "diagram None {\n  kind: class\n  include: shop@*\n}\n"
        "diagram OpenAlone {\n  kind: class\n  include: shop@*\n  open:\n}\n"
        "diagram Open {\n  kind: class\n  include: shop@*\n  open: shop@sales\n}\n"
        "diagram Close {\n  kind: class\n  include: shop@*\n  close: shop@warehouse\n}\n"
        "diagram Kind {\n  kind: class\n  include: shop@*\n  close: module\n}\n"
        "diagram Prefix {\n  kind: class\n  include: shop@*\n  close: shop@sales@*\n}\n"
        "diagram Through {\n  kind: class\n  include: shop@*\n  close: module\n  open: shop@sales@till\n}\n"
        "diagram Lent {\n  kind: class\n  include: shop@*\n  cut: Through\n}\n"
        "diagram LentAndMore {\n  kind: class\n  include: shop@*\n  cut: Kind\n  open: shop@warehouse@stock\n}\n"
        "diagram AsToday {\n  kind: sequence\n  include: shop@*\n}\n"
        "diagram Deep {\n  kind: sequence\n  include: shop@*\n  depth: 2\n}\n"
        "diagram OnACut {\n  kind: sequence\n  include: shop@*\n  close: module\n}\n"
        "diagram Unfold {\n  kind: sequence\n  include: shop@*\n  close: module\n  unfold: shop@sales\n}\n"
        "diagram Fold {\n  kind: sequence\n  include: shop@*\n  fold: shop@sales@till::Till::charge\n}\n"
        "diagram Both {\n  kind: sequence\n  include: shop@*\n  fold: shop@sales\n  unfold: shop@sales@desk\n}\n"
        "diagram DeepAndFold {\n  kind: sequence\n  include: shop@*\n  depth: 1\n  fold: shop@warehouse\n  unfold: shop@sales@till\n}\n"
        "diagram Ring {\n  kind: class\n  include: shop@*\n  cut: Gnir\n}\n"
        "diagram Gnir {\n  kind: class\n  include: shop@*\n  cut: Ring\n  close: package\n}\n");
    ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
    binder.bind(file);
    const auto steps = ds::check::steps();
    ds::check::Checker(model, steps).run(binder);
  }

  ds::axis::Cut cut(const std::string& diagram) const { return ds::axis::Cut(model, *model.find("diagram:" + diagram)); }

  ds::model::Model model;
};

TEST_F(TestCut, ADiagramThatWritesNeitherOpenNorCloseHasNoCut) {
  const auto none = cut("None");
  EXPECT_FALSE(none.written());
  EXPECT_FALSE(none.closed("shop@sales"));
  EXPECT_EQ(none.standsFor("shop@sales@desk::Desk"), "shop@sales@desk::Desk");
}

TEST_F(TestCut, OpenAloneClosesEveryBoxBelowTheOutermostPackage) {
  const auto alone = cut("OpenAlone");
  EXPECT_TRUE(alone.written());
  EXPECT_TRUE(alone.closed("shop@sales"));
  EXPECT_TRUE(alone.closed("shop@warehouse"));
  EXPECT_FALSE(alone.closed("shop"));  // the outermost package is the drawing itself
}

TEST_F(TestCut, OpenKeepsOpenTheBoxItNamesAndTheWayToIt) {
  const auto open = cut("Open");
  EXPECT_FALSE(open.closed("shop@sales"));
  EXPECT_TRUE(open.closed("shop@sales@desk"));  // what it holds was not named: closed like every other
  EXPECT_TRUE(open.closed("shop@warehouse"));
}

TEST_F(TestCut, CloseNamesWhatClosesAndTheRestStaysOpen) {
  const auto close = cut("Close");
  EXPECT_TRUE(close.written());
  EXPECT_TRUE(close.closed("shop@warehouse"));
  EXPECT_FALSE(close.closed("shop@sales"));
  EXPECT_FALSE(close.closed("shop@sales@desk"));
}

TEST_F(TestCut, AKindClosesEveryBoxOfThatKindWhereverItIs) {
  const auto kind = cut("Kind");
  EXPECT_TRUE(kind.closed("shop@sales@desk"));
  EXPECT_TRUE(kind.closed("shop@sales@till"));
  EXPECT_TRUE(kind.closed("shop@warehouse@stock"));
  EXPECT_FALSE(kind.closed("shop@sales"));
  EXPECT_FALSE(kind.closed("shop@warehouse"));
}

TEST_F(TestCut, APrefixClosesEveryBoxThatBeginsSo) {
  const auto prefix = cut("Prefix");
  EXPECT_TRUE(prefix.closed("shop@sales@desk"));
  EXPECT_TRUE(prefix.closed("shop@sales@till"));
  EXPECT_FALSE(prefix.closed("shop@sales"));
  EXPECT_FALSE(prefix.closed("shop@warehouse@stock"));
}

TEST_F(TestCut, OpenGoesThroughWhatCloseCloses) {
  const auto through = cut("Through");
  EXPECT_FALSE(through.closed("shop@sales@till"));  // a module, and close: module is written: open: wins
  EXPECT_TRUE(through.closed("shop@sales@desk"));
  EXPECT_TRUE(through.closed("shop@warehouse@stock"));
}

TEST_F(TestCut, WhatStandsForAnIdIsTheOutermostClosedBoxAroundItOrItself) {
  const auto alone = cut("OpenAlone");
  EXPECT_EQ(alone.standsFor("shop@sales@desk::Desk"), "shop@sales");  // the outermost, not the module
  EXPECT_EQ(alone.standsFor("shop@sales"), "shop@sales");             // a closed box stands for itself
  const auto through = cut("Through");
  EXPECT_EQ(through.standsFor("shop@sales@desk::Desk"), "shop@sales@desk");
  EXPECT_EQ(through.standsFor("shop@sales@till::Till"), "shop@sales@till::Till");  // nothing around it is closed
}

TEST_F(TestCut, ADiagramTakesTheCutOfAnotherAndAddsWhatItWritesItself) {
  const auto lent = cut("Lent");
  EXPECT_TRUE(lent.written());
  EXPECT_TRUE(lent.closed("shop@sales@desk"));
  EXPECT_FALSE(lent.closed("shop@sales@till"));
  const auto more = cut("LentAndMore");
  EXPECT_TRUE(more.closed("shop@sales@desk"));        // the kind, from the diagram it takes
  EXPECT_FALSE(more.closed("shop@warehouse@stock"));  // its own open: goes through it
}

TEST_F(TestCut, ARingOfDiagramsThatTakeEachOthersCutEnds) {
  const auto ring = cut("Ring");
  EXPECT_TRUE(ring.closed("shop@sales"));  // what the other one writes, read once
  EXPECT_TRUE(ring.closed("shop@warehouse"));
}

TEST_F(TestCut, WithNothingWrittenACallIsFollowedAsDeepAsDepthSays) {
  EXPECT_FALSE(cut("AsToday").follows("shop@sales@desk::Desk::sell", 0));  // no depth: none, as before
  EXPECT_TRUE(cut("Deep").follows("shop@sales@desk::Desk::sell", 2));
  EXPECT_FALSE(cut("Deep").follows("shop@sales@desk::Desk::sell", 0));  // its two levels are spent
}

TEST_F(TestCut, ASequenceThatStandsOnACutFollowsEveryCall) {
  EXPECT_TRUE(cut("OnACut").follows("shop@sales@desk::Desk::sell", 0));
  EXPECT_TRUE(cut("OnACut").follows("shop@warehouse@stock::Shelf::pick", 0));
}

TEST_F(TestCut, UnfoldAloneFollowsWhatItNamesAndFoldsEveryOtherCall) {
  const auto unfold = cut("Unfold");
  EXPECT_TRUE(unfold.follows("shop@sales@desk::Desk::sell", 0));  // a call that lands in the box it names
  EXPECT_TRUE(unfold.follows("shop@sales@till::Till::charge", 0));
  EXPECT_FALSE(unfold.follows("shop@warehouse@stock::Shelf::pick", 0));  // though it stands on a cut
}

TEST_F(TestCut, FoldStopsAtWhatItNamesAndEveryOtherCallIsFollowed) {
  const auto fold = cut("Fold");  // no cut to stand on: fold: alone says the rest is followed
  EXPECT_FALSE(fold.follows("shop@sales@till::Till::charge", 0));
  EXPECT_TRUE(fold.follows("shop@sales@desk::Desk::sell", 0));
}

TEST_F(TestCut, UnfoldGoesThroughWhatFoldFolds) {
  const auto both = cut("Both");
  EXPECT_TRUE(both.follows("shop@sales@desk::Desk::sell", 0));        // inside what fold: names, and unfold: names it
  EXPECT_FALSE(both.follows("shop@sales@till::Till::charge", 0));     // folded
  EXPECT_TRUE(both.follows("shop@warehouse@stock::Shelf::pick", 0));  // fold: is written, so the rest is followed
}

TEST_F(TestCut, DepthSaysHowFarForTheCallsNeitherNames) {
  const auto mixed = cut("DeepAndFold");
  EXPECT_TRUE(mixed.follows("shop@sales@desk::Desk::sell", 1));
  EXPECT_FALSE(mixed.follows("shop@sales@desk::Desk::sell", 0));
  EXPECT_FALSE(mixed.follows("shop@warehouse@stock::Shelf::pick", 1));  // folded, whatever is left
  EXPECT_TRUE(mixed.follows("shop@sales@till::Till::charge", 0));       // unfolded, whatever is left
}

}  // namespace
