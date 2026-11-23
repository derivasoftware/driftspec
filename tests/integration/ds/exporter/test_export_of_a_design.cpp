#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "ds/api/Api.hpp"
#include "ds/export/JsonExport.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/parse/Parser.hpp"

namespace {

const char* kDesign =
    "package p {\n"
    "  module m {\n"
    "    class A {\n"
    "      - refs shares b: B\n"
    "      + run() -> void\n"
    "    }\n"
    "    class B\n"
    "  }\n"
    "}\n";

/// The model of a design as JSON (REQ-00005-1): what the texts declare, relate and get wrong comes
/// out as three arrays, each entry located in the text that wrote it.
class TestExportOfADesign : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  static const nlohmann::json* byId(const nlohmann::json& entities, const std::string& id) {
    for (const auto& e : entities)
      if (e["id"] == id) return &e;
    return nullptr;
  }
};

TEST_F(TestExportOfADesign, TheThreeArraysCarryWhatTheTextDeclaresRelatesAndGetsWrong) {
  const auto doc = nlohmann::json::parse(ds::api::Api(metamodel).exportModel({{"a.ds", kDesign}}));
  const nlohmann::json* a = byId(doc["entities"], "p@m::A");
  ASSERT_NE(a, nullptr);
  EXPECT_EQ((*a)["kind"], "class");
  EXPECT_EQ((*a)["scope"], "p@m");
  EXPECT_EQ((*a)["location"]["file"], "a.ds");
  EXPECT_EQ((*a)["location"]["line"], 3);
  EXPECT_FALSE((*a)["expected"]);
  const nlohmann::json* attribute = byId(doc["entities"], "p@m::A::b");
  ASSERT_NE(attribute, nullptr);
  EXPECT_EQ((*attribute)["flags"][0], "refs");
  EXPECT_EQ((*attribute)["fields"]["visibility"], "-");
  const nlohmann::json* b = byId(doc["entities"], "p@m::B");
  ASSERT_NE(b, nullptr);
  EXPECT_FALSE((*b)["expected"]);

  bool refs = false;
  for (const auto& r : doc["relations"]) {
    if (r["kind"] != "refs") continue;
    refs = true;
    EXPECT_EQ(r["from"], "p@m::A::b");
    EXPECT_EQ(r["to"], "p@m::B");
    EXPECT_TRUE(r.contains("multiplicity"));
    EXPECT_EQ(r["location"]["line"], 4);
  }
  EXPECT_TRUE(refs);

  ASSERT_EQ(doc["findings"].size(), 1u);
  EXPECT_EQ(doc["findings"][0]["code"], "E-MODIFIER-CONFLICT");  // refs and shares at once: the first is kept
  EXPECT_EQ(doc["findings"][0]["severity"], "error");
  EXPECT_EQ(doc["findings"][0]["location"]["file"], "a.ds");
  EXPECT_EQ(doc["findings"][0]["location"]["line"], 4);
}

TEST_F(TestExportOfADesign, TheCompactFormSaysTheSameOnOneLine) {
  ds::parse::Parser parser;
  const ds::model::Model model = ds::api::Api(metamodel).load({parser.parseText("a.ds", kDesign)});
  const std::string pretty = ds::exporter::JsonExport(metamodel).render(model, true);
  const std::string compact = ds::exporter::JsonExport(metamodel).render(model, false);
  EXPECT_NE(pretty.find('\n'), std::string::npos);
  EXPECT_EQ(compact.find('\n'), std::string::npos);
  EXPECT_EQ(nlohmann::json::parse(pretty), nlohmann::json::parse(compact));
  EXPECT_EQ(nlohmann::json::parse(compact)["entities"],
            nlohmann::json::parse(ds::api::Api(metamodel).exportModel({{"a.ds", kDesign}}))["entities"]);
}

}  // namespace
