#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "ds/app/Output.hpp"

namespace {

/// Where a command writes what it was asked for (DS-283, DS-409): the file -o names, or stdout.
class Outputs : public ::testing::Test {
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

class TestOutput : public Outputs {};

TEST_F(TestOutput, TheTextAndItsEndGoToTheFileNamedOrToStdout) {
  EXPECT_TRUE(ds::app::Output((root / "out.json").string()).write("{}", "\n"));
  EXPECT_EQ(contents(root / "out.json"), "{}\n");
  EXPECT_TRUE(ds::app::Output((root / "out.json").string()).write("a\r\nb"));  // written again, whole, as bytes
  EXPECT_EQ(contents(root / "out.json"), "a\r\nb");
  ::testing::internal::CaptureStdout();
  EXPECT_TRUE(ds::app::Output().write("to stdout", "\n"));
  EXPECT_EQ(::testing::internal::GetCapturedStdout(), "to stdout\n");
}

TEST_F(TestOutput, AFileThatCannotBeWrittenIsNotWritten) {
  const ds::app::Output output((root / "no-such-dir" / "out.txt").string());
  EXPECT_FALSE(output.write("x"));
  EXPECT_EQ(output.file(), (root / "no-such-dir" / "out.txt").string());
  EXPECT_FALSE(std::filesystem::exists(root / "no-such-dir"));
}

}  // namespace
