#ifndef DRIFTSPEC_API_API_HPP
#define DRIFTSPEC_API_API_HPP

#include <map>
#include <string>

namespace ds {
namespace api {

/// A workspace as text: each .ds file by its path, as an editor holds it (saved or not).
using Files = std::map<std::string, std::string>;

/// The engine for a caller in the same process (DS-84): text in, the JSON of the command line out,
/// so a Python face, a language server or the command line itself see one contract.
class Api {
 public:
  /// The findings of the workspace, then its counts: what `ds check --json` prints.
  static std::string check(const Files& files);
  /// The model with its relations, findings and metamodel: what `ds export` prints.
  static std::string exportModel(const Files& files);
  /// What the grammar takes at a place of a text (0-based line and column): keywords, an
  /// annotation, a field; the words an editor completes with.
  static std::string expectedAt(const std::string& text, int line, int column);
};

}  // namespace api
}  // namespace ds

#endif  // DRIFTSPEC_API_API_HPP
