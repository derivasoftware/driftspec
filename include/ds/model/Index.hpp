#ifndef DRIFTSPEC_MODEL_INDEX_HPP
#define DRIFTSPEC_MODEL_INDEX_HPP

#include <cstddef>
#include <map>
#include <set>
#include <string>
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
  std::map<std::string, std::vector<std::size_t>> from;      ///< the relations that leave an identity, by position
  std::map<std::string, std::vector<std::size_t>> to;        ///< the relations that reach it
  std::map<std::string, std::vector<std::size_t>> kind;      ///< the relations of one kind
  std::map<std::string, std::set<std::string>> members;      ///< what each scope declares
  std::map<std::string, std::set<std::string>> revisions;    ///< the declared ids of an artefact, by its id without revision
  std::map<std::string, std::string> parent;                 ///< what holds each entity
  std::map<std::string, std::vector<std::string>> children;  ///< what each entity holds
  bool treeStale = true;                                     ///< the tree is built again at the next question
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
  /// The relations that leave or reach `id`, each once, in the order stated.
  std::vector<Relation> touching(const std::string& id) const;
  /// Every relation of one kind, in the order stated.
  std::vector<Relation> ofKind(const std::string& kind) const;
  /// Every relation of any of these kinds, in the order stated.
  std::vector<Relation> ofKinds(const std::vector<std::string>& kinds) const;
  /// Every relation, in the order stated: for a reader of the whole model (an export, a diff).
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
  /// The identity the workspace declares for an artefact written with any revision or with none;
  /// "" when nothing declares it.
  std::string revisionOf(const std::string& artefact) const;

 private:
  std::vector<Relation> at(const std::map<std::string, std::vector<std::size_t>>& table, const std::string& key) const;
  void tree() const;
  const Model& model_;
};

}  // namespace model
}  // namespace ds

#endif  // DRIFTSPEC_MODEL_INDEX_HPP
