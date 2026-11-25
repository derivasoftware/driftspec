#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "ds/app/App.hpp"
#include "ds/app/CommandLine.hpp"
#include "ds/model/Metamodel.hpp"

namespace {

/// The ds command line run in process (DS-409): one parse over every verb, the verb named run, and its outcome or its
/// failure turned into the exit code the manual gives it (DS-79), with what the outcome says on stderr.
class CommandLines : public ::testing::Test {
 protected:
  const std::string hola = std::string(DRIFTSPEC_EXAMPLES_DIR) + "/hola";
  std::filesystem::path root;

  void SetUp() override {
    root = std::filesystem::path(::testing::TempDir()) / ::testing::UnitTest::GetInstance()->current_test_info()->name();
    std::filesystem::create_directories(root / "empty");
  }

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

  /// What one command line ends in: its exit code, and what it wrote on stdout and on stderr.
  struct Ran {
    int code;
    std::string out;
    std::string err;
  };

  static Ran run(std::vector<std::string> words) {
    Line line(std::move(words));
    ::testing::internal::CaptureStdout();
    ::testing::internal::CaptureStderr();
    const int code = ds::app::CommandLine().run(line.argc(), line.argv.data());
    const std::string out = ::testing::internal::GetCapturedStdout();
    return {code, out, ::testing::internal::GetCapturedStderr()};
  }

  /// A workspace of one file whose call gives a parameter too many: an error among its findings.
  std::string broken() const {
    const auto dir = root / "broken";
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "broken.ds") << "package p {\n"
                                        "  module m {\n"
                                        "    class Leaf {\n"
                                        "      + work(a: int) -> int\n"
                                        "    }\n"
                                        "    class Root {\n"
                                        "      + run() -> int\n"
                                        "    }\n"
                                        "    sequence Root::run {\n"
                                        "      local leaf: Leaf\n"
                                        "      -> leaf.work(1, 2) -> x\n"
                                        "      return x\n"
                                        "    }\n"
                                        "  }\n"
                                        "}\n";
    return dir.string();
  }
};

class TestCommandLine : public CommandLines {};

TEST_F(TestCommandLine, EachOutcomeOfAVerbEndsInItsExitCode) {
  EXPECT_EQ(run({"ds", "check", hola}).code, 0);                                 // default
  EXPECT_EQ(run({"ds", "check", broken()}).code, 1);                             // errors
  EXPECT_EQ(run({"ds", "export", hola}).code, 0);                                // default
  EXPECT_EQ(run({"ds", "render", hola, "CL_Hola", "-f", "svg"}).code, 0);        // default
  EXPECT_EQ(run({"ds", "render", hola, "CL_Hola", "-f", "layout"}).code, 0);     // default, of see-design
  EXPECT_EQ(run({"ds", "query", hola, "diagrams"}).code, 0);                     // default
  EXPECT_EQ(run({"ds", "manual", "grammar"}).code, 0);                           // default
  const Ran missing = run({"ds", "render", hola, "CL_Nowhere", "-f", "cells"});  // no-diagram, of see-design
  EXPECT_EQ(missing.code, 1);
  EXPECT_EQ(missing.err, "no diagram named CL_Nowhere\n");
  EXPECT_EQ(run({"ds", "render", hola, "CL_Nowhere", "-f", "svg"}).code, 1);  // no-diagram, the door's drawing
  const Ran weather = run({"ds", "query", hola, "weather"});                  // unknown-question
  EXPECT_EQ(weather.code, 1);
  EXPECT_EQ(weather.out, "");
  EXPECT_EQ(weather.err.rfind("weather: not a question ds answers\nquestions: diagrams", 0), 0u) << weather.err;
  const Ran topic = run({"ds", "manual", "no-such-topic"});  // no-topic
  EXPECT_EQ(topic.code, 1);
  EXPECT_EQ(topic.err.rfind("no page 'no-such-topic' in the manual; there are: ", 0), 0u) << topic.err;
  const Ran which = run({"ds", "render", hola, "-f", "svg"});  // bad-usage, the door's
  EXPECT_EQ(which.code, 2);
  EXPECT_EQ(which.err, "which diagram? ds query " + hola + " diagrams lists them\n");
}

