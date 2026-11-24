#include <gtest/gtest.h>

#include "ds/bind/LegacyBinder.hpp"

namespace {

/// The walk the binder had (DS-355): since DS-356 every construct has a binder of its own, and it takes no kind.
TEST(TestLegacyBinder, ItTakesNoKindAnyMore) {
  const ds::bind::LegacyBinder legacy;
  EXPECT_TRUE(legacy.kinds().empty());
}

}  // namespace
