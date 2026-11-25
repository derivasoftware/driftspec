#include <gtest/gtest.h>

#include <string>

#include "ds/api/Drawings.hpp"
#include "ds/api/Workspace.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/theme/Theme.hpp"

namespace {

const char* const kDesign =
    "package p {\n"
    "  module m {\n"
    "    class A {\n"
    "      + run() -> none\n"
    "    }\n"
    "  }\n"
    "}\n"
    "diagram CL_P {\n"
    "  kind: class\n"
    "  include: p@*\n"
    "}\n"
    "diagram WALK_P {\n"
    "  kind: walkthrough\n"
    "  step A {\n"
    "    show: CL_P\n"
    "    text: A.\n"
    "  }\n"
    "}\n";

/// The one path from a diagram to its svg (DS-470): every diagram but a walkthrough, each drawn as it is drawn alone,
/// and nothing for a name that is no diagram.
TEST(TestDrawings, EveryDiagramButAWalkthroughIsDrawnAsItIsDrawnAlone) {
  const ds::model::Metamodel metamodel;
  const ds::theme::Theme theme;
  const auto ws = ds::api::Workspace::madeFor(metamodel, {{"p.ds", kDesign}});
  const auto all = ds::api::Drawings::of(ws.model(), metamodel, theme);
  ASSERT_EQ(all.size(), 1u);
  EXPECT_EQ(all.at("CL_P"), ds::api::Drawings::svg(ws.model(), metamodel, theme, "CL_P", 0));
  EXPECT_EQ(all.at("CL_P").rfind("<svg", 0), 0u);
  EXPECT_EQ(ds::api::Drawings::svg(ws.model(), metamodel, theme, "CL_None", 0), "");
}

}  // namespace
