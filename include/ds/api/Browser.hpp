#ifndef DRIFTSPEC_API_BROWSER_HPP
#define DRIFTSPEC_API_BROWSER_HPP

#include <string>

#include "ds/api/Workspace.hpp"
#include "ds/emit/Sink.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace api {

/// The engine for a page in a browser (REQ-00113-1): one call per edit. The request is
/// {"files": {"<path>": "<text>", ...}, "width": 0}; the answer is one document, a copy
/// (CST-00009-1): the report of ds check --json, the identities the workspace declares and where
/// each is written, its artefacts as rows of a table, and its diagrams in the order they are
/// written, each drawn as svg with what it asks for. A request that cannot be read is answered
/// {"error": "..."}.
class Browser {
 public:
  /// Answers in the language of `metamodel`, which outlives it (DS-341).
  explicit Browser(const model::Metamodel& metamodel) : metamodel_(metamodel) {}
  std::string answer(const std::string& request) const;

 private:
  /// The answer to a request, written into the sink it is lent: a page of the manual, or the workspace's
  /// artefacts, diagrams, identities, locations and report, each record's fields in byte order.
  void respond(const std::string& request, emit::ISink& out_sink) const;
  /// The artefacts as rows: what each one refines, what keeps it, what proves it.
  void artefacts(const model::Model& model, emit::ISink& out_sink) const;
  /// The diagrams in the order they are written, each drawn as svg with what it takes.
  void diagrams(const Workspace& workspace, int width, emit::ISink& out_sink) const;

  const model::Metamodel& metamodel_;
};

}  // namespace api
}  // namespace ds

#endif
