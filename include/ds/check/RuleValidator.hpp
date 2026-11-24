#ifndef DRIFTSPEC_CHECK_RULEVALIDATOR_HPP
#define DRIFTSPEC_CHECK_RULEVALIDATOR_HPP

#include <regex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ds/check/Facts.hpp"
#include "ds/check/Resolver.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace check {

/// Whether each rule, definition and table is well formed before it is evaluated (REQ-00097-1, DS-380): everything
/// it names exists in the metamodel or the model, every definition it applies takes that many arguments, every
/// variable is bound by a positive literal before anything tests it, and no relation literal can never match. It
/// reads the facts, resolves names with the resolver and keeps what it finds for the evaluator to take.
class RuleValidator {
 public:
  RuleValidator(const model::Metamodel& metamodel, const Resolver& resolver, const Facts& facts)
      : metamodel_(metamodel), resolver_(resolver), facts_(facts) {}

  /// Whether a formula is well formed; each name it writes and the identity it resolves to ("" when it names nothing)
  /// is added to `out_resolved` in the order it is read.
  bool validate(const std::string& id, const model::Formula& formula, std::vector<std::pair<std::string, std::string>>& out_resolved);
  /// A relation literal whose bound end is a kind the relation never joins there (W-RULE-NEVER-MATCHES, DS-213).
  void checkVacuity(const std::string& id, const model::Formula& formula);
  /// What it has found so far, in the order found; the list is left empty.
  std::vector<model::Finding> take();
  /// The patterns the formula it last validated matches against, each as written and compiled once (DS-403); the list
  /// is left empty.
  std::vector<std::pair<std::string, std::regex>> takePatterns();

 private:
  bool validateBody(const std::string& id, const std::vector<model::Literal>& body, std::set<std::string>& out_bound,
                    std::vector<std::pair<std::string, std::string>>& out_resolved);
  /// Whether the pattern a literal matches against is a string that compiles; kept compiled when it does (DS-403).
  bool validatePattern(const std::string& id, const model::Literal& l);
  void report(model::Code code, const std::string& owner, const std::string& message, const model::Location& at);
  /// Whether a relation of the metamodel may leave (or reach) an entity of a kind.
  static bool joinsAtEnd(const model::RelationSpec& spec, model::Kind kind, bool from);

  const model::Metamodel& metamodel_;
  const Resolver& resolver_;
  const Facts& facts_;
  std::vector<model::Finding> found_;
  std::vector<std::pair<std::string, std::regex>> patterns_;  ///< compiled since the last takePatterns
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_RULEVALIDATOR_HPP
