#include <gtest/gtest.h>

#include "ds/model/Identity.hpp"

namespace {

using ds::model::ArtefactName;
using ds::model::Identity;
using ds::model::Scope;

/// How identities are written, in one place (DS-124), as three values (DS-184): packages join with @,
/// the rest with ::, a behaviour is its subject's identity with ::sequence[/scenario] or ::state, a
/// template carries its arity, a diagram is diagram:NAME.

TEST(TestIdentity, ItsLastNameIsTheNameAfterTheLastSeparator) {
  EXPECT_EQ(Identity("app@core@model::Entity").lastName(), "Entity");
  EXPECT_EQ(Identity("app@core").lastName(), "core");
  EXPECT_EQ(Identity("app").lastName(), "app");
}

TEST(TestIdentity, ItIsWrittenInTheScopeOutsideIt) {
  EXPECT_EQ(Identity("p@m::C").scope(), Scope("p@m"));
  EXPECT_EQ(Identity("p@m").scope(), Scope("p@"));
  EXPECT_EQ(Identity("p@m::C").asScope().qualify("run"), Identity("p@m::C::run"));
}

TEST(TestIdentity, ItIsQualifiedByItsPathAndAbsoluteFromAPackage) {
  EXPECT_TRUE(Identity("m::C").isQualified());
  EXPECT_FALSE(Identity("C").isQualified());
  EXPECT_FALSE(Identity("m::C").isAbsolute());
  EXPECT_TRUE(Identity("p@m::C").isAbsolute());
}

TEST(TestIdentity, AMemberOrAScenarioLiesWithin) {
  EXPECT_TRUE(Identity("p@m::C::run").within(Identity("p@m::C")));
  EXPECT_TRUE(Identity("p@m::C::run::sequence/out").within(Identity("p@m::C::run::sequence")));
  EXPECT_FALSE(Identity("p@m::Cart").within(Identity("p@m::C")));
}

TEST(TestIdentity, ATemplateCarriesItsArity) { EXPECT_EQ(Identity("Index").withArity(2), Identity("Index<2>")); }

TEST(TestIdentity, ADiagramIsNamedOnce) {
  EXPECT_EQ(Identity::ofDiagram("CL_Model"), Identity("diagram:CL_Model"));
  EXPECT_EQ(Identity::ofDiagram("diagram:CL_Model"), Identity("diagram:CL_Model"));
  EXPECT_EQ(Identity("diagram:CL_Model").diagramName(), "CL_Model");
  EXPECT_EQ(Identity("CL_Model").diagramName(), "CL_Model");
}

TEST(TestIdentity, ItsBehavioursAreItsSequenceAndItsStateMachine) {
  EXPECT_EQ(Identity("p@m::C::run").sequence(), Identity("p@m::C::run::sequence"));
  EXPECT_EQ(Identity("p@m::C::run").sequence("out"), Identity("p@m::C::run::sequence/out"));
  EXPECT_EQ(Identity("p@m::C::run::sequence").withScenario("out"), Identity("p@m::C::run::sequence/out"));
  EXPECT_EQ(Identity("p@m::C").state(), Identity("p@m::C::state"));
}

TEST(TestIdentity, ABehaviourAndWhatIsInItNameTheirSubject) {
  const Identity message("p@m::C::run::sequence/out::m2");
  EXPECT_EQ(message.subject(), Identity("p@m::C::run"));
  EXPECT_EQ(message.behaviour(), Identity("p@m::C::run::sequence/out"));
  EXPECT_EQ(message.scenario(), "out");
  EXPECT_EQ(Identity("p@m::C::run::sequence::m2").scenario(), "");
  EXPECT_TRUE(Identity("p@m::C::run").subject().empty());
  EXPECT_TRUE(Identity("p@m::C::run").behaviour().empty());
}

TEST(TestIdentity, AMemberNamedStateIsNoStateMachine) {
  // the state machine of C and a member of C named state share an identity: only a known machine is cut
  EXPECT_EQ(Identity("p@m::C").state().machineSubject(), Identity("p@m::C"));
  EXPECT_TRUE(Identity("p@m::Live::state").subject().empty());  // a method named state is not a behaviour
  EXPECT_TRUE(Identity("p@m::C::run").machineSubject().empty());
}

TEST(TestScope, APackageJoinsWithAtAndTheRestWithColons) {
  EXPECT_EQ(Scope("p@").qualify("C"), Identity("p@C"));
  EXPECT_EQ(Scope("p@m").qualify("C"), Identity("p@m::C"));
  EXPECT_EQ(Scope("").qualify("p"), Identity("p"));
}

TEST(TestScope, EachStepOutLeavesOneScope) {
  EXPECT_EQ(Scope("p@m::C").outer(), Scope("p@m"));
  EXPECT_EQ(Scope("p@m").outer(), Scope("p@"));
  EXPECT_EQ(Scope("app@core@").outer(), Scope("app@"));
  EXPECT_TRUE(Scope("p@").outer().empty());
}

TEST(TestScope, APackageScopeNamesItsPackageAndAPackageHasOneInside) {
  EXPECT_EQ(Scope("app@core@").box(), Identity("app@core"));
  EXPECT_EQ(Scope("p@m").box(), Identity("p@m"));
  EXPECT_EQ(Scope("app@core@m::C").package(), Scope("app@core@"));
  EXPECT_TRUE(Scope("C").package().empty());
  EXPECT_EQ(Scope::inside(Identity("app@core")), Scope("app@core@"));
  EXPECT_EQ(Scope::inside(Identity("app@core@")), Scope("app@core@"));
}

TEST(TestScope, OnlyTheOutermostPackageIsTheSystem) {
  EXPECT_TRUE(Scope("app@").isSystem());
  EXPECT_FALSE(Scope("app@core@").isSystem());  // an inner package: its own level
  EXPECT_FALSE(Scope("app@core").isSystem());   // a module's scope is no package scope
  EXPECT_FALSE(Scope("").isSystem());
}

TEST(TestArtefactName, AnArtefactIsNamedPrefixNumberAndRevision) {
  EXPECT_TRUE(ArtefactName::matches("REQ-00001"));
  EXPECT_TRUE(ArtefactName::matches("DS-C-0001-0"));
  EXPECT_FALSE(ArtefactName::matches("Greeter"));
  EXPECT_TRUE(ArtefactName("REQ-00001-2").hasRevision());
  EXPECT_FALSE(ArtefactName("REQ-00001").hasRevision());
}

TEST(TestArtefactName, OnlyLetterPartsThenNumberPartsBetweenSingleDashesAreAnArtefact) {
  // the edges the hand-written recogniser walks, held to what the pattern it replaces accepted (DS-335)
  EXPECT_TRUE(ArtefactName::matches("A-1"));
  EXPECT_TRUE(ArtefactName::matches("A-B-C-1-2"));
  EXPECT_FALSE(ArtefactName::matches(""));
  EXPECT_FALSE(ArtefactName::matches("REQ"));         // no number
  EXPECT_FALSE(ArtefactName::matches("00001-2"));     // no letter part
  EXPECT_FALSE(ArtefactName::matches("-REQ-00001"));  // a leading dash
  EXPECT_FALSE(ArtefactName::matches("REQ-00001-"));  // a trailing dash
  EXPECT_FALSE(ArtefactName::matches("REQ--00001"));  // a doubled dash
  EXPECT_FALSE(ArtefactName::matches("Req-00001"));   // lowercase
  EXPECT_FALSE(ArtefactName::matches("REQ1-00001"));  // a part both letters and digits
  EXPECT_FALSE(ArtefactName::matches("REQ-1-DS-2"));  // letters after a number
  EXPECT_FALSE(ArtefactName::matches("REQ-1-2-3"));   // three numbers
  EXPECT_FALSE(ArtefactName::matches("REQ-00001 "));  // a trailing space
  EXPECT_FALSE(ArtefactName("A-1").hasRevision());
  EXPECT_TRUE(ArtefactName("A-1-0").hasRevision());
  EXPECT_EQ(ArtefactName("A-1-0").withoutRevision(), ArtefactName("A-1"));
  EXPECT_EQ(ArtefactName("REQ-1-2-3").withoutRevision(), ArtefactName("REQ-1-2-3"));  // not an artefact: kept
}

TEST(TestArtefactName, ItLosesItsRevisionAndNothingElseDoes) {
  EXPECT_EQ(ArtefactName("REQ-00001-2").withoutRevision(), ArtefactName("REQ-00001"));
  EXPECT_EQ(ArtefactName("DS-C-0001-0").withoutRevision(), ArtefactName("DS-C-0001"));
  EXPECT_EQ(ArtefactName("p@m::C").withoutRevision(), ArtefactName("p@m::C"));
}

}  // namespace
