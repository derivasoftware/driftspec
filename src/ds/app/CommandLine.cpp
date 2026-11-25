#include "ds/app/CommandLine.hpp"

#include <CLI/CLI.hpp>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ds/emit/NotImplemented.hpp"

namespace ds {
namespace app {

CommandLine::CommandLine() : manual_(metamodel_), commands_(metamodel_, theme_) {}

int CommandLine::run(int argc, char** argv) const {
  try {
    return execute(argc, argv);
  } catch (const emit::NotImplemented& e) {
    // a document asked in a technology ds does not write it in: bad usage, as a wrong command line is (DS-398)
    std::cerr << "ds: " << e.what() << "\n";
    return 2;
  } catch (const service::BadRequest& e) {
    // a request its use case cannot take: what the command line gave it is wrong, bad usage too (DS-407)
    std::cerr << "ds: " << e.what() << "\n";
    return 2;
  } catch (const std::exception& e) {
    // a file that cannot be read, or anything else the input makes impossible: said, with the code of bad input, never
    // an abort (DS-279)
    std::cerr << "ds: " << e.what() << "\n";
    return 2;
  }
}

int CommandLine::execute(int argc, char** argv) const {
  CLI::App app{"ds: a software design as text, checked, drawn and asked", "ds"};
  app.set_version_flag("--version", "ds " + metamodel_.version());
  app.require_subcommand(1);
  app.footer("A workspace is a directory of .ds files read as one. Each command takes --help; ds manual tells the rest.");
  // each verb declares itself on its subcommand, and every one says its exit codes, from the one table the manual
  // prints too (DS-79)
  std::vector<std::pair<CLI::App*, std::unique_ptr<ICommand>>> verbs;
  for (const auto& name : CommandFactory::names()) {
    std::unique_ptr<ICommand> command = commands_.of(name);
    CLI::App* sub = app.add_subcommand(command->name(), command->summary());
    command->declare(*sub);
    if (const model::CommandSpec* spec = manual_.commandOf(command->name())) {
      std::string exits = "Exit codes:";
      for (const auto& e : spec->exits) exits += "\n  " + std::to_string(e.first) + "  " + e.second;
      sub->footer(exits);
    }
    verbs.emplace_back(sub, std::move(command));
  }
  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& e) {
    const int code = app.exit(e);
    return code == 0 ? 0 : 2;  // help is not a failure; a wrong command line is bad usage
  }
  for (const auto& [sub, command] : verbs) {
    if (!sub->parsed()) continue;
    const service::Outcome outcome = command->run();
    if (!outcome.message.empty()) std::cerr << outcome.message << "\n";  // why it did not end well, as ds has said it
    return exitOf(outcome);
  }
  return 2;  // CLI11 requires one verb: a parse that names none was said above
}

int CommandLine::exitOf(const service::Outcome& outcome) {
  // the exit table of design/ds/cli.ds: 0 when it went well, 1 for an end of the use case, 2 for bad usage
  static const std::map<std::string, int> codes = {
      {"default", 0},  {"errors", 1},    {"no-diagram", 1}, {"unknown-question", 1},
      {"no-topic", 1}, {"unwritten", 1}, {"bad-usage", 2},  {"no-workspace", 2},
  };
  const auto code = codes.find(outcome.name);
  return code == codes.end() ? 2 : code->second;
}

}  // namespace app
}  // namespace ds
