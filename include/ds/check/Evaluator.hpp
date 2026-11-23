#ifndef DRIFTSPEC_CHECK_EVALUATOR_HPP
#define DRIFTSPEC_CHECK_EVALUATOR_HPP

#include <map>
#include <set>
#include <string>
#include <vector>

#include "ds/check/Resolver.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace check {

/// Evaluates the structural rules over the model (REQ-00096-1): Datalog with stratified negation over
/// a finite model, so every rule is decided. A definition is computed bottom-up to its fixpoint; a
/// rule is broken by every binding of its ranges where its holds has no answer, or its never has one.
class Evaluator {
 public:
  Evaluator(model::Model& model, const Resolver& resolver);

  /// Validate every formula, compute the definitions and report each rule that the model breaks.
  /// Evaluate every rule: breaches go into the model, findings come back to the step that runs it.
  std::vector<model::Finding> run();

 private:
  void index();
  bool validate(const std::string& id, const model::Formula& formula);
  bool validateBody(const std::string& id, const std::vector<model::Literal>& body, std::set<std::string>& out_bound);
  /// A relation literal whose bound end is a kind the relation never joins there (W-RULE-NEVER-MATCHES).
  void checkVacuity(const std::string& id, const model::Formula& formula);
  bool stratify();
  void computeDefinitions();
  /// The definitions of one stratum, each after the ones of the stratum it applies; empty when one of them reaches
  /// itself through the others (recursion), which is fixed by passes to its fixpoint instead (DS-338).
  std::vector<std::string> dependencyOrder(const std::vector<std::string>& definitions) const;
  void evaluateRule(const std::string& id, const model::Formula& formula);
  /// A table (DS-268): a row per binding of its ranges that passes its where, a cell per column.
  void tabulate(const std::string& id, const model::Formula& formula);

  /// Every answer of `body` that extends `bindings`; `first` stops at the first one.
  std::vector<std::map<std::string, std::string>> solve(const std::string& owner, const std::vector<model::Literal>& body,
                                                        const std::map<std::string, std::string>& bindings, bool first) const;
  void solveFrom(const std::string& owner, const std::vector<model::Literal>& body, std::vector<bool> done,
                 const std::map<std::string, std::string>& bindings, bool first,
                 std::vector<std::map<std::string, std::string>>& out_answers) const;
  bool ready(const model::Literal& literal, const std::map<std::string, std::string>& bindings) const;
  /// The bindings that extend `bindings` with an answer of one positive literal.
  std::vector<std::map<std::string, std::string>> answers(const std::string& owner, const model::Literal& literal,
                                                          const std::map<std::string, std::string>& bindings) const;
  bool value(const std::string& owner, const model::Term& term, const std::map<std::string, std::string>& bindings,
             std::string& out_value) const;
  bool unify(const std::string& owner, const model::Term& term, const std::string& entity,
             std::map<std::string, std::string>& out_bindings) const;
  std::string definitionOf(const std::string& owner, const std::string& name) const;
  std::string scopeOf(const std::string& owner) const;
  bool contains(const std::string& outer, const std::string& inner) const;
  void report(const std::string& code, const std::string& owner, const std::string& message, const model::Location& at);

  model::Model& model_;
  std::vector<model::Finding> found_;
  const Resolver& resolver_;
  std::map<std::string, std::vector<std::string>> byKind_;                   ///< the declared entities of each kind name
  std::map<std::string, std::multimap<std::string, std::string>> forward_;   ///< relation, from, to
  std::map<std::string, std::multimap<std::string, std::string>> backward_;  ///< relation, to, from
  std::map<std::string, std::string> constants_;                             ///< owner and name as written, to identity
  std::map<std::string, std::set<std::vector<std::string>>> extension_;      ///< what each definition holds for
  std::map<std::string, int> stratum_;
  std::set<std::string> invalid_;  ///< formulas with an error: not evaluated
};

}  // namespace check
}  // namespace ds

#endif
