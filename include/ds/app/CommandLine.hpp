#ifndef DRIFTSPEC_APP_COMMANDLINE_HPP
#define DRIFTSPEC_APP_COMMANDLINE_HPP

#include "ds/app/CommandFactory.hpp"
#include "ds/model/Manual.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/service/UseCase.hpp"
#include "ds/theme/Theme.hpp"

namespace ds {
namespace app {

/// The ds command line (DS-409): one CLI11 application owns the parse, the help of every verb, --version and the
/// footers, so every help text and every message about a wrong command line is the one it has always been; the verb
/// the parse names is run, and its outcome or its failure is turned into ds's exit code here and nowhere else (the
/// exit table of design/ds/cli.ds), with what the outcome says written on stderr.
class CommandLine {
 public:
  /// The language, its manual and the style of the drawings, made once where the command starts (DS-341, DS-342,
  /// DS-393), and the factory of the verbs over them.
  CommandLine();
  /// The command line given, parsed and run: its exit code. What cannot be read, a pair ds does not write
  /// (NotImplemented, DS-398) or a request a use case cannot take (BadRequest) is said, exit 2, never an abort (DS-279).
  int run(int argc, char** argv) const;

 private:
  /// The verb the command line names, run: its exit code; a wrong command line is said by CLI11, exit 2.
  int execute(int argc, char** argv) const;
  /// The exit code of an outcome, by its name: 0 default; 1 errors, no-diagram, unknown-question, no-topic and
  /// unwritten; 2 the door's bad-usage and no-workspace.
  static int exitOf(const service::Outcome& outcome);

  const model::Metamodel metamodel_;
  const model::Manual manual_;
  const theme::Theme theme_;
  const CommandFactory commands_;
};

}  // namespace app
}  // namespace ds

#endif  // DRIFTSPEC_APP_COMMANDLINE_HPP
