#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/app/App.hpp"
#include "ds/app/CommandLine.hpp"
#include "ds/model/Metamodel.hpp"

namespace {

/// The command line (DS-409): one CLI11 application declares every verb, parses, runs the verb named and turns its
/// outcome into the exit code; main runs it on a thread with a large stack (DS-279).
class CommandLines : public ::testing::Test {
 protected:
  const std::string hola = std::string(DRIFTSPEC_EXAMPLES_DIR) + "/hola";

  /// What one command line ends in: its exit code, and what it wrote on stdout and on stderr.
  struct Ran {
    int code;
    std::string out;
    std::string err;
  };

  static Ran run(const std::vector<std::string>& words) {
    ::testing::internal::CaptureStdout();
    ::testing::internal::CaptureStderr();
    const int code = ds::app::CommandLine().run(words);
    const std::string out = ::testing::internal::GetCapturedStdout();
    return {code, out, ::testing::internal::GetCapturedStderr()};
  }
};

class TestCommandLine : public CommandLines {};

TEST_F(TestCommandLine, AWrongCommandLineIsBadUsageAndHelpAndTheVersionAreNot) {
  EXPECT_EQ(run({"ds"}).code, 2);
  EXPECT_EQ(run({"ds", "no-such-command"}).code, 2);
  const Ran help = run({"ds", "--help"});
  EXPECT_EQ(help.code, 0);
  for (const std::string verb : {"check", "export", "render", "query", "manual"})  // every verb, in the order of the factory
    EXPECT_NE(help.out.find("  " + verb), std::string::npos) << verb;
  EXPECT_NE(run({"ds", "query", "--help"}).out.find("Exit codes:"), std::string::npos);  // each says its exit codes
  EXPECT_EQ(run({"ds", "--version"}).out, "ds " + ds::model::Metamodel().version() + "\n");
}

TEST_F(TestCommandLine, TheVerbNamedRunsAndItsOutcomeIsItsExitCodeWithWhatItSaysOnStderr) {
  const Ran answered = run({"ds", "query", hola, "diagrams"});
  EXPECT_EQ(answered.code, 0);
  EXPECT_NE(answered.out.find("CL_Hola"), std::string::npos);
  EXPECT_EQ(answered.err, "");
  const Ran topic = run({"ds", "manual", "no-such-topic"});
  EXPECT_EQ(topic.code, 1);
  EXPECT_EQ(topic.out, "");
  EXPECT_EQ(topic.err.rfind("no page 'no-such-topic' in the manual; there are: ", 0), 0u) << topic.err;
  EXPECT_EQ(topic.err.back(), '\n');
}

class TestMain : public CommandLines {};

TEST_F(TestMain, TheCommandLineRunsOnItsOwnThreadToItsExitCode) {
  std::vector<std::string> words = {"ds", "check", hola};
  std::vector<char*> argv;
  for (auto& word : words) argv.push_back(word.data());
  argv.push_back(nullptr);
  ::testing::internal::CaptureStdout();
  EXPECT_EQ(ds::app::main(static_cast<int>(words.size()), argv.data()), 0);
  EXPECT_NE(::testing::internal::GetCapturedStdout().find("0 findings"), std::string::npos);
}

}  // namespace
