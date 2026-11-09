#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <set>
#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/export/JsonExport.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace {

/// The exported document carries the metamodel its model is written in (REQ-00068-1),
/// over the design of ds: a reader learns every kind and relation it meets from the document.
class TestExportMetamodel : public ::testing::Test {
 protected:
  nlohmann::json doc;

  void SetUp() override {
    ds::model::Model model;
    ds::parse::Parser parser;
    ds::bind::Binder binder(model);
    for (const auto& f : parser.parseDirectory(std::string(DRIFTSPEC_SOURCE_DIR) + "/design")) binder.bind(f);
    const auto checkSteps = ds::check::steps();
    ds::check::Checker(model, checkSteps).run(binder);
    doc = nlohmann::json::parse(ds::exporter::JsonExport().render(model, false));
  }
};

TEST_F(TestExportMetamodel, EveryKindAndRelationOfTheDocumentIsInItsMetamodel) {
  std::set<std::string> kinds, relations;
  for (const auto& k : doc["metamodel"]["kinds"]) kinds.insert(k["kind"].get<std::string>());
  for (const auto& r : doc["metamodel"]["relations"]) relations.insert(r["kind"].get<std::string>());
  ASSERT_FALSE(doc["entities"].empty());
  for (const auto& e : doc["entities"]) EXPECT_TRUE(kinds.count(e["kind"].get<std::string>())) << e["id"];
  for (const auto& r : doc["relations"]) EXPECT_TRUE(relations.count(r["kind"].get<std::string>())) << r["kind"];
}

TEST_F(TestExportMetamodel, EachFieldSaysItsTypeAndWhoSetsIt) {
  for (const auto& k : doc["metamodel"]["kinds"]) {
    if (k["kind"] != "requirement") continue;
    ASSERT_FALSE(k["fields"].empty());
    EXPECT_EQ(k["fields"][0]["name"], "title");
    EXPECT_EQ(k["fields"][0]["type"], "text");
    EXPECT_EQ(k["fields"][0]["source"], "written");
    return;
  }
  FAIL() << "no requirement in the metamodel";
}

}  // namespace
