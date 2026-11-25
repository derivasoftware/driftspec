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
    ds::check::CheckerFactory::make(model, metamodel)->run(binder.notes());
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
  const std::string queryClasses = "driftspec@ds@query::query-operations-belong-to-classes";
  const std::string stepsAsInterface = "driftspec@ds@check::checker-knows-steps-as-istep";
  const std::string stepsAlone = "driftspec@ds@check::no-step-knows-another";
  const std::string checkClasses = "driftspec@ds@check::check-operations-belong-to-classes";
  const std::string sceneClasses = "driftspec@ds@scene::scene-operations-belong-to-classes";
  const std::string sceneReads = "driftspec@ds@scene::the-scene-reads-the-model";
  const std::string layerLayout = "driftspec@ds@scene::no-layer-knows-the-layout";
  const std::string packerModel = "driftspec@ds@layout::layout-knows-no-model";
  const std::string layoutRender = "driftspec@ds@layout::layout-knows-no-render";
  const std::string layoutScene = "driftspec@ds@layout::layout-knows-no-scene";
  const std::string sceneEngine = "driftspec@ds@layout::the-scene-places-through-iengine";
  const std::string renderEngine = "driftspec@ds@layout::render-places-through-iengine";
  const std::string renderClasses = "driftspec@ds@render::render-operations-belong-to-classes";
  const std::string projectors = "driftspec@ds@render::projectors-reach-each-other-through-iprojector";
  const std::string drawingsModel = "driftspec@ds@render::the-drawings-read-only-the-ir";
  const std::string drawingsQuery = "driftspec@ds@render::the-drawings-ask-no-query";
  const std::string drawingsAxis = "driftspec@ds@render::the-drawings-know-no-cut";
  const std::string layersAlone = "driftspec@ds@scene::no-layer-knows-another";
  const std::string namedI = "driftspec@ds@pure-interfaces-are-named-i";
  const std::string namedA = "driftspec@ds@abstract-classes-are-named-a";
  const std::string namedFactory = "driftspec@ds@factories-are-named-factory";
  const std::string throughInterface = "driftspec@ds@families-are-reached-through-their-interface";
  const std::string givenAtConstruction = "driftspec@ds@what-a-class-refers-to-is-given-to-it";
};

