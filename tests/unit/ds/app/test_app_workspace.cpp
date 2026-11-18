#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "ds/app/App.hpp"

namespace {

/// What the command line takes for a workspace (DS-83): a directory with a .ds file under it, loaded as the Api
/// loads texts (DS-120).
class Workspaces : public ::testing::Test {
 protected:
  const std::string hola = std::string(DRIFTSPEC_EXAMPLES_DIR) + "/hola";
  std::string empty;
  std::string file;

  void SetUp() override {
    const auto root = std::filesystem::path(::testing::TempDir()) / ::testing::UnitTest::GetInstance()->current_test_info()->name();
    std::filesystem::create_directories(root / "empty" / "inner");
    empty = (root / "empty").string();
    file = (root / "note.txt").string();
    std::ofstream(file) << "not a design\n";
  }
};

class TestIsDirectory : public Workspaces {};

TEST_F(TestIsDirectory, ADirectoryIsOneAndAFileOrNothingIsNot) {
  EXPECT_TRUE(ds::app::isDirectory(hola));
  EXPECT_FALSE(ds::app::isDirectory(file));
  EXPECT_FALSE(ds::app::isDirectory(hola + "/no-such-thing"));
}

class TestHoldsDesign : public Workspaces {};

TEST_F(TestHoldsDesign, ADirectoryHoldsADesignWhenADsFileLiesUnderIt) {
  EXPECT_TRUE(ds::app::holdsDesign(hola));
  EXPECT_FALSE(ds::app::holdsDesign(empty));
}

class TestLoad : public Workspaces {};

TEST_F(TestLoad, TheFilesOfADirectoryAreReadAsOneModel) {
  const ds::model::Model model = ds::app::load(hola);
  EXPECT_FALSE(model.entities().empty());
}

class TestLoadWorkspace : public Workspaces {};

TEST_F(TestLoadWorkspace, AWorkspaceIsLoadedAndAnythingElseIsSaidAndRefused) {
  ds::model::Model model;
  EXPECT_TRUE(ds::app::loadWorkspace(hola, &model));
  EXPECT_FALSE(model.entities().empty());
  ::testing::internal::CaptureStderr();
  ds::model::Model none;
  EXPECT_FALSE(ds::app::loadWorkspace(file, &none));
  EXPECT_FALSE(ds::app::loadWorkspace(empty, &none));
  const std::string said = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(said.find("not a directory"), std::string::npos);
  EXPECT_NE(said.find("no workspace"), std::string::npos);
}

}  // namespace
