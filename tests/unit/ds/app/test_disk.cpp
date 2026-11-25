#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "ds/app/Disk.hpp"

namespace {

/// What the command line reads from the disk (DS-83, DS-409): a directory with a .ds file under it is a workspace,
/// read as the Api takes it; a file is read whole; a workspace's page is titled after its last name.
class Disks : public ::testing::Test {
 protected:
  const std::string hola = std::string(DRIFTSPEC_EXAMPLES_DIR) + "/hola";
  std::filesystem::path root;
  std::string empty;
  std::string file;

  void SetUp() override {
    root = std::filesystem::path(::testing::TempDir()) / ::testing::UnitTest::GetInstance()->current_test_info()->name();
    std::filesystem::create_directories(root / "empty" / "inner");
    empty = (root / "empty").string();
    file = (root / "note.txt").string();
    std::ofstream(file) << "not a design\n";
  }
};

class TestDisk : public Disks {};

TEST_F(TestDisk, ADirectoryIsOneAndAFileOrNothingIsNot) {
  EXPECT_TRUE(ds::app::Disk::isDirectory(hola));
  EXPECT_FALSE(ds::app::Disk::isDirectory(file));
  EXPECT_FALSE(ds::app::Disk::isDirectory(hola + "/no-such-thing"));
}

TEST_F(TestDisk, ADirectoryHoldsADesignWhenADsFileLiesUnderIt) {
  EXPECT_TRUE(ds::app::Disk::holdsDesign(hola));
  EXPECT_FALSE(ds::app::Disk::holdsDesign(empty));
}

TEST_F(TestDisk, WhyAPathIsNoWorkspaceIsSaidAndAWorkspaceHasNoWhy) {
  EXPECT_EQ(ds::app::Disk::whyNoWorkspace(hola), "");
  EXPECT_EQ(ds::app::Disk::whyNoWorkspace(file), "cannot read " + file + ": not a directory");
  EXPECT_EQ(ds::app::Disk::whyNoWorkspace(empty), "no .ds file under " + empty + ": it is no workspace");
}

TEST_F(TestDisk, TheFilesOfAWorkspaceAreEveryDsFileUnderItReadWholeByItsPath) {
  const auto dir = root / "ws";
  std::filesystem::create_directories(dir / "b");
  std::ofstream(dir / "z.ds", std::ios::binary) << "package z {}\r\n";
  std::ofstream(dir / "b" / "a.ds", std::ios::binary) << "package a {}\n";
  std::ofstream(dir / "notes.md") << "not a design\n";
  const ds::api::Files files = ds::app::Disk::files(dir.string());
  ASSERT_EQ(files.size(), 2u);
  EXPECT_EQ(files.at((dir / "b" / "a.ds").string()), "package a {}\n");
  EXPECT_EQ(files.at((dir / "z.ds").string()), "package z {}\r\n");  // as it is on the disk: the parser reads the line ends
}

TEST_F(TestDisk, AFileOfAWorkspaceThatCannotBeReadFails) {
  const auto dir = root / "unreadable";
  std::filesystem::create_directories(dir);
  const auto locked = dir / "a.ds";
  std::ofstream(locked) << "package p {}\n";
  std::filesystem::permissions(locked, std::filesystem::perms::none);
  const bool readable = static_cast<bool>(std::ifstream(locked));
  if (!readable) EXPECT_THROW(ds::app::Disk::files(dir.string()), std::runtime_error);
  std::filesystem::permissions(locked, std::filesystem::perms::owner_all);
  if (readable) GTEST_SKIP() << "this user reads any file";
}

TEST_F(TestDisk, AFileIsReadWholeAndAMissingOneIsEmpty) {
  std::ofstream(root / "a.txt", std::ios::binary) << "one\ntwo\n";
  EXPECT_TRUE(ds::app::Disk::readable((root / "a.txt").string()));
  EXPECT_EQ(ds::app::Disk::read((root / "a.txt").string()), "one\ntwo\n");
  EXPECT_FALSE(ds::app::Disk::readable((root / "missing.txt").string()));
  EXPECT_EQ(ds::app::Disk::read((root / "missing.txt").string()), "");
}

TEST_F(TestDisk, ThePageIsTitledAfterTheLastNameOfThePath) {
  EXPECT_EQ(ds::app::Disk::titleOf("examples/hola"), "hola");
  EXPECT_EQ(ds::app::Disk::titleOf("examples/hola//"), "hola");
  EXPECT_EQ(ds::app::Disk::titleOf("design"), "design");
}

}  // namespace
