#ifndef DRIFTSPEC_API_API_HPP
#define DRIFTSPEC_API_API_HPP

#include <map>
#include <string>
#include <vector>

#include "ds/api/Workspace.hpp"
#include "ds/check/Selection.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace ds {
namespace api {

/// The engine for a caller in the same process (DS-84): text in, the JSON of the command line out,
/// so a Python face, a language server or the command line itself see one contract. It reads the
/// language from the metamodel it is given, which outlives it (DS-341).
class Api {
 public:
  explicit Api(const model::Metamodel& metamodel) : metamodel_(metamodel) {}
  /// The findings of the workspace, then its counts: what `ds check --json` prints, built by report.
  std::string check(const Files& files) const;
  /// The files parsed, bound and checked, findings in file and line order: how every caller loads a
  /// workspace, the command line from a directory and the Api from texts (DS-120). The check runs only the
  /// steps its output reads (DS-376); `diagram` is the one a drawing draws.
  model::Model load(const std::vector<parse::ParsedFile>& files, Output output = Output::Model, const std::string& diagram = "") const;
  /// The steps of the check a load runs for what it is to give, over the model as bound: the one place that
  /// chooses them (DS-376). Every step, but for a drawing: the relations the linker makes, the calls behaviour
  /// typing makes and the stereotypes derivation makes, and the rules when the diagram is a table written as
  /// rows and columns, whose cells are the rules' answers.
  static check::Selection stepsFor(Output output, const model::Model& bound, const std::string& diagram);
  /// The findings, then the counts: entities declared, expected, relations, errors. The one shape of
  /// a check, printed by `ds check --json` and returned by check (DS-120).
  std::string report(const model::Model& model, bool pretty = false) const;
  /// The model with its relations, findings and metamodel: what `ds export` prints.
  std::string exportModel(const Files& files) const;
  /// What the grammar takes at a place of a text (0-based line and column): keywords, an
  /// annotation, a field; the words an editor completes with.
  static std::string expectedAt(const std::string& text, int line, int column);
  /// The severity table of the language (DS-379), as JSON: each severity's name, whether a finding of it fails, whether
  /// it is kept apart as coverage, and what an editor shows it as; in the order of Severity (DS-386).
  std::string severities() const;
  /// A diagram of the workspace drawn as text (box characters) or svg, wrapped to a width (0: none);
  /// empty when there is no such diagram.
  std::string render(const Files& files, const std::string& diagram, const std::string& format, int width) const;
  /// The viewer as one page, framed on the identities given (a diagram's, an entity's).
  std::string page(const Files& files, const std::string& title, const std::vector<std::string>& focus) const;

 private:
  const model::Metamodel& metamodel_;
};

}  // namespace api
}  // namespace ds

#endif  // DRIFTSPEC_API_API_HPP
