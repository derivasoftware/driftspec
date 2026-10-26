#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/IrExport.hpp"

namespace {

/// The requirement view: the umbrella of an artefact, the tree of a package, artefacts on a class.
class TestRequirementsView : public ::testing::Test {
 protected:
  ds::model::Model model;

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/artefacts")) binder.bind(f);
    ds::check::Checker(model).run(binder);
  }

  nlohmann::json diagram(const std::string& name) {
    return nlohmann::json::parse(ds::render::IrExport(model).render("diagram:" + name, false));
  }

  static int count(const nlohmann::json& doc, const char* what, const std::string& value) {
    int n = 0;
    for (const auto& item : doc[std::string(what) == "classifier" ? "nodes" : "edges"]) n += item.value(what, "") == value;
    return n;
  }
};

TEST_F(TestRequirementsView, TheUmbrellaOfARequirement) {
  const auto doc = diagram("REQ_Tracker");
  EXPECT_EQ(count(doc, "classifier", "requirement"), 5);   // SREQ-00001-2 and the four that refine it (drift included)
  EXPECT_EQ(count(doc, "classifier", "verification"), 3);  // VER-00050-1, VER-00001-3, VER-00006-1
  EXPECT_EQ(count(doc, "classifier", "class"), 1);         // Projection realises REQ-00001-2
  EXPECT_EQ(count(doc, "label", "refines"), 4);
  EXPECT_EQ(count(doc, "label", "verified by"), 3);
  EXPECT_EQ(count(doc, "label", "realised by"), 1);
  bool titleAsSection = false, statusInStereotype = false;
  for (const auto& n : doc["nodes"]) {
    if (n["id"] != "SREQ-00001-2") continue;
    titleAsSection = n.contains("sections") && n["sections"][0][0].get<std::string>().rfind("The system shall", 0) == 0;
    statusInStereotype = n["stereotype"] == "requirement, approved, system";
  }
  EXPECT_TRUE(titleAsSection);
  EXPECT_TRUE(statusInStereotype);
}

TEST_F(TestRequirementsView, ThePackageTreeWithoutDesign) {
  const auto doc = diagram("REQ_Iris");
  EXPECT_EQ(count(doc, "classifier", "class"), 0);  // show: verifications leaves design out
  EXPECT_GE(count(doc, "classifier", "requirement"), 7);
  EXPECT_GE(count(doc, "classifier", "constraint"), 2);
}

TEST_F(TestRequirementsView, AClassDiagramHangsItsArtefactsAsANote) {
  const auto doc = diagram("CL_Projection");
  bool note = false;
  for (const auto& n : doc["nodes"]) {
    if (n["kind"] == "note" && n["id"] == "iris@projection::Projection::artefacts") {
      const std::string label = n["label"];
      note = label.find("REQ-00001-2") != std::string::npos && label.find("CST-00002-1") != std::string::npos;
    }
  }
  EXPECT_TRUE(note);
}

}  // namespace
