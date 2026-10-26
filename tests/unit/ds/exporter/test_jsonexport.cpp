#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "ds/export/JsonExport.hpp"
#include "ds/model/Model.hpp"

namespace {

/// The model as JSON: entities, relations and findings, each located (REQ-00005-1).
class TestJsonExport : public ::testing::Test {
 protected:
  ds::model::Model model;

  void SetUp() override {
    ds::model::Entity e;
    e.kind = ds::model::Kind::Class;
    e.id = "p@m::A";
    e.name = "A";
    e.scope = "p@m";
    e.fields["title"] = "t";
    e.flags.push_back("abstract");
    e.location = {"a.ds", 3, 5};
    model.declare(e);
    model.relate({"uses", "p@m::A", "p@m::B", {"a.ds", 4, 1}, "*"});
    model.report({"W-REF-UNDECLARED", "warning", "B is declared nowhere", {"a.ds", 4, 1}});
  }
};

TEST_F(TestJsonExport, TheThreeArraysWithTheirFields) {
  const auto doc = nlohmann::json::parse(ds::exporter::JsonExport().render(model));
  ASSERT_EQ(doc["entities"].size(), 1u);
  const auto& e = doc["entities"][0];
  EXPECT_EQ(e["id"], "p@m::A");
  EXPECT_EQ(e["kind"], "class");
  EXPECT_EQ(e["scope"], "p@m");
  EXPECT_EQ(e["fields"]["title"], "t");
  EXPECT_EQ(e["flags"][0], "abstract");
  EXPECT_EQ(e["location"]["line"], 3);
  EXPECT_FALSE(e["expected"]);
  const auto& r = doc["relations"][0];
  EXPECT_EQ(r["kind"], "uses");
  EXPECT_EQ(r["multiplicity"], "*");
  const auto& f = doc["findings"][0];
  EXPECT_EQ(f["code"], "W-REF-UNDECLARED");
  EXPECT_EQ(f["location"]["file"], "a.ds");
}

TEST_F(TestJsonExport, CompactOnRequest) {
  const std::string pretty = ds::exporter::JsonExport().render(model, true);
  const std::string compact = ds::exporter::JsonExport().render(model, false);
  EXPECT_NE(pretty.find('\n'), std::string::npos);
  EXPECT_EQ(compact.find('\n'), std::string::npos);
  EXPECT_EQ(nlohmann::json::parse(pretty), nlohmann::json::parse(compact));
}

}  // namespace