TEST_F(TestCommandLine, AWrongCommandLineIsBadUsageAndHelpIsNot) {
  EXPECT_EQ(run({"ds"}).code, 2);
  EXPECT_EQ(run({"ds", "no-such-command"}).code, 2);
  EXPECT_EQ(run({"ds", "check"}).code, 2);
  EXPECT_EQ(run({"ds", "render", hola, "-f", "yaml"}).code, 2);
  const Ran help = run({"ds", "render", "--help"});
  EXPECT_EQ(help.code, 0);
  EXPECT_NE(help.out.find("Exit codes:"), std::string::npos);  // every verb says its exit codes, from the manual
  const Ran version = run({"ds", "--version"});
  EXPECT_EQ(version.code, 0);
  EXPECT_EQ(version.out, "ds " + ds::model::Metamodel().version() + "\n");
}

TEST_F(TestCommandLine, APathThatIsNoWorkspaceIsSaidAndEveryVerbExitsTwo) {
  const std::string file = (root / "note.txt").string();
  std::ofstream(file) << "not a design\n";
  const std::string empty = (root / "empty").string();
  for (const std::string verb : {"check", "export", "query"}) {
    std::vector<std::string> words = {"ds", verb, file};
    if (verb == "query") words.push_back("diagrams");
    const Ran ran = run(words);
    EXPECT_EQ(ran.code, 2) << verb;
    EXPECT_EQ(ran.err, "cannot read " + file + ": not a directory\n") << verb;
    words[2] = empty;
    EXPECT_EQ(run(words).err, "no .ds file under " + empty + ": it is no workspace\n") << verb;
  }
  const std::string missing = (root / "missing").string();
  const Ran nothing = run({"ds", "render", missing, "-f", "svg"});
  EXPECT_EQ(nothing.code, 2);
  EXPECT_EQ(nothing.err, "cannot read " + missing + ": neither a workspace nor a render-IR document\n");
  EXPECT_EQ(run({"ds", "render", empty, "-f", "svg"}).code, 2);
}

TEST_F(TestCommandLine, AFileThatCannotBeReadIsSaidWithTheCodeOfBadInput) {
  const auto dir = root / "unreadable-workspace";
  std::filesystem::create_directories(dir);
  const auto file = dir / "a.ds";
  std::ofstream(file) << "package p {}\n";
  std::filesystem::permissions(file, std::filesystem::perms::none);
  if (std::ifstream(file)) {
    std::filesystem::permissions(file, std::filesystem::perms::owner_all);
    GTEST_SKIP() << "this user reads any file";
  }
  const Ran ran = run({"ds", "check", dir.string()});
  std::filesystem::permissions(file, std::filesystem::perms::owner_all);
  EXPECT_EQ(ran.code, 2);
  EXPECT_EQ(ran.err, "ds: cannot read " + file.string() + "\n");
}

TEST_F(TestCommandLine, AFileThatCannotBeWrittenIsSaidAndExitsOne) {
  const std::string to = (root / "no-such-dir" / "out.svg").string();
  const Ran ran = run({"ds", "render", hola, "CL_Hola", "-f", "svg", "-o", to});
  EXPECT_EQ(ran.code, 1);  // unwritten, the door's
  EXPECT_EQ(ran.err, "cannot write " + to + "\n");
  EXPECT_EQ(ran.out, "");
}

TEST_F(TestCommandLine, ADocumentIsWrittenToTheFileOutputNames) {
  const std::string to = (root / "scene.json").string();
  const Ran ran = run({"ds", "render", hola, "-f", "json", "-o", to});
  EXPECT_EQ(ran.code, 0);
  EXPECT_EQ(ran.out, "");
  std::ifstream in(to, std::ios::binary);
  const std::string written((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  EXPECT_EQ(written, run({"ds", "render", hola, "-f", "json"}).out);
}

class TestMain : public CommandLines {};

TEST_F(TestMain, TheCommandLineRunsOnItsOwnThreadToItsExitCode) {
  Line line({"ds", "check", hola});
  ::testing::internal::CaptureStdout();
  EXPECT_EQ(ds::app::main(line.argc(), line.argv.data()), 0);
  EXPECT_NE(::testing::internal::GetCapturedStdout().find("0 findings"), std::string::npos);
}

}  // namespace
