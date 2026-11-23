#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "ds/app/App.hpp"
#include "ds/model/Manual.hpp"
#include "ds/model/Metamodel.hpp"

namespace {

/// The commands of ds run in process, with the exit codes the manual gives them (DS-79).
class Commands : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  const ds::model::Manual book{metamodel};  // the manual, made from the metamodel as run makes it (DS-342)
  const std::string hola = std::string(DRIFTSPEC_EXAMPLES_DIR) + "/hola";

  /// A command line as main receives it.
  struct Line {
    std::vector<std::string> words;
    std::vector<char*> argv;
    explicit Line(std::vector<std::string> w) : words(std::move(w)) {
      for (auto& word : words) argv.push_back(word.data());
      argv.push_back(nullptr);
    }
    int argc() const { return static_cast<int>(words.size()); }
  };
};

class TestManual : public Commands {};

TEST_F(TestManual, APageIsPrintedAsTextOrJson) {
  ::testing::internal::CaptureStdout();
  EXPECT_EQ(ds::app::manual(metamodel, book, "grammar", false), 0);
  EXPECT_FALSE(::testing::internal::GetCapturedStdout().empty());
  ::testing::internal::CaptureStdout();
  EXPECT_EQ(ds::app::manual(metamodel, book, "class", true), 0);
  const auto page = nlohmann::json::parse(::testing::internal::GetCapturedStdout());
  EXPECT_EQ(page["topic"], "class");
  EXPECT_TRUE(page.contains("kind"));
}

TEST_F(TestManual, AnUnknownTopicListsTheTopicsAndExitsOne) {
  ::testing::internal::CaptureStderr();
  EXPECT_EQ(ds::app::manual(metamodel, book, "no-such-topic", false), 1);
  EXPECT_NE(::testing::internal::GetCapturedStderr().find("grammar"), std::string::npos);
}

class TestCheck : public Commands {};

TEST_F(TestCheck, ACleanWorkspaceChecksToZeroAndNoWorkspaceIsBadUsage) {
  ::testing::internal::CaptureStdout();
  EXPECT_EQ(ds::app::check(metamodel, hola, true), 0);
  const auto report = nlohmann::json::parse(::testing::internal::GetCapturedStdout());
  EXPECT_EQ(report["errors"], 0);
  ::testing::internal::CaptureStderr();
  EXPECT_EQ(ds::app::check(metamodel, hola + "/no-such-thing", false), 2);
  ::testing::internal::GetCapturedStderr();
}

class TestRun : public Commands {};

TEST_F(TestRun, TheCommandNamedIsRunAndAWrongCommandLineIsBadUsage) {
  Line query({"ds", "query", hola, "diagrams"});
  ::testing::internal::CaptureStdout();
  EXPECT_EQ(ds::app::run(query.argc(), query.argv.data()), 0);
  EXPECT_NE(::testing::internal::GetCapturedStdout().find("CL_Hola"), std::string::npos);
  Line wrong({"ds", "no-such-command"});
  ::testing::internal::CaptureStderr();
  EXPECT_EQ(ds::app::run(wrong.argc(), wrong.argv.data()), 2);
  ::testing::internal::GetCapturedStderr();
}

class TestWork : public Commands {};

TEST_F(TestWork, TheCommandIsRunToItsExitCode) {
  Line line({"ds", "manual", "grammar"});
  ::testing::internal::CaptureStdout();
  EXPECT_EQ(ds::app::work(line.argc(), line.argv.data()), 0);
  ::testing::internal::GetCapturedStdout();
}

TEST_F(TestWork, AFileThatCannotBeReadIsSaidWithTheCodeOfBadInput) {
  const auto root = std::filesystem::path(::testing::TempDir()) / "unreadable-workspace";
  std::filesystem::create_directories(root);
  const auto file = root / "a.ds";
  std::ofstream(file) << "package p {}\n";
  std::filesystem::permissions(file, std::filesystem::perms::none);
  if (std::ifstream(file)) GTEST_SKIP() << "this user reads any file";
  Line line({"ds", "check", root.string()});
  ::testing::internal::CaptureStderr();
  const int code = ds::app::work(line.argc(), line.argv.data());
  const std::string said = ::testing::internal::GetCapturedStderr();
  std::filesystem::permissions(file, std::filesystem::perms::owner_all);
  EXPECT_EQ(code, 2);
  EXPECT_NE(said.find("ds: cannot read"), std::string::npos);
}

class TestMain : public Commands {};

TEST_F(TestMain, TheCommandLineRunsOnItsOwnThreadToItsExitCode) {
  Line line({"ds", "check", hola});
  ::testing::internal::CaptureStdout();
  EXPECT_EQ(ds::app::main(line.argc(), line.argv.data()), 0);
  EXPECT_NE(::testing::internal::GetCapturedStdout().find("0 findings"), std::string::npos);
}

}  // namespace
