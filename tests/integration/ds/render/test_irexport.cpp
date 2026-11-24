#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/IrExport.hpp"
#include "ds/scene/Scene.hpp"

namespace {

/// A diagram is a projection of the model onto render-IR: selection, members, edges.
class TestIrExport : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/static")) binder.bind(f);
    const auto checkSteps = ds::check::StepFactory::of();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  }

  nlohmann::json render(const std::string& diagram) {
    return nlohmann::json::parse(ds::render::IrExport(model, metamodel).render("diagram:" + diagram, false));
  }

  static const nlohmann::json* node(const nlohmann::json& doc, const std::string& id) {
    for (const auto& n : doc["nodes"]) {
      if (n["id"] == id) return &n;
    }
    return nullptr;
  }

  static int edges(const nlohmann::json& doc, const std::string& kind) {
    int n = 0;
    for (const auto& e : doc["edges"]) n += e["kind"] == kind ? 1 : 0;
    return n;
  }
};

TEST_F(TestIrExport, ListsTheDeclaredDiagrams) {
  const auto ids = ds::render::IrExport(model, metamodel).diagrams();
  EXPECT_EQ(ids.size(), 5u);
  EXPECT_EQ(ids[0], "diagram:CL_Around_Model");  // alphabetical
}

TEST_F(TestIrExport, IncludeSelectsAndTheModuleBecomesAContainer) {
  const auto doc = render("CL_Model");
  EXPECT_EQ(doc["ir"], 1);
  ASSERT_NE(node(doc, "ds@ds"), nullptr);
  EXPECT_EQ((*node(doc, "ds@ds"))["kind"], "container");
  ASSERT_NE(node(doc, "ds@ds::Model"), nullptr);
  EXPECT_EQ((*node(doc, "ds@ds::Model"))["parent"], "ds@ds");
  EXPECT_EQ(node(doc, "ds@ds::Vector"), nullptr);  // not included
  EXPECT_EQ(node(doc, "ds@ds::Cache"), nullptr);   // expected nodes are never drawn
}

TEST_F(TestIrExport, DerivedStereotypesAndMembersArePrinted) {
  const auto doc = render("CL_Model");
  const nlohmann::json& check = *node(doc, "ds@ds::Check");
  EXPECT_EQ(check["classifier"], "class");  // every method abstract: an abstract class, never an interface (DS-301)
  EXPECT_EQ(check["abstract"], true);
  EXPECT_EQ(check["stereotype"], "abstract");
  EXPECT_EQ(check["sectionKinds"], nlohmann::json({"methods"}));
  EXPECT_EQ(check["sections"][0][0], "+ abstract name() -> std::string");
  const nlohmann::json& location = *node(doc, "ds@ds::Location");
  EXPECT_EQ(location["stereotype"], "value, equatable, ordered, printable");
  EXPECT_EQ(location["sections"][0][2], "+ column: int = 0");  // source order
}

TEST_F(TestIrExport, TheDiagramReadsARelationAsTheViewerDoes) {
  // the same relation in the scene the viewer draws and in the diagram: one reading (DS-118)
  std::string viewer;
  for (const auto& l : ds::scene::Scene(model, metamodel).build().links)
    if (l.from == "ds@ds::ScopedCheck" && l.to == "ds@ds::Check") viewer = l.kind;
  EXPECT_EQ(viewer, "inherits");
  std::string diagram;
  const auto doc = render("CL_Model");
  for (const auto& e : doc["edges"])
    if (e["from"] == "ds@ds::ScopedCheck" && e["to"] == "ds@ds::Check") diagram = e["kind"];
  EXPECT_EQ(diagram, "inheritance");
}

TEST_F(TestIrExport, EdgesAreLiftedFromMembersToClasses) {
  const auto doc = render("CL_Model");
  EXPECT_EQ(edges(doc, "realization"), 0);  // ScopedCheck -> Check: inheriting is drawn as written (DS-301)
  EXPECT_EQ(edges(doc, "inheritance"), 1);
  EXPECT_EQ(edges(doc, "composition"), 1);  // Finding owns Location, labelled with name and multiplicity
  EXPECT_EQ(edges(doc, "attachment"), 1);   // the note of Finding
  bool labelled = false;
  for (const auto& e : doc["edges"]) labelled |= e["kind"] == "composition" && e["label"] == "location 1";
  EXPECT_TRUE(labelled);
}

TEST_F(TestIrExport, AnEnumIsAValueSoItIsAssociatedNotOwned) {
  const auto doc = render("CL_Values");
  EXPECT_EQ((*node(doc, "ds@ds::Severity"))["classifier"], "enum");
  EXPECT_EQ(edges(doc, "association"), 1);
  EXPECT_EQ(edges(doc, "composition"), 1);
  EXPECT_EQ(node(doc, "ds@ds::Finding")->count("stereotype"), 0u);                            // nothing shown beyond members
  EXPECT_EQ((*node(doc, "ds@ds::Finding"))["sectionKinds"], nlohmann::json({"attributes"}));  // hide: methods
}

}  // namespace
