#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// The principles of the engine, as rules of its design (DS-365): the design of ds keeps every one, and the same
/// design with one deliberate violation written into it is told which rule it breaks.
class TestEnginePrinciples : public ::testing::Test {
 protected:
  /// A line to write into one file of ds's design, after the line that starts with `after`.
  struct Edit {
    std::string file;   ///< the file's path under design/, as ds/bind.ds
    std::string after;  ///< the start of the line it goes after; it must occur once
    std::string line;   ///< what is written, ending in a newline
  };

  /// The rules of ds's design that a model breaks: the rule's identity, once per finding.
  static std::vector<std::string> broken(const std::vector<Edit>& edits) {
    const ds::model::Metamodel metamodel;
    ds::model::Model model{metamodel};
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (auto file : parser.parseDirectory(std::string(DRIFTSPEC_SOURCE_DIR) + "/design")) {
      for (const Edit& edit : edits) {
        const std::string suffix = "/design/" + edit.file;
        if (file.path.size() < suffix.size() || file.path.compare(file.path.size() - suffix.size(), suffix.size(), suffix) != 0) continue;
        const std::size_t at = file.text.find("\n" + edit.after);
        EXPECT_NE(at, std::string::npos) << edit.after;
        EXPECT_EQ(file.text.find("\n" + edit.after, at + 1), std::string::npos) << edit.after << " occurs twice";
        const std::size_t end = file.text.find('\n', at + 1) + 1;
        file = parser.parseText(file.path, file.text.substr(0, end) + edit.line + file.text.substr(end));
      }
      binder.bind(file);
    }
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
    std::vector<std::string> out;
    for (const auto& f : model.findings())
      if (f.code == "E-RULE-BROKEN") out.push_back(f.message.substr(0, f.message.find(' ')));
    return out;
  }
  static bool has(const std::vector<std::string>& rules, const std::string& rule) {
    for (const auto& r : rules)
      if (r == rule) return true;
    return false;
  }

  const std::string context = "driftspec@ds@bind::binders-write-through-the-context";
  const std::string asInterface = "driftspec@ds@bind::binder-knows-binders-as-inodebinder";
  const std::string alone = "driftspec@ds@bind::no-binder-knows-another";
  const std::string builtOnce = "driftspec@ds@model::the-metamodel-is-built-once";
  const std::string binderClasses = "driftspec@ds@bind::binder-operations-belong-to-classes";
  const std::string modelClasses = "driftspec@ds@model::model-operations-belong-to-classes";
  const std::string axisClasses = "driftspec@ds@axis::axis-operations-belong-to-classes";
  const std::string axisReads = "driftspec@ds@axis::the-axis-reads-the-model";
  const std::string exporterClasses = "driftspec@ds@exporter::exporter-operations-belong-to-classes";
  const std::string exporterReads = "driftspec@ds@exporter::the-exporter-reads-the-model";
  const std::string apiChooses = "driftspec@ds@api::only-the-api-chooses-the-steps";
  const std::string machineClasses = "driftspec@ds@machine::machine-operations-belong-to-classes";
  const std::string machineReads = "driftspec@ds@machine::the-machine-reads-the-model";
};

TEST_F(TestEnginePrinciples, TheDesignOfDsKeepsEveryPrinciple) {
  const std::vector<std::string> rules = broken({});
  for (const std::string& rule : {context, asInterface, alone, builtOnce, binderClasses, modelClasses, axisClasses, axisReads,
                                  exporterClasses, exporterReads, apiChooses, machineClasses, machineReads})
    EXPECT_FALSE(has(rules, rule)) << rule;
}

TEST_F(TestEnginePrinciples, ABinderThatHoldsTheModelIsTold) {
  const std::vector<std::string> rules =
      broken({{"ds/bind.ds", "    class PackageBinder : INodeBinder {", "      - refs model_: model::Model\n"}});
  EXPECT_TRUE(has(rules, context));
  EXPECT_FALSE(has(rules, alone));
}

TEST_F(TestEnginePrinciples, TheBinderHoldingAConcreteBinderIsTold) {
  const std::vector<std::string> rules = broken({{"ds/bind.ds", "    class Binder {", "      - owns packages_: PackageBinder\n"}});
  EXPECT_TRUE(has(rules, asInterface));
}

TEST_F(TestEnginePrinciples, ABinderThatKnowsAnotherIsTold) {
  const std::vector<std::string> rules =
      broken({{"ds/bind.ds", "    class ClassBinder : INodeBinder {", "      - refs functions_: FunctionBinder\n"}});
  EXPECT_TRUE(has(rules, alone));
  EXPECT_FALSE(has(rules, asInterface));
}

TEST_F(TestEnginePrinciples, ACallFromOutsideTheModelToWhatBuildsTheMetamodelIsTold) {
  const std::vector<std::string> rules = broken({{"ds/bind.ds", "      local metamodel: model::Metamodel        // the context's",
                                                  "      self -> metamodel.buildCodes() -> codes\n"}});
  EXPECT_TRUE(has(rules, builtOnce));
}

