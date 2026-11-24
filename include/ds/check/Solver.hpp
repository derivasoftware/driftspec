#ifndef DRIFTSPEC_CHECK_SOLVER_HPP
#define DRIFTSPEC_CHECK_SOLVER_HPP

#include <map>
#include <string>
#include <vector>

#include "ds/check/Facts.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace check {

/// Answers the body of a formula over the facts (REQ-00096-1, DS-380): every binding of its variables that makes each
/// literal true, the most bound ready literal first. It reads the facts and never the model, so the rule engine runs
/// over facts alone.
class Solver {
 public:
  explicit Solver(const Facts& facts) : facts_(facts) {}

  /// Every answer of `body` that extends `bindings`; `first` stops at the first one.
  std::vector<std::map<std::string, std::string>> solve(const std::string& owner, const std::vector<model::Literal>& body,
                                                        const std::map<std::string, std::string>& bindings, bool first) const;
  /// Whether a literal compares (!=, <, <=, >, >=): it tests only what positive literals bind.
  static bool compares(model::LiteralForm form);

 private:
  void solveFrom(const std::string& owner, const std::vector<model::Literal>& body, std::vector<bool> done,
                 const std::map<std::string, std::string>& bindings, bool first,
                 std::vector<std::map<std::string, std::string>>& out_answers) const;
  /// Whether a literal can be answered with what is bound.
  bool ready(const model::Literal& literal, const std::map<std::string, std::string>& bindings) const;
  /// The bindings that extend `bindings` with an answer of one positive literal.
  std::vector<std::map<std::string, std::string>> answers(const std::string& owner, const model::Literal& literal,
                                                          const std::map<std::string, std::string>& bindings) const;
  /// What a term stands for under `bindings`: a value as written, a bound variable (or a field of it), a constant.
  bool value(const std::string& owner, const model::Term& term, const std::map<std::string, std::string>& bindings,
             std::string& out_value) const;
  bool unify(const std::string& owner, const model::Term& term, const std::string& entity,
             std::map<std::string, std::string>& out_bindings) const;
  /// A number written as such, read whole: 150, -2.5; anything else is not one.
  static bool number(const std::string& text, double& out_number);
  /// A value written in a sequence: a number or a string, not a name.
  static bool literal(const std::string& text);
  /// Whether `a` and `z` stand in the comparison: numbers as numbers, anything else equal or not as written.
  static bool compare(model::LiteralForm form, const std::string& a, const std::string& z);

  const Facts& facts_;
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_SOLVER_HPP
