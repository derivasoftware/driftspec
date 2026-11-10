#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "ds/format/SceneFormats.hpp"
#include "ds/scene/Scene.hpp"

namespace {

/// Every output of ds is a document written by a format (DS-126): the scene's formats are found by
/// name, and a format writes the document it is given; it needs no model.
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

TEST(TestSceneFormats, EachFormatOfTheSceneIsFoundByItsName) {
  const auto formats = ds::format::sceneFormats();
  ASSERT_EQ(formats.size(), 2u);
  for (const auto& f : formats) EXPECT_EQ(f.first, f.second->name());
  EXPECT_TRUE(formats.count("json"));
  EXPECT_TRUE(formats.count("html"));
}

TEST(TestSceneFormats, AFormatWritesTheDocumentItIsGivenWithoutAModel) {
  const auto written = nlohmann::json::parse(ds::format::SceneJson().write(small(), {}));
  EXPECT_EQ(written["version"], 2);
  ASSERT_EQ(written["nodes"].size(), 1u);
  ds::format::FormatOptions pretty;
  pretty.pretty = true;
  EXPECT_NE(ds::format::SceneJson().write(small(), pretty).find("\n  "), std::string::npos);  // indented for a reader
}

TEST(TestSceneFormats, ThePageIsAnIframeCarryingItWholeWhenAskedToEmbed) {
  ds::format::FormatOptions options;
  options.title = "p";
  const std::string page = ds::format::SceneHtml().write(small(), options);
  EXPECT_EQ(page.rfind("<!doctype html>", 0) == 0 || page.find("<html") != std::string::npos, true);
  options.embed = true;
  options.height = 300;
  const std::string frame = ds::format::SceneHtml().write(small(), options);
  EXPECT_EQ(frame.rfind("<iframe", 0), 0u);
  EXPECT_NE(frame.find("height:300px"), std::string::npos);
  EXPECT_NE(frame.find("srcdoc=\""), std::string::npos);
}

}  // namespace
