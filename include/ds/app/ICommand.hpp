#ifndef DRIFTSPEC_APP_ICOMMAND_HPP
#define DRIFTSPEC_APP_ICOMMAND_HPP

#include <CLI/CLI.hpp>
#include <string>

#include "ds/service/UseCase.hpp"

namespace ds {
namespace app {

/// One verb of ds (DS-409): check, export, render, query or manual. It declares its arguments, options and flags on
/// the subcommand the command line gives it, and keeps what the parse writes into them; after the parse it is run and
/// says how it ended: its use case's outcome, or the door's own (no-workspace, bad-usage, unwritten) when no use case
/// could run. It knows no exit code and writes nothing on stderr: the command line words both.
class ICommand {
 public:
  virtual ~ICommand() = default;
  /// The verb, as it is typed: check, export...
  virtual std::string name() const = 0;
  /// What it does, in the line ds --help lists it with.
  virtual std::string summary() const = 0;
  /// Its arguments, options and flags added to its subcommand, each parsed into a member of its own.
  virtual void declare(CLI::App& sub) = 0;
  /// The verb done, over what the parse gave it: how it ended, with what to say when it did not end well.
  virtual service::Outcome run() const = 0;
};

}  // namespace app
}  // namespace ds

#endif  // DRIFTSPEC_APP_ICOMMAND_HPP
