#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

#include "ds/api/Browser.hpp"
#include "ds/model/Metamodel.hpp"

namespace {

/// The engine for a page in a browser over examples/hola (REQ-00113-1): one call answers what the
/// page shows after an edit.
class TestBrowserOverHola : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  nlohmann::json answer;

  void SetUp() override {
    nlohmann::json files = nlohmann::json::object();
    const std::string root = std::string(DRIFTSPEC_EXAMPLES_DIR) + "/hola";
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
      if (entry.path().extension() != ".ds") continue;
      std::ifstream in(entry.path());
      std::stringstream text;
      text << in.rdbuf();
      files[std::filesystem::relative(entry.path(), root).generic_string()] = text.str();
    }
    answer = nlohmann::json::parse(ds::api::Browser(metamodel).answer(nlohmann::json{{"files", files}}.dump()));
  }
};

TEST_F(TestBrowserOverHola, TheExampleIsCleanAndEachOfItsDiagramsIsDrawnInTheOrderItIsWritten) {
  EXPECT_EQ(answer["report"]["errors"], 0);
  ASSERT_GE(answer["diagrams"].size(), 10u);
  std::string file;
  int line = 0;
  for (const auto& d : answer["diagrams"]) {
    EXPECT_EQ(d["svg"].get<std::string>().rfind("<svg", 0), 0u) << d["id"];
    const std::string f = d["file"];
    EXPECT_TRUE(f > file || (f == file && d["line"].get<int>() > line)) << d["id"];
    file = f;
    line = d["line"];
  }
}

TEST_F(TestBrowserOverHola, TheArtefactsOfTheExampleComeAsRowsNestedByWhatTheyRefine) {
  bool refines = false;
  for (const auto& row : answer["artefacts"]) refines |= row["id"] == "REQ-00001-1" && row["parent"] == "SREQ-00001-1";
  EXPECT_TRUE(refines);
}

TEST_F(TestBrowserOverHola, AClassOfTheExampleIsFoundWhereItIsWritten) {
  const auto& at = answer["locations"]["hola@core::Greeter"];
  EXPECT_EQ(at[0], "core/Greeter.ds");
  EXPECT_LT(at[1].get<int>(), at[2].get<int>());
}

TEST_F(TestBrowserOverHola, TheManualIsAnsweredByItsOwnRequest) {
  const auto index = nlohmann::json::parse(ds::api::Browser(metamodel).answer(nlohmann::json{{"manual", ""}}.dump()))["manual"];
  ASSERT_FALSE(index["guides"].empty());
  EXPECT_FALSE(index["topics"].empty());
  const std::string first = index["guides"][0];
  const auto page = nlohmann::json::parse(ds::api::Browser(metamodel).answer(nlohmann::json{{"manual", first}}.dump()))["manual"];
  EXPECT_EQ(page["topic"], first);
  EXPECT_EQ(page["text"].get<std::string>().rfind("# ", 0), 0U);  // a guide is Markdown that starts with its title
}

}  // namespace