TEST_F(TestEnginePrinciples, AFunctionOfTheBinderIsTold) {
  const std::vector<std::string> rules =
      broken({{"ds/bind.ds", "  module bind {", "    function loose(text: std::string) -> std::string\n"}});
  EXPECT_TRUE(has(rules, binderClasses));
  EXPECT_FALSE(has(rules, modelClasses));
}

TEST_F(TestEnginePrinciples, AFunctionOfTheModelIsTold) {
  const std::vector<std::string> rules =
      broken({{"ds/model.ds", "  module model {", "    function loose(text: std::string) -> std::string\n"}});
  EXPECT_TRUE(has(rules, modelClasses));
  EXPECT_FALSE(has(rules, binderClasses));
}

TEST_F(TestEnginePrinciples, AFunctionOfTheAxisIsTold) {
  const std::vector<std::string> rules =
      broken({{"ds/axis.ds", "  module axis {", "    function loose(text: std::string) -> std::string\n"}});
  EXPECT_TRUE(has(rules, axisClasses));
  EXPECT_FALSE(has(rules, axisReads));
}

TEST_F(TestEnginePrinciples, AnAxisThatChangesTheModelIsTold) {
  const std::vector<std::string> rules = broken({{"ds/axis.ds", "  module axis {",
                                                  "    class Marker {\n"
                                                  "      - refs model_: model::Model\n"
                                                  "      + mark(e: model::Entity) -> void\n"
                                                  "    }\n"
                                                  "    sequence Marker::mark {\n"
                                                  "      self -> model_.declare(e)\n"
                                                  "    }\n"}});
  EXPECT_TRUE(has(rules, axisReads));
  EXPECT_FALSE(has(rules, axisClasses));
}

TEST_F(TestEnginePrinciples, AFunctionOfTheExporterIsTold) {
  const std::vector<std::string> rules =
      broken({{"ds/exporter.ds", "  module exporter {", "    function loose(text: std::string) -> std::string\n"}});
  EXPECT_TRUE(has(rules, exporterClasses));
  EXPECT_FALSE(has(rules, exporterReads));
}

TEST_F(TestEnginePrinciples, AnExporterThatChangesTheModelIsTold) {
  const std::vector<std::string> rules =
      broken({{"ds/exporter.ds", "    sequence JsonExport::render {", "      local e: model::Entity\n      self -> model.declare(e)\n"}});
  EXPECT_TRUE(has(rules, exporterReads));
  EXPECT_FALSE(has(rules, exporterClasses));
}

/// Only the Api chooses which steps of the check a load runs (DS-376): a function of the command line or a class of
/// the drawing that names the selection is told, and so is a behaviour outside the Api that makes the steps itself.
TEST_F(TestEnginePrinciples, ASelectionOfStepsNamedOutsideTheApiIsTold) {
  const std::vector<std::string> byFunction = broken({{"ds/app.ds", "    - function diagramId(name: std::string) -> std::string",
                                                       "    - function drawingSteps(format: std::string) -> check::Selection\n"}});
  EXPECT_TRUE(has(byFunction, apiChooses));
  const std::vector<std::string> byClass = broken({{"ds/render.ds", "    class IrExport {", "      - refs steps_: check::Selection\n"}});
  EXPECT_TRUE(has(byClass, apiChooses));
  EXPECT_FALSE(has(byClass, axisReads));
}

TEST_F(TestEnginePrinciples, StepsMadeOutsideTheApiAreTold) {
  const std::vector<std::string> rules =
      broken({{"ds/app.ds", "      -> parser.parseDirectory(dir) -> files", "      -> check::steps(selection) -> steps\n"}});
  EXPECT_TRUE(has(rules, apiChooses));
  EXPECT_FALSE(has(rules, builtOnce));
}

TEST_F(TestEnginePrinciples, AFunctionOfTheMachineIsTold) {
  const std::vector<std::string> rules =
      broken({{"ds/machine.ds", "  module machine {", "    function loose(text: std::string) -> std::string\n"}});
  EXPECT_TRUE(has(rules, machineClasses));
  EXPECT_FALSE(has(rules, machineReads));
}

TEST_F(TestEnginePrinciples, AMachineThatChangesTheModelIsTold) {
  const std::vector<std::string> rules = broken({{"ds/machine.ds", "  module machine {",
                                                  "    class Marker {\n"
                                                  "      - refs model_: model::Model\n"
                                                  "      + mark(e: model::Entity) -> void\n"
                                                  "    }\n"
                                                  "    sequence Marker::mark {\n"
                                                  "      self -> model_.declare(e)\n"
                                                  "    }\n"}});
  EXPECT_TRUE(has(rules, machineReads));
  EXPECT_FALSE(has(rules, machineClasses));
}

}  // namespace
