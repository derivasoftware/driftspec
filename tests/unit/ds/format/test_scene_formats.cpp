#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "ds/emit/JsonSink.hpp"
#include "ds/format/DescriptionFactory.hpp"
#include "ds/format/FormatFactory.hpp"
#include "ds/format/ViewerPage.hpp"
#include "ds/scene/Scene.hpp"

namespace {

/// Every output of ds is a document (DS-126): the scene is described once, into whichever sink it is lent, and
/// its page is a writer found by name (DS-398); neither needs a model.
ds::scene::SceneDocument small() {
  ds::scene::SceneDocument doc;
  ds::scene::Node n;
  n.id = "p";
  n.kind = "package";
  n.label = "p";
  n.full = {0, 0, 100, 50};
  doc.nodes.push_back(n);
  doc.world = {0, 0, 100, 50};
  doc.leanWorld = doc.world;
  return doc;
}

TEST(TestFormatFactory, TheWriterOfTheSceneIsFoundByItsName) {
  const auto formats = ds::format::FormatFactory::scene();
  ASSERT_EQ(formats.size(), 1u);
  for (const auto& f : formats) EXPECT_EQ(f.first, f.second->name());
  EXPECT_TRUE(formats.count("html"));
}

TEST(TestSceneFormats, TheSceneDescribesItselfIntoTheSinkItIsLentWithoutAModel) {
  ds::emit::JsonSink compact(false, true), indented(true, true);
  const ds::scene::SceneDocument scene = small();
  const auto description = ds::format::DescriptionFactory::scene(scene);
  description->describe(compact);
  const auto written = nlohmann::json::parse(compact.result());
  EXPECT_EQ(written["version"], 2);
  ASSERT_EQ(written["nodes"].size(), 1u);
  description->describe(indented);
  EXPECT_NE(indented.result().find("\n  "), std::string::npos);  // indented as the sink lent
}

TEST(TestSceneFormats, ThePageIsAnIframeCarryingItWholeWhenAskedToEmbed) {
  ds::format::FormatOptions options;
  options.title = "p";
  ds::emit::JsonSink sink(false, true);
  const std::string page = ds::format::ViewerPage().write(small(), options, sink);
  EXPECT_EQ(page.rfind("<!doctype html>", 0) == 0 || page.find("<html") != std::string::npos, true);
  options.embed = true;
  options.height = 300;
  const std::string frame = ds::format::ViewerPage().write(small(), options, sink);
  EXPECT_EQ(frame.rfind("<iframe", 0), 0u);
  EXPECT_NE(frame.find("height:300px"), std::string::npos);
  EXPECT_NE(frame.find("srcdoc=\""), std::string::npos);
}

}  // namespace
