#ifndef DRIFTSPEC_CHECK_EVALUATOR_HPP
#define DRIFTSPEC_CHECK_EVALUATOR_HPP

#include <cstddef>
#include <set>
#include <string>
#include <vector>

#include "ds/check/Facts.hpp"
#include "ds/check/Resolver.hpp"
#include "ds/check/RuleValidator.hpp"
#include "ds/check/Solver.hpp"
#include "ds/check/Strata.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace check {

/// Evaluates the structural rules over the model (REQ-00096-1): Datalog with stratified negation over
/// a finite model, so every rule is decided. A definition is computed bottom-up to its fixpoint; a
/// rule is broken by every binding of its ranges where its holds has no answer, or its never has one.
/// It directs (DS-380): the facts read the model, the validator checks each formula, the strata order the
/// definitions and the solver answers bodies over the facts; it writes what the rules say into the model.
class Evaluator {
 public:
  /// Evaluates over `model`, written in `metamodel` (DS-351), asking `resolver` what a name means.
  Evaluator(model::Model& model, const model::Metamodel& metamodel, const Resolver& resolver);

  /// Validate every formula, compute the definitions and report each rule that the model breaks.
  /// Evaluate every rule: breaches go into the model, findings come back to the step that runs it.
  std::vector<model::Finding> run();

 private:
  /// How many findings a broken rule gets before the rest are counted in one; every breach is kept.
  static constexpr std::size_t kReportsPerRule = 10;

  void computeDefinitions();
  void evaluateRule(const std::string& id, const model::Formula& formula);
  /// A table (DS-268): a row per binding of its ranges that passes its where, a cell per column.
  void tabulate(const std::string& id, const model::Formula& formula);
  void report(model::Code code, const std::string& owner, const std::string& message, const model::Location& at);
  /// Bindings as the message of a finding writes them: x = a, y = b.
  static std::string describe(const std::map<std::string, std::string>& bindings);

  model::Model& model_;  ///< written: the breaches and the tables the rules say; read through the facts
  const model::Metamodel& metamodel_;
  Facts facts_;
  RuleValidator validator_;
  Strata strata_;
  Solver solver_;
  std::vector<model::Finding> found_;
  std::set<std::string> invalid_;  ///< formulas with an error: not evaluated
};

}  // namespace check
}  // namespace ds

#endif