TEST_F(TestEnginePrinciples, TheDesignOfDsKeepsEveryPrinciple) {
  const std::vector<std::string> rules = broken({});
  for (const std::string& rule :
       {context,          asInterface,     alone,         builtOnce,        binderClasses,       modelClasses,  axisClasses,
        axisReads,        exporterClasses, exporterReads, apiChooses,       machineClasses,      machineReads,  queryClasses,
        stepsAsInterface, stepsAlone,      checkClasses,  sceneClasses,     sceneReads,          layerLayout,   packerModel,
        namedI,           namedA,          namedFactory,  throughInterface, givenAtConstruction, layoutRender,  layoutScene,
        sceneEngine,      renderEngine,    renderClasses, projectors,       drawingsModel,       drawingsQuery, drawingsAxis})
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

/// A function of the query is told, a public one or one hidden in the module (DS-385): which filter a diagram has, how
/// a pattern reads and how an entity is answered are each a class's.
TEST_F(TestEnginePrinciples, AFunctionOfTheQueryIsTold) {
  const std::vector<std::string> loose =
      broken({{"ds/query.ds", "  module query {", "    function filterFor(diagram: model::Entity) -> std::string\n"}});
  EXPECT_TRUE(has(loose, queryClasses));
  EXPECT_FALSE(has(loose, axisClasses));
  const std::vector<std::string> hidden =
      broken({{"ds/query.ds", "  module query {", "    - function inPattern(pattern: std::string, entity: model::Entity) -> bool\n"}});
  EXPECT_TRUE(has(hidden, queryClasses));
  EXPECT_FALSE(has(hidden, modelClasses));
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
      broken({{"ds/app.ds", "      -> parser.parseDirectory(dir) -> files", "      -> check::StepFactory::of(selection) -> steps\n"}});
  EXPECT_TRUE(has(rules, apiChooses));
  EXPECT_FALSE(has(rules, builtOnce));
}

/// The check is a runner of steps (DS-383): the checker knows each step only as IStep, no step knows another, and
/// every operation of the check is a member of a class.
TEST_F(TestEnginePrinciples, TheCheckerHoldingAConcreteStepIsTold) {
  const std::vector<std::string> rules = broken({{"ds/check.ds", "    class Checker : IChecker {", "      - owns linker_: Linker\n"}});
  EXPECT_TRUE(has(rules, stepsAsInterface));
  EXPECT_FALSE(has(rules, stepsAlone));
}

TEST_F(TestEnginePrinciples, AStepThatKnowsAnotherIsTold) {
  const std::vector<std::string> rules =
      broken({{"ds/check.ds", "    class InterfaceContracts : ACheck {", "      - refs usecases_: UseCaseContracts\n"}});
  EXPECT_TRUE(has(rules, stepsAlone));
  EXPECT_FALSE(has(rules, stepsAsInterface));
}

TEST_F(TestEnginePrinciples, AFunctionOfTheCheckIsTold) {
  const std::vector<std::string> rules =
      broken({{"ds/check.ds", "  module check {", "    function loose(text: std::string) -> std::string\n"}});
  EXPECT_TRUE(has(rules, checkClasses));
  EXPECT_FALSE(has(rules, machineClasses));
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

TEST_F(TestEnginePrinciples, AFunctionOfTheSceneIsTold) {
  const std::vector<std::string> rules =
      broken({{"ds/scene.ds", "  module scene {", "    function loose(text: std::string) -> std::string\n"}});
  EXPECT_TRUE(has(rules, sceneClasses));
  EXPECT_FALSE(has(rules, sceneReads));
}

TEST_F(TestEnginePrinciples, ASceneThatChangesTheModelIsTold) {
  const std::vector<std::string> rules = broken({{"ds/scene.ds", "  module scene {",
                                                  "    class Marker {\n"
                                                  "      - refs model_: model::Model\n"
                                                  "      + mark(e: model::Entity) -> void\n"
                                                  "    }\n"
                                                  "    sequence Marker::mark {\n"
                                                  "      self -> model_.declare(e)\n"
                                                  "    }\n"}});
  EXPECT_TRUE(has(rules, sceneReads));
  EXPECT_FALSE(has(rules, sceneClasses));
}

TEST_F(TestEnginePrinciples, ALayerThatKnowsTheLayoutIsTold) {
  const std::vector<std::string> rules = broken({{"ds/scene.ds", "    class StructureLayer : ILayer {", "      - refs layout_: Layout\n"}});
  EXPECT_TRUE(has(rules, layerLayout));
  EXPECT_FALSE(has(rules, layersAlone));  // the layout is no layer
  const std::vector<std::string> engine =
      broken({{"ds/scene.ds", "    class StructureLayer : ILayer {", "      - refs engine_: layout::IEngine\n"}});
  EXPECT_TRUE(has(engine, layerLayout));  // every class of the layout module places boxes
}

TEST_F(TestEnginePrinciples, APackerThatKnowsTheModelIsTold) {
  const std::vector<std::string> rules =
      broken({{"ds/layout.ds", "    class PackingEngine : IEngine {", "      - refs model_: model::Model\n"}});
  EXPECT_TRUE(has(rules, packerModel));
  EXPECT_FALSE(has(rules, sceneReads));  // holding the model is not changing it
}

/// The Owner's conventions (DS-405): what a class is named says what it is, a family is reached through its
/// interface, and what a class refers to is given to it when it is made. Each is told when it is broken, and only it.
TEST_F(TestEnginePrinciples, AnInterfaceNamedOtherwiseIsTold) {
  const std::vector<std::string> rules = broken({{"ds/check.ds", "  module check {",
                                                  "    class Shape {\n"
                                                  "      + abstract area() -> int\n"
                                                  "    }\n"}});
  EXPECT_TRUE(has(rules, namedI));
  EXPECT_FALSE(has(rules, namedA));
}

TEST_F(TestEnginePrinciples, AnAbstractClassNamedOtherwiseIsTold) {
  const std::vector<std::string> rules = broken({{"ds/check.ds", "  module check {",
                                                  "    class Shape {\n"
                                                  "      + abstract area() -> int\n"
                                                  "      + name() -> std::string\n"
                                                  "    }\n"}});
  EXPECT_TRUE(has(rules, namedA));
  EXPECT_FALSE(has(rules, namedI));
}

TEST_F(TestEnginePrinciples, AFactoryNamedOtherwiseIsTold) {
  const std::vector<std::string> rules = broken({{"ds/check.ds", "  module check {",
                                                  "    class Maker {\n"
                                                  "      - owns linker_: Linker\n"
                                                  "      - owns derivation_: Derivation\n"
                                                  "    }\n"}});
  EXPECT_TRUE(has(rules, namedFactory));
  EXPECT_FALSE(has(rules, throughInterface));  // a factory may name the family it makes
}

TEST_F(TestEnginePrinciples, AConcreteClassOfAFamilyReachedFromOutsideIsTold) {
  const std::vector<std::string> rules = broken({{"ds/render.ds", "    class IrExport {", "      - owns classes_: query::ClassFilter\n"}});
  EXPECT_TRUE(has(rules, throughInterface));
  EXPECT_FALSE(has(rules, namedFactory));
}

/// A factory is exempt by its name, so a factory of one product names it in the design (DS-451); a class that is no
/// factory and names one concrete class of a family is still told.
TEST_F(TestEnginePrinciples, OneConcreteNamedByAClassThatIsNoFactoryIsTold) {
  const std::vector<std::string> rules = broken({{"ds/emit.ds", "  module emit {",
                                                  "    class Writer {\n"
                                                  "      + write() -> void\n"
                                                  "    }\n"
                                                  "    sequence Writer::write {\n"
                                                  "      local sink: JsonSink\n"
                                                  "    }\n"}});
  EXPECT_TRUE(has(rules, throughInterface));
  EXPECT_FALSE(has(rules, namedFactory));  // one product: no family is named twice
}

/// A family rooted in an abstract class with concrete members is reached through it too (DS-465): format's
/// descriptions are rooted in ADescription, and a class that is no factory and names one of them is told.
TEST_F(TestEnginePrinciples, AConcreteOfAFamilyRootedInAnAbstractClassReachedFromOutsideIsTold) {
  const std::vector<std::string> rules = broken({{"ds/format.ds", "  module format {",
                                                  "    class Printer {\n"
                                                  "      + print() -> void\n"
                                                  "    }\n"
                                                  "    sequence Printer::print {\n"
                                                  "      local d: ReportDescription\n"
                                                  "    }\n"}});
  EXPECT_TRUE(has(rules, throughInterface));
  EXPECT_FALSE(has(rules, namedFactory));  // one concrete named: no factory
}

TEST_F(TestEnginePrinciples, AReferenceNoConstructorGivesIsTold) {
  const std::vector<std::string> rules = broken({{"ds/check.ds", "  module check {",
                                                  "    class Holder {\n"
                                                  "      - refs model_: model::Model\n"
                                                  "    }\n"}});
  EXPECT_TRUE(has(rules, givenAtConstruction));
  EXPECT_FALSE(has(rules, throughInterface));
}

/// The layout depends on nothing of render or the scene (DS-392): a class of the layout that knows either is told.
TEST_F(TestEnginePrinciples, ALayoutThatKnowsRenderOrTheSceneIsTold) {
  const std::vector<std::string> render =
      broken({{"ds/layout.ds", "    class PackingEngine : IEngine {", "      - refs ir_: render::IrDocument\n"}});
  EXPECT_TRUE(has(render, layoutRender));
  EXPECT_FALSE(has(render, layoutScene));
  EXPECT_FALSE(has(render, packerModel));  // the IR is no part of the model
  const std::vector<std::string> scene =
      broken({{"ds/layout.ds", "    class PackingEngine : IEngine {", "      - refs doc_: scene::SceneDocument\n"}});
  EXPECT_TRUE(has(scene, layoutScene));
  EXPECT_FALSE(has(scene, layoutRender));
}

/// Whoever places reaches an engine only through IEngine (DS-392): the scene holding a concrete engine is told, and
/// so is render holding a phase of the layered engine, which no family names.
TEST_F(TestEnginePrinciples, APlacerThatHoldsAnEngineOrAPhaseIsTold) {
  const std::vector<std::string> engine = broken({{"ds/scene.ds", "    class Layout {", "      - refs engine_: layout::PackingEngine\n"}});
  EXPECT_TRUE(has(engine, sceneEngine));
  EXPECT_FALSE(has(engine, renderEngine));
  const std::vector<std::string> phase = broken({{"ds/render.ds", "    class Layout {", "      - refs framer_: layout::Framer\n"}});
  EXPECT_TRUE(has(phase, renderEngine));
  EXPECT_FALSE(has(phase, throughInterface));  // a phase is in no family: only the layout's rule sees it
}

/// Every operation of render is a member of a class (DS-372): a function written in render is told.
TEST_F(TestEnginePrinciples, AFunctionOfRenderIsTold) {
  const std::vector<std::string> rules =
      broken({{"ds/render.ds", "    @requirement REQ-00007-1                 // the IR drawn as SVG and as terminal text\n",
               "    function stray() -> int\n"}});
  EXPECT_TRUE(has(rules, renderClasses));
  EXPECT_FALSE(has(rules, projectors));
}

/// A projector is reached through IProjector (DS-372): the tour holding the sequence's projector is told, which the
/// family rule allows a sibling, and so is the export holding the class diagram's.
TEST_F(TestEnginePrinciples, AProjectorHeldByAnotherClassOfRenderIsTold) {
  const std::vector<std::string> tour =
      broken({{"ds/render.ds", "    class TourProjector : IProjector {\n", "      - owns sequence_: SequenceProjector\n"}});
  EXPECT_TRUE(has(tour, projectors));
  EXPECT_FALSE(has(tour, throughInterface));  // a sibling of the family: only render's rule sees it
  const std::vector<std::string> exporter = broken({{"ds/render.ds", "    class IrExport {\n", "      - owns boxes_: ClassProjector\n"}});
  EXPECT_TRUE(has(exporter, projectors));
}

/// The drawings read the IR alone (DS-372): Svg knowing the model, Text a filter and the layout a cut are each told.
TEST_F(TestEnginePrinciples, ADrawingThatKnowsTheModelIsTold) {
  const std::vector<std::string> model =
      broken({{"ds/render.ds", "    class Svg {                              // the SVG drawing, with its links, tooltips and script\n",
               "      - refs model_: model::Model\n"}});
  EXPECT_TRUE(has(model, drawingsModel));
  const std::vector<std::string> query =
      broken({{"ds/render.ds", "    class Text {                             // the terminal drawing, box characters and optional colour\n",
               "      - refs filter_: query::IFilter\n"}});
  EXPECT_TRUE(has(query, drawingsQuery));
  const std::vector<std::string> axis = broken({{"ds/render.ds", "    class Layout {\n", "      - refs cut_: axis::Cut\n"}});
  EXPECT_TRUE(has(axis, drawingsAxis));
  EXPECT_FALSE(has(axis, drawingsModel));
}

}  // namespace
