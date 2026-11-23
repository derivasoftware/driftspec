#ifndef DRIFTSPEC_API_WORKSPACE_HPP
#define DRIFTSPEC_API_WORKSPACE_HPP

#include <map>
#include <string>
#include <vector>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace api {

/// A workspace as text: each .ds file by its path, as an editor holds it (saved or not).
using Files = std::map<std::string, std::string>;

/// A workspace loaded once (DS-173): its files and its model, parsed, bound and checked, kept for as
/// many questions as the caller asks. Its questions are the engine's own: what the index answers in
/// one hop, a behaviour's tree, path and walk, what ds query answers and a diagram takes, a diagram
/// drawn and the viewer, and what changed from another version. Every answer is a document, a copy
/// written as JSON (or the drawing asked for): the caller can neither change the model nor be left
/// holding what a later load moves. Several may be loaded at once; each keeps its own model.
class Workspace {
 public:
  /// The files loaded in the language of `metamodel`, which outlives it (DS-341).
  Workspace(const model::Metamodel& metamodel, const Files& files);
  /// The model it keeps, for a caller in this process.
  const model::Model& model() const { return model_; }

  /// The whole model, as ds export writes it; the check, as ds check --json writes it.
  std::string exportModel() const;
  std::string report() const;

  /// One entity, as the export writes it; "null" when nothing has that identity.
  std::string find(const std::string& id) const;
  /// The relations that leave, reach or touch an entity, as the export writes them, in the order stated.
  std::string from(const std::string& id) const;
  std::string to(const std::string& id) const;
  std::string touching(const std::string& id) const;
  /// Identities: what a scope declares, what holds an entity, what it holds; the declared revision.
  std::string members(const std::string& scope) const;
  std::string parent(const std::string& id) const;
  std::string children(const std::string& id) const;
  std::string revisionOf(const std::string& artefact) const;

  /// A behaviour's tree: its messages in the order written, its frames with what each holds.
  std::string tree(const std::string& sequence) const;
  /// The messages a scenario's path takes ("*" every branch, "" the default), and where each goes on.
  std::string steps(const std::string& sequence, const std::string& path) const;
  /// A behaviour walked into what its calls enter, down to a depth.
  std::string walk(const std::string& sequence, const std::string& path, int depth) const;

  /// A question of ds query, answered as ds query --json does; what a diagram takes.
  std::string ask(const std::string& question, const std::string& subject) const;
  std::string shows(const std::string& diagram) const;

  /// A diagram drawn in a format (svg, text, cells, ir, layout); "" when there is no such diagram.
  std::string render(const std::string& diagram, const std::string& format, int width) const;
  /// The viewer as one page, framed on the identities given.
  std::string page(const std::string& title, const std::vector<std::string>& focus) const;

 private:
  model::Model model_;
};

/// How an identity is written (model/Identity), asked from another language rather than copied
/// into it (DS-173).
class Identity {
 public:
  static std::string qualify(const std::string& scope, const std::string& name);
  static std::string outer(const std::string& scope);
  static std::string boxOf(const std::string& scope);
  static std::string packageOf(const std::string& scope);
  static std::string behaviourOf(const std::string& id);
  static std::string subjectOf(const std::string& id);
  static std::string scenarioOf(const std::string& id);
  static std::string withoutRevision(const std::string& id);
};

}  // namespace api
}  // namespace ds

#endif  // DRIFTSPEC_API_WORKSPACE_HPP
