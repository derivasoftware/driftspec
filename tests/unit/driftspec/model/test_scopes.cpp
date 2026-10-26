#include <gtest/gtest.h>

#include "driftspec/model/Model.hpp"

namespace {

/// The system scope: the inside of an outermost package, and nothing deeper (nested packages, 2026-10-26).
class TestIsSystemScope : public ::testing::Test {};

TEST_F(TestIsSystemScope, OnlyTheOutermostPackageIsTheSystem) {
  EXPECT_TRUE(driftspec::model::isSystemScope("app@"));
  EXPECT_FALSE(driftspec::model::isSystemScope("app@core@"));  // an inner package: its own level
  EXPECT_FALSE(driftspec::model::isSystemScope("app@core"));   // a module's scope is no package scope
  EXPECT_FALSE(driftspec::model::isSystemScope(""));
}

/// The simple name at the end of an identity, whatever separators lead to it.
class TestLastName : public ::testing::Test {};

TEST_F(TestLastName, TheNameAfterTheLastSeparator) {
  EXPECT_EQ(driftspec::model::lastName("app@core@model::Entity"), "Entity");
  EXPECT_EQ(driftspec::model::lastName("app@core"), "core");
  EXPECT_EQ(driftspec::model::lastName("app"), "app");
}

}  // namespace
