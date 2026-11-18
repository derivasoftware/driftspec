#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "ds/app/App.hpp"

namespace {

/// What the command line reads and writes besides a workspace: a file whole, the output -o names or stdout,
/// the title of a page and the identity of a diagram.
class Files : public ::testing::Test {
 protected:
  std::filesystem::path root;

  void SetUp() override {
    root = std::filesystem::path(::testing::TempDir()) / ::testing::UnitTest::GetInstance()->current_test_info()->name();
    std::filesystem::create_directories(root);
  }

  static std::string contents(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
  }
};

class TestReadFile : public Files {};

TEST_F(TestReadFile, AFileIsReadWholeAndAMissingOneIsEmpty) {
  std::ofstream(root / "a.txt", std::ios::binary) << "one\ntwo\n";
  EXPECT_EQ(ds::app::readFile((root / "a.txt").string()), "one\ntwo\n");
  EXPECT_EQ(ds::app::readFile((root / "missing.txt").string()), "");
}

class TestEmit : public Files {};

TEST_F(TestEmit, TheTextGoesToTheFileNamedOrToStdout) {
  EXPECT_EQ(ds::app::emit((root / "out.txt").string(), "a\nb\n"), 0);
  EXPECT_EQ(contents(root / "out.txt"), "a\nb\n");
  ::testing::internal::CaptureStdout();
  EXPECT_EQ(ds::app::emit("", "to stdout"), 0);
  EXPECT_EQ(::testing::internal::GetCapturedStdout(), "to stdout");
}

TEST_F(TestEmit, AFileThatCannotBeWrittenIsSaidAndExitsOne) {
  ::testing::internal::CaptureStderr();
  EXPECT_EQ(ds::app::emit((root / "no-such-dir" / "out.txt").string(), "x"), 1);
  EXPECT_NE(::testing::internal::GetCapturedStderr().find("cannot write"), std::string::npos);
}

class TestTitleOf : public Files {};

TEST_F(TestTitleOf, ThePageIsTitledAfterTheLastNameOfThePath) {
  EXPECT_EQ(ds::app::titleOf("examples/hola"), "hola");
  EXPECT_EQ(ds::app::titleOf("examples/hola//"), "hola");
  EXPECT_EQ(ds::app::titleOf("design"), "design");
}

class TestDiagramId : public Files {};

TEST_F(TestDiagramId, ADiagramIsNamedWithOrWithoutItsPrefix) {
  EXPECT_EQ(ds::app::diagramId("CL_Hola"), "diagram:CL_Hola");
  EXPECT_EQ(ds::app::diagramId("diagram:CL_Hola"), "diagram:CL_Hola");
}

}  // namespace
