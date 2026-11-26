#ifndef DRIFTSPEC_CHECK_SOLVER_HPP
#define DRIFTSPEC_CHECK_SOLVER_HPP

#include <string>
#include <unordered_set>
#include <vector>

#include "ds/check/Facts.hpp"
#include "ds/check/Variables.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace check {

/// Answers the body of a formula over the facts (REQ-00096-1, DS-380): every binding of its variables that makes each
/// literal true, the most bound ready literal first. It reads the facts and never the model, so the rule engine runs
/// over facts alone.
///
/// A binding is a vector with one slot per variable of the formula (DS-381), in the order Variables gives them: each
/// slot points at the text its variable is bound to, or is null while unbound. What a slot points at lives in the facts
/// or among the solver's own texts, and neither moves while the evaluation runs, so a candidate copies a few pointers
/// and never a string.
class Solver {
 public:
  explicit Solver(const Facts& facts) : facts_(facts) {}

  /// Every answer of `body` that extends `bindings`, bindings of `variables`; `first` stops at the first one.
  std::vector<std::vector<const std::string*>> solve(const std::string& owner, const std::vector<model::Literal>& body,
                                                     const Variables& variables, const std::vector<const std::string*>& bindings,
                                                     bool first) const;
  /// Whether a literal tests what is bound and binds nothing: a comparison (!=, <, <=, >, >=), a match against a pattern
  /// or a member's flag (DS-403); it tests only what positive literals bind.
  static bool compares(model::LiteralForm form);

 private:
  /// The answers of what is left of `body`, extending `work`: one binding the search binds, continues and undoes
  /// (DS-401), the literals done marked and unmarked; false once `first` has its answer.
  /// `slots` is the body as Variables compiled it: each term's slot, read without its name (DS-507).
  bool solveFrom(const std::string& owner, const std::vector<model::Literal>& body, const Variables::Slots& slots, std::vector<bool>& done,
                 std::vector<const std::string*>& work, bool first, std::vector<std::vector<const std::string*>>& out_answers) const;
  /// Whether a literal can be answered with what is bound; `slots` its terms' slots.
  static bool ready(const model::Literal& literal, const std::vector<int>& slots, const std::vector<const std::string*>& bindings);
  /// Each answer of one literal, as it reads positively, bound into `work` while `next` continues and undone after
  /// it, in the order the facts give them (DS-401); false when `next` stops the search. Nothing is copied per answer.
  template <class Next>
  bool answers(const std::string& owner, const model::Literal& literal, const std::vector<int>& slots,
               std::vector<const std::string*>& work, Next&& next) const;
  /// What a term stands for under `bindings`: a value as written, a bound variable (or a field of it), a constant;
  /// null for a variable not bound yet. `slot` is the term's, as compiled.
  const std::string* value(const std::string& owner, const model::Term& term, int slot,
                           const std::vector<const std::string*>& bindings) const;
  bool unify(const std::string& owner, const model::Term& term, int slot, const std::string* entity,
             std::vector<const std::string*>& out_bindings) const;
  /// A text the solver computes or reads from outside the facts, kept once among its own texts.
  const std::string* text(const std::string& text) const { return &*texts_.insert(text).first; }
  /// A number written as such, read whole: 150, -2.5; anything else is not one.
  static bool number(const std::string& text, double& out_number);
  /// A value written in a sequence: a number or a string, not a name.
  static bool literal(const std::string& text);
  /// Whether `a` and `z` stand in the comparison: numbers as numbers, anything else equal or not as written.
  static bool compare(model::LiteralForm form, const std::string& a, const std::string& z);

  const Facts& facts_;
  mutable std::unordered_set<std::string> texts_;  ///< what the solver binds from outside the facts: an element never moves
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_SOLVER_HPP
