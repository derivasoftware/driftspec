#include "ds/machine/StateRule.hpp"

#include <cstdlib>

#include "ds/machine/WrittenFields.hpp"

namespace ds {
namespace machine {

std::vector<StateRule> StateRule::of(const model::Entity& machine) {
  std::vector<StateRule> out;  // the records the binder read (DS-384)
  for (const model::WrittenRule& written : machine.rules) {
    StateRule rule;
    rule.state = written.state;
    std::vector<std::string> said;
    for (const model::RuleLiteral& l : written.literals) {
      rule.literals.push_back({l.negated, l.left, l.op, l.right});
      said.push_back(rule.literals.back().written());
    }
    rule.text = WrittenFields::joined(said, ", ");
    rule.line = written.location.line;
    out.push_back(rule);
  }
  return out;
}

}  // namespace machine
}  // namespace ds
