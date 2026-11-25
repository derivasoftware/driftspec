#include <gtest/gtest.h>

#include "ds/render/EdgeMarkup.hpp"

namespace {

/// An edge drawn in SVG (DS-371): its path and its label are kept apart, so every label is written above every path.
TEST(TestEdgeMarkup, ThePathAndTheLabelAreWrittenApart) {
  const ds::render::EdgeMarkup m{"<path d=\"M0,0 L1,1\"/>", "<text>uses</text>"};
  EXPECT_EQ(m.path, "<path d=\"M0,0 L1,1\"/>");
  EXPECT_EQ(m.label, "<text>uses</text>");
  EXPECT_TRUE(ds::render::EdgeMarkup{}.label.empty());  // an edge with no label writes none
}

}  // namespace
