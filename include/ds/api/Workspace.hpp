#ifndef DRIFTSPEC_API_WORKSPACE_HPP
#define DRIFTSPEC_API_WORKSPACE_HPP

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ds/check/Selection.hpp"
#include "ds/emit/Sink.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/query/Query.hpp"
#include "ds/service/UseCases.hpp"
#include "ds/theme/Theme.hpp"

namespace ds {
namespace api {

/// A workspace as text: each .ds file by its path, as an editor holds it (saved or not).
using Files = std::map<std::string, std::string>;

/// What a workspace is loaded to give (DS-376), which says which steps of the check its load runs: the findings (ds
/// check), the whole model (ds export, and a workspace kept for many questions), the answers of a query, the viewer
/// (its page or its scene) or one diagram drawn (svg, text, cells, the IR or its layout). The Api chooses the steps.
enum class Output { Findings, Model, Answers, Viewer, Drawing };

/// A workspace loaded once (DS-173): its files and its model, parsed, bound and checked, kept for as
/// many questions as the caller asks. Its questions are the engine's own: what the index answers in
/// one hop, a behaviour's tree, path and walk, what ds query answers and a diagram takes, a diagram
/// drawn and the viewer, and what changed from another version. Every answer is a document, a copy
/// written as JSON (or the drawing asked for): the caller can neither change the model nor be left
/// holding what a later load moves. Several may be loaded at once; each keeps its own model.
class Workspace {
 public:
  /// The files loaded in the language of `metamodel`, which outlives it (DS-341) and which it hands each reader of
  /// the model it asks (DS-351), for what it is to give (DS-376): every question, unless it is loaded to draw one
  /// diagram; then only the drawing of that diagram is an answer.
  static Workspace madeFor(const model::Metamodel& metamodel, const Files& files, Output output = Output::Model,
                           const std::string& diagram = "");
  /// A workspace that lives as long as its caller (DS-408): made by aggregating the use cases it serves (the factory,
  /// one reference to the metamodel, kept as it is given) and through them the metamodel, which outlives it; it holds
  /// no design until reload gives it files, and loads what each use case it runs needs.
  explicit Workspace(const service::UseCaseFactory& useCases);
  /// The workspace loaded again from the files given, for what it was loaded to give, without being made anew (DS-420):
  /// a file whose text is the one it holds is not parsed again; the model is bound and checked again whole. What a
  /// caller asked before is a copy and stays as it was.
  void reload(const Files& files);
  /// Run the use case of that name over the design it holds (DS-408), describing into the sink it is lent: the model is
  /// bound once per reload and checked with every step the use cases asked of it so far (a load cannot take more steps
  /// once checked, so one that needs more is bound again from the parsed files, never parsed again).
  service::Outcome run(const std::string& name, const service::Request& request, emit::ISink& sink);
  /// The steps its model is checked with: every one the use cases it ran asked for (DS-408).
  const check::Selection& selection() const { return selection_; }
  /// How many times it bound and checked its files: once a reload, and once more for a use case that needed more.
  std::size_t loads() const { return loads_; }
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

  /// A diagram drawn in a format (svg, text, cells, ir, layout); "" when there is no such diagram. A format that is
  /// none of them is a pair ds does not write: emit::NotImplemented, naming the diagram and the format (DS-398).
  std::string render(const std::string& diagram, const std::string& format, int width) const;
  /// The viewer as one page, framed on the identities given.
  std::string page(const std::string& title, const std::vector<std::string>& focus) const;

 private:
  /// The files given parsed, each one the workspace holds with the same text kept as it was parsed (DS-420).
  void parse(const Files& files);
  /// The files it holds, as parsed, in the order of their paths.
  std::vector<parse::ParsedFile> files() const;
  /// Texts as a JSON list; one text as a JSON string; relations as the export writes them.
  static std::string texts(const std::vector<std::string>& list);
  static std::string text(const std::string& value);
  std::string relations(const std::vector<model::Relation>& relations) const;
  /// A query's answers described in JSON, as ds query --json writes them (DS-398).
  static std::string answers(const std::vector<query::Answer>& answers);

  const service::UseCaseFactory useCases_;  ///< the use cases it serves, aggregated (DS-408)
  const model::Metamodel& metamodel_;
  const theme::Theme theme_;  ///< the style its drawings are written in, made once with it (DS-393)
  Output output_;
  std::string diagram_;
  bool given_ = false;  ///< made for an output (the doors before DS-409): every reload checks what that output reads
  std::map<std::string, parse::ParsedFile> parsed_;        ///< each file as it was last parsed, by its path
  check::Selection selection_ = check::Selection::of({});  ///< the steps its model is checked with
  bool stale_ = true;                                      ///< the files changed since its model was checked, or it was never checked
  std::size_t loads_ = 0;
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
