#ifndef DRIFTSPEC_API_API_HPP
#define DRIFTSPEC_API_API_HPP

#include <map>
#include <string>
#include <vector>

#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace ds {
namespace api {

/// A workspace as text: each .ds file by its path, as an editor holds it (saved or not).
using Files = std::map<std::string, std::string>;

/// The engine for a caller in the same process (DS-84): text in, the JSON of the command line out,
/// so a Python face, a language server or the command line itself see one contract.
class Api {
 public:
  /// The findings of the workspace, then its counts: what `ds check --json` prints, built by report.
  static std::string check(const Files& files);
  /// The files parsed, bound and checked, findings in file and line order: how every caller loads a
  /// workspace, the command line from a directory and the Api from texts (DS-120).
  static model::Model load(const std::vector<parse::ParsedFile>& files);
  /// The findings, then the counts: entities declared, expected, relations, errors. The one shape of
  /// a check, printed by `ds check --json` and returned by check (DS-120).
  static std::string report(const model::Model& model, bool pretty = false);
  /// The model with its relations, findings and metamodel: what `ds export` prints.
  static std::string exportModel(const Files& files);
  /// What the grammar takes at a place of a text (0-based line and column): keywords, an
  /// annotation, a field; the words an editor completes with.
  static std::string expectedAt(const std::string& text, int line, int column);
  /// A diagram of the workspace drawn as text (box characters) or svg, wrapped to a width (0: none);
  /// empty when there is no such diagram.
  static std::string render(const Files& files, const std::string& diagram, const std::string& format, int width);
  /// The viewer as one page, framed on the identities given (a diagram's, an entity's).
  static std::string page(const Files& files, const std::string& title, const std::vector<std::string>& focus);
};

}  // namespace api
}  // namespace ds

#endif  // DRIFTSPEC_API_API_HPP
