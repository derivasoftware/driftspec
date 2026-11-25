#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "ds/emit/JsonSink.hpp"
#include "ds/format/DescriptionFactory.hpp"
#include "ds/format/DiagramFormats.hpp"
#include "ds/format/FormatFactory.hpp"
#include "ds/theme/Theme.hpp"

namespace {

/// The style the drawings are written in, made once for the file (DS-393).
// built on first use, not when the executable starts: a test reaches the theme only if it uses it (DS-473)
const ds::theme::Theme& theme() {
  static const ds::theme::Theme built;
  return built;
}

/// A diagram is its render-IR (DS-126): three writers write it, found by name, its layout and its cells are
/// descriptions (DS-398), and none needs a model.
// built on first use, not when the executable starts (DS-473)
const ds::render::IrDocument& diagram() {
  static const ds::render::IrDocument built =
      ds::render::IrDocument::parse(R"({"ir":1,"title":"T","nodes":[{"id":"p@m::A","kind":"box","label":"A","classifier":"class"},)"
                                    R"({"id":"p@m::B","kind":"box","label":"B","classifier":"class"}],)"
                                    R"("edges":[{"from":"p@m::A","to":"p@m::B","kind":"dependency"}]})");
  return built;
}

TEST(TestFormatFactory, EachWriterOfADiagramIsFoundByItsName) {
  const auto formats = ds::format::FormatFactory::diagram(theme());
  ASSERT_EQ(formats.size(), 2u);  // svg and text: the IR is a description, written by the door that asks (DS-458)
  for (const auto& f : formats) EXPECT_EQ(f.first, f.second->name());
}

TEST(TestDiagramFormats, EachDrawingFormatDrawsTheSameDiagram) {
  ds::emit::JsonSink sink(false, true);  // lent as a door lends it, checked (DS-397)
  const std::string svg = ds::format::DiagramSvg(theme()).write(diagram(), {}, sink);
  EXPECT_NE(svg.find("<svg"), std::string::npos);
  EXPECT_NE(svg.find(">A<"), std::string::npos);
  EXPECT_NE(ds::format::DiagramText(theme()).write(diagram(), {}, sink).find('B'), std::string::npos);
  ds::format::DescriptionFactory::cells(diagram(), theme(), 0)->describe(sink);
  EXPECT_FALSE(sink.result().empty());
  ds::format::DescriptionFactory::layout(diagram(), theme(), 0)->describe(sink);
  const auto layout = nlohmann::json::parse(sink.result());
  EXPECT_EQ(layout["nodes"].size(), 2u);
  EXPECT_EQ(layout["routes"].size(), 1u);
}

}  // namespace
