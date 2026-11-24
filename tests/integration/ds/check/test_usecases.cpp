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

/// Use cases (REQ-00081-1, REQ-00082-1): a contract with its outcomes, realised by interface
/// operations, specialised by a specific one; over examples/hola, and over small workspaces that
/// break one rule each.
class TestUseCases : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void load(const std::string& text) {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    const auto file = parser.parseText("u.ds", text);
    ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
    binder.bind(file);
    const auto checkSteps = ds::check::StepFactory::of();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  }

  void loadExample() {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/hola")) binder.bind(f);
    const auto checkSteps = ds::check::StepFactory::of();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  }

  std::vector<std::string> codes() const {
    std::vector<std::string> out;
    for (const auto& f : model.findings()) out.push_back(f.code);
    for (const auto& f : model.coverage()) out.push_back(f.code);  // kept apart (DS-216)
    return out;
  }

  bool has(const std::string& code) const {
    for (const auto& c : codes())
      if (c == code) return true;
    return false;
  }

  bool related(const std::string& kind, const std::string& from, const std::string& to) const {
    for (const auto& r : model.index().all())
      if (r.kind == kind && r.from == from && r.to == to) return true;
    return false;
  }

  // a use case greet(name) with two outcomes; an interface operation realises it with what follows
  static std::string contract(const std::string& operation, const std::string& sequences, const std::string& extra = "") {
    return "package p {\n"
           "  usecase UC-00001-1 greet(name: string) -> int {\n"
           "    title: Greet by name\n"
           "    outcome default: a greeting\n"
           "    outcome empty-name: nobody is greeted\n"
           "  }\n" +
           extra +
           "  interface shell {\n"
           "    kind: cli\n" +
           operation + "  }\n" + sequences + "}\n";
  }
};

TEST_F(TestUseCases, HolaRealisesItsUseCasesFromBothInterfacesAndChecksClean) {
  loadExample();
  EXPECT_TRUE(related("realises", "hola@shell::greet", "UC-00001-1"));
  EXPECT_TRUE(related("realises", "hola@web::greet", "UC-00002-1"));
  EXPECT_TRUE(related("inherits", "UC-00002-1", "UC-00001-1"));  // the specific specialises the generic
  EXPECT_TRUE(related("satisfies", "UC-00001-1", "SREQ-00001-1"));
  const ds::model::Entity* outcome = model.find("UC-00001-1::empty-name");
  ASSERT_NE(outcome, nullptr);
  EXPECT_EQ(outcome->kind, ds::model::Kind::Outcome);
  EXPECT_EQ(outcome->fields.at("returns"), "none");
  EXPECT_TRUE(model.findings().empty()) << codes().front();
}

TEST_F(TestUseCases, AnOperationThatHandlesEveryOutcomeKeepsTheContract) {
  load(contract("    operation greet(name: string) -> int : UC-00001-1\n",
                "  sequence shell::greet {\n    return 0\n  }\n  sequence shell::greet / empty-name {\n    return 2\n  }\n"));
  EXPECT_FALSE(has("W-USECASE-OUTCOME-UNHANDLED"));
  EXPECT_FALSE(has("W-USECASE-SIGNATURE"));
  EXPECT_FALSE(has("W-SCENARIO-NO-OUTCOME"));
}

TEST_F(TestUseCases, AnOperationThatGivesSomethingElseBreaksTheContract) {
  load(contract("    operation greet(name: string) -> string : UC-00001-1\n",
                "  sequence shell::greet {\n  }\n  sequence shell::greet / empty-name {\n  }\n"));
  EXPECT_TRUE(has("W-USECASE-SIGNATURE"));
}

TEST_F(TestUseCases, AnOutcomeNoScenarioHandlesIsReported) {
  load(contract("    operation greet(name: string) -> int : UC-00001-1\n", "  sequence shell::greet {\n    return 0\n  }\n"));
  EXPECT_TRUE(has("W-USECASE-OUTCOME-UNHANDLED"));  // empty-name has no scenario
}

TEST_F(TestUseCases, AScenarioNoOutcomeNamesIsReported) {
  load(contract("    operation greet(name: string) -> int : UC-00001-1\n",
                "  sequence shell::greet {\n    return 0\n  }\n  sequence shell::greet / empty-name {\n    return 2\n  }\n"
                "  sequence shell::greet / too-long {\n    return 3\n  }\n"));
  EXPECT_TRUE(has("W-SCENARIO-NO-OUTCOME"));
}

TEST_F(TestUseCases, ASpecificUseCaseInheritsTheOutcomesOfItsGeneric) {
  // UC-2 adds nothing of its own: an operation that realises it still owes empty-name
  load(contract("    operation greet(name: string) -> int : UC-00002-1\n", "  sequence shell::greet {\n    return 0\n  }\n",
                "  usecase UC-00002-1 greet(name: string) -> int : UC-00001-1 {\n    title: Greet from a page\n  }\n"));
  EXPECT_TRUE(has("W-USECASE-OUTCOME-UNHANDLED"));
  EXPECT_FALSE(has("W-USECASE-UNREALISED"));  // the generic is realised through the specific
}

TEST_F(TestUseCases, AUseCaseNothingRealisesAndAnUnknownOneAreReported) {
  load(contract("    operation greet(name: string) -> int : UC-00009-1\n", "  sequence shell::greet {\n    return 0\n  }\n"));
  EXPECT_TRUE(has("W-USECASE-UNDECLARED"));  // UC-00009-1 does not exist
  EXPECT_TRUE(has("W-USECASE-UNREALISED"));  // and nothing realises UC-00001-1
}

TEST_F(TestUseCases, ADesignWithoutUseCasesOwesNone) {
  load(
      "package p {\n  interface shell {\n    kind: cli\n    operation greet(name: string) -> int\n  }\n"
      "  sequence shell::greet {\n    return 0\n  }\n}\n");
  for (const auto& code : codes()) EXPECT_EQ(code.find("USECASE"), std::string::npos) << code;
}

}  // namespace
