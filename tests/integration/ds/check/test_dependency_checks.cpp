#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// What a box declares it uses, compared with what its behaviours call (REQ-00094-2).
class TestDependencyChecks : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void load(const std::string& consumer) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    const auto file = parser.parseText("a.ds",
                                       "package p {\n"
                                       "  interface engine {\n"
                                       "    kind: library\n"
                                       "    operation check(text: string) -> string\n"
                                       "  }\n"
                                       "  sequence engine::check {\n"
                                       "    local api: core::Api\n"
                                       "    -> api.check(text) -> json\n"
                                       "    return json\n"
                                       "  }\n"
                                       "  module core {\n"
                                       "    class Api {\n"
                                       "      @interface engine\n"
                                       "      + check(text: string) -> string\n"
                                       "    }\n"
                                       "  }\n"
                                       "}\n" +
                                           consumer);
    ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
    binder.bind(file);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, metamodel, checkSteps).run(binder);
  }

  std::vector<std::string> uses() const {  // the findings about dependencies, by code
    std::vector<std::string> out;
    for (const auto& f : model.findings())
      if (f.code.rfind("W-CONSUMES", 0) == 0) out.push_back(f.code);
    return out;
  }

  static std::string face(const std::string& declared, const std::string& call) {
    return "package q {\n"
           "  module face {\n" +
           declared +
           "    function check(text: string) -> string\n"
           "    sequence check {\n" +
           call +
           "      return json\n"
           "    }\n"
           "  }\n"
           "}\n";
  }
};

TEST_F(TestDependencyChecks, ACallToAnotherBoxsInterfaceNeedsAUseOnTheCallerOrWhereItLives) {
  load(face("", "      local ds: p@engine\n      -> ds.check(text) -> json\n"));
  EXPECT_EQ(uses(), std::vector<std::string>{"W-CONSUMES-UNDECLARED"});
}

TEST_F(TestDependencyChecks, AUseOnTheModuleOrInAWiringBlockOnItsPackageCoversTheCall) {
  load(face("    @consumes p@engine\n", "      local ds: p@engine\n      -> ds.check(text) -> json\n"));
  EXPECT_TRUE(uses().empty());
  ds::model::Model again{metamodel};
  model = std::move(again);
  load(face("", "      local ds: p@engine\n      -> ds.check(text) -> json\n") + "package q {\n  @consumes p@engine\n}\n");
  EXPECT_TRUE(uses().empty());
}

TEST_F(TestDependencyChecks, AUseNoBehaviourCallsIsDeadOnceTheBoxHasBehaviours) {
  load(face("    @consumes p@engine\n", "      local json: string\n"));
  EXPECT_EQ(uses(), std::vector<std::string>{"W-CONSUMES-UNUSED"});
  ds::model::Model again{metamodel};
  model = std::move(again);
  load("package q {\n  @consumes p@engine\n}\n");  // drawn before any behaviour: a plan, not yet a claim
  EXPECT_TRUE(uses().empty());
}

TEST_F(TestDependencyChecks, CallingTheClassBehindAnInterfaceFromOutsideGoesAroundIt) {
  load(face("", "      local api: p@core::Api\n      -> api.check(text) -> json\n"));
  EXPECT_EQ(uses(), std::vector<std::string>{"W-CONSUMES-BYPASS"});
}

TEST_F(TestDependencyChecks, AMethodTheInterfaceDoesNotOfferIsNoBypass) {
  // Api realises engine, and reset is not one of engine's operations: it cannot be called through it (DS-106)
  model = ds::model::Model(metamodel);
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  const auto file = parser.parseText("a.ds",
                                     "package p {\n"
                                     "  interface engine {\n"
                                     "    kind: library\n"
                                     "    operation check(text: string) -> string\n"
                                     "  }\n"
                                     "  sequence engine::check {\n"
                                     "    local api: core::Api\n"
                                     "    -> api.check(text) -> json\n"
                                     "    return json\n"
                                     "  }\n"
                                     "  module core {\n"
                                     "    class Api {\n"
                                     "      @interface engine\n"
                                     "      + check(text: string) -> string\n"
                                     "      + reset() -> none\n"
                                     "    }\n"
                                     "  }\n"
                                     "}\n" +
                                         face("", "      local api: p@core::Api\n      -> api.reset()\n      local json: string\n"));
  ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
  binder.bind(file);
  const auto checkSteps = ds::check::steps();
  ds::check::Checker(model, metamodel, checkSteps).run(binder);
  EXPECT_TRUE(uses().empty());
}

TEST_F(TestDependencyChecks, AnInterfacesOwnSequenceRealisesItAndNothingIsReported) {
  load("");
  EXPECT_TRUE(uses().empty());
}

TEST(TestRealisation, AMethodThatAnswersAnInterfaceOperationWithAnotherSignatureIsReported) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  binder.bind(parser.parseText("a.ds",
                               "package zoo {\n"
                               "  interface keeper {\n"
                               "    kind: library\n"
                               "    operation feed(animal: std::string) -> void\n"
                               "    operation clean(cage: int) -> void\n"
                               "    operation count(names: std::vector<std::string>) -> int\n"
                               "  }\n"
                               "  module animals {\n"
                               "    class Staff {\n"
                               "      @interface keeper\n"
                               "      + feed(animal: int, extra: bool) -> void\n"
                               "      + count(names: std::vector<std::string>) -> int\n"
                               "    }\n"
                               "  }\n"
                               "}\n"));
  const auto checkSteps = ds::check::steps();
  ds::check::Checker(model, metamodel, checkSteps).run(binder);
  std::vector<std::string> found;
  for (const auto& f : model.findings())
    if (f.code == "W-INTERFACE-SIGNATURE") found.push_back(f.message);
  ASSERT_EQ(found.size(), 1u);  // clean, which Staff does not declare, is no finding; count says the same
  EXPECT_EQ(found[0].rfind("zoo@animals::Staff::feed", 0), 0u);
}

}  // namespace
