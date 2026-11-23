#ifndef DRIFTSPEC_API_BROWSER_HPP
#define DRIFTSPEC_API_BROWSER_HPP

#include <string>

#include "ds/model/Metamodel.hpp"

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
  const model::Metamodel& metamodel_;
};

}  // namespace api
}  // namespace ds

#endif
