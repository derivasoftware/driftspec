#include "ds/machine/StateRule.hpp"

#include <cstdlib>

#include "ds/machine/WrittenFields.hpp"

namespace ds {
namespace machine {

std::vector<StateRule> StateRule::of(const model::Entity& machine) {
  std::vector<StateRule> out;
  for (const std::string& line : WrittenFields::pieces(machine.field("rules"), '\n')) {
    const std::vector<std::string> parts = WrittenFields::pieces(line, '|');
    if (parts.size() < 3) continue;
    StateRule rule;
    rule.state = parts[0];
    std::vector<std::string> said;
    for (const std::string& text : WrittenFields::pieces(parts[1], ',')) {
      rule.literals.push_back(Literal::read(text));
      said.push_back(rule.literals.back().written());
    }
    rule.text = WrittenFields::joined(said, ", ");
    rule.line = std::atoi(parts[2].c_str());
    out.push_back(rule);
  }
  return out;
}

}  // namespace machine
}  // namespace ds
