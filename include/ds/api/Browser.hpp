#ifndef DRIFTSPEC_API_BROWSER_HPP
#define DRIFTSPEC_API_BROWSER_HPP

#include <string>

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
  static std::string answer(const std::string& request);
};

}  // namespace api
}  // namespace ds

#endif
