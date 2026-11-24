#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/IrExport.hpp"

namespace {

/// The requirement view: the umbrella of an artefact, the tree of a package, artefacts on a class.
class TestRequirementsView : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};

  void SetUp() override {
    ds::parse::Parser parser;
    ds::bind::Binder binder(model, metamodel);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_CORPUS_DIR) + "/artefacts")) binder.bind(f);
    const auto checkSteps = ds::check::Steps::of();
    ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  }

  nlohmann::json diagram(const std::string& name) {
    return nlohmann::json::parse(ds::render::IrExport(model, metamodel).render("diagram:" + name, false));
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
  EXPECT_EQ(count(doc, "label", "satisfied by"), 1);
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

/// A use case that answers to a requirement realises it: the umbrella takes it, drawn as its card.
TEST(TestRequirementsViewOfAUseCase, AUseCaseThatRealisesARequirementIsDrawnAsItsCard) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  const auto file = parser.parseText("shop.ds",
                                     "package shop {\n"
                                     "  requirement SREQ-00001-1: The shop shall sell an item\n"
                                     "  usecase UC-00001-1 buy(item: string) -> int {\n"
                                     "    title: Buy an item\n"
                                     "    status: draft\n"
                                     "    @requirement SREQ-00001-1\n"
                                     "    outcome default: the order\n"
                                     "  }\n"
                                     "}\n"
                                     "diagram REQ_Shop {\n"
                                     "  kind: requirement\n"
                                     "  include: shop@*\n"
                                     "}\n");
  ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
  binder.bind(file);
  const auto checkSteps = ds::check::Steps::of();
  ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  const auto doc = nlohmann::json::parse(ds::render::IrExport(model, metamodel).render("diagram:REQ_Shop", false));
  bool card = false;
  for (const auto& n : doc["nodes"])
    if (n["id"] == "UC-00001-1")
      card = n["classifier"] == "usecase" && n["stereotype"] == "usecase, draft" && n.contains("sections") &&
             n["sections"][0][0] == "Buy an item";
  EXPECT_TRUE(card);
  bool realised = false;
  for (const auto& e : doc["edges"]) realised |= e["from"] == "SREQ-00001-1" && e.value("label", "") == "satisfied by";
  EXPECT_TRUE(realised);
}

TEST(TestRequirementsViewOfAnyAnswer, WhateverAnswersToARequirementIsDrawnAndAMemberThroughItsHolder) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  const auto file = parser.parseText("shop.ds",
                                     "package shop {\n"
                                     "  @requirement SREQ-00001-1\n"
                                     "  requirement SREQ-00001-1: The shop shall sell an item\n"
                                     "  interface shell {\n"
                                     "    kind: cli\n"
                                     "    operation buy(item: string) -> int {\n"
                                     "      @requirement SREQ-00001-1\n"
                                     "    }\n"
                                     "  }\n"
                                     "  module store {\n"
                                     "    function price(item: string) -> int {\n"
                                     "      @requirement SREQ-00001-1\n"
                                     "    }\n"
                                     "    class Cart {\n"
                                     "      @requirement SREQ-00001-1\n"
                                     "      + add(item: string) -> bool {\n"
                                     "        @requirement SREQ-00001-1\n"
                                     "      }\n"
                                     "    }\n"
                                     "  }\n"
                                     "}\n"
                                     "diagram REQ_Shop {\n"
                                     "  kind: requirement\n"
                                     "  include: SREQ-00001-1\n"
                                     "}\n");
  ASSERT_FALSE(file.hasSyntaxErrors) << file.errorDetail;
  binder.bind(file);
  const auto checkSteps = ds::check::Steps::of();
  ds::check::Checker(model, metamodel, checkSteps).run(binder.notes());
  const auto doc = nlohmann::json::parse(ds::render::IrExport(model, metamodel).render("diagram:REQ_Shop", false));
  std::set<std::string> nodes;
  for (const auto& n : doc["nodes"]) nodes.insert(n["id"].get<std::string>());
  EXPECT_EQ(nodes, (std::set<std::string>{"SREQ-00001-1", "shop", "shop@shell", "shop@store::price", "shop@store::Cart"}));
  int toCart = 0;
  for (const auto& e : doc["edges"]) toCart += e["to"] == "shop@store::Cart";
  EXPECT_EQ(toCart, 1);  // the class and its method answer once
}

}  // namespace
