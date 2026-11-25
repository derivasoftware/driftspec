#include <gtest/gtest.h>

#include <string>

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
  EXPECT_EQ(model.find("shop@desk::Ghost"), nullptr);  // no expected node is made for it
  EXPECT_TRUE(model.findings().empty());               // the walkthrough's check tells it (DS-469)
}

}  // namespace
