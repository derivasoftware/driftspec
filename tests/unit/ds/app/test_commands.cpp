#include <gtest/gtest.h>

#include <CLI/CLI.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "ds/app/CommandFactory.hpp"
#include "ds/app/ICommand.hpp"
#include "ds/emit/NotImplemented.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/theme/Theme.hpp"

namespace {

/// Each verb of ds as CommandFactory makes it (DS-409): declared on a subcommand, parsed, run over a workspace it opens,
/// what it was asked for on stdout, and its use case's outcome, or the door's own, with what that outcome says.
class Commands : public ::testing::Test {
 protected:
  const ds::model::Metamodel metamodel;
  const ds::theme::Theme theme;
  const ds::app::CommandFactory factory{metamodel, theme};
  const std::string hola = std::string(DRIFTSPEC_EXAMPLES_DIR) + "/hola";

  /// How a verb ended, and what it wrote on stdout.
  struct Ended {
    ds::service::Outcome outcome;
    std::string out;
  };

  /// The verb `words` names, declared on its subcommand as the command line declares it, parsed and run.
  Ended run(const std::vector<std::string>& words) const {
    const std::unique_ptr<ds::app::ICommand> command = factory.of(words.front());
    CLI::App app{"ds"};
    command->declare(*app.add_subcommand(command->name(), command->summary()));
    std::vector<std::string> reversed(words.rbegin(), words.rend());  // CLI11 takes a vector backwards
    app.parse(reversed);
    ::testing::internal::CaptureStdout();
    const ds::service::Outcome outcome = command->run();
    return {outcome, ::testing::internal::GetCapturedStdout()};
  }
};

class TestCommandFactory : public Commands {};

TEST_F(TestCommandFactory, TheFactoryNamesEveryVerbInTheOrderOfTheHelpAndAnyOtherIsNotImplemented) {
  EXPECT_EQ(ds::app::CommandFactory::names(), (std::vector<std::string>{"check", "export", "render", "query", "manual"}));
  for (const auto& name : ds::app::CommandFactory::names()) {
    EXPECT_EQ(factory.of(name)->name(), name);
    EXPECT_FALSE(factory.of(name)->summary().empty()) << name;
  }
  EXPECT_THROW(factory.of("teleport"), ds::emit::NotImplemented);
}

class TestCheckCommand : public Commands {};

TEST_F(TestCheckCommand, AWorkspaceIsCheckedAndAPathThatHoldsNoDesignIsTheDoorsBadUsage) {
  const Ended checked = run({"check", hola, "--json"});
  EXPECT_EQ(checked.outcome.name, "default");
  EXPECT_EQ(nlohmann::json::parse(checked.out)["errors"], 0);
  const Ended none = run({"check", hola + "/no-such-thing"});
  EXPECT_EQ(none.outcome.name, "bad-usage");
  EXPECT_EQ(none.outcome.message, "cannot read " + hola + "/no-such-thing: not a directory");
  EXPECT_EQ(none.out, "");
}

class TestExportCommand : public Commands {};

TEST_F(TestExportCommand, TheModelIsWrittenAsJsonAndAPathThatHoldsNoDesignIsNoWorkspace) {
  const Ended exported = run({"export", hola});
  EXPECT_EQ(exported.outcome.name, "default");
  EXPECT_EQ(nlohmann::json::parse(exported.out)["version"], metamodel.version());
  EXPECT_EQ(run({"export", hola + "/no-such-thing"}).outcome.name, "no-workspace");
}

class TestRenderCommand : public Commands {};

TEST_F(TestRenderCommand, ADiagramIsDrawnAndOneTheWorkspaceLacksIsNoDiagram) {
  const Ended drawn = run({"render", hola, "CL_Hola", "-f", "svg"});
  EXPECT_EQ(drawn.outcome.name, "default");
  EXPECT_EQ(drawn.out.rfind("<svg", 0), 0u);
  const Ended cells = run({"render", hola, "CL_Hola", "-f", "cells"});  // see-design's
  EXPECT_EQ(cells.outcome.name, "default");
  EXPECT_TRUE(nlohmann::json::parse(cells.out).is_object());
  for (const std::string format : {"cells", "svg", "html"}) {
    const Ended missing = run({"render", hola, "CL_Nowhere", "-f", format});
    EXPECT_EQ(missing.outcome.name, "no-diagram") << format;
    EXPECT_EQ(missing.outcome.message, "no diagram named CL_Nowhere") << format;
    EXPECT_EQ(missing.out, "") << format;
  }
  const Ended which = run({"render", hola, "-f", "text"});
  EXPECT_EQ(which.outcome.name, "bad-usage");
}

TEST_F(TestRenderCommand, ARenderIrDocumentIsDrawnAsItCameAndTheViewerNeedsAWorkspace) {
  const Ended ir = run({"render", hola, "CL_Hola", "-f", "ir"});
  ASSERT_EQ(ir.outcome.name, "default");
  const auto file = std::filesystem::path(::testing::TempDir()) / "hola.ir.json";
  std::ofstream(file, std::ios::binary) << ir.out;
  EXPECT_EQ(run({"render", file.string(), "-f", "svg"}).outcome.name, "default");
  const Ended viewer = run({"render", file.string(), "-f", "html"});
  EXPECT_EQ(viewer.outcome.name, "bad-usage");
  EXPECT_EQ(viewer.outcome.message, "a render-IR document draws as svg, text, cells or layout; the viewer needs a workspace");
}

class TestQueryCommand : public Commands {};

TEST_F(TestQueryCommand, AQuestionIsAnsweredAndAnUnknownOneSaysWhatIsAskedAsJsonToo) {
  const Ended answered = run({"query", hola, "diagrams"});
  EXPECT_EQ(answered.outcome.name, "default");
  EXPECT_NE(answered.out.find("CL_Hola"), std::string::npos);
  const Ended unknown = run({"query", hola, "weather"});
  EXPECT_EQ(unknown.outcome.name, "unknown-question");
  EXPECT_EQ(unknown.outcome.message.rfind("weather: not a question ds answers\nquestions: ", 0), 0u);
  EXPECT_EQ(unknown.out, "");  // as text, the error is only said
  const Ended asJson = run({"query", hola, "weather", "--json"});
  EXPECT_EQ(asJson.outcome.name, "unknown-question");
  EXPECT_EQ(nlohmann::json::parse(asJson.out)[0]["kind"], "error");  // asked for JSON, the error is an answer too
}

class TestManualCommand : public Commands {};

TEST_F(TestManualCommand, APageIsPrintedAsTextOrJsonAndAnUnknownTopicListsTheTopics) {
  const Ended page = run({"manual", "grammar"});
  EXPECT_EQ(page.outcome.name, "default");
  EXPECT_FALSE(page.out.empty());
  EXPECT_EQ(page.out.back(), '\n');
  const Ended json = run({"manual", "class", "--json"});
  EXPECT_EQ(json.outcome.name, "default");
  const auto described = nlohmann::json::parse(json.out);
  EXPECT_EQ(described["topic"], "class");
  EXPECT_TRUE(described.contains("kind"));
  EXPECT_EQ(described["text"], run({"manual", "class"}).out);  // the page printed is the page described
  const Ended none = run({"manual", "no-such-topic"});
  EXPECT_EQ(none.outcome.name, "no-topic");
  EXPECT_NE(none.outcome.message.find(" grammar"), std::string::npos);
  EXPECT_EQ(none.out, "");
}

}  // namespace
