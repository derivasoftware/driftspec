#ifndef DRIFTSPEC_CHECK_FACTS_HPP
#define DRIFTSPEC_CHECK_FACTS_HPP

#include <cstddef>
#include <map>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
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
/// Every string a binding of the solver points at lives here or among the solver's own texts (DS-381): storage the
/// evaluation owns and that does not move while it runs. What holds what is copied from the model's index once, so
/// nothing the solver binds points into the model, which the evaluator writes breaches and tables into.
class Facts {
 public:
  Facts(const model::Model& model, const model::Metamodel& metamodel) : model_(model), metamodel_(metamodel) {}

  /// Reads the model's relations and declared entities into the joins a rule asks for, and copies what holds what
  /// from the model's index (DS-171), once.
  void index();

  /// The declared entities of a kind, by its name; nullptr when there are none.
  const std::vector<std::string>* ofKind(const std::string& kind) const {
    const auto it = byKind_.find(kind);
    return it == byKind_.end() ? nullptr : &it->second;
  }
  /// The declared entities of a kind inside a scope, by contains (DS-268 kept), in ofKind's order and as pointers into
  /// ofKind's own storage, so a binding points where it did when it walked ofKind (DS-503). Built from contains on
  /// the first ask for that kind and scope and kept while the facts last: a scoped range, nested or not, walks only
  /// what its scope holds instead of every entity of its kind again for each binding of the ranges before it.
  const std::vector<const std::string*>& ofKindIn(const std::string& kind, const std::string& scope) const;
  /// A relation's pairs from its source to its target; nullptr when the model has none of it.
  const std::multimap<std::string, std::string>* forward(const std::string& relation) const {
    const auto it = forward_.find(relation);
    return it == forward_.end() ? nullptr : &it->second;
  }
  /// The same pairs from the target to the source; the relation must have pairs.
  const std::multimap<std::string, std::string>& backward(const std::string& relation) const { return backward_.at(relation); }

  /// Records what a name written in a formula resolves to (the evaluator records what the validator resolved).
  void record(const std::string& owner, const std::string& written, const std::string& identity) {
    if (readers_) throw std::logic_error("the facts grow only between solves: a binding points into them");
    constants_[key(owner, written)] = identity;
  }
  /// What a name written in a formula resolves to; false when it was never resolved.
  bool constant(const std::string& owner, const std::string& written, std::string& out_identity) const {
    const auto it = constants_.find(key(owner, written));
    if (it == constants_.end()) return false;
    out_identity = it->second;
    return true;
  }
  /// What a name written in a formula resolves to, where the facts keep it; nullptr when it was never resolved.
  const std::string* constantOf(const std::string& owner, const std::string& written) const {
    const auto it = constants_.find(key(owner, written));
    return it == constants_.end() ? nullptr : &it->second;
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
  bool derive(const std::string& definition, const std::vector<std::string>& tuple) {
    if (readers_) throw std::logic_error("the facts grow only between solves: a binding points into them");
    return extension_[definition].insert(tuple).second;
  }
  /// A solve starts reading the facts, and ends; while one reads, the facts refuse to grow (DS-381).
  void reading() const { ++readers_; }
  void done() const { --readers_; }

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
  /// What an entity is, as a rule reads it with name(x), id(x) or kind(x): its name, its identity or the name of its
  /// kind; "" when the model holds none of that identity (DS-403).
  std::string propertyOf(const std::string& id, const std::string& property) const;
  /// Whether an entity carries a flag, as `m is abstract` reads it (DS-403).
  bool flagged(const std::string& id, const std::string& flag) const;
  /// Keeps the pattern a formula matches against, compiled once by the validator (DS-403).
  void learn(const std::string& owner, const std::string& written, std::regex pattern) {
    if (readers_) throw std::logic_error("the facts grow only between solves: a binding points into them");
    patterns_.insert_or_assign(key(owner, written), std::move(pattern));
  }
  /// The pattern a formula writes, as compiled; nullptr when it was never compiled.
  const std::regex* patternOf(const std::string& owner, const std::string& written) const {
    const auto it = patterns_.find(key(owner, written));
    return it == patterns_.end() ? nullptr : &it->second;
  }
  /// Whether the model keeps a rule: it has no breach.
  bool holds(const std::string& rule) const;
  /// What holds an entity, by contains, where the facts keep it; nullptr at the top.
  const std::string* parentOf(const std::string& id) const {
    const auto it = parent_.find(id);
    return it == parent_.end() ? nullptr : &it->second;
  }
  /// What an entity holds, by contains, where the facts keep it; nullptr when it holds nothing.
  const std::vector<std::string>* childrenOf(const std::string& id) const {
    const auto it = children_.find(id);
    return it == children_.end() ? nullptr : &it->second;
  }
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
  std::map<std::string, std::regex> patterns_;                               ///< owner and pattern as written, compiled
  std::map<std::string, std::set<std::vector<std::string>>> extension_;      ///< what each definition holds for
  std::unordered_map<std::string, std::string> parent_;                      ///< what holds each entity, copied from the index
  std::unordered_map<std::string, std::vector<std::string>> children_;       ///< what each entity holds, in the index's order
  /// kind and scope, to the entities of that kind the scope holds (DS-503); a new entry never moves the others
  mutable std::map<std::pair<std::string, std::string>, std::vector<const std::string*>> inScope_;
  mutable std::size_t readers_ = 0;  ///< solves reading now
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_FACTS_HPP
