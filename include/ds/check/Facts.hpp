#ifndef DRIFTSPEC_CHECK_FACTS_HPP
#define DRIFTSPEC_CHECK_FACTS_HPP

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace check {

/// The model as facts a rule is evaluated over (DS-380): the declared entities of each kind, every relation both ways,
/// the names a rule writes resolved to identities, and what each definition holds for. It is the one reader of the
/// model for the evaluator's pieces: the solver, the validator and the strata ask it, never the model. Only the
/// evaluator, which directs, writes into it: the constants the validator resolves and what the definitions derive.
class Facts {
 public:
  Facts(const model::Model& model, const model::Metamodel& metamodel) : model_(model), metamodel_(metamodel) {}

  /// Reads the model's relations and declared entities into the joins a rule asks for (what holds what stays the
  /// model's own index, DS-171).
  void index();

  /// The declared entities of a kind, by its name; nullptr when there are none.
  const std::vector<std::string>* ofKind(const std::string& kind) const {
    const auto it = byKind_.find(kind);
    return it == byKind_.end() ? nullptr : &it->second;
  }
  /// A relation's pairs from its source to its target; nullptr when the model has none of it.
  const std::multimap<std::string, std::string>* forward(const std::string& relation) const {
    const auto it = forward_.find(relation);
    return it == forward_.end() ? nullptr : &it->second;
  }
  /// The same pairs from the target to the source; the relation must have pairs.
  const std::multimap<std::string, std::string>& backward(const std::string& relation) const { return backward_.at(relation); }

  /// Records what a name written in a formula resolves to (the evaluator records what the validator resolved).
  void record(const std::string& owner, const std::string& written, const std::string& identity) {
    constants_[key(owner, written)] = identity;
  }
  /// What a name written in a formula resolves to; false when it was never resolved.
  bool constant(const std::string& owner, const std::string& written, std::string& out_identity) const {
    const auto it = constants_.find(key(owner, written));
    if (it == constants_.end()) return false;
    out_identity = it->second;
    return true;
  }
  /// The definition a formula applies by the name it writes; "" when it names none.
  std::string definitionOf(const std::string& owner, const std::string& name) const {
    std::string out;
    constant(owner, "call:" + name, out);
    return out;
  }
  /// What a definition holds for, as derived so far; nullptr when nothing is derived for it.
  const std::set<std::vector<std::string>>* derived(const std::string& definition) const {
    const auto it = extension_.find(definition);
    return it == extension_.end() ? nullptr : &it->second;
  }
  /// Adds a tuple a definition holds for; true when it is new.
  bool derive(const std::string& definition, const std::vector<std::string>& tuple) { return extension_[definition].insert(tuple).second; }

  /// The formulas of the model, by the identity of the rule, definition or table that writes each.
  const std::map<std::string, model::Formula>& formulas() const { return model_.formulas(); }
  /// Whether an entity is declared and of a kind of the language.
  bool kindIs(const std::string& id, model::Kind kind) const;
  /// The kind of an entity; false when the model holds none of that identity.
  bool kindOf(const std::string& id, model::Kind& out_kind) const;
  /// Whether an entity is declared (not expected) and of the kind a rule names.
  bool isOfKind(const std::string& id, const std::string& kind) const;
  /// The scope an entity is declared in; "" when the model holds none of that identity.
  std::string scopeOf(const std::string& id) const;
  /// Where an entity is written; empty when the model holds none of that identity.
  model::Location locationOf(const std::string& id) const;
  /// A field of an entity: its name, its identity or a field it writes; "" when the model holds none.
  std::string fieldOf(const std::string& id, const std::string& field) const;
  /// Whether the model keeps a rule: it has no breach.
  bool holds(const std::string& rule) const;
  /// What holds an entity, by contains; "" at the top.
  std::string parentOf(const std::string& id) const { return model_.index().parent(id); }
  /// What an entity holds, by contains.
  std::vector<std::string> childrenOf(const std::string& id) const { return model_.index().children(id); }
  /// Whether `outer` holds `inner`, by contains, else by the box an artefact was written in (DS-268).
  bool contains(const std::string& outer, const std::string& inner) const;
  /// What a call passes, as the sequence writes it: each parameter of the operation it calls with the argument in
  /// its place, in order; empty when the call or what it calls is not in the model.
  std::vector<std::pair<std::string, std::string>> argsTo(const std::string& call) const;

 private:
  /// The key of a name an owner writes.
  static std::string key(const std::string& owner, const std::string& written) { return owner + "\n" + written; }

  const model::Model& model_;
  const model::Metamodel& metamodel_;
  std::map<std::string, std::vector<std::string>> byKind_;                   ///< the declared entities of each kind name
  std::map<std::string, std::multimap<std::string, std::string>> forward_;   ///< relation, from, to
  std::map<std::string, std::multimap<std::string, std::string>> backward_;  ///< relation, to, from
  std::map<std::string, std::string> constants_;                             ///< owner and name as written, to identity
  std::map<std::string, std::set<std::vector<std::string>>> extension_;      ///< what each definition holds for
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_FACTS_HPP
