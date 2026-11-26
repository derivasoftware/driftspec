#ifndef DRIFTSPEC_MODEL_INDEX_HPP
#define DRIFTSPEC_MODEL_INDEX_HPP

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ds {
namespace model {

class Model;
struct Relation;

/// What the model keeps beside its entities and relations so a question by identity needs no pass
/// over them (DS-171): where each relation is, by its ends and its kind; what each scope declares;
/// the declared revisions of each artefact; and what holds what, as a tree. The model keeps it in
/// step with every declare, relate and rename; nothing here is a fact of its own.
struct Indexed {
  std::map<std::string, std::vector<std::size_t>> from;    ///< the relations that leave an identity, by position
  std::map<std::string, std::vector<std::size_t>> to;      ///< the relations that reach it
  std::map<std::string, std::vector<std::size_t>> kind;    ///< the relations of one kind
  std::map<std::string, std::set<std::string>> members;    ///< what each scope declares
  std::map<std::string, std::set<std::string>> revisions;  ///< the declared ids of an artefact, by its id without revision
  std::map<std::pair<std::string, std::string>, std::set<std::string>>
      named;                                                 ///< the declared classes and enums, by package and simple name (DS-336)
  std::map<std::string, std::string> parent;                 ///< what holds each entity
  std::map<std::string, std::vector<std::string>> children;  ///< what each entity holds
  bool treeStale = true;                                     ///< the tree is built again at the next question
  // Built when first asked and dropped whenever the tree is built again (DS-506). The model keeps all of this in a
  // `mutable` member because a const reader's question fills it: ds reads a model from one thread only (the command
  // line runs its work on one worker thread, the Python face and the language server one call at a time), so no lock.
  std::unordered_map<std::string, std::pair<std::size_t, std::size_t>> span;  ///< each node's enter and exit, pre-order
  std::unordered_map<std::string, std::vector<std::string>> holders;          ///< each id's chain of holders, memoised
};

/// The one way into the model's relations (DS-171), whoever asks: the model knows none of its
/// readers. Every answer is a copy, in the order the model states the relations, so a reader can
/// neither change the model nor be left holding what a later change moves; and whoever asks here
/// says what a pass over the whole model said. Besides the relations: what a scope declares, what
/// holds what, and the declared revision of an artefact.
class Index {
 public:
  explicit Index(const Model& model) : model_(model) {}
  /// The relations that leave `id`, in the order stated.
  std::vector<Relation> from(const std::string& id) const;
  /// The relations that reach `id`, in the order stated.
  std::vector<Relation> to(const std::string& id) const;
  /// The relations of one kind that leave `id`, in the order stated: only those are copied (DS-345).
  std::vector<Relation> fromOfKind(const std::string& id, const std::string& kind) const;
  /// The relations of one kind that reach `id`, in the order stated: only those are copied (DS-345).
  std::vector<Relation> toOfKind(const std::string& id, const std::string& kind) const;
  /// The relations that leave or reach `id`, each once, in the order stated.
  std::vector<Relation> touching(const std::string& id) const;
  /// Every relation of one kind, in the order stated.
  std::vector<Relation> ofKind(const std::string& kind) const;
  /// Every relation of any of these kinds, in the order stated.
  std::vector<Relation> ofKinds(const std::vector<std::string>& kinds) const;
  /// Every relation, in the order stated: for a reader of the whole model (an export).
  std::vector<Relation> all() const;
  /// How many relations the model states.
  std::size_t count() const;
  /// The identities declared in `scope` (their scope is that one), expected ones included, by identity.
  std::vector<std::string> members(const std::string& scope) const;
  /// What holds `id`: the box a contains relation names, else its scope when the model declares it;
  /// "" at the top. A tree: a link that would close a cycle is left out, so every walk up ends.
  std::string parent(const std::string& id) const;
  /// What `id` holds, in the order the tree was built.
  std::vector<std::string> children(const std::string& id) const;
  /// What `id` holds that is an entity of `kind` (as the language writes it: class, module...), in the order of
  /// children (DS-506).
  std::vector<std::string> childrenOfKind(const std::string& id, const std::string& kind) const;
  /// Whether `ancestor` holds `id`, directly or through others, in the tree parent and children say: O(1) by a
  /// pre-order interval of each node, numbered when first asked after a change (DS-506). Nothing descends from itself,
  /// and an id the tree does not hold descends from nothing.
  bool descends(const std::string& id, const std::string& ancestor) const;
  /// What holds `id`, then what holds that, up to the top: parent after parent, `id` itself left out; empty at the
  /// top or for an id the tree does not hold. Memoised per id (DS-506): the reference holds until the model changes.
  const std::vector<std::string>& holders(const std::string& id) const;
  /// The identity the workspace declares for an artefact written with any revision or with none;
  /// "" when nothing declares it.
  std::string revisionOf(const std::string& artefact) const;
  /// The declared classes and enums named `name` (simple, without arity) in the package `package`, by identity;
  /// expected ones are not counted (DS-336).
  std::vector<std::string> named(const std::string& package, const std::string& name) const;

 private:
  std::vector<Relation> at(const std::map<std::string, std::vector<std::size_t>>& table, const std::string& key) const;
  std::vector<Relation> atOfKind(const std::map<std::string, std::vector<std::size_t>>& table, const std::string& key,
                                 const std::string& kind) const;
  void tree() const;
  void spans() const;
  const Model& model_;
};

}  // namespace model
}  // namespace ds

#endif  // DRIFTSPEC_MODEL_INDEX_HPP
