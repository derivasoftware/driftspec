#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

#include "ds/api/Api.hpp"

namespace {

/// The engine in process over examples/hola (REQ-00085-1): the texts an editor holds win over the
/// disk, a diagram draws as text or as its render-IR, the viewer opens framed where it is asked.
class TestApiOverHola : public ::testing::Test {
 protected:
  ds::api::Files files;

  void SetUp() override {
    const std::string root = std::string(DRIFTSPEC_EXAMPLES_DIR) + "/hola";
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
      if (entry.path().extension() != ".ds") continue;
      std::ifstream in(entry.path());
      std::stringstream text;
      text << in.rdbuf();
      files[entry.path().string()] = text.str();
    }
  }
};

TEST_F(TestApiOverHola, TheExampleChecksCleanAsTheCommandLineSays) {
  const auto out = nlohmann::json::parse(ds::api::Api::check(files));
  EXPECT_TRUE(out["findings"].empty());
  EXPECT_EQ(out["counts"]["findings"], 0);
}

TEST_F(TestApiOverHola, TheModelOfTheExampleExportsByIdentity) {
  const auto out = nlohmann::json::parse(ds::api::Api::exportModel(files));
  bool greeter = false;
  for (const auto& e : out["entities"]) greeter |= e["id"] == "hola@core::Greeter" && e["kind"] == "class";
  EXPECT_TRUE(greeter);
}

TEST_F(TestApiOverHola, AnUnsavedBufferIsCheckedAsItIsTyped) {
  for (auto& f : files)
    if (f.first.find("Greeter.ds") != std::string::npos) f.second.replace(f.second.find("class Greeter"), 5, "clas");
  const auto out = nlohmann::json::parse(ds::api::Api::check(files));
  bool syntax = false;  // where the buffer stops parsing, in the buffer's own file
  for (const auto& f : out["findings"])
    syntax |= f["code"] == "E-SYNTAX" && f["location"]["file"].get<std::string>().find("Greeter.ds") != std::string::npos;
  EXPECT_TRUE(syntax);
}

TEST_F(TestApiOverHola, ADiagramDrawsAsTextOrAsItsRenderIr) {
  EXPECT_NE(ds::api::Api::render(files, "CL_Hola", "text", 120).find("Greeter"), std::string::npos);
  EXPECT_NO_THROW(nlohmann::json::parse(ds::api::Api::render(files, "CL_Hola", "ir", 0)));
  EXPECT_EQ(ds::api::Api::render(files, "NoSuchDiagram", "text", 0), "");
}

TEST_F(TestApiOverHola, TheViewerOpensFramedOnWhatItIsAsked) {
  const std::string page = ds::api::Api::page(files, "hola", {"hola@core::Greeter"});
  EXPECT_NE(page.find("const PACKED='"), std::string::npos);
  EXPECT_NE(page.find("const FOCUS=[\"hola@core::Greeter\"];"), std::string::npos);
}

TEST_F(TestApiOverHola, WhatTheGrammarTakesWhereAnEditorAsksInTheExample) {
  std::string greeter;
  for (const auto& f : files)
    if (f.first.find("core/Greeter.ds") != std::string::npos) greeter = f.second;
  ASSERT_FALSE(greeter.empty());
  int line = 0;  // counted from 0, as an editor does: the line after the class opens, where a member goes
  for (size_t i = 0; i < greeter.find("class Greeter"); ++i) line += greeter[i] == '\n';
  const auto words = nlohmann::json::parse(ds::api::Api::expectedAt(greeter, line + 1, 0));
  const auto has = [&](const std::string& w) { return std::find(words.begin(), words.end(), w) != words.end(); };
  EXPECT_TRUE(has("@annotation"));  // what a class holds
  EXPECT_TRUE(has("requirement"));
  EXPECT_FALSE(has("module"));  // and not what a package holds
}

}  // namespace
