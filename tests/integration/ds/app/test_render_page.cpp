#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "ds/api/Workspace.hpp"
#include "ds/app/CommandLine.hpp"
#include "ds/app/Disk.hpp"
#include "ds/model/Metamodel.hpp"

namespace {

/// The viewer's page is see-design's for every door (DS-474): what `ds render <ws> -f html` writes is what the engine's
/// Workspace::page writes for the same workspace, titled after it, the walkthroughs' cards and mode included when the
/// workspace writes a walkthrough (DS-470) and left out when it writes none.
class TestRenderPage : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;

  /// What the command line writes on stdout, run as main runs it.
  static std::string written(const std::vector<std::string>& words) {
    ::testing::internal::CaptureStdout();
    ::testing::internal::CaptureStderr();
    const int code = ds::app::CommandLine().run(words);
    const std::string out = ::testing::internal::GetCapturedStdout();
    ::testing::internal::GetCapturedStderr();
    EXPECT_EQ(code, 0);
    return out;
  }

  /// The engine's workspace over the same files the command line reads.
  ds::api::Workspace loaded(const std::string& dir) const { return ds::api::Workspace::madeFor(metamodel, ds::app::Disk::files(dir)); }
};

TEST_F(TestRenderPage, TheCommandLinesPageIsTheEnginesWithTheWalkthroughsCardsOnlyWhenThereAreWalkthroughs) {
  for (const std::string name : {"shop", "hola"}) {
    const std::string dir = std::string(DRIFTSPEC_EXAMPLES_DIR) + "/" + name;
    const std::string page = written({"ds", "render", dir, "-f", "html"});
    EXPECT_EQ(page, loaded(dir).page(ds::app::Disk::titleOf(dir), {})) << name;
    const bool walked = name == "shop";  // WALK_Buy (DS-472)
    EXPECT_EQ(page.find("const DRAWINGS='") != std::string::npos, walked) << name;
  }
}

TEST_F(TestRenderPage, ThePageFramedOnADiagramIsTheEnginesFramedOnWhatItShows) {
  const std::string dir = std::string(DRIFTSPEC_EXAMPLES_DIR) + "/shop";
  const ds::api::Workspace workspace = loaded(dir);
  const auto diagrams = nlohmann::json::parse(workspace.ask("diagrams", ""));
  ASSERT_FALSE(diagrams.empty());
  const std::string id = diagrams.front()["id"].get<std::string>();
  const std::string diagram = id.substr(id.find("diagram:") + 8);
  std::vector<std::string> focus;
  for (const auto& shown : nlohmann::json::parse(workspace.shows(diagram))) focus.push_back(shown["id"].get<std::string>());
  EXPECT_EQ(written({"ds", "render", dir, diagram}), workspace.page(ds::app::Disk::titleOf(dir), focus)) << diagram;
}

}  // namespace
