#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/Ir.hpp"
#include "ds/render/IrExport.hpp"
#include "ds/render/Layout.hpp"
#include "ds/render/Svg.hpp"
#include "ds/render/Text.hpp"

namespace {

using ds::render::IrDocument;

/// Each piece of the drawing on its own (DS-186): the IR a diagram exports, the document it parses
/// into, the metrics of each surface, and the two drawings.

const char* const kIr = R"({"ir":1,"title":"T","nodes":[{"id":"p@m::A","kind":"box","label":"A","classifier":"class"},)"
                        R"({"id":"p@m::B","kind":"box","label":"B","classifier":"class"}],)"
                        R"("edges":[{"from":"p@m::A","to":"p@m::B","kind":"dependency","label":"uses"},)"
                        R"({"from":"p@m::A","to":"p@m::B","kind":"dependency","label":"calls"}]})";

TEST(TestIrExport, ItListsTheDiagramsAndExportsEachAsItsIr) {
  const ds::model::Metamodel metamodel;
  ds::model::Model model{metamodel};
  ds::parse::Parser parser;
  ds::bind::Binder binder(model, metamodel);
  binder.bind(parser.parseText("a.ds",
                               "package p {\n  module m {\n    class A {\n      - b: B\n    }\n    class B { }\n  }\n}\n"
                               "diagram CL_M {\n  kind: class\n  include: p@m::*\n}\n"));
  const auto steps = ds::check::StepFactory::of();
  ds::check::Checker(model, metamodel, steps).run(binder.notes());
  const ds::render::IrExport ir(model, metamodel);
  EXPECT_EQ(ir.diagrams(), (std::vector<std::string>{"diagram:CL_M"}));
  const auto json = nlohmann::json::parse(ir.render("diagram:CL_M", false));
  std::set<std::string> ids;
  for (const auto& n : json["nodes"]) ids.insert(n["id"].get<std::string>());
  EXPECT_TRUE(ids.count("p@m::A") && ids.count("p@m::B"));
}

TEST(TestIrDocument, ItParsesItsContractAndBundlesRelationsThatDifferOnlyInText) {
  const IrDocument doc = IrDocument::parse(kIr);
  EXPECT_EQ(doc.title, "T");
  ASSERT_EQ(doc.nodes.size(), 2u);
  ASSERT_EQ(doc.edges.size(), 2u);
  const IrDocument one = doc.bundled();
  ASSERT_EQ(one.edges.size(), 1u);  // same ends and kind: drawn once
  EXPECT_NE(one.edges[0].label.find("uses"), std::string::npos);
  EXPECT_NE(one.edges[0].label.find("calls"), std::string::npos);
}

TEST(TestMetrics, AScreenCellIsOneCharacterWhereAPixelTableIsWider) {
  const auto svg = ds::render::Metrics::svg();
  const auto text = ds::render::Metrics::text();
  EXPECT_EQ(text.CHAR_W, 1);
  EXPECT_EQ(text.LINE_H, 1);
  EXPECT_GT(svg.CHAR_W, text.CHAR_W);
  EXPECT_GT(svg.LINE_H, text.LINE_H);
}

TEST(TestSvg, AnSvgOfTheDocumentWithEveryBox) {
  const std::string svg = ds::render::Svg().render(IrDocument::parse(kIr), 0);
  EXPECT_NE(svg.find("<svg"), std::string::npos);
  EXPECT_NE(svg.find(">A<"), std::string::npos);
  EXPECT_NE(svg.find(">B<"), std::string::npos);
}

TEST(TestText, TheDocumentInBoxCharactersAndAsCells) {
  const ds::render::Text text;
  const std::string drawn = text.render(IrDocument::parse(kIr), false, 0);
  EXPECT_NE(drawn.find('A'), std::string::npos);
  EXPECT_NE(drawn.find('B'), std::string::npos);
  EXPECT_EQ(drawn.find("\x1b["), std::string::npos);  // no colour unless asked
  const auto cells = nlohmann::json::parse(text.cells(IrDocument::parse(kIr), 0));
  EXPECT_FALSE(cells.empty());
}

const char* const kParts = R"({"ir":1,"title":"T","parts":["boxes","again"],)"
                           R"("nodes":[{"id":"p@m::A","kind":"box","label":"A","classifier":"class","part":"boxes"},)"
                           R"({"id":"p@m::A","kind":"box","label":"A","classifier":"class","part":"again"},)"
                           R"({"id":"p@m::B","kind":"box","label":"B","classifier":"class","part":"again"}],)"
                           R"("edges":[{"from":"p@m::A","to":"p@m::B","kind":"dependency","part":"again"}]})";

TEST(TestIrDocument, ADocumentMadeOfPartsGivesEachAsADocumentOfItsOwn) {
  const IrDocument doc = IrDocument::parse(kParts);
  EXPECT_EQ(doc.parts, (std::vector<std::string>{"boxes", "again"}));
  EXPECT_EQ(doc.part("boxes").nodes.size(), 1u);
  EXPECT_EQ(doc.part("again").nodes.size(), 2u);  // the same entity may be drawn in two parts
  EXPECT_EQ(doc.part("again").edges.size(), 1u);
  EXPECT_TRUE(doc.part("boxes").edges.empty());
  EXPECT_NE(doc.part("boxes").title, doc.part("again").title);  // each its own identity
}

TEST(TestSvg, ADocumentMadeOfPartsIsOneSvgThatHoldsOneForEachPart) {
  const std::string svg = ds::render::Svg().render(IrDocument::parse(kParts));
  EXPECT_EQ(svg.find("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox="), 0u);
  EXPECT_NE(svg.find("data-part=\"boxes\""), std::string::npos);
  EXPECT_NE(svg.find("data-part=\"again\""), std::string::npos);
}

TEST(TestText, ADocumentMadeOfPartsIsDrawnOnePartUnderTheOther) {
  const IrDocument doc = IrDocument::parse(kParts);
  const std::string text = ds::render::Text().render(doc, false);
  const auto first = text.find("C A");
  ASSERT_NE(first, std::string::npos);
  EXPECT_NE(text.find("C A", first + 1), std::string::npos);  // the same box twice: once for each part
  EXPECT_NE(ds::render::Text().cells(doc).find("\"rows\""), std::string::npos);
}

}  // namespace
