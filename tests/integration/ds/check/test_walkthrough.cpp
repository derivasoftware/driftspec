#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// A walkthrough in a workspace (REQ-00241-1, DS-468): its steps are related to the diagram each shows and to what
/// each lights, by identity; a name that resolves to nothing is left to the walkthrough's check, with no finding yet.
class TestWalkthrough : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void load(const std::string& walk) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    binder.bind(parser.parseText("shop.ds",
                                 "package shop {\n"
                                 "  module desk {\n"
                                 "    class Desk {\n"
                                 "      + sell(item: string) -> int\n"
                                 "    }\n"
                                 "  }\n"
                                 "}\n"
                                 "diagram CL_Shop {\n"
                                 "  kind: class\n"
                                 "  include: shop@*\n"
                                 "}\n"));
    const auto file = parser.parseText("walk.ds", walk);
    ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
    binder.bind(file);
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
  }
  /// The messages of one code, each after its subject.
  std::vector<std::string> said(const std::string& code) const {
    std::vector<std::string> out;
    for (const auto& f : model.findings())
      if (f.code == code) out.push_back(f.subject + ": " + f.message);
    return out;
  }
  bool related(const std::string& kind, const std::string& from, const std::string& to) const {
    for (const auto& r : model.index().fromOfKind(from, kind))
      if (r.to == to) return true;
    return false;
  }
};

TEST_F(TestWalkthrough, AStepIsRelatedToWhatItShowsAndLights) {
  load(
      "diagram WALK_Shop {\n"
      "  kind: walkthrough\n"
      "  title: The shop\n"
      "  step Who sells {\n"
      "    show: CL_Shop\n"
      "    focus: shop@desk::Desk\n"
      "    highlight: shop@desk::Desk::sell\n"
      "    at: shop@desk::Desk\n"
      "    text: The desk sells.\n"
      "  }\n"
      "}\n");
  EXPECT_TRUE(related("shows", "diagram:WALK_Shop::1", "diagram:CL_Shop"));
  EXPECT_TRUE(related("focuses", "diagram:WALK_Shop::1", "shop@desk::Desk"));
  EXPECT_TRUE(related("highlights", "diagram:WALK_Shop::1", "shop@desk::Desk::sell"));
  EXPECT_TRUE(related("points-at", "diagram:WALK_Shop::1", "shop@desk::Desk"));
  EXPECT_TRUE(model.findings().empty());
}

TEST_F(TestWalkthrough, ANameThatResolvesToNothingIsLeftToTheCheck) {
  load(
      "diagram WALK_Shop {\n"
      "  kind: walkthrough\n"
      "  step Nowhere {\n"
      "    show: CL_Gone\n"
      "    focus: shop@desk::Ghost\n"
      "    text: Nothing is here.\n"
      "  }\n"
      "}\n");
  EXPECT_TRUE(model.index().fromOfKind("diagram:WALK_Shop::1", "shows").empty());
  EXPECT_TRUE(model.index().fromOfKind("diagram:WALK_Shop::1", "focuses").empty());
  EXPECT_EQ(model.find("shop@desk::Ghost"), nullptr);                             // no expected node is made for it
  for (const auto& f : model.findings()) EXPECT_EQ(f.code, "W-WALK-UNDECLARED");  // the walkthrough's check tells it (DS-469)
}

/// What the check says of a walkthrough (REQ-00242-1, DS-469), each at its step and with the step as its subject.
TEST_F(TestWalkthrough, AStepThatNamesNothingIsTold) {
  load(
      "diagram WALK_Shop {\n"
      "  kind: walkthrough\n"
      "  step Nowhere {\n"
      "    show: CL_Gone\n"
      "    focus: shop@desk::Ghost\n"
      "    at: shop@desk::Phantom\n"
      "    text: Nothing is here.\n"
      "  }\n"
      "}\n");
  const auto told = said("W-WALK-UNDECLARED");
  ASSERT_EQ(told.size(), 3u);  // the diagram, the focus and the anchor
  for (const auto& t : told) EXPECT_EQ(t.rfind("diagram:WALK_Shop::1: ", 0), 0u) << t;
}

TEST_F(TestWalkthrough, AStepThatLightsWhatItsDiagramDoesNotDrawIsTold) {
  load(
      "diagram REQ_None {\n"
      "  kind: requirement\n"
      "  include: nothing@*\n"
      "}\n"
      "diagram WALK_Shop {\n"
      "  kind: walkthrough\n"
      "  step Elsewhere {\n"
      "    show: REQ_None\n"
      "    focus: shop@desk::Desk\n"
      "    text: The desk is not on this drawing.\n"
      "  }\n"
      "  step Here {\n"
      "    show: CL_Shop\n"
      "    focus: shop@desk::Desk::sell\n"
      "    text: A member is drawn in its box.\n"
      "  }\n"
      "}\n");
  const auto told = said("W-WALK-NOT-SHOWN");
  ASSERT_EQ(told.size(), 1u);
  EXPECT_NE(told[0].find("shop@desk::Desk, which REQ_None does not draw"), std::string::npos) << told[0];
}

TEST_F(TestWalkthrough, ACalloutPointingAtWhatTheStepDoesNotLightIsTold) {
  load(
      "diagram WALK_Shop {\n"
      "  kind: walkthrough\n"
      "  step Pointing away {\n"
      "    show: CL_Shop\n"
      "    focus: shop@desk::Desk::sell\n"
      "    at: shop@desk::Desk\n"
      "    text: The callout points at the class, the step lights its operation.\n"
      "  }\n"
      "}\n");
  EXPECT_EQ(said("W-WALK-AT").size(), 1u);
  EXPECT_TRUE(said("W-WALK-NOT-SHOWN").empty());
}

TEST_F(TestWalkthrough, AMissingScenarioAndAnEmptyWalkthroughAreTold) {
  load(
      "diagram WALK_Shop {\n"
      "  kind: walkthrough\n"
      "  step A scenario that is not {\n"
      "    show: CL_Shop / exploded\n"
      "  }\n"
      "}\n"
      "diagram WALK_Nothing {\n"
      "  kind: walkthrough\n"
      "}\n");
  EXPECT_EQ(said("W-SCENARIO-UNDECLARED").size(), 1u);
  const auto empty = said("W-WALK-EMPTY");
  ASSERT_EQ(empty.size(), 2u);  // the step says nothing; the other walkthrough has no step
  bool nothing = false;
  for (const auto& e : empty) nothing = nothing || e.rfind("diagram:WALK_Nothing: ", 0) == 0;
  EXPECT_TRUE(nothing);
}

}  // namespace
