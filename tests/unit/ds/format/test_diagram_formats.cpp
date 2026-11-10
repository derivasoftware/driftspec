#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "ds/format/DiagramFormats.hpp"

namespace {

/// A diagram is its render-IR (DS-126): five formats write it, found by name, and none needs a model.
const ds::render::IrText kDiagram{R"({"ir":1,"title":"T","nodes":[{"id":"p@m::A","kind":"box","label":"A","classifier":"class"},)"
                                  R"({"id":"p@m::B","kind":"box","label":"B","classifier":"class"}],)"
                                  R"("edges":[{"from":"p@m::A","to":"p@m::B","kind":"dependency"}]})"};

TEST(TestDiagramFormats, EachFormatOfADiagramIsFoundByItsName) {
  const auto formats = ds::format::diagramFormats();
  ASSERT_EQ(formats.size(), 5u);
  for (const auto& f : formats) EXPECT_EQ(f.first, f.second->name());
}

TEST(TestDiagramFormats, TheIrIsWrittenAsItCameUnlessAskedToIndent) {
  EXPECT_EQ(ds::format::DiagramIr().write(kDiagram, {}), kDiagram.json);
  ds::format::FormatOptions indented;
  indented.pretty = true;
  EXPECT_EQ(ds::format::DiagramIr().write(kDiagram, indented), nlohmann::json::parse(kDiagram.json).dump(2));
}

TEST(TestDiagramFormats, EachDrawingFormatDrawsTheSameDiagram) {
  const std::string svg = ds::format::DiagramSvg().write(kDiagram, {});
  EXPECT_NE(svg.find("<svg"), std::string::npos);
  EXPECT_NE(svg.find(">A<"), std::string::npos);
  EXPECT_NE(ds::format::DiagramText().write(kDiagram, {}).find('B'), std::string::npos);
  EXPECT_FALSE(ds::format::DiagramCells().write(kDiagram, {}).empty());
  const auto layout = nlohmann::json::parse(ds::format::DiagramLayout().write(kDiagram, {}));
  EXPECT_EQ(layout["nodes"].size(), 2u);
  EXPECT_EQ(layout["routes"].size(), 1u);
}

}  // namespace
