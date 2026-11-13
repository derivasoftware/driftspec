#include <gtest/gtest.h>

#include "ds/api/Workspace.hpp"

namespace {

using ds::api::Identity;

/// How an identity is written, asked from another language through the binary interface (DS-173):
/// each answer is the model's own.

TEST(TestIdentity, ANameIsWrittenInAScopeAndTheScopeStepsOut) {
  EXPECT_EQ(Identity::qualify("p@m", "C"), "p@m::C");
  EXPECT_EQ(Identity::outer("p@m::C"), "p@m");
  EXPECT_EQ(Identity::boxOf("app@core@"), "app@core");
  EXPECT_EQ(Identity::packageOf("app@core@m::C"), "app@core@");
}

TEST(TestIdentity, ABehaviourNamesItsSubjectAndItsScenario) {
  EXPECT_EQ(Identity::behaviourOf("p@m::C::run::sequence/out::m2"), "p@m::C::run::sequence/out");
  EXPECT_EQ(Identity::subjectOf("p@m::C::run::sequence/out::m2"), "p@m::C::run");
  EXPECT_EQ(Identity::scenarioOf("p@m::C::run::sequence/out"), "out");
  EXPECT_EQ(Identity::withoutRevision("REQ-00001-2"), "REQ-00001");
}

}  // namespace
