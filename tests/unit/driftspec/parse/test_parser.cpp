#include <gtest/gtest.h>

#include "driftspec/parse/Parser.hpp"

namespace {

class TestParser : public ::testing::Test {};

TEST_F(TestParser, ParsesAWholeWorkspaceWithoutSyntaxErrors) {
  driftspec::parse::Parser parser;
  const auto files = parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/diag");
  ASSERT_EQ(files.size(), 13u);
  for (const auto& f : files) EXPECT_FALSE(f.hasSyntaxErrors) << f.path;
}

TEST_F(TestParser, ReportsSyntaxErrors) {
  driftspec::parse::Parser parser;
  const auto file = parser.parseText("x.ds", "package ds { class {");
  EXPECT_TRUE(file.hasSyntaxErrors);
}

}  // namespace
