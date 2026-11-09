#include <gtest/gtest.h>

#include "ds/model/Identity.hpp"

namespace {

namespace m = ds::model;

/// How identities are written, in one place (DS-124): packages join with @, the rest with ::, a
/// behaviour is its subject's identity with ::sequence[/scenario], a template carries its arity.

TEST(TestQualify, APackageJoinsWithAtAndTheRestWithColons) {
  EXPECT_EQ(m::qualify("p@", "C"), "p@C");
  EXPECT_EQ(m::qualify("p@m", "C"), "p@m::C");
  EXPECT_EQ(m::qualify("", "p"), "p");
}

TEST(TestOuter, EachStepLeavesOneScope) {
  EXPECT_EQ(m::outer("p@m::C"), "p@m");
  EXPECT_EQ(m::outer("p@m"), "p@");
  EXPECT_EQ(m::outer("app@core@"), "app@");
  EXPECT_EQ(m::outer("p@"), "");
}

TEST(TestBoxOf, APackageScopeNamesItsPackage) {
  EXPECT_EQ(m::boxOf("app@core@"), "app@core");
  EXPECT_EQ(m::boxOf("p@m"), "p@m");
  EXPECT_EQ(m::packageOf("app@core@m::C"), "app@core@");
  EXPECT_EQ(m::scopeIn("app@core"), "app@core@");
  EXPECT_TRUE(m::isQualified("m::C"));
  EXPECT_FALSE(m::isAbsolute("m::C"));
  EXPECT_TRUE(m::isAbsolute("p@m::C"));
}

TEST(TestSubjectOf, ABehaviourAndWhatIsInItNameTheirSubject) {
  EXPECT_EQ(m::behaviourId("p@m::C::run"), "p@m::C::run::sequence");
  EXPECT_EQ(m::behaviourId("p@m::C::run", "out"), "p@m::C::run::sequence/out");
  EXPECT_EQ(m::subjectOf("p@m::C::run::sequence/out::m2"), "p@m::C::run");
  EXPECT_EQ(m::behaviourOf("p@m::C::run::sequence/out::m2"), "p@m::C::run::sequence/out");
  EXPECT_EQ(m::scenarioOf("p@m::C::run::sequence/out::m2"), "out");
  EXPECT_EQ(m::scenarioOf("p@m::C::run::sequence::m2"), "");
  EXPECT_EQ(m::subjectOf("p@m::C::run"), "");
}

TEST(TestMachineSubjectOf, AMemberNamedStateIsNoStateMachine) {
  // the state machine of C and a member of C named state share an identity: only a known machine is cut
  EXPECT_EQ(m::machineSubjectOf(m::stateId("p@m::C")), "p@m::C");
  EXPECT_EQ(m::subjectOf("p@m::Live::state"), "");  // a method named state is not a behaviour
}

TEST(TestWithin, AMemberOrAScenarioLiesInside) {
  EXPECT_TRUE(m::within("p@m::C::run", "p@m::C"));
  EXPECT_TRUE(m::within("p@m::C::run::sequence/out", "p@m::C::run::sequence"));
  EXPECT_FALSE(m::within("p@m::Cart", "p@m::C"));
}

TEST(TestWithoutRevision, AnArtefactIdLosesItsRevisionAndNothingElseDoes) {
  EXPECT_EQ(m::withoutRevision("REQ-00001-2"), "REQ-00001");
  EXPECT_EQ(m::withoutRevision("DS-C-0001-0"), "DS-C-0001");
  EXPECT_EQ(m::withoutRevision("p@m::C"), "p@m::C");
  EXPECT_TRUE(m::hasRevision("REQ-00001-2"));
  EXPECT_FALSE(m::hasRevision("REQ-00001"));
  EXPECT_TRUE(m::isArtefactName("REQ-00001"));
  EXPECT_FALSE(m::isArtefactName("Greeter"));
}

TEST(TestArityId, ATemplateCarriesItsArity) { EXPECT_EQ(m::arityId("Index", 2), "Index<2>"); }

TEST(TestDiagramId, ADiagramIsNamedOnce) {
  EXPECT_EQ(m::diagramId("CL_Model"), "diagram:CL_Model");
  EXPECT_EQ(m::diagramId("diagram:CL_Model"), "diagram:CL_Model");
  EXPECT_EQ(m::diagramName("diagram:CL_Model"), "CL_Model");
}

}  // namespace
