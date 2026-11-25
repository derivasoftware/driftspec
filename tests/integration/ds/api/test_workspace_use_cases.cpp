#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "ds/api/Workspace.hpp"
#include "ds/emit/SinkFactory.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/service/UseCases.hpp"

namespace {

namespace fs = std::filesystem;
using ds::service::Request;

const std::string kGolden = std::string(DRIFTSPEC_SOURCE_DIR) + "/tests/system/python/golden";

std::string read(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::stringstream text;
  text << in.rdbuf();
  return text.str();
}

/// A golden workspace's files as an editor holds them: by the path ds reads them under, from the workspaces' folder.
ds::api::Files filesOf(const std::string& ws) {
  ds::api::Files out;
  const fs::path root = kGolden + "/workspaces";
  for (const auto& e : fs::recursive_directory_iterator(root / ws))
    if (e.is_regular_file() && e.path().extension() == ".ds") out[fs::relative(e.path(), root).generic_string()] = read(e.path());
  return out;
}

/// The long-lived workspace (DS-408): made over the use cases, given files by reload, running each by name.
class TestWorkspaceUseCases : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  const ds::service::UseCaseFactory useCases{metamodel};

  std::string run(ds::api::Workspace& ws, const std::string& name, const Request& request, bool pretty) const {
    const auto sink = ds::emit::SinkFactory::json(pretty);
    ws.run(name, request, *sink);
    std::string text = sink->result() + "\n";
    const std::string version = metamodel.version();
    for (std::size_t at = text.find(version); !version.empty() && at != std::string::npos; at = text.find(version, at + 9))
      text.replace(at, version.size(), "<version>");
    return text;
  }
};

TEST_F(TestWorkspaceUseCases, EachUseCaseRunThroughTheWorkspaceWritesWhatItsDoorWrites) {
  for (const std::string ws : {"small", "kinds"}) {
    const fs::path expected = kGolden + "/expected/" + ws;
    // one workspace for the session: each use case asks what it needs of the same design, held once
    ds::api::Workspace workspace(useCases);
    workspace.reload(filesOf(ws));
    EXPECT_EQ(run(workspace, "check-design", {}, true), read(expected / "report.json")) << ws;
    EXPECT_EQ(run(workspace, "hand-model", {}, true), read(expected / "export.json")) << ws;
    EXPECT_EQ(run(workspace, "see-design", {{{"format", "json"}}}, true), read(expected / "scene.json")) << ws;
    EXPECT_EQ(run(workspace, "ask-model", {{{"question", "coverage"}}}, false), read(expected / "answers" / "coverage.json")) << ws;
  }
}

TEST_F(TestWorkspaceUseCases, AOneShotDrawingChecksOnlyWhatItReadsAndAUseCaseThatNeedsMoreBindsOnceMore) {
  ds::api::Workspace workspace(useCases);
  workspace.reload(filesOf("small"));
  EXPECT_EQ(workspace.loads(), 0u);  // nothing loaded until a use case asks
  // a drawing: bound once, checked with the steps it reads, as DS-376's load for one diagram
  const std::string layout = run(workspace, "see-design", {{{"format", "layout"}, {"diagram", "CL_Small"}}}, false);
  EXPECT_EQ(layout, read(kGolden + "/expected/small/diagrams/CL_Small.layout.json"));
  EXPECT_EQ(workspace.loads(), 1u);
  EXPECT_TRUE(workspace.selection().has(ds::check::Step::Linker));
  EXPECT_FALSE(workspace.selection().has(ds::check::Step::Structure));
  // what it already ran covers another drawing: nothing loaded again
  run(workspace, "see-design", {{{"format", "cells"}, {"diagram", "CL_Small"}}}, false);
  EXPECT_EQ(workspace.loads(), 1u);
  // the check needs every step: bound once more from the files it holds, with the union
  EXPECT_EQ(run(workspace, "check-design", {}, true), read(kGolden + "/expected/small/report.json"));
  EXPECT_EQ(workspace.loads(), 2u);
  EXPECT_TRUE(workspace.selection().has(ds::check::Step::Structure));
  // a reload binds once more at the next use case, with every step asked so far
  workspace.reload(filesOf("small"));
  run(workspace, "see-design", {{{"format", "layout"}, {"diagram", "CL_Small"}}}, false);
  EXPECT_EQ(workspace.loads(), 3u);
  EXPECT_TRUE(workspace.selection().has(ds::check::Step::Structure));
}

TEST_F(TestWorkspaceUseCases, AWorkspaceMadeForAnOutputChecksAtOnceAsBefore) {
  // the doors before DS-409: made for an output, loaded and checked when given files, every step for the model
  ds::api::Workspace workspace = ds::api::Workspace::madeFor(metamodel, filesOf("small"));
  EXPECT_EQ(workspace.loads(), 1u);
  EXPECT_TRUE(workspace.selection().has(ds::check::Step::Structure));
  EXPECT_EQ(run(workspace, "check-design", {}, true), read(kGolden + "/expected/small/report.json"));
  EXPECT_EQ(workspace.loads(), 1u);  // already checked with every step
}

}  // namespace
