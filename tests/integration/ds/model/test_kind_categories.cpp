#include <gtest/gtest.h>

#include <map>
#include <nlohmann/json.hpp>
#include <string>

#include "ds/api/Api.hpp"
#include "ds/emit/SinkFactory.hpp"
#include "ds/export/JsonExport.hpp"
#include "ds/format/DescriptionFactory.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/scene/Scene.hpp"

namespace {

/// What a kind is part of (REQ-00264-1, DS-427), over examples/hola, as the metamodel gives it and the doors write it.
class TestKindCategories : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  ds::model::Model model = load();

  ds::model::Model load() const {
    ds::parse::Parser parser;
    return ds::api::Api(metamodel).load(parser.parseDirectory(std::string(DRIFTSPEC_EXAMPLES_DIR) + "/hola"));
  }
};

TEST_F(TestKindCategories, TheExportAndTheSceneSayEachKindsCategoryAsTheMetamodelGivesIt) {
  std::map<std::string, std::string> given;
  for (const auto& k : metamodel.kinds()) given[metamodel.nameOf(k.kind)] = ds::model::Metamodel::categoryName(k.category);
  // the export: each kind of its metamodel with its category
  const auto exported = ds::emit::SinkFactory::json(false);
  ds::exporter::JsonExport(metamodel).render(model, *exported);
  const auto doc = nlohmann::json::parse(exported->result());
  std::map<std::string, std::string> written;
  for (const auto& k : doc["metamodel"]["kinds"]) written[k["kind"].get<std::string>()] = k["category"].get<std::string>();
  EXPECT_EQ(written, given);
  // the scene: the same, for the viewer to group by
  const auto scened = ds::emit::SinkFactory::json(false);
  ds::format::DescriptionFactory::scene(ds::scene::Scene(model, metamodel).build())->describe(*scened);
  const auto scene = nlohmann::json::parse(scened->result());
  std::map<std::string, std::string> grouped;
  for (const auto& [kind, category] : scene["categories"].items()) grouped[kind] = category.get<std::string>();
  EXPECT_EQ(grouped, given);
  // five, each one the Owner named
  std::map<std::string, int> per;
  for (const auto& g : given) ++per[g.second];
  EXPECT_EQ(per.size(), 5u);
  EXPECT_EQ(given.at("state"), "logic");
  EXPECT_EQ(given.at("operator"), "structure");
  EXPECT_EQ(given.at("outcome"), "artefact");
  EXPECT_EQ(given.at("diagram"), "view");
}

}  // namespace
