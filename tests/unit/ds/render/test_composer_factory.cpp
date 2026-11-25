#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <utility>

#include "ds/render/BoxesSvgComposer.hpp"
#include "ds/render/BoxesTextComposer.hpp"
#include "ds/render/ComposerFactory.hpp"
#include "ds/render/Ir.hpp"
#include "ds/render/Layout.hpp"
#include "ds/render/SequenceSvgComposer.hpp"
#include "ds/render/SequenceTextComposer.hpp"
#include "ds/theme/Theme.hpp"

namespace {

/// The style the drawings are written in, made once for the file (DS-393).
const ds::theme::Theme kTheme;

ds::render::IrNode box(const std::string& id, const std::string& label) {
  ds::render::IrNode n;
  n.id = id;
  n.kind = "box";
  n.label = label;
  return n;
}

/// Two boxes, the second inheriting from the first.
ds::render::IrDocument twoBoxes() {
  ds::render::IrDocument doc;
  doc.nodes = {box("a", "Base"), box("b", "Derived")};
  ds::render::IrEdge e;
  e.from = "b";
  e.to = "a";
  e.kind = "inheritance";
  doc.edges = {e};
  return doc;
}

/// The one place that names the composers (DS-400), by kind and technology.
TEST(TestComposerFactory, EachKindAndTechnologyHasItsComposerAndAnyOtherIsRefused) {
  const ds::render::Metrics m = ds::render::Metrics::svg(kTheme.measures(ds::theme::Sink::svg));
  const auto svg = ds::render::ComposerFactory::of("boxes", "svg", m);
  EXPECT_NE(dynamic_cast<const ds::render::BoxesSvgComposer*>(svg.get()), nullptr);
  const auto text = ds::render::ComposerFactory::of("boxes", "text", m);
  EXPECT_NE(dynamic_cast<const ds::render::BoxesTextComposer*>(text.get()), nullptr);
  EXPECT_THROW(ds::render::ComposerFactory::of("boxes", "png", m), std::invalid_argument);
  const auto sequenceSvg = ds::render::ComposerFactory::of("sequence", "svg", m);
  EXPECT_NE(dynamic_cast<const ds::render::SequenceSvgComposer*>(sequenceSvg.get()), nullptr);
  const auto sequenceText = ds::render::ComposerFactory::of("sequence", "text", m);
  EXPECT_NE(dynamic_cast<const ds::render::SequenceTextComposer*>(sequenceText.get()), nullptr);
  EXPECT_THROW(ds::render::ComposerFactory::of("table", "svg", m), std::invalid_argument);  // not yet below the drawing
}

}  // namespace
